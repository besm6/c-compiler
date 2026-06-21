// ============================================================================
// dead_store.c — dead-store elimination via liveness analysis.
//
// A store is *dead* if it assigns a value that is never read before the variable
// is overwritten again or the function exits. Such stores have no observable
// effect and can be deleted. We find them with a backward dataflow analysis,
// "variable liveness":
//
//   - A variable is live at a point if some path from there reads it before any
//     redefinition.
//   - Lattice element: a set of live variable names.
//   - Initial value at Exit: the static-duration and address-taken variables
//     (they may be observed by the caller after we return); everything else dead.
//   - Meet at merge points: union — live if live on any outgoing path.
//   - Transfer for one instruction, applied backward:
//       kill def(i): the destination is not live just before the instruction;
//       gen use(i):  every operand the instruction reads becomes live.
//
// An instruction is a dead store when its destination is not in the live set
// *after* it — provided the instruction is pure (is_removable). Side-effecting
// or control-flow instructions are kept even when their dst is dead.
//
// Conservatism around aliasing (see alias.c): static-duration and address-taken
// variables are seeded live at Exit, and a FunCall re-livens them (the callee
// may read them) along with its arguments.
//
// See docs/TAC_Optimization.md §"Dead store elimination".
// ============================================================================

#include <stdbool.h>

#include "alias.h"
#include "cfg.h"
#include "optimize.h"
#include "string_map.h"
#include "tac.h"
#include "xalloc.h"

// ============================================================================
// Live-set primitives. A live set is a StringMap whose key is a variable name;
// the same name is mirrored as the intptr_t value because map_iterate callbacks
// receive only the value, not the key. Membership = the variable is live.
// ============================================================================

static void live_name_free(intptr_t v)
{
    xfree((char *)v);
}

static void live_set_destroy(StringMap *ls)
{
    map_destroy_free(ls, live_name_free);
}

// Skip insert when key already present (avoids duplicate xstrdup).
static void live_add(StringMap *ls, const char *name)
{
    if (!name || map_get(ls, name, NULL))
        return;
    const char *dup = xstrdup(name);
    map_insert(ls, dup, (intptr_t)dup, 0);
}

static void live_add_val(StringMap *ls, const Tac_Val *v)
{
    if (v && v->kind == TAC_VAL_VAR)
        live_add(ls, v->u.var_name);
}

static void live_remove(StringMap *ls, const char *name)
{
    if (!name)
        return;
    intptr_t oval = 0;
    if (!map_get(ls, name, &oval))
        return;
    map_remove_key(ls, name);
    live_name_free(oval);
}

// ============================================================================
// live_set_copy / live_set_union
// ============================================================================

typedef struct {
    StringMap *dst;
} LiveCtx;

static void live_copy_cb(intptr_t value, const void *arg)
{
    live_add(((const LiveCtx *)arg)->dst, (const char *)value);
}

static void live_set_copy(StringMap *dst, const StringMap *src)
{
    LiveCtx ctx = { dst };
    map_iterate((StringMap *)src, live_copy_cb, &ctx);
}

// The meet operator: result ∪= other. Used to combine successors' in-sets into
// a block's out-set, and to seed the Exit out-set with the aliased variables.
static void live_set_union(StringMap *result, const StringMap *other)
{
    LiveCtx ctx = { result };
    map_iterate((StringMap *)other, live_copy_cb, &ctx);
}

// ============================================================================
// live_set_equal: set equality, used as the fixed-point test for the backward
// dataflow loop. Checked in both directions.
// ============================================================================

typedef struct {
    bool *equal;
    const StringMap *other;
} LiveEqualCtx;

static void live_equal_cb(intptr_t value, const void *arg)
{
    const LiveEqualCtx *ctx = (const LiveEqualCtx *)arg;
    if (!*ctx->equal)
        return;
    if (!map_get((StringMap *)ctx->other, (const char *)value, NULL))
        *ctx->equal = false;
}

static bool live_set_equal(const StringMap *a, const StringMap *b)
{
    bool eq          = true;
    LiveEqualCtx ctx = { &eq, b };
    map_iterate((StringMap *)a, live_equal_cb, &ctx);
    if (!eq)
        return false;
    ctx.other = a;
    map_iterate((StringMap *)b, live_equal_cb, &ctx);
    return eq;
}

