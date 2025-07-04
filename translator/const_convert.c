#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "symtab.h"
#include "translator.h"

//
// Convert literal to int64
//
int64_t literal_to_int64(const Literal *lit)
{
    switch (lit->kind) {
    case LITERAL_CHAR:
        return (int64_t)lit->u.char_val; // Sign-extend
    case LITERAL_INT:
        return (int64_t)lit->u.int_val; // Sign-extend
    case LITERAL_FLOAT:
        return (int64_t)lit->u.real_val; // Truncate to int64
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
    case LITERAL_FLOAT:
        return (uint64_t)lit->u.real_val; // Truncate to uint64
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
    case LITERAL_FLOAT:
        return (double)lit->u.real_val;
    case LITERAL_STRING:
        fatal_error("literal_to_double: Cannot convert string %s", lit->u.string_val);
    case LITERAL_ENUM:
        fatal_error("literal_to_double: Cannot convert enum %d", lit->u.enum_const);
    default:
        fatal_error("literal_to_double: Unknown kind %d", lit->kind);
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
        result            = new_tac_static_init(TAC_STATIC_INIT_I32);
        result->u.int_val = (literal_to_int64(lit) != 0);
        break;

    case TYPE_CHAR:
    case TYPE_SCHAR:
        result             = new_tac_static_init(TAC_STATIC_INIT_I8);
        result->u.char_val = (int8_t)literal_to_int64(lit);
        break;

    case TYPE_UCHAR:
        result              = new_tac_static_init(TAC_STATIC_INIT_U8);
        result->u.uchar_val = (uint8_t)literal_to_int64(lit);
        break;

    case TYPE_SHORT:
    case TYPE_INT:
        result            = new_tac_static_init(TAC_STATIC_INIT_I32);
        result->u.int_val = (int32_t)literal_to_int64(lit);
        break;

    case TYPE_USHORT:
    case TYPE_UINT:
        result             = new_tac_static_init(TAC_STATIC_INIT_U32);
        result->u.uint_val = (uint32_t)literal_to_int64(lit);
        break;

    case TYPE_LONG:
    case TYPE_LONG_LONG:
        result             = new_tac_static_init(TAC_STATIC_INIT_I64);
        result->u.long_val = literal_to_int64(lit);
        break;

    case TYPE_ULONG:
    case TYPE_ULONG_LONG:
        result              = new_tac_static_init(TAC_STATIC_INIT_U64);
        result->u.ulong_val = literal_to_uint64(lit);
        break;

    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
        result               = new_tac_static_init(TAC_STATIC_INIT_DOUBLE);
        result->u.double_val = literal_to_double(lit);
        break;
    default:
        fatal_error("Unsupported constant type for initializer");
    }
    return result;
}
