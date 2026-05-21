#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "abi.h"
#include "besm.h"
#include "codegen.h"
#include "frame.h"
#include "xalloc.h"

// Forward declarations.
static void codegen_function(const Tac_TopLevel *tl, FILE *out);
static void codegen_instr(const Tac_Instruction *instr, const Frame *f,
                          Besm_Block *block, Besm_Instr **tail);

_Noreturn static void fatal_error(const char *fmt, ...)
{
    fprintf(stderr, "codegen error: ");
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

// Append a new instruction to a block, maintaining *tail.
static Besm_Instr *emit(Besm_Block *block, Besm_Instr **tail, Besm_InstrKind kind)
{
    Besm_Instr *i = besm_new_instr(kind);
    if (!block->body)
        block->body = i;
    else
        (*tail)->next = i;
    *tail = i;
    return i;
}

// Emit XTA: A = mem[reg + off].
static void emit_xta(Besm_Block *b, Besm_Instr **t, int reg, int off)
{
    Besm_Instr *i            = emit(b, t, BESM_INSTR_MEM);
    i->u.mem.kind            = BESM_MEM_XTA;
    i->u.mem.u.addr.kind     = BESM_MEM_ADDR_REG;
    i->u.mem.u.addr.reg.num  = reg;
    i->u.mem.u.addr.u.offset = off;
}

// Emit ATX: mem[reg + off] = A.
static void emit_atx(Besm_Block *b, Besm_Instr **t, int reg, int off)
{
    Besm_Instr *i            = emit(b, t, BESM_INSTR_MEM);
    i->u.mem.kind            = BESM_MEM_ATX;
    i->u.mem.u.addr.kind     = BESM_MEM_ADDR_REG;
    i->u.mem.u.addr.reg.num  = reg;
    i->u.mem.u.addr.u.offset = off;
}

// Frame lookup with fatal_error on miss.
static void lookup(const Frame *f, const char *name, int *reg, int *off)
{
    if (!frame_lookup(f, name, reg, off))
        fatal_error("variable '%s' not in frame", name);
}

void codegen_program(const Tac_TopLevel *tl, FILE *out)
{
    switch (tl->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        codegen_function(tl, out);
        break;
    case TAC_TOPLEVEL_STATIC_VARIABLE:
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        fatal_error("TODO: static data generation (Phase C)");
    }
}

static void codegen_function(const Tac_TopLevel *tl, FILE *out)
{
    const char *name = tl->u.function.name;

    int num_params = 0;
    for (const Tac_Param *p = tl->u.function.params; p; p = p->next)
        num_params++;
    bool needs_param_setup = (num_params >= 2) || tl->u.function.variadic;
    bool is_empty          = (tl->u.function.body == NULL);

    Besm_Module *module = besm_new_module(name);
    Besm_Func   *func   = besm_new_func(name, BESM_CC_BESM6_C);
    module->funcs       = func;

    Besm_Block  *block  = besm_new_block();
    func->blocks        = block;

    Besm_Instr *tail = NULL;

    Besm_Instr *iname = emit(block, &tail, BESM_INSTR_NAME);
    iname->u.name     = xstrdup(name);

    if (is_empty) {
        // Optimized prologue for empty functions: no b/save or b/ret.
        if (needs_param_setup) {
            // 14 ,utc, 1
            Besm_Instr *utc14              = emit(block, &tail, BESM_INSTR_MOD);
            utc14->u.mod.kind              = BESM_MOD_UTC;
            utc14->u.mod.addr.kind         = BESM_MEM_ADDR_REG;
            utc14->u.mod.addr.reg.num      = REG_CNT;
            utc14->u.mod.addr.u.offset     = 1;
            // 15 ,utm,
            Besm_Instr *utm15              = emit(block, &tail, BESM_INSTR_REG);
            utm15->u.reg.kind              = BESM_REG_UTM;
            utm15->u.reg.u.vtm.dst.num     = REG_SP;
            utm15->u.reg.u.vtm.value       = 0;
        }
        // 13 ,uj,
        Besm_Instr *uj13                   = emit(block, &tail, BESM_INSTR_BRANCH);
        uj13->u.branch.kind                = BESM_BRANCH_UJ;
        uj13->u.branch.u.addr.kind         = BESM_MEM_ADDR_REG;
        uj13->u.branch.u.addr.reg.num      = REG_RET;
        uj13->u.branch.u.addr.u.offset     = 0;
        emit(block, &tail, BESM_INSTR_END);
    } else {
        // Full prologue: push last argument + capture return address, call b/save,
        // then extend the stack by the number of auto-variable slots.
        Besm_Instr *subp_cret = emit(block, &tail, BESM_INSTR_SUBP);
        subp_cret->u.name     = xstrdup("b/ret");

        Besm_Instr *its13          = emit(block, &tail, BESM_INSTR_MEM);
        its13->u.mem.kind          = BESM_MEM_ITS;
        its13->u.mem.u.ireg        = REG_RET;

        Besm_Instr *call_csave = emit(block, &tail, BESM_INSTR_CALL);
        call_csave->u.name     = xstrdup("b/save");

        Frame *f      = frame_build(tl);
        int num_autos = frame_num_autos(f);
        if (num_autos > 0) {
            Besm_Instr *utm_sp               = emit(block, &tail, BESM_INSTR_REG);
            utm_sp->u.reg.kind               = BESM_REG_UTM;
            utm_sp->u.reg.u.vtm.dst.num      = REG_SP;
            utm_sp->u.reg.u.vtm.value        = num_autos;
        }

        for (const Tac_Instruction *instr = tl->u.function.body; instr; instr = instr->next)
            codegen_instr(instr, f, block, &tail);

        Besm_Instr *uj_cret                     = emit(block, &tail, BESM_INSTR_BRANCH);
        uj_cret->u.branch.kind                  = BESM_BRANCH_UJ;
        uj_cret->u.branch.u.addr.kind           = BESM_MEM_ADDR_LABEL;
        uj_cret->u.branch.u.addr.reg.num        = 0;
        uj_cret->u.branch.u.addr.u.name         = xstrdup("b/ret");

        emit(block, &tail, BESM_INSTR_END);

        frame_free(f);
    }

    emit_madlen_module(out, module);
    besm_free_module(module);
}

static void codegen_instr(const Tac_Instruction *instr, const Frame *f,
                          Besm_Block *block, Besm_Instr **tail)
{
    switch (instr->kind) {
    // COPY  dst = src
    //
    // In C:  b = a;
    // TAC:   copy src → dst   (both src and dst are TAC_VAL_VAR frame slots)
    //
    // BESM-6 sequence:
    //   reg_src ,XTA, off_src   — load src from its frame slot into A
    //   reg_dst ,ATX, off_dst   — store A into dst's frame slot
    //
    // frame_lookup resolves each name to (reg, offset) where reg is REG_PAR (r6)
    // for function parameters or REG_AUTO (r7) for local variables.
    // The XTA/ATX pair works uniformly regardless of which base register is used.
    //
    // COPY from a constant (e.g. b = 0) requires INT-format encoding and is
    // deferred to task #21.
    case TAC_INSTRUCTION_COPY: {
        const Tac_Val *src = instr->u.copy.src;
        const Tac_Val *dst = instr->u.copy.dst;
        if (src->kind != TAC_VAL_VAR)
            fatal_error("TODO: COPY from constant (Phase B task 21)");
        int sr, so, dr, doff;
        lookup(f, src->u.var_name, &sr, &so);
        lookup(f, dst->u.var_name, &dr, &doff);
        emit_xta(block, tail, sr, so);
        emit_atx(block, tail, dr, doff);
        break;
    }
    // GET_ADDRESS  dst = &src
    //
    // In C:  p = &a;
    // TAC:   get_address src → dst   (src is the variable being addressed;
    //                                 dst receives its runtime word address)
    //
    // BESM-6 sequence (r1 is a scratch index register):
    //   reg_src ,MTJ, 1      — M[1] = M[reg_src]: copy the frame base pointer
    //                           (r6 for params, r7 for autos) into r1
    //   1 ,UTM, off_src      — M[1] += off_src: advance r1 to the exact slot
    //                           so r1 now holds the word address of src
    //   ,ITA, 1              — A = M[1]: load that address into the accumulator
    //   reg_dst ,ATX, off_dst — store A (the address) into dst's frame slot
    //
    // The UTM instruction is emitted unconditionally; when off_src == 0 the
    // Madlen emitter omits the address field ("1 ,utm,") but the instruction
    // is still present for uniformity.
    case TAC_INSTRUCTION_GET_ADDRESS: {
        int sr, so, dr, doff;
        lookup(f, instr->u.get_address.src->u.var_name, &sr, &so);
        lookup(f, instr->u.get_address.dst->u.var_name, &dr, &doff);
        Besm_Instr *mtj          = emit(block, tail, BESM_INSTR_MEM);
        mtj->u.mem.kind          = BESM_MEM_MTJ;
        mtj->u.mem.u.mtj.src.num = sr;
        mtj->u.mem.u.mtj.dst_j   = 1;
        Besm_Instr *utm          = emit(block, tail, BESM_INSTR_REG);
        utm->u.reg.kind          = BESM_REG_UTM;
        utm->u.reg.u.vtm.dst.num = 1;
        utm->u.reg.u.vtm.value   = so;
        Besm_Instr *ita   = emit(block, tail, BESM_INSTR_MEM);
        ita->u.mem.kind   = BESM_MEM_ITA;
        ita->u.mem.u.ireg = 1;
        emit_atx(block, tail, dr, doff);
        break;
    }
    // LOAD  dst = *src_ptr
    //
    // In C:  b = *p;
    // TAC:   load *src_ptr → dst   (src_ptr is a pointer variable in the frame;
    //                               dst receives the dereferenced value)
    //
    // BESM-6 sequence (r1 is used as a pointer index register):
    //   reg_ptr ,XTA, off_ptr   — load the pointer value (a word address) into A
    //   ,ATI, 1                 — M[1] = A: store the pointer into index register r1
    //   1 ,XTA, 0               — A = mem[M[1]+0]: dereference — load the word that
    //                              r1 points to into A
    //   reg_dst ,ATX, off_dst   — store the loaded value into dst's frame slot
    //
    // All BESM-6 pointers are word addresses; the offset in the final XTA is always
    // 0 because TAC LOAD always reads the base of the pointed-to object.
    case TAC_INSTRUCTION_LOAD: {
        int pr, po, dr, doff;
        lookup(f, instr->u.load.src_ptr->u.var_name, &pr, &po);
        lookup(f, instr->u.load.dst->u.var_name, &dr, &doff);
        emit_xta(block, tail, pr, po);
        Besm_Instr *ati   = emit(block, tail, BESM_INSTR_MEM);
        ati->u.mem.kind   = BESM_MEM_ATI;
        ati->u.mem.u.ireg = 1;
        emit_xta(block, tail, 1, 0);
        emit_atx(block, tail, dr, doff);
        break;
    }
    // STORE  *dst_ptr = src
    //
    // In C:  *p = a;
    // TAC:   store src → *dst_ptr   (dst_ptr is a pointer variable in the frame;
    //                                src is the value to write through it)
    //
    // BESM-6 sequence (r1 is used as a pointer index register):
    //   reg_ptr ,XTA, off_ptr   — load the pointer value (a word address) into A
    //   ,ATI, 1                 — M[1] = A: store the pointer into index register r1
    //   reg_src ,XTA, off_src   — load the source value into A
    //   1 ,ATX, 0               — mem[M[1]+0] = A: write A through the pointer
    //
    // The pointer must be loaded before the source because ATI consumes A.
    // The write offset is always 0 for the same reason as in LOAD above.
    case TAC_INSTRUCTION_STORE: {
        int pr, po, sr, so;
        lookup(f, instr->u.store.dst_ptr->u.var_name, &pr, &po);
        lookup(f, instr->u.store.src->u.var_name, &sr, &so);
        emit_xta(block, tail, pr, po);
        Besm_Instr *ati   = emit(block, tail, BESM_INSTR_MEM);
        ati->u.mem.kind   = BESM_MEM_ATI;
        ati->u.mem.u.ireg = 1;
        emit_xta(block, tail, sr, so);
        emit_atx(block, tail, 1, 0);
        break;
    }
    // FUN_CALL  [dst =] fun(args...)
    //
    // In C:  OKHO();
    // TAC:   fun_call fun_name [args] [→ dst]
    //
    // BESM-6 sequence (zero-arg call, task #16):
    //   ,CALL, fun_name   — set r13 = return address, jump to fun_name
    //
    // Full argument marshalling (XTS per arg, VTM for arg count) and return-value
    // capture (ATX into dst) are implemented in task #28.  For now only the bare
    // CALL instruction is emitted; the callee's prologue (ITS 13 / CALL b/save)
    // handles the rest of the calling convention.
    case TAC_INSTRUCTION_FUN_CALL: {
        Besm_Instr *call = emit(block, tail, BESM_INSTR_CALL);
        call->u.name     = xstrdup(instr->u.fun_call.fun_name);
        break;
    }
    default:
        fatal_error("TODO: codegen for TAC instruction kind %d (Phase B)", (int)instr->kind);
    }
}
