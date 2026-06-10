#include <stdbool.h>
#include "tac.h"

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

// Returns a new Tac_Val for the folded result, or NULL if not foldable.
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

Tac_Instruction *constant_fold(Tac_Instruction *body)
{
    Tac_Instruction *prev = NULL;
    Tac_Instruction *cur  = body;

    while (cur) {
        Tac_Instruction *next = cur->next;

        if (cur->kind == TAC_INSTRUCTION_UNARY &&
            cur->u.unary.src->kind == TAC_VAL_CONSTANT) {

            Tac_Val *folded = fold_unary_const(cur->u.unary.op,
                                               cur->u.unary.src->u.constant);
            if (folded) {
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

                prev = copy;
                cur  = next;
                continue;
            }
        }

        prev = cur;
        cur  = next;
    }

    return body;
}
