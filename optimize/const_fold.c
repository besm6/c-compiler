// ============================================================================
// const_fold.c — constant folding.
//
// Constant folding evaluates expressions whose operands are all constants at
// compile time and replaces the instruction with a simpler one:
//
//   - A Unary or Binary with constant operand(s) becomes a Copy of the folded
//     result:   t.0 = 6 / 2  →  t.0 = 3 ;   t.2 = a == a stays (a is a Var).
//   - A type-conversion instruction with a constant source becomes a Copy of a
//     new constant of the target kind:  SignExtend(ConstInt 3) → Copy(ConstLong 3).
//   - A conditional jump with a constant condition becomes an unconditional
//     Jump (always taken) or is deleted entirely (never taken) — which in turn
//     hands unreachable-code elimination fresh dead blocks to remove.
//
// This is the only pass that walks the flat Tac_Instruction list directly and
// needs no control-flow graph. Integer folding is done in 64-bit and the result
// is re-narrowed to the operand kind, so overflow wraps at the C type boundary
// (defined for unsigned, the implementation-defined wrap our target uses for
// signed). Floating-point folding uses the host's float/double/long-double.
//
// See docs/TAC_Optimization.md §"Constant folding".
// ============================================================================

#include <stdbool.h>
#include <stdint.h>
#include "optimize.h"
#include "tac.h"

// Truthiness test: is this constant equal to zero? Used both to fold the logical
// NOT operator and to resolve conditional jumps. Covers all 11 scalar kinds.
static bool const_is_zero(const Tac_Const *c)
{
    switch (c->kind) {
    case TAC_CONST_INT:        return c->u.int_val == 0;
    case TAC_CONST_LONG:       return c->u.long_val == 0;
    case TAC_CONST_LONG_LONG:  return c->u.long_long_val == 0;
    case TAC_CONST_UINT:       return c->u.uint_val == 0;
    case TAC_CONST_ULONG:      return c->u.ulong_val == 0;
    case TAC_CONST_ULONG_LONG: return c->u.ulong_long_val == 0;
    case TAC_CONST_FLOAT:      return c->u.float_val == 0.0;
    case TAC_CONST_DOUBLE:     return c->u.double_val == 0.0;
    case TAC_CONST_LONG_DOUBLE:return c->u.long_double_val == 0.0L;
    case TAC_CONST_CHAR:       return c->u.char_val == 0;
    case TAC_CONST_UCHAR:      return c->u.uchar_val == 0;
    }
    return false;
}

