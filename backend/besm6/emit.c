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

// Build a heap-allocated Madlen literal-address string for a TAC constant.
// Signed int/char/long: masked to 41 bits (raw two's-complement, exponent=0).
// Unsigned uint/ulong/uchar: masked to 48 bits.
// Float/double: "=r<value>" in %.13g format.
static char *const_lit_name(const Tac_Const *c)
{
    char buf[64];
    switch (c->kind) {
    case TAC_CONST_INT:
        snprintf(buf, sizeof(buf), "=%llo",
                 (unsigned long long)((long long)c->u.int_val & (long long)0x1FFFFFFFFFF));
        break;
    case TAC_CONST_LONG:
        snprintf(buf, sizeof(buf), "=%llo",
                 (unsigned long long)(c->u.long_val & (long long)0x1FFFFFFFFFF));
        break;
    case TAC_CONST_LONG_LONG:
        snprintf(buf, sizeof(buf), "=%llo",
                 (unsigned long long)(c->u.long_long_val & (long long)0x1FFFFFFFFFF));
        break;
    case TAC_CONST_CHAR:
        snprintf(buf, sizeof(buf), "=%llo",
                 (unsigned long long)((long long)c->u.char_val & (long long)0x1FFFFFFFFFF));
        break;
    case TAC_CONST_UINT:
        snprintf(buf, sizeof(buf), "=%llo",
                 (unsigned long long)(c->u.uint_val & 0xFFFFFFFFFFFFULL));
        break;
    case TAC_CONST_ULONG:
        snprintf(buf, sizeof(buf), "=%llo",
                 (unsigned long long)(c->u.ulong_val & 0xFFFFFFFFFFFFULL));
        break;
    case TAC_CONST_ULONG_LONG:
        snprintf(buf, sizeof(buf), "=%llo",
                 (unsigned long long)(c->u.ulong_long_val & 0xFFFFFFFFFFFFULL));
        break;
    case TAC_CONST_UCHAR:
        snprintf(buf, sizeof(buf), "=%llo", (unsigned long long)c->u.uchar_val);
        break;
    case TAC_CONST_FLOAT:
    case TAC_CONST_DOUBLE:
    case TAC_CONST_LONG_DOUBLE: {
        // float ≡ double ≡ long double on BESM-6 (one 48-bit native-FP word).
        double val = (c->kind == TAC_CONST_FLOAT)         ? (double)c->u.float_val
                     : (c->kind == TAC_CONST_LONG_DOUBLE) ? (double)c->u.long_double_val
                                                          : c->u.double_val;
        char num[48];
        mad_format_real(num, sizeof(num), val);
        snprintf(buf, sizeof(buf), "=r%s", num);
        break;
    }
    default:
        fatal_error("unsupported constant kind %d", (int)c->kind);
    }
    return xstrdup(buf);
}

// Emit XTA for a TAC value: variable from frame, UTC+XTA for a global, or =N/=rX for a constant.
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
        i->name       = const_lit_name(v->u.constant);
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
        i->name       = const_lit_name(v->u.constant);
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
        i->name       = const_lit_name(v->u.constant);
    }
}