// ============================================================================
// live_get_dst_name: variable defined by this instruction (or NULL).
// STORE and FUN_CALL are side-effecting: not killed.
// COPY_TO_OFFSET.dst is char* (direct name, not Tac_Val*).
// ============================================================================

static const char *live_get_dst_name(const Tac_Instruction *ins)
{
    switch (ins->kind) {
    case TAC_INSTRUCTION_COPY:
        if (ins->u.copy.dst->kind == TAC_VAL_VAR)
            return ins->u.copy.dst->u.var_name;
        return NULL;
    case TAC_INSTRUCTION_UNARY:
        if (ins->u.unary.dst->kind == TAC_VAL_VAR)
            return ins->u.unary.dst->u.var_name;
        return NULL;
    case TAC_INSTRUCTION_BINARY:
        if (ins->u.binary.dst->kind == TAC_VAL_VAR)
            return ins->u.binary.dst->u.var_name;
        return NULL;
    case TAC_INSTRUCTION_SIGN_EXTEND:
    case TAC_INSTRUCTION_TRUNCATE:
    case TAC_INSTRUCTION_ZERO_EXTEND:
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_INT_TO_FLOAT:
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_INT:
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_PTR_TO_CHAR_PTR:
    case TAC_INSTRUCTION_CHAR_PTR_TO_PTR:
        if (ins->u.sign_extend.dst->kind == TAC_VAL_VAR)
            return ins->u.sign_extend.dst->u.var_name;
        return NULL;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        if (ins->u.get_address.dst->kind == TAC_VAL_VAR)
            return ins->u.get_address.dst->u.var_name;
        return NULL;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        if (ins->u.load.dst->kind == TAC_VAL_VAR)
            return ins->u.load.dst->u.var_name;
        return NULL;
    case TAC_INSTRUCTION_ADD_PTR:
        if (ins->u.add_ptr.dst->kind == TAC_VAL_VAR)
            return ins->u.add_ptr.dst->u.var_name;
        return NULL;
    case TAC_INSTRUCTION_PTR_DIFF:
        if (ins->u.ptr_diff.dst->kind == TAC_VAL_VAR)
            return ins->u.ptr_diff.dst->u.var_name;
        return NULL;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        if (ins->u.copy_from_offset.dst->kind == TAC_VAL_VAR)
            return ins->u.copy_from_offset.dst->u.var_name;
        return NULL;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        return ins->u.copy_to_offset.dst; // char* directly
    default:
        return NULL;
    }
}

// ============================================================================
// live_transfer_backward: the liveness transfer function for one instruction,
// updating live set `ls` in place. Caller walks instructions last-to-first.
// The order is kill-def then gen-use, so an instruction like `x = x + 1` keeps
// x live: x is removed as the def, then re-added as a use.
// ============================================================================

static void live_transfer_backward(StringMap *ls, const Tac_Instruction *ins,
                                   const StringMap *static_names, const StringMap *address_taken)
{
    if (ins->kind == TAC_INSTRUCTION_FUN_CALL ||
        ins->kind == TAC_INSTRUCTION_FUN_CALL_NORETURN) {
        // Callee may read any static or address-taken variable.
        live_set_union(ls, static_names);
        live_set_union(ls, address_taken);
        for (const Tac_Val *a = ins->u.fun_call.args; a; a = a->next)
            live_add_val(ls, a);
        live_add(ls, ins->u.fun_call.fun_name); // indirect call: fun_name is a var
        // FUN_CALL: no defined variable to kill.
        return;
    }

    live_remove(ls, live_get_dst_name(ins));

    switch (ins->kind) {
    case TAC_INSTRUCTION_RETURN:
        live_add_val(ls, ins->u.return_.src);
        break;
    case TAC_INSTRUCTION_SIGN_EXTEND:
    case TAC_INSTRUCTION_TRUNCATE:
    case TAC_INSTRUCTION_ZERO_EXTEND:
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_INT_TO_FLOAT:
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_INT:
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_PTR_TO_CHAR_PTR:
    case TAC_INSTRUCTION_CHAR_PTR_TO_PTR:
        live_add_val(ls, ins->u.sign_extend.src);
        break;
    case TAC_INSTRUCTION_UNARY:
        live_add_val(ls, ins->u.unary.src);
        break;
    case TAC_INSTRUCTION_BINARY:
        live_add_val(ls, ins->u.binary.src1);
        live_add_val(ls, ins->u.binary.src2);
        break;
    case TAC_INSTRUCTION_COPY:
        live_add_val(ls, ins->u.copy.src);
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        live_add_val(ls, ins->u.get_address.src);
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        live_add_val(ls, ins->u.load.src_ptr);
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        live_add_val(ls, ins->u.store.src);
        live_add_val(ls, ins->u.store.dst_ptr);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        live_add_val(ls, ins->u.add_ptr.ptr);
        live_add_val(ls, ins->u.add_ptr.index);
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        live_add_val(ls, ins->u.ptr_diff.ptr_a);
        live_add_val(ls, ins->u.ptr_diff.ptr_b);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        live_add_val(ls, ins->u.copy_to_offset.src);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        live_add(ls, ins->u.copy_from_offset.src); // char*, not Tac_Val*
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
        live_add_val(ls, ins->u.jump_if_zero.condition);
        break;
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        live_add_val(ls, ins->u.jump_if_not_zero.condition);
        break;
    default:
        break;
    }
}