// Fold a unary operator applied to a constant. Returns a new constant-valued
// Tac_Val for the result, or NULL if the operation is not foldable.
//   - NOT yields an int 0/1 (the C logical-negation result type).
//   - NEGATE and COMPLEMENT preserve the source kind; the result is computed in
//     that kind so it wraps exactly as C would. Bitwise complement is undefined
//     for floating-point and so is rejected (NULL) for those kinds.
static Tac_Val *fold_unary_const(Tac_UnaryOperator op, const Tac_Const *src)
{
    Tac_Const *rc = NULL;

    switch (op) {
    case TAC_UNARY_NOT: {
        rc = tac_new_const(TAC_CONST_INT);
        rc->u.int_val = const_is_zero(src) ? 1 : 0;
        break;
    }
    case TAC_UNARY_NEGATE: {
        rc = tac_new_const(src->kind);
        switch (src->kind) {
        case TAC_CONST_INT:        rc->u.int_val         = -src->u.int_val;                          break;
        case TAC_CONST_LONG:       rc->u.long_val        = -src->u.long_val;                         break;
        case TAC_CONST_LONG_LONG:  rc->u.long_long_val   = -src->u.long_long_val;                    break;
        case TAC_CONST_UINT:       rc->u.uint_val        = -src->u.uint_val;                         break;
        case TAC_CONST_ULONG:      rc->u.ulong_val       = -src->u.ulong_val;                        break;
        case TAC_CONST_ULONG_LONG: rc->u.ulong_long_val  = -src->u.ulong_long_val;                   break;
        case TAC_CONST_FLOAT:      rc->u.float_val       = -src->u.float_val;                        break;
        case TAC_CONST_DOUBLE:     rc->u.double_val      = -src->u.double_val;                       break;
        case TAC_CONST_LONG_DOUBLE:rc->u.long_double_val = -src->u.long_double_val;                  break;
        case TAC_CONST_CHAR:       rc->u.char_val        = -src->u.char_val;                         break;
        case TAC_CONST_UCHAR:      rc->u.uchar_val       = (unsigned char)(-src->u.uchar_val);       break;
        }
        break;
    }
    case TAC_UNARY_COMPLEMENT: {
        rc = tac_new_const(src->kind);
        switch (src->kind) {
        case TAC_CONST_INT:        rc->u.int_val         = ~src->u.int_val;                          break;
        case TAC_CONST_LONG:       rc->u.long_val        = ~src->u.long_val;                         break;
        case TAC_CONST_LONG_LONG:  rc->u.long_long_val   = ~src->u.long_long_val;                    break;
        case TAC_CONST_UINT:       rc->u.uint_val        = ~src->u.uint_val;                         break;
        case TAC_CONST_ULONG:      rc->u.ulong_val       = ~src->u.ulong_val;                        break;
        case TAC_CONST_ULONG_LONG: rc->u.ulong_long_val  = ~src->u.ulong_long_val;                   break;
        case TAC_CONST_CHAR:       rc->u.char_val        = (int)(signed char)(~src->u.char_val);     break;
        case TAC_CONST_UCHAR:      rc->u.uchar_val       = (unsigned char)(~src->u.uchar_val);       break;
        default:
            // Floats: complement is undefined in C; should not appear in well-typed TAC.
            tac_free_const(rc);
            return NULL;
        }
        break;
    }
    }

    Tac_Val *rv      = tac_new_val(TAC_VAL_CONSTANT);
    rv->u.constant   = rc;
    return rv;
}

// True for the eight integer constant kinds (everything except the three
// floating-point kinds). Integer and float folding follow different rules.
static bool const_is_integer_kind(Tac_ConstKind k)
{
    return k == TAC_CONST_INT  || k == TAC_CONST_LONG     || k == TAC_CONST_LONG_LONG  ||
           k == TAC_CONST_UINT || k == TAC_CONST_ULONG    || k == TAC_CONST_ULONG_LONG ||
           k == TAC_CONST_CHAR || k == TAC_CONST_UCHAR;
}

// Widen any integer constant to a signed 64-bit value (sign-extending the
// signed kinds, zero-extending the unsigned ones). Signed binary operators are
// evaluated through this view.
static int64_t const_to_int64(const Tac_Const *c)
{
    switch (c->kind) {
    case TAC_CONST_INT:        return c->u.int_val;
    case TAC_CONST_LONG:       return c->u.long_val;
    case TAC_CONST_LONG_LONG:  return c->u.long_long_val;
    case TAC_CONST_UINT:       return (int64_t)c->u.uint_val;
    case TAC_CONST_ULONG:      return (int64_t)c->u.ulong_val;
    case TAC_CONST_ULONG_LONG: return (int64_t)c->u.ulong_long_val;
    case TAC_CONST_CHAR:       return c->u.char_val;
    case TAC_CONST_UCHAR:      return c->u.uchar_val;
    default:                   return 0;
    }
}

// Widen any integer constant to a 64-bit bit pattern. Unsigned operators and
// the wrapping arithmetic operators (add/sub/mul, bitwise, shifts) are evaluated
// through this view; the result is re-narrowed by make_int_const_val.
static uint64_t const_to_uint64(const Tac_Const *c)
{
    switch (c->kind) {
    case TAC_CONST_INT:        return (uint64_t)(int64_t)c->u.int_val;
    case TAC_CONST_LONG:       return (uint64_t)c->u.long_val;
    case TAC_CONST_LONG_LONG:  return (uint64_t)c->u.long_long_val;
    case TAC_CONST_UINT:       return c->u.uint_val;
    case TAC_CONST_ULONG:      return c->u.ulong_val;
    case TAC_CONST_ULONG_LONG: return c->u.ulong_long_val;
    case TAC_CONST_CHAR:       return (uint64_t)(int64_t)(int8_t)c->u.char_val;
    case TAC_CONST_UCHAR:      return c->u.uchar_val;
    default:                   return 0;
    }
}

