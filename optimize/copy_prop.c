#include <string.h>
#include "alias.h"
#include "cfg.h"
#include "string_map.h"
#include "tac.h"
#include "xalloc.h"

// ============================================================================
// CopyPair: stored as intptr_t value in StringMap; embeds the key for
// iteration access (map_iterate callbacks only receive the value, not the key).
// ============================================================================

typedef struct {
    char *name;    // owned (xstrdup); same string as the map key
    Tac_Val *src;  // borrowed — points into the live instruction stream
} CopyPair;

static void pair_free(intptr_t value)
{
    CopyPair *p = (CopyPair *)value;
    if (!p) return;
    xfree(p->name);
    xfree(p);
}

static void copy_set_destroy(StringMap *cs)
{
    map_destroy_free(cs, pair_free);
}

// ============================================================================
// KeyBuf: dynamic array for deferred key removal (cannot modify the AVL tree
// during map_iterate).
// ============================================================================

typedef struct {
    char **keys;
    int count;
    int cap;
} KeyBuf;

static void keybuf_push(KeyBuf *kb, char *key)
{
    if (kb->count == kb->cap) {
        int new_cap = kb->cap ? kb->cap * 2 : 8;
        char **new_keys = xalloc(new_cap * sizeof(char *), __func__, __FILE__, __LINE__);
        for (int i = 0; i < kb->count; i++)
            new_keys[i] = kb->keys[i];
        xfree(kb->keys);
        kb->keys   = new_keys;
        kb->cap    = new_cap;
    }
    kb->keys[kb->count++] = key;
}

// Remove all collected keys from cs; free the associated CopyPair values.
// kb.keys[i] points into the CopyPair's name field; pair_free will free it.
// map_remove_key frees the StringNode's own copy of the key string.
static void keybuf_flush(KeyBuf *kb, StringMap *cs)
{
    for (int i = 0; i < kb->count; i++) {
        intptr_t old_val = 0;
        map_get(cs, kb->keys[i], &old_val);
        map_remove_key(cs, kb->keys[i]);
        if (old_val)
            pair_free(old_val);
    }
    xfree(kb->keys);
    kb->keys  = NULL;
    kb->count = kb->cap = 0;
}

// ============================================================================
// copy_set_copy: deep-copy src into dst (dst must be a freshly initialised
// empty map).
// ============================================================================

typedef struct { StringMap *dst; } CopyCtx;

static void copy_cb(intptr_t value, const void *arg)
{
    const CopyCtx *ctx = (const CopyCtx *)arg;
    const CopyPair *sp = (const CopyPair *)value;
    CopyPair *np = xalloc(sizeof(CopyPair), __func__, __FILE__, __LINE__);
    np->name = xstrdup(sp->name);
    np->src  = sp->src;
    map_insert_free(ctx->dst, np->name, (intptr_t)np, 0, pair_free);
}

static void copy_set_copy(StringMap *dst, const StringMap *src)
{
    CopyCtx ctx = { dst };
    map_iterate((StringMap *)src, copy_cb, &ctx);
}

// ============================================================================
// kill_name: remove all entries where key == name or src is Var(name).
// ============================================================================

typedef struct {
    KeyBuf     *kb;
    const char *name;
} KillNameCtx;

static void kill_name_cb(intptr_t value, const void *arg)
{
    const KillNameCtx *ctx  = (const KillNameCtx *)arg;
    const CopyPair    *pair = (const CopyPair *)value;
    if (strcmp(pair->name, ctx->name) == 0 ||
        (pair->src->kind == TAC_VAL_VAR &&
         strcmp(pair->src->u.var_name, ctx->name) == 0)) {
        keybuf_push(ctx->kb, pair->name);
    }
}

static void kill_name(StringMap *cs, const char *name)
{
    KeyBuf      kb  = {0};
    KillNameCtx ctx = { &kb, name };
    map_iterate(cs, kill_name_cb, &ctx);
    keybuf_flush(&kb, cs);
}

// ============================================================================
// kill_alias_set: remove all entries where key or src-var is in alias map.
// ============================================================================

typedef struct {
    KeyBuf          *kb;
    const StringMap *alias;
} KillAliasCtx;

static void kill_alias_cb(intptr_t value, const void *arg)
{
    const KillAliasCtx *ctx  = (const KillAliasCtx *)arg;
    const CopyPair     *pair = (const CopyPair *)value;
    bool kill = map_get((StringMap *)ctx->alias, pair->name, NULL);
    if (!kill && pair->src->kind == TAC_VAL_VAR)
        kill = map_get((StringMap *)ctx->alias, pair->src->u.var_name, NULL);
    if (kill)
        keybuf_push(ctx->kb, pair->name);
}

