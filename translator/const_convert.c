#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Type definitions from types.ml
typedef enum {
    TYPE_CHAR,
    TYPE_SCHAR,
    TYPE_UCHAR,
    TYPE_INT,
    TYPE_LONG,
    TYPE_UINT,
    TYPE_ULONG,
    TYPE_DOUBLE,
    TYPE_POINTER, // Simplified: we don't store the pointed-to type
    TYPE_VOID,
    TYPE_ARRAY,  // Simplified: we don't store elem_type or size
    TYPE_FUNTYPE,// Simplified: we don't store param_types or ret_type
    TYPE_STRUCTURE // Simplified: we don't store tag
} Type;

// Constant definitions from const.ml
typedef enum {
    CONST_CHAR,
    CONST_UCHAR,
    CONST_INT,
    CONST_LONG,
    CONST_UINT,
    CONST_ULONG,
    CONST_DOUBLE
} ConstTag;

typedef struct {
    ConstTag tag;
    union {
        int8_t char_val;    // ConstChar
        uint8_t uchar_val;  // ConstUChar
        int32_t int_val;    // ConstInt
        int64_t long_val;   // ConstLong
        uint32_t uint_val;  // ConstUInt
        uint64_t ulong_val; // ConstULong
        double double_val;  // ConstDouble
    } value;
} Const;

// Helper function: Get the type of a constant
Type type_of_const(Const c) {
    switch (c.tag) {
        case CONST_CHAR:   return TYPE_SCHAR;
        case CONST_UCHAR:  return TYPE_UCHAR;
        case CONST_INT:    return TYPE_INT;
        case CONST_LONG:   return TYPE_LONG;
        case CONST_UINT:   return TYPE_UINT;
        case CONST_ULONG:  return TYPE_ULONG;
        case CONST_DOUBLE: return TYPE_DOUBLE;
        default:
            fprintf(stderr, "Internal error: unknown constant tag %d\n", c.tag);
            exit(1);
    }
}

// Helper function: Convert constant to int64
int64_t const_to_int64(Const c) {
    switch (c.tag) {
        case CONST_CHAR:   return (int64_t)c.value.char_val;   // Sign-extend
        case CONST_UCHAR:  return (int64_t)c.value.uchar_val;  // Zero-extend
        case CONST_INT:    return (int64_t)c.value.int_val;    // Sign-extend
        case CONST_LONG:   return c.value.long_val;            // Same size
        case CONST_UINT:   return (int64_t)c.value.uint_val;   // Zero-extend
        case CONST_ULONG:  return (int64_t)c.value.ulong_val;  // Preserve representation
        case CONST_DOUBLE: return (int64_t)c.value.double_val; // Truncate to int64
        default:
            fprintf(stderr, "Internal error: unknown constant tag %d\n", c.tag);
            exit(1);
    }
}

// Helper function: Convert int64 to constant of target type
Const const_of_int64(int64_t v, Type target_type) {
    Const result;
    switch (target_type) {
        case TYPE_CHAR:
        case TYPE_SCHAR:
            result.tag = CONST_CHAR;
            result.value.char_val = (int8_t)v; // Wrap modulo 2^8
            return result;
        case TYPE_UCHAR:
            result.tag = CONST_UCHAR;
            result.value.uchar_val = (uint8_t)v; // Wrap modulo 2^8
            return result;
        case TYPE_INT:
            result.tag = CONST_INT;
            result.value.int_val = (int32_t)v; // Wrap modulo 2^32
            return result;
        case TYPE_LONG:
            result.tag = CONST_LONG;
            result.value.long_val = v; // No wrapping needed
            return result;
        case TYPE_UINT:
            result.tag = CONST_UINT;
            result.value.uint_val = (uint32_t)v; // Wrap modulo 2^32
            return result;
        case TYPE_ULONG:
        case TYPE_POINTER:
            result.tag = CONST_ULONG;
            result.value.ulong_val = (uint64_t)v; // Wrap modulo 2^64
            return result;
        case TYPE_DOUBLE:
            result.tag = CONST_DOUBLE;
            result.value.double_val = (double)v; // Convert to double
            return result;
        case TYPE_VOID:
        case TYPE_ARRAY:
        case TYPE_FUNTYPE:
        case TYPE_STRUCTURE:
            fprintf(stderr, "Internal error: can't convert constant to non-scalar type %d\n", target_type);
            exit(1);
        default:
            fprintf(stderr, "Internal error: unknown target type %d\n", target_type);
            exit(1);
    }
}

// Main function: Convert constant to target type
Const const_convert(Type target_type, Const c) {
    // If types match, return unchanged
    if (type_of_const(c) == target_type) {
        return c;
    }

    // Special case: ULong to Double
    if (target_type == TYPE_DOUBLE && c.tag == CONST_ULONG) {
        Const result;
        result.tag = CONST_DOUBLE;
        result.value.double_val = (double)c.value.ulong_val; // Approximate Z.to_float
        return result;
    }

    // Special case: Double to ULong
    if (target_type == TYPE_ULONG && c.tag == CONST_DOUBLE) {
        Const result;
        result.tag = CONST_ULONG;
        result.value.ulong_val = (uint64_t)c.value.double_val; // Approximate Z.of_float
        return result;
    }

    // General case: Convert through int64
    int64_t as_int64 = const_to_int64(c);
    return const_of_int64(as_int64, target_type);
}