// Mask applied to a shift count so it stays in [0, width). C leaves shifts by
// the type width or more undefined; we fold them by masking the count to the
// operand width, matching the behavior of the target hardware's shift unit.
static int const_shift_mask(Tac_ConstKind k)
{
    switch (k) {
    case TAC_CONST_CHAR:
    case TAC_CONST_UCHAR:      return 7;
    case TAC_CONST_INT:
    case TAC_CONST_UINT:       return 31;
    default:                   return 63; // LONG, LONG_LONG, ULONG, ULONG_LONG
    }
}

// Build a constant-valued Tac_Val of `kind` from a 64-bit result `bits`,
// narrowing to the kind's width. This is where overflow wrapping happens: the
// store into the narrow field discards the high bits exactly as C requires.
static Tac_Val *make_int_const_val(Tac_ConstKind kind, uint64_t bits)
{
    Tac_Const *rc = tac_new_const(kind);
    switch (kind) {
    case TAC_CONST_INT:        rc->u.int_val        = (int)bits;                       break;
    case TAC_CONST_LONG:       rc->u.long_val       = (long)bits;                      break;
    case TAC_CONST_LONG_LONG:  rc->u.long_long_val  = (long long)bits;                 break;
    case TAC_CONST_UINT:       rc->u.uint_val       = (unsigned int)bits;              break;
    case TAC_CONST_ULONG:      rc->u.ulong_val      = (unsigned long)bits;             break;
    case TAC_CONST_ULONG_LONG: rc->u.ulong_long_val = (unsigned long long)bits;        break;
    case TAC_CONST_CHAR:       rc->u.char_val       = (int)(int8_t)bits;               break;
    case TAC_CONST_UCHAR:      rc->u.uchar_val      = (unsigned char)bits;             break;
    default: break;
    }
    Tac_Val *rv    = tac_new_val(TAC_VAL_CONSTANT);
    rv->u.constant = rc;
    return rv;
}

// Fold a binary operator on two floating-point constants of the *same* kind.
// Arithmetic operators produce a constant of that kind; the relational and
// equality operators produce an int 0/1 (the C comparison result type). Returns
// NULL when the kinds differ or the operator is not a float operator.
static Tac_Val *fold_binary_float(Tac_BinaryOperator op,
                                   const Tac_Const *c1,
                                   const Tac_Const *c2)
{
    if (c1->kind != c2->kind)
        return NULL;

    if (c1->kind == TAC_CONST_LONG_DOUBLE) {
        long double ld1 = c1->u.long_double_val, ld2 = c2->u.long_double_val;
        long double ldr;
        switch (op) {
        case TAC_BINARY_ADD:              ldr = ld1 + ld2; break;
        case TAC_BINARY_SUBTRACT:         ldr = ld1 - ld2; break;
        case TAC_BINARY_MULTIPLY:         ldr = ld1 * ld2; break;
        case TAC_BINARY_DIVIDE:           ldr = ld1 / ld2; break;
        case TAC_BINARY_EQUAL:            return make_int_const_val(TAC_CONST_INT, ld1 == ld2);
        case TAC_BINARY_NOT_EQUAL:        return make_int_const_val(TAC_CONST_INT, ld1 != ld2);
        case TAC_BINARY_LESS_THAN:        return make_int_const_val(TAC_CONST_INT, ld1 <  ld2);
        case TAC_BINARY_LESS_OR_EQUAL:    return make_int_const_val(TAC_CONST_INT, ld1 <= ld2);
        case TAC_BINARY_GREATER_THAN:     return make_int_const_val(TAC_CONST_INT, ld1 >  ld2);
        case TAC_BINARY_GREATER_OR_EQUAL: return make_int_const_val(TAC_CONST_INT, ld1 >= ld2);
        default: return NULL;
        }
        Tac_Const *rc = tac_new_const(TAC_CONST_LONG_DOUBLE);
        rc->u.long_double_val = ldr;
        Tac_Val *rv = tac_new_val(TAC_VAL_CONSTANT);
        rv->u.constant = rc;
        return rv;
    }

    if (c1->kind != TAC_CONST_FLOAT && c1->kind != TAC_CONST_DOUBLE)
        return NULL;

    double dv1 = (c1->kind == TAC_CONST_FLOAT) ? c1->u.float_val : c1->u.double_val;
    double dv2 = (c2->kind == TAC_CONST_FLOAT) ? c2->u.float_val : c2->u.double_val;
    double dr;
    switch (op) {
    case TAC_BINARY_ADD:              dr = dv1 + dv2; break;
    case TAC_BINARY_SUBTRACT:         dr = dv1 - dv2; break;
    case TAC_BINARY_MULTIPLY:         dr = dv1 * dv2; break;
    case TAC_BINARY_DIVIDE:           dr = dv1 / dv2; break;
    case TAC_BINARY_EQUAL:            return make_int_const_val(TAC_CONST_INT, dv1 == dv2);
    case TAC_BINARY_NOT_EQUAL:        return make_int_const_val(TAC_CONST_INT, dv1 != dv2);
    case TAC_BINARY_LESS_THAN:        return make_int_const_val(TAC_CONST_INT, dv1 <  dv2);
    case TAC_BINARY_LESS_OR_EQUAL:    return make_int_const_val(TAC_CONST_INT, dv1 <= dv2);
    case TAC_BINARY_GREATER_THAN:     return make_int_const_val(TAC_CONST_INT, dv1 >  dv2);
    case TAC_BINARY_GREATER_OR_EQUAL: return make_int_const_val(TAC_CONST_INT, dv1 >= dv2);
    default: return NULL;
    }
    Tac_Const *rc = tac_new_const(c1->kind);
    if (c1->kind == TAC_CONST_FLOAT)
        rc->u.float_val = dr;
    else
        rc->u.double_val = dr;
    Tac_Val *rv = tac_new_val(TAC_VAL_CONSTANT);
    rv->u.constant = rc;
    return rv;
}

