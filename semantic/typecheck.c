//
// Core type-checking utilities and entry points.
//
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic.h"
#include "structtab.h"
#include "symtab.h"
#include "typecheck.h"
#include "typetab.h"

// Enable debug output
int semantic_debug;

// Level of scope for nested compound operators.
int scope_level;

void scope_increment(void)
{
    scope_level++;
}

void scope_decrement(void)
{
    scope_level--;
    symtab_purge(scope_level);
    structtab_purge(scope_level);
    typetab_purge(scope_level);
}

int round_away_from_zero(int alignment, int size)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (size % alignment == 0) {
        return size;
    }

    if (size < 0) {
        return size - alignment - (size % alignment);
    } else {
        return size + alignment - (size % alignment);
    }
}

size_t get_array_size(const Type *t)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (t->kind != TYPE_ARRAY) {
        fatal_error("get_array_size: Array is expected");
    }
    if (!t->u.array.size) {
        return 0;
    }
    if (t->u.array.size->kind != EXPR_LITERAL) {
        fatal_error("get_array_size: Size is not a literal");
    }
    if (!t->u.array.size->u.literal) {
        fatal_error("get_array_size: No literal in size");
    }
    assert(t->u.array.size->u.literal);
    if (t->u.array.size->u.literal->kind != LITERAL_INT) {
        fatal_error("get_array_size: Non-integer size");
    }
    return t->u.array.size->u.literal->u.int_val;
}

void set_array_size(Type *t, size_t size)
{
    t->u.array.size                       = new_expression(EXPR_LITERAL);
    t->u.array.size->u.literal            = new_literal(LITERAL_INT);
    t->u.array.size->u.literal->u.int_val = size;
}

// Validate a type (recursive).
void validate_type(const Type *t)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
        print_type(stdout, t, 4);
    }
    if (!t)
        return;
    switch (t->kind) {
    case TYPE_ARRAY:
        if (!is_complete(t->u.array.element)) {
            fatal_error("Array of incomplete type");
        }
        validate_type(t->u.array.element);
        break;
    case TYPE_POINTER:
        validate_type(t->u.pointer.target);
        break;
    case TYPE_FUNCTION:
        validate_type(t->u.function.return_type);
        for (const Param *p = t->u.function.params; p; p = p->next) {
            validate_type(p->type);
        }
        break;
    case TYPE_VOID:
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_UCHAR:
    case TYPE_SHORT:
    case TYPE_USHORT:
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_LONG:
    case TYPE_ULONG:
    case TYPE_LONG_LONG:
    case TYPE_ULONG_LONG:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_STRUCT:
    case TYPE_UNION:
    case TYPE_ENUM:
        break;
    default:
        fatal_error("Unsupported type kind %d", t->kind);
    }
}

// Convert an expression to a target type.
Expr *convert_to_type(Expr *e, const Type *target_type)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    const Type *src = e->type;
    const Type *tgt = target_type;
    if (src->kind == tgt->kind &&
        (!is_pointer(src) || src->u.pointer.target->kind == tgt->u.pointer.target->kind))
        return e; // Avoid unnecessary casts

    Expr *cast        = new_expression(EXPR_CAST);
    cast->u.cast.type = clone_type(target_type, __func__, __FILE__, __LINE__);
    cast->u.cast.expr = e;
    cast->type        = clone_type(target_type, __func__, __FILE__, __LINE__);
    return cast;
}

Expr *convert_to_kind(Expr *e, TypeKind target_kind)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (e->type->kind == target_kind)
        return e; // Avoid unnecessary casts

    Expr *cast        = new_expression(EXPR_CAST);
    cast->u.cast.type = new_type(target_kind, __func__, __FILE__, __LINE__);
    cast->u.cast.expr = e;
    cast->type        = new_type(target_kind, __func__, __FILE__, __LINE__);
    return cast;
}

