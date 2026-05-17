//
// Helpers for Type.
//
#include <assert.h>

#include "semantic.h"
#include "structtab.h"
#include "typetab.h"

// Replace every TYPE_TYPEDEF_NAME node in the type tree with a cloned, fully-resolved
// type. Returns the (potentially new) root pointer; caller must use the return value.
Type *resolve_typedef_names(Type *t)
{
    if (!t)
        return NULL;
    if (t->kind == TYPE_TYPEDEF_NAME) {
        const Type *resolved = typetab_resolve(t->u.typedef_name.name);
        Type *cloned = clone_type(resolved, __func__, __FILE__, __LINE__);
        free_type(t);
        return resolve_typedef_names(cloned); // handles chains and nested typedef names
    }
    switch (t->kind) {
    case TYPE_POINTER:
        t->u.pointer.target = resolve_typedef_names(t->u.pointer.target);
        break;
    case TYPE_ARRAY:
        t->u.array.element = resolve_typedef_names(t->u.array.element);
        break;
    case TYPE_FUNCTION:
        t->u.function.return_type = resolve_typedef_names(t->u.function.return_type);
        for (Param *p = t->u.function.params; p; p = p->next)
            p->type = resolve_typedef_names(p->type);
        break;
    default:
        break; // primitive, struct, union, enum — no child types
    }
    return t;
}

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
