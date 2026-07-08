#include <stdio.h>
#include <string.h>

#include "besm.h"
#include "frame.h"
#include "internal.h"
#include "tac.h"
#include "xalloc.h"

// Append a new instruction to a block, maintaining *tail.
Besm_Instr *emit(Besm_Block *block, Besm_Instr **tail, Besm_InstrKind kind)
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
void emit_xta(Besm_Block *b, Besm_Instr **t, int reg, int off)
{
    Besm_Instr *i = emit(b, t, BESM_MEM_XTA);
    i->reg        = reg;
    i->addr       = off;
}

// Emit ATX: mem[reg + off] = A.
void emit_atx(Besm_Block *b, Besm_Instr **t, int reg, int off)
{
    Besm_Instr *i = emit(b, t, BESM_MEM_ATX);
    i->reg        = reg;
    i->addr       = off;
}

// Emit an arithmetic instruction: A op= mem[reg + off].
static void emit_arith(Besm_Block *b, Besm_Instr **t, Besm_InstrKind kind, int reg, int off)
{
    Besm_Instr *i = emit(b, t, kind);
    i->reg        = reg;
    i->addr       = off;
}

// Frame lookup with fatal_error on miss.
void lookup(const Frame *f, const char *name, int *reg, int *off)
{
    if (!frame_lookup(f, name, reg, off))
        fatal_error("variable '%s' not in frame", name);
}

// Store the accumulator A into a variable, whether a frame slot or a module-level
// global (UTC sets C to the global's address, then a bare ATX writes mem[C]).
void emit_store_a(Besm_Block *b, Besm_Instr **t, const Frame *f, const char *name)
{
    int reg, off;
    if (frame_lookup(f, name, &reg, &off)) {
        emit_atx(b, t, reg, off);
    } else {
        Besm_Instr *utc = emit(b, t, BESM_MOD_UTC);
        utc->name       = xstrdup(name); // C = &global
        emit(b, t, BESM_MEM_ATX);        // reg=0, addr=0 → mem[C] = A
    }
}

// Set the C address-modifier register to the value of a pointer variable, so a
// following bare XTA/ATX dereferences it (EA = C).  For a frame-resident pointer
// this is a single WTC from its slot.  For a module-level (global) pointer the
// pointer word is not in the frame: UTC sets C to the global's address, then a
// bare WTC reads mem[C] (the pointer word) back into C.  Neither instruction
// touches A, so this is safe between a source load and a store.
void emit_wtc_ptr(Besm_Block *b, Besm_Instr **t, const Frame *f, const char *name)
{
    int reg, off;
    if (frame_lookup(f, name, &reg, &off)) {
        Besm_Instr *wtc = emit(b, t, BESM_MOD_WTC);
        wtc->reg        = reg;
        wtc->addr       = off; // C = pointer word (bits 15:1)
    } else {
        Besm_Instr *utc = emit(b, t, BESM_MOD_UTC);
        utc->name       = xstrdup(name); // C = &global
        emit(b, t, BESM_MOD_WTC);        // reg=0, addr=0 → C = mem[C] = pointer word
    }
}

// Emit ASX addressing a pointer variable's word.  A byte load through a fat pointer shifts
// the accumulator right by the pointer's byte offset, which lives in the exponent field of
// the pointer word itself — so the ASX operand is the pointer word.  For a frame-resident
// pointer this is a single ASX reg=slot.  For a module-level (global) pointer the pointer
// word is not in the frame: UTC sets C to the global's address, then a bare ASX (EA = C)
// uses mem[C] as its operand.  UTC does not touch A, so the shifted value is preserved.
void emit_asx_ptr(Besm_Block *b, Besm_Instr **t, const Frame *f, const char *name)
{
    int reg, off;
    if (frame_lookup(f, name, &reg, &off)) {
        Besm_Instr *asx = emit(b, t, BESM_EXP_SHIFTX);
        asx->reg        = reg;
        asx->addr       = off;
    } else {
        Besm_Instr *utc = emit(b, t, BESM_MOD_UTC);
        utc->name       = xstrdup(name); // C = &global
        emit(b, t, BESM_EXP_SHIFTX);     // reg=0, addr=0 → operand mem[C] = pointer word
    }
}

// Attach a TAC constant to an instruction as a structural operand.  The scalar is
// copied (Tac_Const is a flat POD) so the instruction owns it independently of the
// TAC being freed; each dialect's emitter later formats it into its own literal syntax
// (Madlen =octal/=rX, Unix #const pool, Bemsh =Ю'…').  Kept off `->name` so the
// peephole pass can still use `name == NULL` plus `konst == NULL` to recognise a plain
// frame-slot operand.
static Tac_Const *dup_const(const Tac_Const *c)
{
    Tac_Const *d = tac_new_const(c->kind);
    d->u         = c->u;
    return d;
}

// Emit XTA for a TAC value: variable from frame, UTC+XTA for a global, or a constant.
void emit_xta_val(Besm_Block *b, Besm_Instr **t, const Frame *f, const Tac_Val *v)
{
    if (v->kind == TAC_VAL_VAR) {
        int reg, off;
        if (frame_lookup(f, v->u.var_name, &reg, &off)) {
            emit_xta(b, t, reg, off);
        } else {
            Besm_Instr *utc = emit(b, t, BESM_MOD_UTC);
            utc->name       = xstrdup(v->u.var_name);
            emit_xta(b, t, 0, 0);
        }
    } else {
        Besm_Instr *i = emit(b, t, BESM_MEM_XTA);
        i->konst      = dup_const(v->u.constant);
    }
}

// Emit XTS for a TAC value (push A to stack, load v into A) — used for args 1..N-1.
// For a global variable: UTC sets C to the global's address, then XTS loads mem[C+0].
void emit_xts_val(Besm_Block *b, Besm_Instr **t, const Frame *f, const Tac_Val *v)
{
    if (v->kind == TAC_VAL_VAR) {
        int reg, off;
        if (frame_lookup(f, v->u.var_name, &reg, &off)) {
            Besm_Instr *i = emit(b, t, BESM_MEM_XTS);
            i->reg        = reg;
            i->addr       = off;
        } else {
            Besm_Instr *utc = emit(b, t, BESM_MOD_UTC);
            utc->name       = xstrdup(v->u.var_name);
            Besm_Instr *i   = emit(b, t, BESM_MEM_XTS);
            (void)i; // reg=0, addr=0 → XTS mem[C+0]
        }
    } else {
        Besm_Instr *i = emit(b, t, BESM_MEM_XTS);
        i->konst      = dup_const(v->u.constant);
    }
}

// Emit an arithmetic instruction for a TAC value: local, global (via UTC), or constant literal.
void emit_arith_val(Besm_Block *b, Besm_Instr **t, Besm_InstrKind kind, const Frame *f,
                    const Tac_Val *v)
{
    if (v->kind == TAC_VAL_VAR) {
        int reg, off;
        if (frame_lookup(f, v->u.var_name, &reg, &off)) {
            emit_arith(b, t, kind, reg, off);
        } else {
            Besm_Instr *utc = emit(b, t, BESM_MOD_UTC);
            utc->name       = xstrdup(v->u.var_name);
            emit(b, t, kind); // reg=0, addr=0 → op mem[C+0]
        }
    } else {
        Besm_Instr *i = emit(b, t, kind);
        i->konst      = dup_const(v->u.constant);
    }
}