// Get common type for arithmetic operations.
const Type *get_common_type(const Type *t1, const Type *t2)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    static const Type int_type    = { .kind = TYPE_INT };
    static const Type double_type = { .kind = TYPE_DOUBLE };
    static const Type float_type  = { .kind = TYPE_FLOAT };
    if (is_character(t1))
        t1 = &int_type;
    if (is_character(t2))
        t2 = &int_type;
    if (t1->kind == TYPE_SHORT || t1->kind == TYPE_USHORT)
        t1 = &int_type;
    if (t2->kind == TYPE_SHORT || t2->kind == TYPE_USHORT)
        t2 = &int_type;
    if (t1->kind == t2->kind)
        return t1;
    if (t1->kind == TYPE_DOUBLE || t2->kind == TYPE_DOUBLE)
        return &double_type;
    if (t1->kind == TYPE_FLOAT || t2->kind == TYPE_FLOAT)
        return &float_type;
    if (get_size(t1) == get_size(t2))
        return is_signed(t1) ? t2 : t1;
    return get_size(t1) > get_size(t2) ? t1 : t2;
}

// Check if a constant is a zero integer.
bool is_zero_int(const Literal *c)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    switch (c->kind) {
    case LITERAL_INT:
        return c->u.int_val == 0;
    case LITERAL_CHAR:
        return c->u.char_val == 0;
    case LITERAL_ENUM:
        return false; // Conservative
    default:
        return false;
    }
}

// Check if an expression is a null pointer constant.
bool is_null_pointer_constant(const Expr *e)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    return e->kind == EXPR_LITERAL && is_zero_int(e->u.literal);
}

// Get common pointer type for pointer-involved binary operations.
Type *common_pointer_type(const Expr *e1, const Expr *e2)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (e1->type->kind == e2->type->kind &&
        e1->type->u.pointer.target->kind == e2->type->u.pointer.target->kind)
        return e1->type;
    if (is_null_pointer_constant(e1))
        return e2->type;
    if (is_null_pointer_constant(e2))
        return e1->type;

    Type *void_type = new_type(TYPE_VOID, __func__, __FILE__, __LINE__);
    if ((e1->type->kind == TYPE_POINTER && e1->type->u.pointer.target->kind == TYPE_VOID) ||
        (e2->type->kind == TYPE_POINTER && e2->type->u.pointer.target->kind == TYPE_VOID)) {
        Type *void_ptr             = new_type(TYPE_POINTER, __func__, __FILE__, __LINE__);
        void_ptr->u.pointer.target = void_type;
        return void_ptr;
    }
    fatal_error("Incompatible pointer types");
}

// Parser represents f(void) as a single unnamed TYPE_VOID param; treat as no params.
static const Param *params_for_compat(const Type *fn_type)
{
    const Param *params = fn_type->u.function.params;
    if (params && !params->next && params->type->kind == TYPE_VOID && !params->name)
        return NULL;
    return params;
}

// Return true if src may initialize or be assigned to target (target is the lhs type).
bool compatible_type(const Type *target, const Type *src)
{
    if (!target && !src)
        return true;
    if (!target || !src)
        return false;
    if (target->kind != src->kind)
        return false;
    switch (target->kind) {
    case TYPE_FUNCTION: {
        if (target->u.function.variadic != src->u.function.variadic)
            return false;
        if (!compatible_type(target->u.function.return_type, src->u.function.return_type))
            return false;
        const Param *tp = params_for_compat(target);
        const Param *sp = params_for_compat(src);
        if (!tp || !sp)
            return true; // old-style or f(void): no prototype to compare
        return compare_param(tp, sp);
    }
    case TYPE_POINTER:
        return compatible_type(target->u.pointer.target, src->u.pointer.target);
    case TYPE_STRUCT:
    case TYPE_UNION:
        return strcmp(target->u.struct_t.name, src->u.struct_t.name) == 0;
    default:
        return compare_type(target, src);
    }
}

