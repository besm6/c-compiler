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

    Besm_Module *module = besm_new_module(name);
    Besm_Func   *func   = besm_new_func(name, BESM_CC_BESM6_C);
    module->funcs       = func;

    Besm_Block  *block  = besm_new_block();
    func->blocks        = block;

    Besm_Instr *tail = NULL;

    // Subprogram header directives.
    Besm_Instr *iname = emit(block, &tail, BESM_INSTR_NAME);
    iname->u.name     = xstrdup(name);

    // Prologue: push last argument + capture return address, then call c/save,
    // then extend the stack by the number of auto-variable slots.
    Besm_Instr *its13          = emit(block, &tail, BESM_INSTR_MEM);
    its13->u.mem.kind          = BESM_MEM_ITS;
    its13->u.mem.u.ireg        = REG_RET;

    Besm_Instr *vjm_csave                             = emit(block, &tail, BESM_INSTR_BRANCH);
    vjm_csave->u.branch.kind                          = BESM_BRANCH_VJM;
    vjm_csave->u.branch.u.jump.reg.num                = REG_RET;
    vjm_csave->u.branch.u.jump.tgt.kind               = BESM_TARGET_LABEL;
    vjm_csave->u.branch.u.jump.tgt.u.name             = xstrdup("c/save");

    // Build frame map (params → REG_PAR slots, temporaries → REG_AUTO slots).
    Frame *f      = frame_build(tl);
    int num_autos = frame_num_autos(f);
    if (num_autos > 0) {
        Besm_Instr *utm_sp               = emit(block, &tail, BESM_INSTR_REG);
        utm_sp->u.reg.kind               = BESM_REG_UTM;
        utm_sp->u.reg.u.vtm.dst.num      = REG_SP;
        utm_sp->u.reg.u.vtm.value        = num_autos;
    }

    // Instruction selection — each TAC instruction is a Phase B stub.
    for (const Tac_Instruction *instr = tl->u.function.body; instr; instr = instr->next)
        codegen_instr(instr, f, block, &tail);

    // Epilogue: return via c/ret.
    Besm_Instr *uj_cret                         = emit(block, &tail, BESM_INSTR_BRANCH);
    uj_cret->u.branch.kind                      = BESM_BRANCH_UJ;
    uj_cret->u.branch.u.addr.kind               = BESM_MEM_ADDR_LABEL;
    uj_cret->u.branch.u.addr.reg.num            = 0;
    uj_cret->u.branch.u.addr.u.name             = xstrdup("c/ret");

    emit(block, &tail, BESM_INSTR_END);

    emit_madlen_module(out, module);

    besm_free_module(module);
    frame_free(f);
}

// Phase B stubs — one per TAC instruction kind.
static void codegen_instr(const Tac_Instruction *instr, const Frame *f,
                          Besm_Block *block, Besm_Instr **tail)
{
    (void)f;
    (void)block;
    (void)tail;

    fatal_error("TODO: codegen for TAC instruction kind %d (Phase B)", (int)instr->kind);
}
