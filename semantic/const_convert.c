#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "semantic.h"
#include "symtab.h"

//
// Convert literal to int64
//
int64_t literal_to_int64(const Literal *lit)
{
    switch (lit->kind) {
    case LITERAL_CHAR:
        return (int64_t)lit->u.char_val;
    case LITERAL_INT:
        return (int64_t)lit->u.int_val;
    case LITERAL_LONG:
        return (int64_t)lit->u.long_val;
    case LITERAL_LONG_LONG:
        return (int64_t)lit->u.long_long_val;
    case LITERAL_UINT:
        return (int64_t)lit->u.uint_val;
    case LITERAL_ULONG:
        return (int64_t)lit->u.ulong_val;
    case LITERAL_ULONG_LONG:
        return (int64_t)lit->u.ulong_long_val;
    case LITERAL_FLOAT:
    case LITERAL_DOUBLE:
        return (int64_t)lit->u.real_val;
    case LITERAL_LONG_DOUBLE:
        return (int64_t)lit->u.long_double_val;
    case LITERAL_STRING:
        fatal_error("literal_to_int64: Cannot convert string %s", lit->u.string_val);
    case LITERAL_ENUM:
        fatal_error("literal_to_int64: Cannot convert enum %d", lit->u.enum_const);
    default:
        fatal_error("literal_to_int64: Unknown kind %d", lit->kind);
    }
}

//
// Convert literal to uint64
//
uint64_t literal_to_uint64(const Literal *lit)
{
    switch (lit->kind) {
    case LITERAL_CHAR:
        return (uint64_t)lit->u.char_val;
    case LITERAL_INT:
        return (uint64_t)lit->u.int_val;
    case LITERAL_LONG:
        return (uint64_t)lit->u.long_val;
    case LITERAL_LONG_LONG:
        return (uint64_t)lit->u.long_long_val;
    case LITERAL_UINT:
        return (uint64_t)lit->u.uint_val;
    case LITERAL_ULONG:
        return (uint64_t)lit->u.ulong_val;
    case LITERAL_ULONG_LONG:
        return (uint64_t)lit->u.ulong_long_val;
    case LITERAL_FLOAT:
    case LITERAL_DOUBLE:
        return (uint64_t)lit->u.real_val;
    case LITERAL_LONG_DOUBLE:
        return (uint64_t)lit->u.long_double_val;
    case LITERAL_STRING:
        fatal_error("literal_to_uint64: Cannot convert string %s", lit->u.string_val);
    case LITERAL_ENUM:
        fatal_error("literal_to_uint64: Cannot convert enum %d", lit->u.enum_const);
    default:
        fatal_error("literal_to_uint64: Unknown kind %d", lit->kind);
    }
}

//
// Convert literal to double
//
double literal_to_double(const Literal *lit)
{
    switch (lit->kind) {
    case LITERAL_CHAR:
        return (double)lit->u.char_val;
    case LITERAL_INT:
        return (double)lit->u.int_val;
    case LITERAL_LONG:
        return (double)lit->u.long_val;
    case LITERAL_LONG_LONG:
        return (double)lit->u.long_long_val;
    case LITERAL_UINT:
        return (double)lit->u.uint_val;
    case LITERAL_ULONG:
        return (double)lit->u.ulong_val;
    case LITERAL_ULONG_LONG:
        return (double)lit->u.ulong_long_val;
    case LITERAL_FLOAT:
    case LITERAL_DOUBLE:
        return (double)lit->u.real_val;
    case LITERAL_LONG_DOUBLE:
        return (double)lit->u.long_double_val;
    case LITERAL_STRING:
        fatal_error("literal_to_double: Cannot convert string %s", lit->u.string_val);
    case LITERAL_ENUM:
        fatal_error("literal_to_double: Cannot convert enum %d", lit->u.enum_const);
    default:
        fatal_error("literal_to_double: Unknown kind %d", lit->kind);
    }
}