// Convert an expression for assignment to target_type.
Expr *coerce_for_assignment(Expr *e, const Type *target_type)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    const Type *e_type = e->type;
    if (e_type->kind == target_type->kind && compatible_type(target_type, e_type))
        return e;
    if (is_arithmetic(e_type) && is_arithmetic(target_type))
        return convert_to_type(e, target_type);
    if (is_null_pointer_constant(e) && is_pointer(target_type))
        return convert_to_type(e, target_type);
    if ((target_type->kind == TYPE_POINTER && target_type->u.pointer.target->kind == TYPE_VOID &&
         is_pointer(e_type)) ||
        (is_pointer(target_type) && e_type->kind == TYPE_POINTER &&
         e_type->u.pointer.target->kind == TYPE_VOID)) {
        return convert_to_type(e, target_type);
    }
    // C11 §6.7.6.3p7: array param adjusts to pointer; int* arg matches int[N] param
    if (e_type->kind == TYPE_POINTER && target_type->kind == TYPE_ARRAY &&
        e_type->u.pointer.target->kind == target_type->u.array.element->kind)
        return e;
    fatal_error("Cannot convert type for assignment");
}

// Evaluate a constant integer expression; return false if not constant.
bool try_eval_const_int(const Expr *e, long *out)
{
    switch (e->kind) {
    case EXPR_LITERAL:
        switch (e->u.literal->kind) {
        case LITERAL_INT:
            *out = e->u.literal->u.int_val;
            return true;
        case LITERAL_CHAR:
            *out = (unsigned char)e->u.literal->u.char_val;
            return true;
        default:
            return false;
        }
    case EXPR_CAST: {
        long inner;
        if (try_eval_const_int(e->u.cast.expr, &inner)) {
            *out = inner;
            return true;
        }
        return false;
    }
    case EXPR_UNARY_OP: {
        long inner;
        if (!try_eval_const_int(e->u.unary_op.expr, &inner))
            return false;
        switch (e->u.unary_op.op) {
        case UNARY_NEG:
            *out = -inner;
            return true;
        case UNARY_PLUS:
            *out = inner;
            return true;
        case UNARY_BIT_NOT:
            *out = ~inner;
            return true;
        default:
            return false;
        }
    }
    case EXPR_BINARY_OP: {
        long left, right;
        if (!try_eval_const_int(e->u.binary_op.left, &left) ||
            !try_eval_const_int(e->u.binary_op.right, &right))
            return false;
        switch (e->u.binary_op.op) {
        case BINARY_MUL:
            *out = left * right;
            return true;
        case BINARY_DIV:
        case BINARY_MOD:
            if (right == 0)
                return false;
            *out = (e->u.binary_op.op == BINARY_DIV) ? left / right : left % right;
            return true;
        case BINARY_ADD:
            *out = left + right;
            return true;
        case BINARY_SUB:
            *out = left - right;
            return true;
        case BINARY_LEFT_SHIFT:
            if (right < 0 || (unsigned long)right >= sizeof(long) * 8)
                return false;
            *out = (long)((unsigned long)left << (unsigned long)right);
            return true;
        case BINARY_RIGHT_SHIFT:
            if (right < 0 || (unsigned long)right >= sizeof(long) * 8)
                return false;
            *out = left >> right;
            return true;
        case BINARY_LT:
            *out = left < right;
            return true;
        case BINARY_GT:
            *out = left > right;
            return true;
        case BINARY_LE:
            *out = left <= right;
            return true;
        case BINARY_GE:
            *out = left >= right;
            return true;
        case BINARY_EQ:
            *out = left == right;
            return true;
        case BINARY_NE:
            *out = left != right;
            return true;
        case BINARY_BIT_AND:
            *out = left & right;
            return true;
        case BINARY_BIT_XOR:
            *out = left ^ right;
            return true;
        case BINARY_BIT_OR:
            *out = left | right;
            return true;
        case BINARY_LOG_AND:
            *out = (left != 0 && right != 0) ? 1L : 0L;
            return true;
        case BINARY_LOG_OR:
            *out = (left != 0 || right != 0) ? 1L : 0L;
            return true;
        default:
            return false;
        }
    }
    default:
        return false;
    }
}

// Type-check a global declaration and label its loops.
void typecheck_decl(ExternalDecl *d)
{
    typecheck_global_decl(d);
    label_loops(d);
}

// Type-check an entire program.
void typecheck_program(Program *p)
{
    for (ExternalDecl *d = p->decls; d; d = d->next) {
        typecheck_decl(d);
    }
}
