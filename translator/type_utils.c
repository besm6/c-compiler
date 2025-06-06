//
// Helpers for Type.
//
#include "typetab.h"
#include "translator.h"

//
// Get size in bytes for a given type.
//
size_t get_size(const Type *t)
{
    switch (t->kind) {
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_UCHAR:
        return 1;
    case TYPE_INT:
    case TYPE_UINT:
        return 4;
    case TYPE_LONG:
    case TYPE_ULONG:
    case TYPE_DOUBLE:
    case TYPE_POINTER:
        return 8;
    case TYPE_ARRAY:
        if (!t->u.array.size) {
            fatal_error("get_size: Array size not specified");
        }
        if (t->u.array.size->kind != EXPR_LITERAL) {
            fatal_error("get_size: Array size is not literal");
        }
        return t->u.array.size->u.literal->u.int_val * get_size(t->u.array.element);
    case TYPE_STRUCT:
        return typetab_find(t->u.struct_t.name)->size;
    case TYPE_FUNCTION:
    case TYPE_VOID:
    default:
        fatal_error("get_size: Type %s doesn't have size", type_kind_str[t->kind]);
    }
    return 0; // Unreachable
}

size_t get_alignment(const Type *t)
{
    switch (t->kind) {
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_UCHAR:
        return 1;
    case TYPE_INT:
    case TYPE_UINT:
        return 4;
    case TYPE_LONG:
    case TYPE_ULONG:
    case TYPE_DOUBLE:
    case TYPE_POINTER:
        return 8;
    case TYPE_ARRAY:
        return get_alignment(t->u.array.element);
    case TYPE_STRUCT:
        return typetab_find(t->u.struct_t.name)->alignment;
    case TYPE_FUNCTION:
    case TYPE_VOID:
    default:
        fatal_error("get_alignment: Type %s doesn't have alignment", type_kind_str[t->kind]);
    }
    return 0; // Unreachable
}

bool is_signed(const Type *t)
{
    switch (t->kind) {
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_CHAR:
    case TYPE_SCHAR:
        return true;
    case TYPE_UINT:
    case TYPE_ULONG:
    case TYPE_UCHAR:
    case TYPE_POINTER:
        return false;
    case TYPE_DOUBLE:
    case TYPE_FUNCTION:
    case TYPE_ARRAY:
    case TYPE_VOID:
    case TYPE_STRUCT:
    default:
        fatal_error("is_signed: Signedness doesn't make sense for non-integral type %s", type_kind_str[t->kind]);
    }
    return false; // Unreachable
}

bool is_pointer(const Type *t)
{
    return t->kind == TYPE_POINTER;
}

bool is_integer(const Type *t)
{
    switch (t->kind) {
    case TYPE_CHAR:
    case TYPE_UCHAR:
    case TYPE_SCHAR:
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_LONG:
    case TYPE_ULONG:
        return true;
    case TYPE_DOUBLE:
    case TYPE_ARRAY:
    case TYPE_POINTER:
    case TYPE_FUNCTION:
    case TYPE_VOID:
    case TYPE_STRUCT:
    default:
        return false;
    }
}

bool is_array(const Type *t)
{
    return t->kind == TYPE_ARRAY;
}

bool is_character(const Type *t)
{
    switch (t->kind) {
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_UCHAR:
        return true;
    default:
        return false;
    }
}

bool is_arithmetic(const Type *t)
{
    switch (t->kind) {
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_LONG:
    case TYPE_ULONG:
    case TYPE_CHAR:
    case TYPE_UCHAR:
    case TYPE_SCHAR:
    case TYPE_DOUBLE:
        return true;
    case TYPE_FUNCTION:
    case TYPE_POINTER:
    case TYPE_ARRAY:
    case TYPE_VOID:
    case TYPE_STRUCT:
    default:
        return false;
    }
}

bool is_scalar(const Type *t)
{
    switch (t->kind) {
    case TYPE_ARRAY:
    case TYPE_VOID:
    case TYPE_FUNCTION:
    case TYPE_STRUCT:
        return false;
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_LONG:
    case TYPE_ULONG:
    case TYPE_CHAR:
    case TYPE_UCHAR:
    case TYPE_SCHAR:
    case TYPE_DOUBLE:
    case TYPE_POINTER:
        return true;
    default:
        return false;
    }
}

bool is_complete(const Type *t)
{
    switch (t->kind) {
    case TYPE_VOID:
        return false;
    case TYPE_STRUCT:
        return typetab_exists(t->u.struct_t.name);
    default:
        return true;
    }
}

bool is_complete_pointer(const Type *t)
{
    if (t->kind == TYPE_POINTER) {
        return is_complete(t->u.pointer.target);
    }
    return false;
}