// Fold a binary operator on two constant operands. Returns a new constant-valued
// Tac_Val, or NULL if not foldable. If either operand is floating-point the work
// is delegated to fold_binary_float; the rest of this function handles integers.
//
// Division and remainder by a zero constant are *not* folded (returns NULL): we
// leave the instruction in place rather than invoke undefined behavior at
// compile time. Comparisons produce an int 0/1; the signed/unsigned operator
// variants pick the signed (s1/s2) or unsigned (u1/u2) 64-bit view accordingly.
// Wrapping arithmetic is done in uint64 and narrowed back to c1's kind.
static Tac_Val *fold_binary_const(Tac_BinaryOperator op,
                                   const Tac_Const *c1,
                                   const Tac_Const *c2)
{
    if (!const_is_integer_kind(c1->kind) || !const_is_integer_kind(c2->kind))
        return fold_binary_float(op, c1, c2);

    if ((op == TAC_BINARY_DIVIDE          || op == TAC_BINARY_REMAINDER ||
         op == TAC_BINARY_DIVIDE_UNSIGNED || op == TAC_BINARY_REMAINDER_UNSIGNED) &&
        const_is_zero(c2))
        return NULL;

    // Both a signed and an unsigned 64-bit view of each operand; operators pick
    // whichever matches their C semantics.
    int64_t  s1  = const_to_int64(c1),  s2  = const_to_int64(c2);
    uint64_t u1  = const_to_uint64(c1), u2  = const_to_uint64(c2);
    uint64_t result;

    switch (op) {
    case TAC_BINARY_ADD:       result = u1 + u2; break;
    case TAC_BINARY_SUBTRACT:  result = u1 - u2; break;
    case TAC_BINARY_MULTIPLY:  result = u1 * u2; break;
    case TAC_BINARY_DIVIDE:    result = (uint64_t)(s1 / s2); break;
    case TAC_BINARY_REMAINDER: result = (uint64_t)(s1 % s2); break;
    case TAC_BINARY_DIVIDE_UNSIGNED:    result = u1 / u2; break;
    case TAC_BINARY_REMAINDER_UNSIGNED: result = u1 % u2; break;

    case TAC_BINARY_EQUAL:              return make_int_const_val(TAC_CONST_INT, u1 == u2);
    case TAC_BINARY_NOT_EQUAL:          return make_int_const_val(TAC_CONST_INT, u1 != u2);
    case TAC_BINARY_LESS_THAN:          return make_int_const_val(TAC_CONST_INT, s1 < s2);
    case TAC_BINARY_LESS_OR_EQUAL:      return make_int_const_val(TAC_CONST_INT, s1 <= s2);
    case TAC_BINARY_GREATER_THAN:       return make_int_const_val(TAC_CONST_INT, s1 > s2);
    case TAC_BINARY_GREATER_OR_EQUAL:   return make_int_const_val(TAC_CONST_INT, s1 >= s2);

    case TAC_BINARY_LESS_THAN_UNSIGNED:        return make_int_const_val(TAC_CONST_INT, u1 < u2);
    case TAC_BINARY_LESS_OR_EQUAL_UNSIGNED:    return make_int_const_val(TAC_CONST_INT, u1 <= u2);
    case TAC_BINARY_GREATER_THAN_UNSIGNED:     return make_int_const_val(TAC_CONST_INT, u1 > u2);
    case TAC_BINARY_GREATER_OR_EQUAL_UNSIGNED: return make_int_const_val(TAC_CONST_INT, u1 >= u2);

    case TAC_BINARY_BITWISE_AND: result = u1 & u2; break;
    case TAC_BINARY_BITWISE_OR:  result = u1 | u2; break;
    case TAC_BINARY_BITWISE_XOR: result = u1 ^ u2; break;

    case TAC_BINARY_LEFT_SHIFT: {
        int amt = (int)((unsigned)u2 & (unsigned)const_shift_mask(c1->kind));
        result = u1 << (unsigned)amt;
        break;
    }
    case TAC_BINARY_RIGHT_SHIFT: {
        // Arithmetic right shift: operate on the signed view so the sign bit
        // is replicated (sign-preserving), matching signed >> in C.
        int amt = (int)((unsigned)u2 & (unsigned)const_shift_mask(c1->kind));
        result = (uint64_t)(s1 >> amt);
        break;
    }
    case TAC_BINARY_RIGHT_SHIFT_LOGICAL: {
        // Logical right shift: operate on the unsigned view (zero-fill).
        int amt = (int)((unsigned)u2 & (unsigned)const_shift_mask(c1->kind));
        result = u1 >> (unsigned)amt;
        break;
    }
    default: return NULL;
    }

    return make_int_const_val(c1->kind, result);
}

