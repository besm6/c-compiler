#ifndef BESM6_ABI_H
#define BESM6_ABI_H

#include "tac.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// Type sizes.
// BESM-6 is word-addressed; every C scalar and pointer fits in one 48-bit word.
// Arrays occupy element_size * N consecutive words.
// Alignment is always 1 (no sub-word alignment requirement).
//
static inline int codegen_sizeof(const Tac_Type *t)
{
    switch (t->kind) {
    case TAC_TYPE_CHAR:
    case TAC_TYPE_SCHAR:
    case TAC_TYPE_UCHAR:
    case TAC_TYPE_SHORT:
    case TAC_TYPE_USHORT:
    case TAC_TYPE_INT:
    case TAC_TYPE_UINT:
    case TAC_TYPE_LONG:
    case TAC_TYPE_ULONG:
    case TAC_TYPE_LONG_LONG:
    case TAC_TYPE_ULONG_LONG:
    case TAC_TYPE_FLOAT:
    case TAC_TYPE_DOUBLE:
    case TAC_TYPE_LONG_DOUBLE:
    case TAC_TYPE_POINTER:
        return 1;
    case TAC_TYPE_ARRAY:
        return codegen_sizeof(t->u.array.elem_type) * t->u.array.size;
    default:
        return 1;
    }
}

static inline int codegen_alignof(const Tac_Type *t)
{
    (void)t;
    return 1; // word-addressed; all types are 1-word aligned
}

//
// INT-format encoding for integers.
// An integer value V is stored as a 48-bit floating-point word with:
//   bits 48-42 (exponent field) = BESM_INT_EXP = 0150 octal = 104 decimal
//   bit  41    (sign bit)       = 0
//   bits 40-1  (mantissa)       = 40-bit two's-complement representation of V
// Range: BESM_INT_MIN .. BESM_INT_MAX
//
#define BESM_INT_EXP  0150              // octal; exponent field for INT-format words
#define BESM_INT_MAX  ((1LL << 40) - 1) // +2^40 - 1
#define BESM_INT_MIN  (-(1LL << 40) + 1) // -(2^40 - 1)

//
// Calling convention register assignments (Dubna C ABI).
// See docs/Besm6_Calling_Conventions.md for full protocol.
//
// After b/save:
//   r6 (REG_PAR)  points to the parameter block: r6+i = address of param i
//   r7 (REG_AUTO) points to the auto-variable block: r7+j = local variable j
//
// b/save and b/ret are external Madlen routines; the backend declares them
// with ",CALL, b/save" and ",CALL, b/ret" — it does not emit their bodies.
//
#define REG_AUTO  7   // auto-variable pointer (set by b/save)
#define REG_PAR   6   // parameter pointer (set by b/save)
#define REG_CNT   14  // negative argument count (set by caller)
#define REG_RET   13  // return address (set by caller via VJM)
#define REG_SP    15  // stack pointer (grows toward higher addresses)

#ifdef __cplusplus
}
#endif

#endif // BESM6_ABI_H