static void kill_alias_set(StringMap *cs, const StringMap *alias)
{
    KeyBuf       kb  = {0};
    KillAliasCtx ctx = { &kb, alias };
    map_iterate(cs, kill_alias_cb, &ctx);
    keybuf_flush(&kb, cs);
}

// ============================================================================
// get_defining_dst: returns the dst Tac_Val* for instructions that define a
// named variable (excludes COPY, FUN_CALL, STORE — handled separately).
// Returns NULL for non-defining instructions.
// ============================================================================

static const Tac_Val *get_defining_dst(const Tac_Instruction *ins)
{
    switch (ins->kind) {
    case TAC_INSTRUCTION_UNARY:
        return ins->u.unary.dst;
    case TAC_INSTRUCTION_BINARY:
        return ins->u.binary.dst;
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
        return ins->u.sign_extend.dst;   // all 14 conversions share this layout
    case TAC_INSTRUCTION_GET_ADDRESS:
        return ins->u.get_address.dst;
    case TAC_INSTRUCTION_LOAD:
        return ins->u.load.dst;
    case TAC_INSTRUCTION_ADD_PTR:
        return ins->u.add_ptr.dst;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
        return ins->u.copy_from_offset.dst;
    default:
        return NULL;
    }
}

// ============================================================================
// apply_transfer: update copy-set cs in place for one instruction.
// ============================================================================

static void apply_transfer(StringMap *cs, const Tac_Instruction *ins,
                            const StringMap *static_names,
                            const StringMap *address_taken)
{
    if (ins->kind == TAC_INSTRUCTION_COPY) {
        const Tac_Val *dst = ins->u.copy.dst;
        if (dst->kind == TAC_VAL_VAR) {
            kill_name(cs, dst->u.var_name);
            CopyPair *p = xalloc(sizeof(CopyPair), __func__, __FILE__, __LINE__);
            p->name = xstrdup(dst->u.var_name);
            p->src  = ins->u.copy.src;
            map_insert_free(cs, p->name, (intptr_t)p, 0, pair_free);
        }
        return;
    }

    if (ins->kind == TAC_INSTRUCTION_FUN_CALL) {
        kill_alias_set(cs, static_names);
        kill_alias_set(cs, address_taken);
        const Tac_Val *dst = ins->u.fun_call.dst;
        if (dst && dst->kind == TAC_VAL_VAR)
            kill_name(cs, dst->u.var_name);
        return;
    }

    if (ins->kind == TAC_INSTRUCTION_STORE) {
        kill_alias_set(cs, address_taken);
        return;
    }

    const Tac_Val *dst = get_defining_dst(ins);
    if (dst && dst->kind == TAC_VAL_VAR)
        kill_name(cs, dst->u.var_name);
}

// ============================================================================
// copy_set_intersect: restrict result to entries that agree with other
// (same key and src); removes any entry absent or conflicting in other.
// ============================================================================

typedef struct {
    KeyBuf          *kb;
    const StringMap *other;
} IntersectCtx;

static void intersect_cb(intptr_t value, const void *arg)
{
    const IntersectCtx *ctx  = (const IntersectCtx *)arg;
    const CopyPair     *pair = (const CopyPair *)value;
    intptr_t oval = 0;
    if (!map_get((StringMap *)ctx->other, pair->name, &oval)) {
        keybuf_push(ctx->kb, pair->name);
        return;
    }
    const CopyPair *op = (const CopyPair *)oval;
    if (!tac_compare_val(pair->src, op->src))
        keybuf_push(ctx->kb, pair->name);
}

static void copy_set_intersect(StringMap *result, const StringMap *other)
{
    KeyBuf       kb  = {0};
    IntersectCtx ctx = { &kb, other };
    map_iterate(result, intersect_cb, &ctx);
    keybuf_flush(&kb, result);
}

// ============================================================================
// copy_set_equal: returns true iff a and b have identical (name, src) entries.
// ============================================================================

typedef struct {
    bool            *equal;
    const StringMap *other;
} EqualCtx;

static void equal_cb(intptr_t value, const void *arg)
{
    const EqualCtx *ctx  = (const EqualCtx *)arg;
    if (!*ctx->equal) return;
    const CopyPair *pair = (const CopyPair *)value;
    intptr_t oval = 0;
    if (!map_get((StringMap *)ctx->other, pair->name, &oval)) {
        *ctx->equal = false;
        return;
    }
    const CopyPair *op = (const CopyPair *)oval;
    if (!tac_compare_val(pair->src, op->src))
        *ctx->equal = false;
}

