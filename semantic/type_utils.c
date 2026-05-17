//
// Helpers for Type.
//
#include <assert.h>

#include "semantic.h"
#include "structtab.h"
#include "typetab.h"

//
// Get size in bytes for a given type.
//
size_t get_size(const Type *t)
{
    switch (t->kind) {
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_UCHAR:
        return 1;
    case TYPE_SHORT:
    case TYPE_USHORT:
        return 2;
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_FLOAT:
    case TYPE_ENUM:
        return 4;
    case TYPE_LONG:
    case TYPE_ULONG:
    case TYPE_LONG_LONG:
    case TYPE_ULONG_LONG:
    case TYPE_DOUBLE:
    case TYPE_POINTER:
        return 8;
    case TYPE_LONG_DOUBLE:
        return 16;
    case TYPE_ARRAY:
        if (!t->u.array.size) {
            fatal_error("get_size: Array size not specified");
        }
        assert(t->u.array.size);
        if (t->u.array.size->kind != EXPR_LITERAL) {
            fatal_error("get_size: Array size is not literal");
        }
        return t->u.array.size->u.literal->u.int_val * get_size(t->u.array.element);
    case TYPE_STRUCT:
    case TYPE_UNION:
        return structtab_find(t->u.struct_t.name)->size;
    case TYPE_TYPEDEF_NAME:
        return get_size(typetab_resolve(t->u.typedef_name.name));
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
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_UCHAR:
        return 1;
    case TYPE_SHORT:
    case TYPE_USHORT:
        return 2;
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_FLOAT:
    case TYPE_ENUM:
        return 4;
    case TYPE_LONG:
    case TYPE_ULONG:
    case TYPE_LONG_LONG:
    case TYPE_ULONG_LONG:
    case TYPE_DOUBLE:
    case TYPE_POINTER:
        return 8;
    case TYPE_LONG_DOUBLE:
        return 16;
    case TYPE_ARRAY:
        return get_alignment(t->u.array.element);
    case TYPE_STRUCT:
    case TYPE_UNION:
        return structtab_find(t->u.struct_t.name)->alignment;
    case TYPE_TYPEDEF_NAME:
        return get_alignment(typetab_resolve(t->u.typedef_name.name));
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
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_LONG_LONG:
    case TYPE_ENUM:
        return true;
    case TYPE_BOOL:
    case TYPE_UCHAR:
    case TYPE_USHORT:
    case TYPE_UINT:
    case TYPE_ULONG:
    case TYPE_ULONG_LONG:
    case TYPE_POINTER:
        return false;
    case TYPE_TYPEDEF_NAME:
        return is_signed(typetab_resolve(t->u.typedef_name.name));
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
    case TYPE_FUNCTION:
    case TYPE_ARRAY:
    case TYPE_VOID:
    case TYPE_STRUCT:
    case TYPE_UNION:
    default:
        fatal_error("is_signed: Signedness doesn't make sense for non-integral type %s",
                    type_kind_str[t->kind]);
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
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_UCHAR:
    case TYPE_SCHAR:
    case TYPE_SHORT:
    case TYPE_USHORT:
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_LONG:
    case TYPE_ULONG:
    case TYPE_LONG_LONG:
    case TYPE_ULONG_LONG:
    case TYPE_ENUM:
        return true;
    case TYPE_TYPEDEF_NAME:
        return is_integer(typetab_resolve(t->u.typedef_name.name));
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
    case TYPE_ARRAY:
    case TYPE_POINTER:
    case TYPE_FUNCTION:
    case TYPE_VOID:
    case TYPE_STRUCT:
    case TYPE_UNION:
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
    case TYPE_TYPEDEF_NAME:
        return is_character(typetab_resolve(t->u.typedef_name.name));
    default:
        return false;
    }
}

bool is_arithmetic(const Type *t)
{
    switch (t->kind) {
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_UCHAR:
    case TYPE_SCHAR:
    case TYPE_SHORT:
    case TYPE_USHORT:
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_LONG:
    case TYPE_ULONG:
    case TYPE_LONG_LONG:
    case TYPE_ULONG_LONG:
    case TYPE_ENUM:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
        return true;
    case TYPE_TYPEDEF_NAME:
        return is_arithmetic(typetab_resolve(t->u.typedef_name.name));
    case TYPE_FUNCTION:
    case TYPE_POINTER:
    case TYPE_ARRAY:
    case TYPE_VOID:
    case TYPE_STRUCT:
    case TYPE_UNION:
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
    case TYPE_UNION:
        return false;
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_UCHAR:
    case TYPE_SCHAR:
    case TYPE_SHORT:
    case TYPE_USHORT:
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_LONG:
    case TYPE_ULONG:
    case TYPE_LONG_LONG:
    case TYPE_ULONG_LONG:
    case TYPE_ENUM:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
    case TYPE_POINTER:
        return true;
    case TYPE_TYPEDEF_NAME:
        return is_scalar(typetab_resolve(t->u.typedef_name.name));
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
        return structtab_exists(t->u.struct_t.name);
    case TYPE_TYPEDEF_NAME:
        return is_complete(typetab_resolve(t->u.typedef_name.name));
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