// ============================================================================
// is_removable: true for pure instructions, whose only effect is to define their
// destination — safe to delete when that destination is dead. Excluded (kept
// even when dst is dead): STORE and COPY_TO_OFFSET (write through a pointer /
// into a struct field — observable), FUN_CALL (arbitrary side effects), and all
// control-flow instructions. GET_ADDRESS, LOAD, ADD_PTR and COPY_FROM_OFFSET are
// pure reads and so are removable when their result is unused.
// ============================================================================

static bool is_removable(Tac_InstructionKind kind)
{
    switch (kind) {
    case TAC_INSTRUCTION_COPY:
    case TAC_INSTRUCTION_UNARY:
    case TAC_INSTRUCTION_BINARY:
    case TAC_INSTRUCTION_SIGN_EXTEND:
    case TAC_INSTRUCTION_TRUNCATE:
    case TAC_INSTRUCTION_ZERO_EXTEND:
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_INT_TO_FLOAT:
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_INT:
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_PTR_TO_CHAR_PTR:
    case TAC_INSTRUCTION_CHAR_PTR_TO_PTR:
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
    case TAC_INSTRUCTION_ADD_PTR:
    case TAC_INSTRUCTION_PTR_DIFF:
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        return true;
    default:
        return false;
    }
}

// ============================================================================
// eliminate_dead_stores: entry point. Runs alias pre-analysis, the backward
// liveness fixpoint, then the removal walk, and frees all scaffolding.
// ============================================================================