static bool copy_set_equal(const StringMap *a, const StringMap *b)
{
    bool     eq  = true;
    EqualCtx ctx = { &eq, b };
    map_iterate((StringMap *)a, equal_cb, &ctx);
    if (!eq) return false;
    ctx.other = a;
    map_iterate((StringMap *)b, equal_cb, &ctx);
    return eq;
}

// ============================================================================
// dup_val: deep-copy a single Tac_Val node without following .next.
// ============================================================================

static Tac_Val *dup_val(const Tac_Val *v)
{
    Tac_Val *nv = tac_new_val(v->kind);
    if (v->kind == TAC_VAL_CONSTANT) {
        Tac_Const *nc = tac_new_const(v->u.constant->kind);
        *nc = *v->u.constant;
        nv->u.constant = nc;
    } else {
        nv->u.var_name = xstrdup(v->u.var_name);
    }
    return nv;
}

// ============================================================================
// subst_val: if *vp is Var(x) and cs maps x→src, replace *vp with dup of src.
// ============================================================================

static void subst_val(Tac_Val **vp, const StringMap *cs)
{
    Tac_Val *v = *vp;
    if (!v || v->kind != TAC_VAL_VAR) return;
    intptr_t oval = 0;
    if (!map_get((StringMap *)cs, v->u.var_name, &oval)) return;
    const CopyPair *p = (const CopyPair *)oval;
    Tac_Val *repl = dup_val(p->src);
    v->next = NULL;    // isolate before freeing; tac_free_val follows .next
    tac_free_val(v);
    *vp = repl;
}

// ============================================================================
// subst_args: substitute each element of the fun_call.args linked list.
// ============================================================================

static void subst_args(Tac_Val **head, const StringMap *cs)
{
    while (*head) {
        Tac_Val *arg = *head;
        if (arg->kind == TAC_VAL_VAR) {
            intptr_t oval = 0;
            if (map_get((StringMap *)cs, arg->u.var_name, &oval)) {
                const CopyPair *p  = (const CopyPair *)oval;
                Tac_Val        *repl = dup_val(p->src);
                repl->next = arg->next;
                *head      = repl;
                arg->next  = NULL;
                tac_free_val(arg);
                head = &repl->next;
                continue;
            }
        }
        head = &(*head)->next;
    }
}

// ============================================================================
// subst_instruction: substitute all source operands (not dst) of one instruction.
// GET_ADDRESS.src is intentionally excluded: it is address-taken, not a value use.
// ============================================================================

static void subst_instruction(Tac_Instruction *ins, const StringMap *cs)
{
    switch (ins->kind) {
    case TAC_INSTRUCTION_RETURN:
        if (ins->u.return_.src)
            subst_val(&ins->u.return_.src, cs);
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
        subst_val(&ins->u.sign_extend.src, cs);
        break;
    case TAC_INSTRUCTION_UNARY:
        subst_val(&ins->u.unary.src, cs);
        break;
    case TAC_INSTRUCTION_BINARY:
        subst_val(&ins->u.binary.src1, cs);
        subst_val(&ins->u.binary.src2, cs);
        break;
    case TAC_INSTRUCTION_COPY:
        subst_val(&ins->u.copy.src, cs);
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
        break;
    case TAC_INSTRUCTION_LOAD:
        subst_val(&ins->u.load.src_ptr, cs);
        break;
    case TAC_INSTRUCTION_STORE:
        subst_val(&ins->u.store.src, cs);
        subst_val(&ins->u.store.dst_ptr, cs);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        subst_val(&ins->u.add_ptr.ptr, cs);
        subst_val(&ins->u.add_ptr.index, cs);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
        subst_val(&ins->u.copy_to_offset.src, cs);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
        subst_val(&ins->u.jump_if_zero.condition, cs);
        break;
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        subst_val(&ins->u.jump_if_not_zero.condition, cs);
        break;
    case TAC_INSTRUCTION_FUN_CALL:
        subst_args(&ins->u.fun_call.args, cs);
        break;
    default:
        break;
    }
}

// ============================================================================
// propagate_copies: main entry point.
// ============================================================================