static long double literal_to_long_double(const Literal *lit)
{
    switch (lit->kind) {
    case LITERAL_CHAR:
        return (long double)lit->u.char_val;
    case LITERAL_INT:
        return (long double)lit->u.int_val;
    case LITERAL_LONG:
        return (long double)lit->u.long_val;
    case LITERAL_LONG_LONG:
        return (long double)lit->u.long_long_val;
    case LITERAL_UINT:
        return (long double)lit->u.uint_val;
    case LITERAL_ULONG:
        return (long double)lit->u.ulong_val;
    case LITERAL_ULONG_LONG:
        return (long double)lit->u.ulong_long_val;
    case LITERAL_FLOAT:
    case LITERAL_DOUBLE:
        return (long double)lit->u.real_val;
    case LITERAL_LONG_DOUBLE:
        return lit->u.long_double_val;
    case LITERAL_STRING:
        fatal_error("literal_to_long_double: Cannot convert string %s", lit->u.string_val);
    case LITERAL_ENUM:
        fatal_error("literal_to_long_double: Cannot convert enum %d", lit->u.enum_const);
    default:
        fatal_error("literal_to_long_double: Unknown kind %d", lit->kind);
    }
}

//
// Convert literal to given arithmetic type and return as Tac_StaticInit.
//
Tac_StaticInit *new_static_init_from_literal(const Type *target_type, const Literal *lit)
{
    if (!is_arithmetic(target_type)) {
        fatal_error("Invalid static initializer for type %d", target_type->kind);
    }

    Tac_StaticInit *result = NULL;
    switch (target_type->kind) {
    case TYPE_BOOL:
        result            = tac_new_static_init(TAC_STATIC_INIT_I32);
        result->u.int_val = (literal_to_int64(lit) != 0);
        break;

    case TYPE_CHAR:
    case TYPE_SCHAR:
        result             = tac_new_static_init(TAC_STATIC_INIT_I8);
        result->u.char_val = (int8_t)literal_to_int64(lit);
        break;

    case TYPE_UCHAR:
        result              = tac_new_static_init(TAC_STATIC_INIT_U8);
        result->u.uchar_val = (uint8_t)literal_to_int64(lit);
        break;

    case TYPE_SHORT:
        result              = tac_new_static_init(TAC_STATIC_INIT_I16);
        result->u.short_val = (int16_t)literal_to_int64(lit);
        break;

    case TYPE_INT:
        // BESM-6 int is 48-bit; use the 64-bit init slot so multi-character
        // constants (up to 5 bytes / 40 bits) are not truncated. The backend
        // emits INIT_I64 identically to INIT_I32 (one word, masked to 41 bits).
        result             = tac_new_static_init(TAC_STATIC_INIT_I64);
        result->u.long_val = literal_to_int64(lit);
        break;

    case TYPE_USHORT:
        result               = tac_new_static_init(TAC_STATIC_INIT_U16);
        result->u.ushort_val = (uint16_t)literal_to_int64(lit);
        break;

    case TYPE_UINT:
        result              = tac_new_static_init(TAC_STATIC_INIT_U64);
        result->u.ulong_val = literal_to_uint64(lit);
        break;

    case TYPE_LONG:
    case TYPE_LONG_LONG:
        result             = tac_new_static_init(TAC_STATIC_INIT_I64);
        result->u.long_val = literal_to_int64(lit);
        break;

    case TYPE_ULONG:
    case TYPE_ULONG_LONG:
        result              = tac_new_static_init(TAC_STATIC_INIT_U64);
        result->u.ulong_val = literal_to_uint64(lit);
        break;

    case TYPE_FLOAT:
        result              = tac_new_static_init(TAC_STATIC_INIT_FLOAT);
        result->u.float_val = literal_to_double(lit);
        break;

    case TYPE_DOUBLE:
        result               = tac_new_static_init(TAC_STATIC_INIT_DOUBLE);
        result->u.double_val = literal_to_double(lit);
        break;

    case TYPE_LONG_DOUBLE:
        result                    = tac_new_static_init(TAC_STATIC_INIT_LONG_DOUBLE);
        result->u.long_double_val = literal_to_long_double(lit);
        break;
    default:
        fatal_error("Unsupported constant type for initializer");
    }
    return result;
}
