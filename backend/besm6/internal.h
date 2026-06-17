#ifndef BESM6_INTERNAL_H
#define BESM6_INTERNAL_H

#include <stdio.h>

#include "besm.h"
#include "frame.h"
#include "tac.h"

// Shared helpers used across the BESM-6 codegen translation units
// (codegen.c, emit.c, static.c, instr.c).  Helpers used in only one
// file stay `static` in that file and are not declared here.

_Noreturn void fatal_error(const char *fmt, ...);

// Append a new instruction to a block, maintaining *tail.
Besm_Instr *emit(Besm_Block *block, Besm_Instr **tail, Besm_InstrKind kind);

// Emit XTA: A = mem[reg + off].
void emit_xta(Besm_Block *b, Besm_Instr **t, int reg, int off);

// Emit ATX: mem[reg + off] = A.
void emit_atx(Besm_Block *b, Besm_Instr **t, int reg, int off);

// Frame lookup with fatal_error on miss.
void lookup(const Frame *f, const char *name, int *reg, int *off);

// Emit XTA for a TAC value: variable from frame, UTC+XTA for a global, or =N/=rX for a constant.
void emit_xta_val(Besm_Block *b, Besm_Instr **t, const Frame *f, const Tac_Val *v);

// Emit XTS for a TAC value (push A to stack, load v into A) — used for args 1..N-1.
void emit_xts_val(Besm_Block *b, Besm_Instr **t, const Frame *f, const Tac_Val *v);

// Emit an arithmetic instruction for a TAC value: local, global (via UTC), or constant literal.
void emit_arith_val(Besm_Block *b, Besm_Instr **t, Besm_InstrKind kind, const Frame *f,
                    const Tac_Val *v);

// Lower one TAC instruction (defined in instr.c).
void codegen_instr(const Tac_Instruction *instr, const Frame *f, Besm_Block *block,
                   Besm_Instr **tail);

// Emit a module-level static variable (defined in static.c).  `program` is the full
// toplevel chain, used to fold referenced string constants into this module.
void codegen_static_variable(const Tac_TopLevel *program, const Tac_TopLevel *tl, FILE *out);

// Pack a string static-init into a BESM_DATA_LOG chain; the first word is labeled
// `label` when non-NULL (defined in static.c).
Besm_Instr *besm_string_log_items(const Tac_StaticInit *init, const char *label);

// Fold every string constant `module` references into the module as a local label,
// dropping the constant's external SUBP (defined in static.c).
void besm_fold_string_constants(Besm_Module *module, const Tac_TopLevel *program);

#endif // BESM6_INTERNAL_H