void propagate_copies(OptCfg *cfg, const Tac_TopLevel *toplevel)
{
    if (cfg->nblocks == 0) return;

    // Task 16: alias pre-analysis ----------------------------------------

    StringMap static_names, address_taken;
    collect_alias_sets(cfg, toplevel, &static_names, &address_taken);

    // Task 17: reaching-copies dataflow ----------------------------------

    int n = cfg->nblocks;

    // Build predecessor lists from successor edges.
    int  *npreds = xalloc(n * sizeof(int),   __func__, __FILE__, __LINE__);
    int **preds  = xalloc(n * sizeof(int *), __func__, __FILE__, __LINE__);
    for (int i = 0; i < n; i++)
        npreds[i] = 0;
    for (int i = 0; i < n; i++) {
        const OptBlock *b = cfg->blocks[i];
        for (int j = 0; j < b->nsucc; j++)
            npreds[b->succs[j]->id]++;
    }
    for (int i = 0; i < n; i++) {
        preds[i] = npreds[i]
            ? xalloc(npreds[i] * sizeof(int), __func__, __FILE__, __LINE__)
            : NULL;
        npreds[i] = 0;
    }
    for (int i = 0; i < n; i++) {
        const OptBlock *b = cfg->blocks[i];
        for (int j = 0; j < b->nsucc; j++) {
            int sid = b->succs[j]->id;
            preds[sid][npreds[sid]++] = i;
        }
    }

    // Allocate in/out sets, all starting empty (no copies reaching).
    StringMap *in_sets  = xalloc(n * sizeof(StringMap), __func__, __FILE__, __LINE__);
    StringMap *out_sets = xalloc(n * sizeof(StringMap), __func__, __FILE__, __LINE__);
    for (int i = 0; i < n; i++) {
        map_init(&in_sets[i]);
        map_init(&out_sets[i]);
    }

    // Fixpoint iteration: repeat until no out-set changes.
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < n; i++) {
            const OptBlock *b = cfg->blocks[i];
            if (!b->reachable || !b->first)
                continue;

            // Compute new in[b] = intersection of out[pred] for all preds.
            StringMap new_in;
            map_init(&new_in);
            if (npreds[i] > 0) {
                copy_set_copy(&new_in, &out_sets[preds[i][0]]);
                for (int k = 1; k < npreds[i]; k++)
                    copy_set_intersect(&new_in, &out_sets[preds[i][k]]);
            }

            // Compute new out[b] = transfer(new_in, block instructions).
            StringMap new_out;
            map_init(&new_out);
            copy_set_copy(&new_out, &new_in);
            for (const Tac_Instruction *ins = b->first; ins; ins = ins->next)
                apply_transfer(&new_out, ins, &static_names, &address_taken);

            if (!copy_set_equal(&new_out, &out_sets[i]))
                changed = true;

            copy_set_destroy(&in_sets[i]);
            copy_set_destroy(&out_sets[i]);
            in_sets[i]  = new_in;
            out_sets[i] = new_out;
        }
    }

    // Task 18: substitution
    for (int i = 0; i < n; i++) {
        OptBlock *b = cfg->blocks[i];
        if (!b->reachable || !b->first)
            continue;

        StringMap current_in;
        map_init(&current_in);
        copy_set_copy(&current_in, &in_sets[i]);

        Tac_Instruction *prev = NULL;
        Tac_Instruction *ins  = b->first;
        while (ins) {
            Tac_Instruction *next = ins->next;

            subst_instruction(ins, &current_in);

            // Remove self-copies: Copy(Var(x), Var(x))
            if (ins->kind == TAC_INSTRUCTION_COPY &&
                ins->u.copy.src->kind == TAC_VAL_VAR &&
                ins->u.copy.dst->kind == TAC_VAL_VAR &&
                strcmp(ins->u.copy.src->u.var_name,
                       ins->u.copy.dst->u.var_name) == 0) {
                if (prev)
                    prev->next = next;
                else
                    b->first = next;
                if (b->last == ins)
                    b->last = prev;
                ins->next = NULL;
                tac_free_instruction(ins);
                ins = next;
                continue;
            }

            apply_transfer(&current_in, ins, &static_names, &address_taken);
            prev = ins;
            ins  = next;
        }

        copy_set_destroy(&current_in);
    }

    // Cleanup.
    for (int i = 0; i < n; i++) {
        copy_set_destroy(&in_sets[i]);
        copy_set_destroy(&out_sets[i]);
        xfree(preds[i]);
    }
    xfree(in_sets);
    xfree(out_sets);
    xfree(preds);
    xfree(npreds);
    map_destroy(&static_names);
    map_destroy(&address_taken);
}
