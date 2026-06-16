#include "codegen.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "abi.h"
#include "besm.h"
#include "frame.h"
#include "internal.h"
#include "string_map.h"
#include "tac.h"
#include "xalloc.h"

// Forward declaration.
static void codegen_function(const Tac_TopLevel *program, const Tac_TopLevel *tl, FILE *out);

void codegen_program(const Tac_TopLevel *program, const Tac_TopLevel *tl, FILE *out)
{
    switch (tl->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        codegen_function(program, tl, out);
        break;
    case TAC_TOPLEVEL_STATIC_VARIABLE:
        codegen_static_variable(tl, out);
        break;
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        codegen_static_constant(tl, out);
        break;
    case TAC_TOPLEVEL_DECLARE_ARRAY:
        // Extern array declaration: allocates no storage and emits nothing.
        // It only informs global_is_array() that the name decays to its address.
        break;
    }
}

// Declare `v` as an external (SUBP) if it is a module-level name that has no
// frame slot and has not been declared yet.  SUBP allocates no memory; it just
// tells the single-pass assembler the name is external, and must precede the
// first UTC that references it.
static void declare_global_name(Besm_Block *block, Besm_Instr **tail, const Frame *f,
                                StringMap *declared, const char *name)
{
    int sr, so;
    intptr_t dummy;
    if (frame_lookup(f, name, &sr, &so))
        return; // local / param
    if (map_get(declared, name, &dummy))
        return; // already declared
    Besm_Instr *ssubp = emit(block, tail, BESM_STMT_SUBP);
    ssubp->name       = xstrdup(name);
    map_insert(declared, name, 1, 0);
}

static void declare_global_operand(Besm_Block *block, Besm_Instr **tail, const Frame *f,
                                   StringMap *declared, const Tac_Val *v)
{
    if (!v || v->kind != TAC_VAL_VAR)
        return;
    declare_global_name(block, tail, f, declared, v->u.var_name);
}

//
// After the peephole pass, compute how many auto-variable words the frame still
// needs.  Peephole can delete the only store/reload of a temporary (collapsing it
// into pure A-register dataflow), leaving its slot unreferenced, so the prologue
// stack extension emitted up front may now reserve dead words.
//
// Only compiler temporaries ('%'+digit) are reclaimed, and only when nothing in the
// final stream references their slot.  Named locals and aggregates are always kept:
// their address can be taken (e.g. `&x`), and GET_ADDRESS of a slot-0 local emits
// `ita 7` — the register number rides in `addr`, not `reg` — so a direct reg==REG_AUTO
// scan would miss it.  Temporaries are never address-taken, so a reg==REG_AUTO load/
// store fully captures their use.  We therefore keep every non-temp slot plus every
// referenced temp slot, and shrink to the highest such offset + 1 (slots are absolute
// r7+off offsets and cannot be renumbered, so only trailing dead temps are reclaimed).
//
static int used_auto_words(const Besm_Func *func, const Frame *f)
{
    int orig = frame_num_autos(f);
    if (orig <= 0)
        return 0;

    bool *referenced = (bool *)xalloc(orig * sizeof(bool), __func__, __FILE__, __LINE__);
    for (int i = 0; i < orig; i++)
        referenced[i] = false;
    for (const Besm_Func *fn = func; fn; fn = fn->next)
        for (const Besm_Block *block = fn->blocks; block; block = block->next)
            for (const Besm_Instr *i = block->body; i; i = i->next)
                if (i->name == NULL && (int)i->reg == REG_AUTO && i->addr >= 0 &&
                    i->addr < orig)
                    referenced[i->addr] = true;

    int used = 0;
    for (int off = 0; off < orig; off++)
        if (!frame_slot_is_temp(f, REG_AUTO, off) || referenced[off])
            used = off + 1; // this slot must stay reserved

    xfree(referenced);
    return used;
}

// Unlink `target` from the block's instruction list and free it.  `besm_free_instr`
// recurses on ->next, so unlink first (set target->next = NULL) to free only `target`.
static void remove_instr(Besm_Block *block, const Besm_Instr *target)
{
    Besm_Instr *prev = NULL;
    for (Besm_Instr *i = block->body; i; prev = i, i = i->next) {
        if (i != target)
            continue;
        if (prev)
            prev->next = i->next;
        else
            block->body = i->next;
        i->next = NULL;
        besm_free_instr(i);
        return;
    }
}