// True for all 14 type-conversion instruction kinds (the three integer-width
// conversions plus the twelve floating-point conversions). They all share the
// same {src, dst} union layout, so the driver can treat them uniformly.
static bool is_conversion(Tac_InstructionKind k)
{
    switch (k) {
    case TAC_INSTRUCTION_SIGN_EXTEND:
    case TAC_INSTRUCTION_ZERO_EXTEND:
    case TAC_INSTRUCTION_TRUNCATE:
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_FLOAT:
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_INT:
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
        return true;
    default:
        return false;
    }
}

// Fold a type conversion of a constant source into a new constant of the target
// kind. Returns NULL if the source kind does not match what the conversion
// expects (e.g. a Truncate applied to a float). The target kind is implied by
// the conversion instruction: SignExtend widens char→int→long→long long;
// ZeroExtend does the unsigned ladder; Truncate narrows; the *To* conversions
// move between integer and floating-point. Float→int conversions truncate
// toward zero, as C casts do.
static Tac_Val *fold_conversion(Tac_InstructionKind kind, const Tac_Const *src)
{
    Tac_Const *rc = NULL;

    switch (kind) {

    /* ---- integer width conversions ---- */

    case TAC_INSTRUCTION_SIGN_EXTEND:
        rc = tac_new_const(src->kind == TAC_CONST_CHAR ? TAC_CONST_INT :
                           src->kind == TAC_CONST_INT  ? TAC_CONST_LONG :
                           src->kind == TAC_CONST_LONG ? TAC_CONST_LONG_LONG : TAC_CONST_INT);
        switch (src->kind) {
        case TAC_CONST_CHAR:      rc->u.int_val       = (int)(int8_t)src->u.char_val;             break;
        case TAC_CONST_INT:       rc->u.long_val      = (long)src->u.int_val;                     break;
        case TAC_CONST_LONG:      rc->u.long_long_val = (long long)src->u.long_val;               break;
        default: tac_free_const(rc); return NULL;
        }
        break;

    case TAC_INSTRUCTION_ZERO_EXTEND:
        rc = tac_new_const(src->kind == TAC_CONST_UCHAR ? TAC_CONST_UINT :
                           src->kind == TAC_CONST_UINT  ? TAC_CONST_ULONG :
                           src->kind == TAC_CONST_ULONG ? TAC_CONST_ULONG_LONG : TAC_CONST_UINT);
        switch (src->kind) {
        case TAC_CONST_UCHAR: rc->u.uint_val       = (unsigned)src->u.uchar_val;                  break;
        case TAC_CONST_UINT:  rc->u.ulong_val      = (unsigned long)src->u.uint_val;              break;
        case TAC_CONST_ULONG: rc->u.ulong_long_val = (unsigned long long)src->u.ulong_val;        break;
        default: tac_free_const(rc); return NULL;
        }
        break;

    case TAC_INSTRUCTION_TRUNCATE:
        switch (src->kind) {
        case TAC_CONST_LONG:
        case TAC_CONST_LONG_LONG:
            rc = tac_new_const(TAC_CONST_INT);
            rc->u.int_val = (int)const_to_int64(src);
            break;
        case TAC_CONST_ULONG:
        case TAC_CONST_ULONG_LONG:
            rc = tac_new_const(TAC_CONST_UINT);
            rc->u.uint_val = (unsigned)const_to_uint64(src);
            break;
        case TAC_CONST_INT:
            rc = tac_new_const(TAC_CONST_CHAR);
            rc->u.char_val = (int)(int8_t)src->u.int_val;
            break;
        case TAC_CONST_UINT:
            rc = tac_new_const(TAC_CONST_UCHAR);
            rc->u.uchar_val = (unsigned char)src->u.uint_val;
            break;
        default: return NULL;
        }
        break;

    /* ---- integer → floating-point ---- */

    case TAC_INSTRUCTION_INT_TO_DOUBLE:
        if (!const_is_integer_kind(src->kind)) return NULL;
        rc = tac_new_const(TAC_CONST_DOUBLE);
        rc->u.double_val = (double)const_to_int64(src);
        break;

    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
        if (!const_is_integer_kind(src->kind)) return NULL;
        rc = tac_new_const(TAC_CONST_DOUBLE);
        rc->u.double_val = (double)const_to_uint64(src);
        break;

    case TAC_INSTRUCTION_INT_TO_FLOAT:
        if (!const_is_integer_kind(src->kind)) return NULL;
        rc = tac_new_const(TAC_CONST_FLOAT);
        rc->u.float_val = (double)(float)const_to_int64(src);
        break;

    case TAC_INSTRUCTION_UINT_TO_FLOAT:
        if (!const_is_integer_kind(src->kind)) return NULL;
        rc = tac_new_const(TAC_CONST_FLOAT);
        rc->u.float_val = (double)(float)const_to_uint64(src);
        break;

    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
        if (!const_is_integer_kind(src->kind)) return NULL;
        rc = tac_new_const(TAC_CONST_LONG_DOUBLE);
        rc->u.long_double_val = (long double)const_to_int64(src);
        break;

    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
        if (!const_is_integer_kind(src->kind)) return NULL;
        rc = tac_new_const(TAC_CONST_LONG_DOUBLE);
        rc->u.long_double_val = (long double)const_to_uint64(src);
        break;

    /* ---- floating-point → integer (truncate toward zero) ---- */

    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_FLOAT_TO_INT: {
        double d;
        if      (src->kind == TAC_CONST_FLOAT)  d = src->u.float_val;
        else if (src->kind == TAC_CONST_DOUBLE) d = src->u.double_val;
        else return NULL;
        rc = tac_new_const(TAC_CONST_INT);
        rc->u.int_val = (int)d;
        break;
    }

    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_FLOAT_TO_UINT: {
        double d;
        if      (src->kind == TAC_CONST_FLOAT)  d = src->u.float_val;
        else if (src->kind == TAC_CONST_DOUBLE) d = src->u.double_val;
        else return NULL;
        rc = tac_new_const(TAC_CONST_UINT);
        rc->u.uint_val = (unsigned)d;
        break;
    }

    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
        if (src->kind != TAC_CONST_LONG_DOUBLE) return NULL;
        rc = tac_new_const(TAC_CONST_INT);
        rc->u.int_val = (int)src->u.long_double_val;
        break;

    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
        if (src->kind != TAC_CONST_LONG_DOUBLE) return NULL;
        rc = tac_new_const(TAC_CONST_UINT);
        rc->u.uint_val = (unsigned)src->u.long_double_val;
        break;

    /* ---- floating-point ↔ floating-point ---- */

    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
        if (src->kind != TAC_CONST_FLOAT) return NULL;
        rc = tac_new_const(TAC_CONST_DOUBLE);
        rc->u.double_val = src->u.float_val; // float_val already stored as double
        break;

    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
        if (src->kind != TAC_CONST_DOUBLE) return NULL;
        rc = tac_new_const(TAC_CONST_FLOAT);
        rc->u.float_val = src->u.double_val;
        break;

    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
        if (src->kind != TAC_CONST_FLOAT) return NULL;
        rc = tac_new_const(TAC_CONST_LONG_DOUBLE);
        rc->u.long_double_val = (long double)src->u.float_val;
        break;

    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
        if (src->kind != TAC_CONST_LONG_DOUBLE) return NULL;
        rc = tac_new_const(TAC_CONST_FLOAT);
        rc->u.float_val = (double)src->u.long_double_val;
        break;

    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
        if (src->kind != TAC_CONST_DOUBLE) return NULL;
        rc = tac_new_const(TAC_CONST_LONG_DOUBLE);
        rc->u.long_double_val = (long double)src->u.double_val;
        break;

    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
        if (src->kind != TAC_CONST_LONG_DOUBLE) return NULL;
        rc = tac_new_const(TAC_CONST_DOUBLE);
        rc->u.double_val = (double)src->u.long_double_val;
        break;

    default:
        return NULL;
    }

    Tac_Val *rv    = tac_new_val(TAC_VAL_CONSTANT);
    rv->u.constant = rc;
    return rv;
}