void eliminate_dead_stores(const OptCfg *cfg, const Tac_TopLevel *fn)
{
    if (cfg->nblocks == 0)
        return;

    // Stage 1: variables that must be treated as live across calls / at exit.
    StringMap static_names, address_taken;
    collect_alias_sets(cfg, fn, &static_names, &address_taken);

    // Stage 2: the backward liveness dataflow.
    int n = cfg->nblocks;

    // Build per-block instruction pointer arrays once (blocks are immutable
    // during analysis; avoids repeated allocation inside the fixpoint loop).
    int *block_nins = xalloc(n * sizeof(int), __func__, __FILE__, __LINE__);
    Tac_Instruction ***block_insts =
        xalloc(n * sizeof(Tac_Instruction **), __func__, __FILE__, __LINE__);
    for (int i = 0; i < n; i++) {
        OptBlock *b = cfg->blocks[i];
        int cnt     = 0;
        for (const Tac_Instruction *ins = b->first; ins; ins = ins->next)
            cnt++;
        block_nins[i] = cnt;
        block_insts[i] =
            cnt ? xalloc(cnt * sizeof(Tac_Instruction *), __func__, __FILE__, __LINE__) : NULL;
        int k = 0;
        for (Tac_Instruction *ins = b->first; ins; ins = ins->next)
            block_insts[i][k++] = ins;
    }

    // Allocate in/out sets (start empty — all variables initially not live).
    StringMap *in_sets  = xalloc(n * sizeof(StringMap), __func__, __FILE__, __LINE__);
    StringMap *out_sets = xalloc(n * sizeof(StringMap), __func__, __FILE__, __LINE__);
    for (int i = 0; i < n; i++) {
        map_init(&in_sets[i]);
        map_init(&out_sets[i]);
    }

    // Backward fixpoint iteration: recompute out[b] then in[b] for every block,
    // visiting blocks in descending order (a backward analysis converges faster
    // bottom-up), until no in-set changes.
    bool changed = true;
    int ds_iter  = 0;
    while (changed) {
        changed = false;
        ds_iter++;
        OPT_TRACE("[dead-store] fixpoint iteration %d\n", ds_iter);
        for (int i = n - 1; i >= 0; i--) {
            const OptBlock *b = cfg->blocks[i];
            // Skip only truly dead blocks. A *reachable* but empty block (its
            // instructions were emptied by unreachable-elim's jump/label cleanup,
            // yet it still carries a successor edge) must participate as an
            // identity node: with zero instructions the transfer below leaves
            // in[b] == out[b], threading a successor's live-in back to this
            // block's predecessors. Skipping it would strand its in-set empty and
            // let a predecessor wrongly drop a store that is live past the gap.
            if (!b->reachable)
                continue;

            // Meet: out[b] = union of in[s] for all successors s.
            StringMap new_out;
            map_init(&new_out);
            for (int j = 0; j < b->nsucc; j++)
                live_set_union(&new_out, &in_sets[b->succs[j]->id]);
            // Exit block (no successors): seed with variables observable after return.
            if (b->nsucc == 0) {
                OPT_TRACE("[dead-store] block %d is exit: seeding live with static+address-taken\n",
                          i);
                live_set_union(&new_out, &static_names);
                live_set_union(&new_out, &address_taken);
            }

            // Transfer: in[b] = backward_transfer(out[b]).
            StringMap new_in;
            map_init(&new_in);
            live_set_copy(&new_in, &new_out);
            for (int j = block_nins[i] - 1; j >= 0; j--)
                live_transfer_backward(&new_in, block_insts[i][j], &static_names, &address_taken);

            if (!live_set_equal(&new_in, &in_sets[i])) {
                OPT_TRACE("[dead-store] block %d in-set changed\n", i);
                changed = true;
            }

            live_set_destroy(&in_sets[i]);
            live_set_destroy(&out_sets[i]);
            in_sets[i]  = new_in;
            out_sets[i] = new_out;
        }
    }
    OPT_TRACE("[dead-store] fixpoint converged after %d iteration(s)\n", ds_iter);

    // Dead store removal: walk each block backward, pruning instructions whose
    // defined variable is dead on exit and which have no observable side-effects.
    // Backward order lets a single pass cascade: when a dead instruction is
    // removed its sources are not added to live, making earlier defs candidates too.
    for (int i = 0; i < n; i++) {
        OptBlock *b = cfg->blocks[i];
        int nins    = block_nins[i];
        if (!b->reachable || nins == 0)
            continue;

        StringMap live;
        map_init(&live);
        live_set_copy(&live, &out_sets[i]);

        for (int j = nins - 1; j >= 0; j--) {
            Tac_Instruction *ins = block_insts[i][j];
            const char *dst      = live_get_dst_name(ins);
            // Dead store: defines a variable that is not live afterward, and is a
            // pure instruction we may drop. Unlink and free it (and do NOT run
            // the transfer, so its sources are not revived — that is what lets
            // chains of dead defs collapse in this single backward pass).
            if (dst && is_removable(ins->kind) && !ins->is_volatile && !map_get(&live, dst, NULL)) {
                OPT_TRACE("[dead-store] block %d: dst '%s' is dead", i, dst);
                opt_trace_instr(" removing:", ins);
                Tac_Instruction *prev = (j > 0) ? block_insts[i][j - 1] : NULL;
                if (prev)
                    prev->next = ins->next;
                else
                    b->first = ins->next;
                if (b->last == ins)
                    b->last = prev;
                ins->next = NULL;
                tac_free_instruction(ins);
            } else {
                live_transfer_backward(&live, ins, &static_names, &address_taken);
            }
        }
        live_set_destroy(&live);
    }

    // Free all dataflow scaffolding and the alias maps.
    for (int i = 0; i < n; i++) {
        live_set_destroy(&in_sets[i]);
        live_set_destroy(&out_sets[i]);
        xfree(block_insts[i]);
    }
    xfree(in_sets);
    xfree(out_sets);
    xfree(block_insts);
    xfree(block_nins);
    map_destroy(&static_names);
    map_destroy(&address_taken);
}