static void codegen_function(const Tac_TopLevel *program, const Tac_TopLevel *tl, FILE *out)
{
    const char *name = tl->u.function.name;

    int num_params = 0;
    for (const Tac_Param *p = tl->u.function.params; p; p = p->next)
        num_params++;
    bool needs_param_setup = (num_params >= 2) || tl->u.function.variadic;
    bool is_empty          = (tl->u.function.body == NULL);

    Besm_Module *module = besm_new_module(name);
    Besm_Func *func     = besm_new_func(name, BESM_CC_BESM6_C);
    module->funcs       = func;

    Besm_Block *block = besm_new_block();
    func->blocks      = block;

    Besm_Instr *tail   = NULL;
    Frame *f           = NULL; // built for non-empty functions; passed to the peephole pass
    Besm_Instr *utm_sp = NULL; // prologue stack-extension; shrunk/dropped after peephole

    Besm_Instr *iname = emit(block, &tail, BESM_STMT_NAME);
    iname->name       = xstrdup(name);

    if (is_empty) {
        // Optimized prologue for empty functions: no b/save or b/ret.
        if (strcmp(name, "main") == 0) {
            Besm_Instr *entry_prog = emit(block, &tail, BESM_STMT_ENTRY);
            entry_prog->name       = xstrdup("program");
        }
        if (needs_param_setup) {
            // 14 ,utc, 1
            Besm_Instr *utc14 = emit(block, &tail, BESM_MOD_UTC);
            utc14->reg        = REG_CNT;
            utc14->addr       = 1;
            // 15 ,utm,
            Besm_Instr *utm15 = emit(block, &tail, BESM_REG_UTM);
            utm15->reg        = REG_SP;
        }
        // 13 ,uj,
        Besm_Instr *uj13 = emit(block, &tail, BESM_BRANCH_UJ);
        uj13->reg        = REG_RET;
        emit(block, &tail, BESM_STMT_END);
    } else {
        // Full prologue: push last argument + capture return address, call b/save,
        // then extend the stack by the number of auto-variable slots.
        //
        // Build the frame early so we can declare SUBP references for static
        // constants before the first instruction that uses them (single-pass assembler).
        f             = frame_build(tl, program);
        int num_autos = frame_num_autos(f);

        Besm_Instr *subp_cret = emit(block, &tail, BESM_STMT_SUBP);
        subp_cret->name       = xstrdup("b/ret");

        if (strcmp(name, "main") == 0) {
            Besm_Instr *entry_prog = emit(block, &tail, BESM_STMT_ENTRY);
            entry_prog->name       = xstrdup("program");
        }

        // Declare each module-level name a function references as a SUBP word.  SUBP
        // allocates no memory; it just tells the assembler the name is external.  It
        // must appear before the first instruction that uses the name (single-pass
        // assembler), so scan every operand of every instruction up front.  A name with
        // a frame slot is a local/param and is skipped; a map avoids duplicates.
        StringMap declared;
        map_init(&declared);
        for (const Tac_Instruction *instr = tl->u.function.body; instr; instr = instr->next) {
            switch (instr->kind) {
            case TAC_INSTRUCTION_RETURN:
                declare_global_operand(block, &tail, f, &declared, instr->u.return_.src);
                break;
            case TAC_INSTRUCTION_COPY:
                declare_global_operand(block, &tail, f, &declared, instr->u.copy.src);
                declare_global_operand(block, &tail, f, &declared, instr->u.copy.dst);
                break;
            case TAC_INSTRUCTION_GET_ADDRESS:
            case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
            case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
                declare_global_operand(block, &tail, f, &declared, instr->u.get_address.src);
                break;
            case TAC_INSTRUCTION_UNARY:
                declare_global_operand(block, &tail, f, &declared, instr->u.unary.src);
                declare_global_operand(block, &tail, f, &declared, instr->u.unary.dst);
                break;
            case TAC_INSTRUCTION_BINARY:
                declare_global_operand(block, &tail, f, &declared, instr->u.binary.src1);
                declare_global_operand(block, &tail, f, &declared, instr->u.binary.src2);
                declare_global_operand(block, &tail, f, &declared, instr->u.binary.dst);
                break;
            case TAC_INSTRUCTION_LOAD:
            case TAC_INSTRUCTION_LOAD_BYTE:
                declare_global_operand(block, &tail, f, &declared, instr->u.load.src_ptr);
                declare_global_operand(block, &tail, f, &declared, instr->u.load.dst);
                break;
            case TAC_INSTRUCTION_STORE:
            case TAC_INSTRUCTION_STORE_BYTE:
                declare_global_operand(block, &tail, f, &declared, instr->u.store.src);
                declare_global_operand(block, &tail, f, &declared, instr->u.store.dst_ptr);
                break;
            case TAC_INSTRUCTION_ADD_PTR:
                declare_global_operand(block, &tail, f, &declared, instr->u.add_ptr.ptr);
                declare_global_operand(block, &tail, f, &declared, instr->u.add_ptr.index);
                declare_global_operand(block, &tail, f, &declared, instr->u.add_ptr.dst);
                break;
            case TAC_INSTRUCTION_PTR_DIFF:
                declare_global_operand(block, &tail, f, &declared, instr->u.ptr_diff.ptr_a);
                declare_global_operand(block, &tail, f, &declared, instr->u.ptr_diff.ptr_b);
                declare_global_operand(block, &tail, f, &declared, instr->u.ptr_diff.dst);
                break;
            case TAC_INSTRUCTION_COPY_TO_OFFSET:
            case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
                declare_global_operand(block, &tail, f, &declared, instr->u.copy_to_offset.src);
                declare_global_name(block, &tail, f, &declared, instr->u.copy_to_offset.dst);
                break;
            case TAC_INSTRUCTION_COPY_FROM_OFFSET:
            case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
                declare_global_name(block, &tail, f, &declared, instr->u.copy_from_offset.src);
                declare_global_operand(block, &tail, f, &declared, instr->u.copy_from_offset.dst);
                break;
            case TAC_INSTRUCTION_JUMP_IF_ZERO:
                declare_global_operand(block, &tail, f, &declared, instr->u.jump_if_zero.condition);
                break;
            case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
                declare_global_operand(block, &tail, f, &declared,
                                       instr->u.jump_if_not_zero.condition);
                break;
            case TAC_INSTRUCTION_FUN_CALL:
                // After copy propagation, a global may appear directly as a FUN_CALL
                // argument (UTC/XTA sequence).
                for (const Tac_Val *a = instr->u.fun_call.args; a; a = a->next)
                    declare_global_operand(block, &tail, f, &declared, a);
                declare_global_operand(block, &tail, f, &declared, instr->u.fun_call.dst);
                break;
            default:
                break;
            }
        }
        map_destroy(&declared);

        Besm_Instr *its13 = emit(block, &tail, BESM_MEM_ITS);
        its13->addr       = REG_RET;

        Besm_Instr *call_csave = emit(block, &tail, BESM_BRANCH_CALL);
        call_csave->name       = xstrdup(num_params == 0 ? "b/save0" : "b/save");

        if (num_autos > 0) {
            utm_sp       = emit(block, &tail, BESM_REG_UTM);
            utm_sp->reg  = REG_SP;
            utm_sp->addr = num_autos;
        }

        for (const Tac_Instruction *instr = tl->u.function.body; instr; instr = instr->next)
            codegen_instr(program, instr, f, block, &tail);

        Besm_Instr *uj_cret = emit(block, &tail, BESM_BRANCH_UJ);
        uj_cret->name       = xstrdup("b/ret");
        emit(block, &tail, BESM_STMT_END);
    }

    // Final polish: peephole-optimize the selected instruction stream before emission.
    // The frame (NULL for empty functions) lets the pass classify temporary slots for
    // dead temp-store elimination; it is freed once the pass no longer needs it.
    besm_peephole(func, f);

    // Peephole may have eliminated every reference to one or more top auto slots, so
    // the prologue stack extension (sized before selection) can now reserve dead words.
    // Shrink it to the slots still in use, dropping the `utm` entirely when none remain.
    if (utm_sp != NULL) {
        int used = used_auto_words(func, f);
        if (used < utm_sp->addr) {
            if (used == 0)
                remove_instr(block, utm_sp);
            else
                utm_sp->addr = used;
        }
    }

    if (f)
        frame_free(f);

    emit_madlen_module(out, module);
    besm_free_module(module);
}