// Walk the flat instruction list once, folding every instruction whose operands
// are all constant. Returns the (possibly new) head of the list.
//
// For Unary/Binary/conversion instructions the replacement pattern is uniform
// and worth describing once: build a fresh Copy whose source is the folded
// constant and whose destination is *stolen* from the original instruction (the
// dst Tac_Val is moved, not copied). Before freeing the original we NULL out the
// stolen dst field so tac_free_instruction does not free it (it now belongs to
// the Copy), and NULL out `cur->next` so the recursive free does not cascade
// into the rest of the list. The Copy is then spliced in where the original was.
Tac_Instruction *constant_fold(Tac_Instruction *body)
{
    Tac_Instruction *prev = NULL;
    Tac_Instruction *cur  = body;

    while (cur) {
        Tac_Instruction *next = cur->next;

        // Never fold or rewrite a volatile access: it must execute verbatim.
        if (cur->is_volatile) {
            prev = cur;
            cur  = next;
            continue;
        }

        // Unary with a constant operand → Copy of the folded result.
        if (cur->kind == TAC_INSTRUCTION_UNARY &&
            cur->u.unary.src->kind == TAC_VAL_CONSTANT) {

            Tac_Val *folded = fold_unary_const(cur->u.unary.op,
                                               cur->u.unary.src->u.constant);
            if (folded) {
                opt_trace_instr("[const-fold] unary fold:", cur);
                Tac_Instruction *copy = tac_new_instruction(TAC_INSTRUCTION_COPY);
                copy->u.copy.src = folded;
                copy->u.copy.dst = cur->u.unary.dst; // steal dst
                copy->next       = next;

                cur->u.unary.dst = NULL; // prevent double-free in tac_free_instruction
                cur->next        = NULL; // prevent cascade-free
                tac_free_instruction(cur);

                if (prev)
                    prev->next = copy;
                else
                    body = copy;

                opt_trace_instr("[const-fold]          →", copy);
                prev = copy;
                cur  = next;
                continue;
            }
        }

        // Binary with two constant operands → Copy of the folded result.
        if (cur->kind == TAC_INSTRUCTION_BINARY &&
            cur->u.binary.src1->kind == TAC_VAL_CONSTANT &&
            cur->u.binary.src2->kind == TAC_VAL_CONSTANT) {

            Tac_Val *folded = fold_binary_const(cur->u.binary.op,
                                                cur->u.binary.src1->u.constant,
                                                cur->u.binary.src2->u.constant);
            if (folded) {
                opt_trace_instr("[const-fold] binary fold:", cur);
                Tac_Instruction *copy = tac_new_instruction(TAC_INSTRUCTION_COPY);
                copy->u.copy.src = folded;
                copy->u.copy.dst = cur->u.binary.dst; // steal dst
                copy->next       = next;

                cur->u.binary.dst = NULL; // prevent double-free in tac_free_instruction
                cur->next         = NULL; // prevent cascade-free
                tac_free_instruction(cur);

                if (prev)
                    prev->next = copy;
                else
                    body = copy;

                opt_trace_instr("[const-fold]           →", copy);
                prev = copy;
                cur  = next;
                continue;
            }
        }

        // Any conversion of a constant source → Copy of the new constant.
        // All 14 conversions share the sign_extend {src, dst} layout.
        if (is_conversion(cur->kind) &&
            cur->u.sign_extend.src->kind == TAC_VAL_CONSTANT) {

            Tac_Val *folded = fold_conversion(cur->kind,
                                              cur->u.sign_extend.src->u.constant);
            if (folded) {
                opt_trace_instr("[const-fold] conversion fold:", cur);
                Tac_Instruction *copy = tac_new_instruction(TAC_INSTRUCTION_COPY);
                copy->u.copy.src      = folded;
                copy->u.copy.dst      = cur->u.sign_extend.dst; // steal dst
                copy->next            = next;

                cur->u.sign_extend.dst = NULL; // prevent double-free
                cur->next              = NULL; // prevent cascade-free
                tac_free_instruction(cur);

                if (prev)
                    prev->next = copy;
                else
                    body = copy;

                opt_trace_instr("[const-fold]              →", copy);
                prev = copy;
                cur  = next;
                continue;
            }
        }

        // Conditional jump with a constant condition → resolve it statically.
        // (JumpIfZero and JumpIfNotZero share the jump_if_zero union layout.)
        // If the branch is always taken, replace it with an unconditional Jump
        // to the same target; if never taken, delete it. Either way the block
        // structure simplifies, which gives unreachable-code elimination new
        // dead blocks to remove on the next CFG pass.
        if ((cur->kind == TAC_INSTRUCTION_JUMP_IF_ZERO ||
             cur->kind == TAC_INSTRUCTION_JUMP_IF_NOT_ZERO) &&
            cur->u.jump_if_zero.condition->kind == TAC_VAL_CONSTANT) {

            bool is_zero = const_is_zero(cur->u.jump_if_zero.condition->u.constant);
            bool take = (cur->kind == TAC_INSTRUCTION_JUMP_IF_ZERO) ? is_zero : !is_zero;

            if (take) {
                // Always taken: build a Jump, stealing the target label string.
                opt_trace_instr("[const-fold] cond-jump always taken:", cur);
                OPT_TRACE("[const-fold]   → unconditional jump to %s\n",
                          cur->u.jump_if_zero.target);
                Tac_Instruction *jmp  = tac_new_instruction(TAC_INSTRUCTION_JUMP);
                jmp->u.jump.target    = cur->u.jump_if_zero.target; // steal
                jmp->next             = next;

                tac_free_val(cur->u.jump_if_zero.condition);
                cur->u.jump_if_zero.condition = NULL;
                cur->u.jump_if_zero.target    = NULL;  // stolen; don't free it
                cur->next                     = NULL;
                tac_free_instruction(cur);

                if (prev) prev->next = jmp; else body = jmp;
                prev = jmp;
            } else {
                // Never taken: unlink and free the conditional jump entirely.
                opt_trace_instr("[const-fold] cond-jump never taken → deleted:", cur);
                if (prev) prev->next = next; else body = next;
                cur->next = NULL;
                tac_free_instruction(cur);
            }
            cur = next;
            continue;
        }

        prev = cur;
        cur  = next;
    }

    return body;
}
