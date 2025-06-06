#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "translator.h"
#include "symtab.h"
#include "typetab.h"
#include "string_map.h"
#include "xalloc.h"

// Forward declarations
void validate_type(const Type *t);
Expr *typecheck_and_convert(Expr *e);
Expr *typecheck_scalar(Expr *e);
Initializer *typecheck_init(const Type *target_type, Initializer *init);
StaticInitializer *static_init_helper(const Type *var_type, const Initializer *init);
Stmt *typecheck_statement(const Type *ret_type, Stmt *s);
void typecheck_local_decl(Declaration *d);

static int round_away_from_zero(int alignment, int size)
{
    if (size % alignment == 0) {
        return size;
    }

    if (size < 0) {
        return size - alignment - (size % alignment);
    } else {
        return size + alignment - (size % alignment);
    }
}

// Check if an expression is an lvalue
bool is_lvalue(const Expr *e)
{
    switch (e->kind) {
    case EXPR_VAR:
    case EXPR_FIELD_ACCESS:
    case EXPR_PTR_ACCESS:
        return true;
    case EXPR_BINARY_OP:
        if (e->u.binary_op.op == BINARY_LOG_AND || e->u.binary_op.op == BINARY_LOG_OR) {
            return false;
        }
        return is_lvalue(e->u.binary_op.left);
    case EXPR_UNARY_OP:
        if (e->u.unary_op.op == UNARY_DEREF) {
            return true;
        }
        return false;
    default:
        return false;
    }
}

// Validate a type (recursive)
void validate_type(const Type *t)
{
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
        for (Param *p = t->u.function.params; p; p = p->next) {
            validate_type(p->type);
        }
        break;
    case TYPE_VOID:
    case TYPE_CHAR:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_SHORT:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_STRUCT:
        break;
    default:
        fatal_error("Unsupported type kind %d", t->kind);
    }
}

// Validate a struct definition
void validate_struct_definition(const char *tag, const Field *members)
{
    if (typetab_exists(tag)) {
        fatal_error("Structure %s was already declared", tag);
    }

    // Check for duplicate member names
    StringMap names;
    map_init(&names);
    for (const Field *m = members; m; m = m->next) {
        if (m->type->kind == TYPE_FUNCTION) {
            fatal_error("Can't declare structure member with function type");
        }
        if (!is_complete(m->type)) {
            fatal_error("Cannot declare structure member with incomplete type");
        }
        if (map_get(&names, m->name, NULL)) {
            fatal_error("Duplicate member %s in structure %s", m->name, tag);
        }
        map_insert(&names, m->name, 0, 0);
        validate_type(m->type);
    }
    map_destroy(&names);
}

// Type-check a struct declaration
void typecheck_struct_decl(const Declaration *d)
{
    if (!d->u.var.declarators)
        return; // Ignore forward declarations

    validate_struct_definition(d->u.var.declarators->type->u.struct_t.name,
                               d->u.var.declarators->type->u.struct_t.fields);

    // Build member definitions
    FieldDef *members     = NULL;
    FieldDef **tail       = &members;
    int current_size      = 0;
    int current_alignment = 1;
    for (Field *f = d->u.var.declarators->type->u.struct_t.fields; f; f = f->next) {
        int member_alignment = get_alignment(f->type);
        int offset           = round_away_from_zero(member_alignment, current_size);

        *tail = new_member(f->name, clone_type(f->type), offset);
        tail = &(*tail)->next;

        current_alignment = current_alignment > member_alignment ? current_alignment : member_alignment;
        current_size      = offset + get_size(f->type);
    }
    int size = round_away_from_zero(current_alignment, current_size);
    typetab_add_struct(d->u.var.declarators->type->u.struct_t.name, current_alignment, size, members);
}

// Convert an expression to a target type
Expr *convert_to_type(Expr *e, const Type *target_type)
{
    if (e->type->kind == target_type->kind &&
        (!is_pointer(e->type) ||
         e->type->u.pointer.target->kind == target_type->u.pointer.target->kind))
        return e; // Avoid unnecessary casts

    Expr *cast = new_expression(EXPR_CAST);
    cast->u.cast.type = clone_type(target_type);
    cast->u.cast.expr = e;
    cast->type = clone_type(target_type);
    return cast;
}

Expr *convert_to_kind(Expr *e, TypeKind target_kind)
{
    if (e->type->kind == target_kind)
        return e; // Avoid unnecessary casts

    Expr *cast = new_expression(EXPR_CAST);
    cast->u.cast.type = new_type(target_kind);
    cast->u.cast.expr = e;
    cast->type = new_type(target_kind);
    return cast;
}

// Get common type for arithmetic operations
const Type *get_common_type(const Type *t1, const Type *t2)
{
    static const Type int_type    = { .kind = TYPE_INT };
    static const Type double_type = { .kind = TYPE_DOUBLE };
    if (is_character(t1))
        t1 = &int_type;
    if (is_character(t2))
        t2 = &int_type;
    if (t1->kind == t2->kind)
        return t1;
    if (t1->kind == TYPE_DOUBLE || t2->kind == TYPE_DOUBLE)
        return &double_type;
    if (get_size(t1) == get_size(t2))
        return is_signed(t1) ? t2 : t1;
    return get_size(t1) > get_size(t2) ? t1 : t2;
}

// Check if a constant is a zero integer
bool is_zero_int(const Literal *c)
{
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

// Check if an expression is a null pointer constant
bool is_null_pointer_constant(const Expr *e)
{
    return e->kind == EXPR_LITERAL && is_zero_int(e->u.literal);
}

// Get common pointer type
Type *get_common_pointer_type(const Expr *e1, const Expr *e2)
{
    if (e1->type->kind == e2->type->kind &&
        e1->type->u.pointer.target->kind == e2->type->u.pointer.target->kind)
        return e1->type;
    if (is_null_pointer_constant(e1))
        return e2->type;
    if (is_null_pointer_constant(e2))
        return e1->type;
    Type *void_type = new_type(TYPE_VOID);
    if ((e1->type->kind == TYPE_POINTER && e1->type->u.pointer.target->kind == TYPE_VOID) ||
        (e2->type->kind == TYPE_POINTER && e2->type->u.pointer.target->kind == TYPE_VOID)) {
        Type *void_ptr             = new_type(TYPE_POINTER);
        void_ptr->u.pointer.target = void_type;
        return void_ptr;
    }
    fatal_error("Incompatible pointer types");
}

// Convert by assignment
Expr *convert_by_assignment(Expr *e, const Type *target_type)
{
    if (e->type->kind == target_type->kind &&
        (!is_pointer(e->type) ||
         e->type->u.pointer.target->kind == target_type->u.pointer.target->kind))
        return e;
    if (is_arithmetic(e->type) && is_arithmetic(target_type))
        return convert_to_type(e, target_type);
    if (is_null_pointer_constant(e) && is_pointer(target_type))
        return convert_to_type(e, target_type);
    if ((target_type->kind == TYPE_POINTER && target_type->u.pointer.target->kind == TYPE_VOID &&
         is_pointer(e->type)) ||
        (is_pointer(target_type) && e->type->kind == TYPE_POINTER &&
         e->type->u.pointer.target->kind == TYPE_VOID)) {
        return convert_to_type(e, target_type);
    }
    fatal_error("Cannot convert type for assignment");
}

// Type-check a variable
Expr *typecheck_var(Expr *e)
{
    const Symbol *sym = symtab_get(e->u.var);

    if (sym->type->kind == TYPE_FUNCTION) {
        fatal_error("Tried to use function name as variable");
    }
    e->type = clone_type(sym->type);
    return e;
}

// Type-check a constant
Expr *typecheck_const(Expr *e)
{
    switch (e->u.literal->kind) {
    case LITERAL_INT:
        e->type = new_type(TYPE_INT);
        break;
    case LITERAL_CHAR:
        e->type = new_type(TYPE_CHAR);
        break;
    case LITERAL_FLOAT:
        e->type = new_type(TYPE_DOUBLE);
        break;
    case LITERAL_STRING: {
        Type *array            = new_type(TYPE_ARRAY);
        array->u.array.element = new_type(TYPE_CHAR);
        array->u.array.size    = (Expr *)(size_t)(strlen(e->u.literal->u.string_val) + 1);
        e->type                = array;
        break;
    }
    case LITERAL_ENUM:
        e->type = new_type(TYPE_INT);
        break;
    default:
        fatal_error("Unsupported literal kind %d", e->u.literal->kind);
    }
    return e;
}

// Type-check a string literal
Expr *typecheck_string(Expr *e)
{
    Type *array            = new_type(TYPE_ARRAY);
    array->u.array.element = new_type(TYPE_CHAR);
    array->u.array.size    = (Expr *)(size_t)(strlen(e->u.literal->u.string_val) + 1);
    e->type                = array;
    return e;
}

// Type-check an expression
Expr *typecheck_exp(Expr *e)
{
    if (!e)
        return NULL;
    switch (e->kind) {
    case EXPR_VAR:
        return typecheck_var(e);
    case EXPR_LITERAL:
        return e->u.literal->kind == LITERAL_STRING ? typecheck_string(e) : typecheck_const(e);
    case EXPR_CAST: {
        validate_type(e->u.cast.type);
        Expr *inner = typecheck_and_convert(e->u.cast.expr);
        if ((e->u.cast.type->kind == TYPE_DOUBLE && is_pointer(inner->type)) ||
            (is_pointer(e->u.cast.type) && inner->type->kind == TYPE_DOUBLE)) {
            fatal_error("Cannot cast between pointer and double");
        }
        if (e->u.cast.type->kind == TYPE_VOID) {
            e->type        = clone_type(e->u.cast.type);
            e->u.cast.expr = inner;
            return e;
        }
        if (!is_scalar(e->u.cast.type) || !is_scalar(inner->type)) {
            fatal_error("Can only cast scalar types");
        }
        e->type        = clone_type(e->u.cast.type);
        e->u.cast.expr = inner;
        return e;
    }
    case EXPR_UNARY_OP: {
        switch (e->u.unary_op.op) {
        case UNARY_LOG_NOT: {
            Expr *inner        = typecheck_scalar(e->u.unary_op.expr);
            e->type            = new_type(TYPE_INT);
            e->u.unary_op.expr = inner;
            return e;
        }
        case UNARY_BIT_NOT: {
            Expr *inner = typecheck_and_convert(e->u.unary_op.expr);
            if (!is_integer(inner->type)) {
                fatal_error("Bitwise complement only valid for integer types");
            }
            if (is_character(inner->type))
                inner = convert_to_kind(inner, TYPE_INT);
            e->type            = clone_type(inner->type);
            e->u.unary_op.expr = inner;
            return e;
        }
        case UNARY_NEG: {
            Expr *inner = typecheck_and_convert(e->u.unary_op.expr);
            if (!is_arithmetic(inner->type)) {
                fatal_error("Can only negate arithmetic types");
            }
            if (is_character(inner->type))
                inner = convert_to_kind(inner, TYPE_INT);
            e->type            = clone_type(inner->type);
            e->u.unary_op.expr = inner;
            return e;
        }
        case UNARY_DEREF: {
            Expr *inner = typecheck_and_convert(e->u.unary_op.expr);
            if (!is_pointer(inner->type)) {
                fatal_error("Tried to dereference non-pointer");
            }
            if (inner->type->u.pointer.target->kind == TYPE_VOID) {
                fatal_error("Can't dereference pointer to void");
            }
            e->type            = clone_type(inner->type->u.pointer.target);
            e->u.unary_op.expr = inner;
            return e;
        }
        case UNARY_ADDRESS: {
            Expr *inner = typecheck_exp(e->u.unary_op.expr);
            if (!is_lvalue(inner)) {
                fatal_error("Cannot take address of non-lvalue");
            }
            Type *ptr             = new_type(TYPE_POINTER);
            ptr->u.pointer.target = clone_type(inner->type);
            e->type               = ptr;
            e->u.unary_op.expr    = inner;
            return e;
        }
        default:
            fatal_error("Unsupported unary op %d", e->u.unary_op.op);
        }
    }
    case EXPR_BINARY_OP: {
        Expr *e1 = e->u.binary_op.left, *e2 = e->u.binary_op.right;
        switch (e->u.binary_op.op) {
        case BINARY_LOG_AND:
        case BINARY_LOG_OR: {
            e1                   = typecheck_scalar(e1);
            e2                   = typecheck_scalar(e2);
            e->type              = new_type(TYPE_INT);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_ADD: {
            e1 = typecheck_and_convert(e1);
            e2 = typecheck_and_convert(e2);
            if (is_arithmetic(e1->type) && is_arithmetic(e2->type)) {
                const Type *common = get_common_type(e1->type, e2->type);
                e1           = convert_to_type(e1, common);
                e2           = convert_to_type(e2, common);
                e->type      = clone_type(common);
            } else if (is_complete_pointer(e1->type) && is_integer(e2->type)) {
                e2      = convert_to_kind(e2, TYPE_LONG);
                e->type = clone_type(e1->type);
            } else if (is_complete_pointer(e2->type) && is_integer(e1->type)) {
                e1      = convert_to_kind(e1, TYPE_LONG);
                e->type = clone_type(e2->type);
            } else {
                fatal_error("Invalid operands for addition");
            }
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_SUB: {
            e1 = typecheck_and_convert(e1);
            e2 = typecheck_and_convert(e2);
            if (is_arithmetic(e1->type) && is_arithmetic(e2->type)) {
                const Type *common = get_common_type(e1->type, e2->type);
                e1           = convert_to_type(e1, common);
                e2           = convert_to_type(e2, common);
                e->type      = clone_type(common);
            } else if (is_complete_pointer(e1->type) && is_integer(e2->type)) {
                e2      = convert_to_kind(e2, TYPE_LONG);
                e->type = clone_type(e1->type);
            } else if (is_complete_pointer(e1->type) && e1->type->kind == e2->type->kind) {
                e->type = new_type(TYPE_LONG);
            } else {
                fatal_error("Invalid operands for subtraction");
            }
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_MUL:
        case BINARY_DIV:
        case BINARY_MOD: {
            e1 = typecheck_and_convert(e1);
            e2 = typecheck_and_convert(e2);
            if (!is_arithmetic(e1->type) || !is_arithmetic(e2->type)) {
                fatal_error("Can only multiply arithmetic types");
            }
            const Type *common = get_common_type(e1->type, e2->type);
            e1           = convert_to_type(e1, common);
            e2           = convert_to_type(e2, common);
            if (e->u.binary_op.op == BINARY_MOD && common->kind == TYPE_DOUBLE) {
                fatal_error("Can't apply %% to double");
            }
            e->type              = clone_type(common);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_EQ:
        case BINARY_NE: {
            e1                   = typecheck_and_convert(e1);
            e2                   = typecheck_and_convert(e2);
            const Type *common   = is_pointer(e1->type) || is_pointer(e2->type)
                                       ? get_common_pointer_type(e1, e2)
                                       : get_common_type(e1->type, e2->type);
            e1                   = convert_to_type(e1, common);
            e2                   = convert_to_type(e2, common);
            e->type              = new_type(TYPE_INT);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_LT:
        case BINARY_GT:
        case BINARY_LE:
        case BINARY_GE: {
            e1           = typecheck_and_convert(e1);
            e2           = typecheck_and_convert(e2);
            const Type *common = is_arithmetic(e1->type) && is_arithmetic(e2->type)
                               ? get_common_type(e1->type, e2->type)
                               : (e1->type->kind == e2->type->kind ? e1->type : NULL);
            if (!common) {
                fatal_error("Invalid types for comparison");
            }
            e1                   = convert_to_type(e1, common);
            e2                   = convert_to_type(e2, common);
            e->type              = new_type(TYPE_INT);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        default:
            fatal_error("Unsupported binary op %d", e->u.binary_op.op);
        }
    }
    case EXPR_ASSIGN: {
        Expr *lhs = typecheck_and_convert(e->u.assign.target);
        if (!is_lvalue(lhs)) {
            fatal_error("Left hand side of assignment is invalid lvalue");
        }
        Expr *rhs          = typecheck_and_convert(e->u.assign.value);
        rhs                = convert_by_assignment(rhs, lhs->type);
        e->type            = clone_type(lhs->type);
        e->u.assign.target = lhs;
        e->u.assign.value  = rhs;
        return e;
    }
    case EXPR_COND: {
        Expr *cond      = typecheck_scalar(e->u.cond.condition);
        Expr *then_expr = typecheck_and_convert(e->u.cond.then_expr);
        Expr *else_expr = typecheck_and_convert(e->u.cond.else_expr);
        const Type *result_type;
        if (then_expr->type->kind == TYPE_VOID && else_expr->type->kind == TYPE_VOID) {
            result_type = new_type(TYPE_VOID);
        } else if (is_pointer(then_expr->type) || is_pointer(else_expr->type)) {
            result_type = get_common_pointer_type(then_expr, else_expr);
        } else if (is_arithmetic(then_expr->type) && is_arithmetic(else_expr->type)) {
            result_type = get_common_type(then_expr->type, else_expr->type);
        } else if (then_expr->type->kind == else_expr->type->kind) {
            result_type = then_expr->type;
        } else {
            fatal_error("Invalid operands for conditional");
        }
        e->type             = clone_type(result_type);
        e->u.cond.condition = cond;
        e->u.cond.then_expr = convert_to_type(then_expr, result_type);
        e->u.cond.else_expr = convert_to_type(else_expr, result_type);
        return e;
    }
    case EXPR_CALL: {
        const Expr *func = e->u.call.func;
        if (func->kind != EXPR_VAR) {
            fatal_error("Function call requires variable name");
        }
        Symbol *sym = symtab_get(func->u.var);
        if (sym->type->kind != TYPE_FUNCTION) {
            fatal_error("Tried to use variable as function name");
        }
        Param *params   = sym->type->u.function.params;
        int param_count = 0, arg_count = 0;
        for (Param *p = params; p; p = p->next)
            param_count++;
        for (Expr *a = e->u.call.args; a; a = a->next)
            arg_count++;
        if (param_count != arg_count) {
            fatal_error("Function called with wrong number of arguments");
        }
        Expr *arg = e->u.call.args, *prev = NULL, *new_args = NULL;
        Param *p = params;
        while (arg && p) {
            Expr *new_arg = convert_by_assignment(typecheck_and_convert(arg), p->type);
            if (!new_args)
                new_args = new_arg;
            if (prev)
                prev->next = new_arg;
            prev = new_arg;
            arg  = arg->next;
            p    = p->next;
        }
        e->type        = clone_type(sym->type->u.function.return_type);
        e->u.call.args = new_args;
        return e;
    }
    case EXPR_SUBSCRIPT: {
        Expr *ptr   = typecheck_and_convert(e->u.subscript.left);
        Expr *index = typecheck_and_convert(e->u.subscript.right);
        const Type *result_type;
        if (is_complete_pointer(ptr->type) && is_integer(index->type)) {
            result_type = ptr->type->u.pointer.target;
            index       = convert_to_kind(index, TYPE_LONG);
        } else if (is_complete_pointer(index->type) && is_integer(ptr->type)) {
            result_type = index->type->u.pointer.target;
            ptr         = convert_to_kind(ptr, TYPE_LONG);
        } else {
            fatal_error("Invalid types for subscript operation");
        }
        e->type              = clone_type(result_type);
        e->u.subscript.left  = ptr;
        e->u.subscript.right = index;
        return e;
    }
    case EXPR_SIZEOF_EXPR: {
        Expr *inner = typecheck_exp(e->u.sizeof_expr);
        if (!is_complete(inner->type)) {
            fatal_error("Can't apply sizeof to incomplete type");
        }
        e->type          = new_type(TYPE_ULONG);
        e->u.sizeof_expr = inner;
        return e;
    }
    case EXPR_SIZEOF_TYPE: {
        validate_type(e->u.sizeof_type);
        if (!is_complete(e->u.sizeof_type)) {
            fatal_error("Can't apply sizeof to incomplete type");
        }
        e->type = new_type(TYPE_ULONG);
        return e;
    }
    case EXPR_FIELD_ACCESS: {
        Expr *strct = typecheck_and_convert(e->u.field_access.expr);
        if (strct->type->kind != TYPE_STRUCT) {
            fatal_error("Dot operator requires structure type");
        }
        const StructDef *entry = typetab_find(strct->type->u.struct_t.name);
        const FieldDef *member = entry->members;
        for (; member; member = member->next) {
            if (strcmp(member->name, e->u.field_access.field) == 0) {
                break;
            }
        }
        if (!member) {
            fatal_error("Struct %s has no member %s", strct->type->u.struct_t.name,
                    e->u.field_access.field);
        }
        e->type                = clone_type(member->type);
        e->u.field_access.expr = strct;
        return e;
    }
    case EXPR_PTR_ACCESS: {
        Expr *strct_ptr = typecheck_and_convert(e->u.ptr_access.expr);
        if (!is_pointer(strct_ptr->type) ||
            strct_ptr->type->u.pointer.target->kind != TYPE_STRUCT) {
            fatal_error("Arrow operator requires pointer to structure");
        }
        const StructDef *entry = typetab_find(strct_ptr->type->u.pointer.target->u.struct_t.name);
        const FieldDef *member = entry->members;
        for (; member; member = member->next) {
            if (strcmp(member->name, e->u.ptr_access.field) == 0) {
                break;
            }
        }
        if (!member) {
            fatal_error("Struct %s has no member %s",
                    strct_ptr->type->u.pointer.target->u.struct_t.name, e->u.ptr_access.field);
        }
        e->type              = clone_type(member->type);
        e->u.ptr_access.expr = strct_ptr;
        return e;
    }
    default:
        fatal_error("Unsupported expression kind %d", e->kind);
    }
}

// Type-check and convert an expression
Expr *typecheck_and_convert(Expr *e)
{
    if (!e)
        return NULL;
    Expr *typed = typecheck_exp(e);
    if (typed->type->kind == TYPE_STRUCT && !is_complete(typed->type)) {
        fatal_error("Incomplete structure type not permitted");
    }
    if (typed->type->kind == TYPE_ARRAY) {
        Type *ptr             = new_type(TYPE_POINTER);
        ptr->u.pointer.target = clone_type(typed->type->u.array.element);
        typed->type           = ptr; // Modify in place
    }
    return typed;
}

// Type-check a scalar expression
Expr *typecheck_scalar(Expr *e)
{
    Expr *typed = typecheck_and_convert(e);
    if (!is_scalar(typed->type)) {
        fatal_error("A scalar operand is required");
    }
    return typed;
}

// Create a zero initializer
Initializer *make_zero_init(Type *t)
{
    if (t->kind == TYPE_ARRAY) {
        Initializer *init = new_initializer(INITIALIZER_COMPOUND);
        init->type        = clone_type(t);

        InitItem **tail = &init->u.items;
        size_t size     = (size_t)t->u.array.size;
        for (size_t i = 0; i < size; i++) {
            InitItem *item    = new_init_item(NULL, make_zero_init(t->u.array.element));
            *tail             = item;
            tail              = &item->next;
        }
        return init;
    }
    if (t->kind == TYPE_STRUCT) {
        Initializer *init = new_initializer(INITIALIZER_COMPOUND);
        init->type        = clone_type(t);

        InitItem **tail   = &init->u.items;
        FieldDef *members = typetab_find(t->u.struct_t.name)->members;
        for (; members; members = members->next) {
            InitItem *item = new_init_item(NULL, make_zero_init(members->type));
            *tail          = item;
            tail           = &item->next;
        }
        return init;
    }

    Initializer *init  = new_initializer(INITIALIZER_SINGLE);
    init->type         = clone_type(t);
    init->u.expr       = new_expression(EXPR_LITERAL);
    init->u.expr->type = clone_type(t);
    switch (t->kind) {
    case TYPE_CHAR:
        init->u.expr->u.literal = new_literal(LITERAL_CHAR);
        break;
    case TYPE_INT:
        init->u.expr->u.literal = new_literal(LITERAL_INT);
        break;
    case TYPE_LONG:
        init->u.expr->u.literal = new_literal(LITERAL_INT); // Simplified
        break;
    case TYPE_DOUBLE:
        init->u.expr->u.literal = new_literal(LITERAL_FLOAT);
        break;
    case TYPE_POINTER:
        init->u.expr->u.literal = new_literal(LITERAL_INT); // Null pointer
        break;
    default:
        fatal_error("Unsupported type for zero init: %d", t->kind);
    }
    return init;
}

// Convert an initializer to a StaticInitializer list
StaticInitializer *static_init_helper(const Type *var_type, const Initializer *init)
{
    if (!init) {
        StaticInitializer *zero = new_static_initializer(INIT_ZERO);
        zero->u.zero_bytes      = get_size(var_type);
        return zero;
    }
    if (var_type->kind == TYPE_ARRAY && init->kind == INITIALIZER_SINGLE &&
        init->u.expr->kind == EXPR_LITERAL && init->u.expr->u.literal->kind == LITERAL_STRING) {
        if (!is_character(var_type->u.array.element)) {
            fatal_error("Can't initialize array of non-character type with string literal");
        }
        const char *s                        = init->u.expr->u.literal->u.string_val;
        size_t len                           = strlen(s);
        size_t array_size                    = (size_t)var_type->u.array.size;
        StaticInitializer *result            = new_static_initializer(INIT_STRING);
        result->u.string_val.str             = xstrdup(s);
        result->u.string_val.null_terminated = (array_size >= len + 1);
        if (array_size > len + 1) {
            StaticInitializer *zero = new_static_initializer(INIT_ZERO);
            zero->u.zero_bytes      = (array_size - (len + 1)) * get_size(var_type->u.array.element);
            result->next            = zero;
        }
        return result;
    }
    if (var_type->kind == TYPE_ARRAY && init->kind == INITIALIZER_SINGLE) {
        fatal_error("Can't initialize array from scalar value");
    }
    if (var_type->kind == TYPE_POINTER && init->kind == INITIALIZER_SINGLE &&
        init->u.expr->kind == EXPR_LITERAL && init->u.expr->u.literal->kind == LITERAL_STRING) {
        if (var_type->u.pointer.target->kind != TYPE_CHAR) {
            fatal_error("String literal can only initialize char *");
        }
        char *str_id              = symtab_add_string(init->u.expr->u.literal->u.string_val);
        StaticInitializer *result = new_static_initializer(INIT_POINTER);
        result->u.ptr_id          = str_id; // owned by static initializer
        return result;
    }
    if (init->kind == INITIALIZER_SINGLE && init->u.expr->kind == EXPR_LITERAL) {
        const Literal *lit = init->u.expr->u.literal;
        if (is_zero_int(lit)) {
            StaticInitializer *zero = new_static_initializer(INIT_ZERO);
            zero->u.zero_bytes      = get_size(var_type);
            return zero;
        }
        if (!is_arithmetic(var_type)) {
            fatal_error("Invalid static initializer for type %d", var_type->kind);
        }
        return new_static_initializer_from_literal(var_type, lit);
    }
    if (var_type->kind == TYPE_STRUCT && init->kind == INITIALIZER_COMPOUND) {
        StructDef *entry = typetab_find(var_type->u.struct_t.name);
        FieldDef *memb   = entry->members;
        StaticInitializer *result = NULL, **tail = &result;
        int current_offset = 0;
        for (InitItem *item = init->u.items; item; item = item->next, memb = memb->next) {
            if (!memb) {
                fatal_error("Too many elements in struct initializer");
            }
            if (current_offset < memb->offset) {
                StaticInitializer *zero = new_static_initializer(INIT_ZERO);
                zero->u.zero_bytes      = memb->offset - current_offset;
                *tail                   = zero;
                tail                    = &zero->next;
            }
            StaticInitializer *member_init = static_init_helper(memb->type, item->init);
            *tail                          = member_init;
            while (*tail)
                tail = &(*tail)->next;
            current_offset = memb->offset + get_size(memb->type);
        }
        if (current_offset < entry->size) {
            StaticInitializer *zero = new_static_initializer(INIT_ZERO);
            zero->u.zero_bytes      = entry->size - current_offset;
            *tail                   = zero;
        }
        return result;
    }
    if (var_type->kind == TYPE_ARRAY && init->kind == INITIALIZER_COMPOUND) {
        size_t array_size = (size_t)var_type->u.array.size;
        int init_count    = 0;
        for (InitItem *item = init->u.items; item; item = item->next)
            init_count++;
        if (init_count > (int)array_size) {
            fatal_error("Too many values in static initializer");
        }
        StaticInitializer *result = NULL, **tail = &result;
        for (int i = 0; i < init_count; i++) {
            StaticInitializer *elem_init =
                static_init_helper(var_type->u.array.element, init->u.items[i].init);
            *tail = elem_init;
            while (*tail)
                tail = &(*tail)->next;
        }
        if (init_count < (int)array_size) {
            StaticInitializer *zero = new_static_initializer(INIT_ZERO);
            zero->u.zero_bytes =
                ((int)array_size - init_count) * get_size(var_type->u.array.element);
            *tail      = zero;
        }
        return result;
    }
    fatal_error("Invalid static initializer for type %d", var_type->kind);
}

// Convert initializer to static initializer
StaticInitializer *to_static_init(const Type *var_type, const Initializer *init)
{
    return static_init_helper(var_type, init);
}

// Type-check an initializer
Initializer *typecheck_init(const Type *target_type, Initializer *init)
{
    if (!init)
        return NULL;
    init->type = clone_type(target_type);
    if (target_type->kind == TYPE_ARRAY && init->kind == INITIALIZER_SINGLE &&
        init->u.expr->kind == EXPR_LITERAL && init->u.expr->u.literal->kind == LITERAL_STRING) {
        if (!is_character(target_type->u.array.element)) {
            fatal_error("Can't initialize non-character type with string literal");
        }
        size_t len = strlen(init->u.expr->u.literal->u.string_val);
        if (len > (size_t)target_type->u.array.size) {
            fatal_error("Too many characters in string literal");
        }
        init->u.expr = typecheck_string(init->u.expr);
        return init;
    }
    if (target_type->kind == TYPE_STRUCT && init->kind == INITIALIZER_COMPOUND) {
        FieldDef *members = typetab_find(target_type->u.struct_t.name)->members;
        InitItem *items = NULL, **tail = &items;

        for (InitItem *old = init->u.items; old; old = old->next, members = members->next) {
            if (!members) {
                fatal_error("Too many elements in structure initializer");
            }
            InitItem *new_item = new_init_item(NULL, typecheck_init(members->type, old->init));
            *tail              = new_item;
            tail               = &new_item->next;
        }
        for (; members; members = members->next) {
            InitItem *new_item = new_init_item(NULL, make_zero_init(members->type));
            *tail              = new_item;
            tail               = &new_item->next;
        }
        free_init_item(init->u.items);
        init->u.items = items;
        return init;
    }
    if (init->kind == INITIALIZER_SINGLE) {
        Expr *expr   = typecheck_and_convert(init->u.expr);
        expr         = convert_by_assignment(expr, target_type);
        init->u.expr = expr;
        return init;
    }
    if (target_type->kind == TYPE_ARRAY && init->kind == INITIALIZER_COMPOUND) {
        size_t array_size = (size_t)target_type->u.array.size;
        int init_count    = 0;
        for (InitItem *item = init->u.items; item; item = item->next)
            init_count++;
        if (init_count > (int)array_size) {
            fatal_error("Too many values in initializer");
        }
        InitItem *items = NULL, **tail = &items;
        for (int i = 0; i < init_count; i++) {
            InitItem *new_item = new_init_item(NULL, typecheck_init(target_type->u.array.element, init->u.items[i].init));
            *tail              = new_item;
            tail               = &new_item->next;
        }
        for (int i = init_count; i < (int)array_size; i++) {
            InitItem *new_item = new_init_item(NULL, make_zero_init(target_type->u.array.element));
            *tail              = new_item;
            tail               = &new_item->next;
        }
        init->u.items = items;
        return init;
    }
    fatal_error("Can't initialize scalar value from compound initializer");
}

// Type-check a block
DeclOrStmt *typecheck_block(const Type *ret_type, DeclOrStmt *block)
{
    for (DeclOrStmt *item = block; item; item = item->next) {
        if (item->kind == DECL_OR_STMT_STMT) {
            item->u.stmt = typecheck_statement(ret_type, item->u.stmt);
        } else {
            typecheck_local_decl(item->u.decl);
        }
    }
    return block;
}

// Type-check a statement
Stmt *typecheck_statement(const Type *ret_type, Stmt *s)
{
    if (!s)
        return NULL;
    switch (s->kind) {
    case STMT_RETURN:
        if (s->u.expr) {
            if (ret_type->kind == TYPE_VOID) {
                fatal_error("Void function cannot return a value");
            }
            s->u.expr = convert_by_assignment(typecheck_and_convert(s->u.expr), ret_type);
        } else if (ret_type->kind != TYPE_VOID) {
            return s;
        }
    case STMT_EXPR: {
        s->u.expr = typecheck_and_convert(s->u.expr);
        return s;
    }
    case STMT_IF: {
        s->u.if_stmt.condition = typecheck_scalar(s->u.if_stmt.condition);
        s->u.if_stmt.then_stmt = typecheck_statement(ret_type, s->u.if_stmt.then_stmt);
        if (s->u.if_stmt.else_stmt) {
            s->u.if_stmt.else_stmt = typecheck_statement(ret_type, s->u.if_stmt.else_stmt);
        }
        return s;
    }
    case STMT_COMPOUND: {
        s->u.compound = typecheck_block(ret_type, s->u.compound);
        return s;
    }
    case STMT_WHILE: {
        s->u.while_stmt.condition = typecheck_scalar(s->u.while_stmt.condition);
        s->u.while_stmt.body      = typecheck_statement(ret_type, s->u.while_stmt.body);
        return s;
    }
    case STMT_DO_WHILE: {
        s->u.do_while.body      = typecheck_statement(ret_type, s->u.do_while.body);
        s->u.do_while.condition = typecheck_scalar(s->u.do_while.condition);
        return s;
    }
    case STMT_FOR: {
        if (s->u.for_stmt.init->kind == FOR_INIT_DECL) {
            if (s->u.for_stmt.init->u.decl->u.var.specifiers &&
                s->u.for_stmt.init->u.decl->u.var.specifiers->storage != STORAGE_CLASS_NONE) {
                fatal_error("Storage class not permitted in for loop header");
            }
            typecheck_local_decl(s->u.for_stmt.init->u.decl);
        } else {
            s->u.for_stmt.init->u.expr = s->u.for_stmt.init->u.expr
                                             ? typecheck_and_convert(s->u.for_stmt.init->u.expr)
                                             : NULL;
        }
        s->u.for_stmt.condition =
            s->u.for_stmt.condition ? typecheck_scalar(s->u.for_stmt.condition) : NULL;
        s->u.for_stmt.update =
            s->u.for_stmt.update ? typecheck_and_convert(s->u.for_stmt.update) : NULL;
        s->u.for_stmt.body = typecheck_statement(ret_type, s->u.for_stmt.body);
        return s;
    }
    case STMT_BREAK:
    case STMT_CONTINUE:
        return s;
    default:
        fatal_error("Unsupported statement kind %d", s->kind);
    }
}

// Type-check a local variable declaration
void typecheck_local_var_decl(Declaration *d)
{
    InitDeclarator *decl = d->u.var.declarators;
    const Type *var_type = decl->type;
    if (var_type->kind == TYPE_VOID) {
        fatal_error("No void declarations");
    }
    validate_type(var_type);
    if (d->u.var.specifiers && d->u.var.specifiers->storage == STORAGE_CLASS_EXTERN) {
        if (decl->init) {
            fatal_error("Initializer on local extern declaration");
        }
        const Symbol *existing = symtab_get_opt(decl->name);
        if (existing && existing->type->kind != var_type->kind) {
            fatal_error("Variable %s redeclared with different type", decl->name);
        }
        if (!existing) {
            symtab_add_static_var(decl->name, var_type, true, INIT_NONE, NULL);
        }
        return;
    }
    if (!is_complete(var_type)) {
        fatal_error("Cannot define a variable with incomplete type");
    }
    if (d->u.var.specifiers && d->u.var.specifiers->storage == STORAGE_CLASS_STATIC) {
        StaticInitializer *static_init =
            decl->init ? to_static_init(var_type, decl->init) : static_init_helper(var_type, NULL);
        symtab_add_static_var(decl->name, var_type, false, INIT_INITIALIZED,
                              static_init);
        decl->init = NULL; // Drop initializer
        return;
    }
    symtab_add_automatic_var(decl->name, var_type);
    decl->init = typecheck_init(var_type, decl->init);
}

// Type-check a function declaration
void typecheck_fn_decl(ExternalDecl *d)
{
    const Type *fun_type = d->u.function.type;
    validate_type(fun_type);
    Type *adjusted_type = clone_type(fun_type);
    if (fun_type->kind == TYPE_FUNCTION) {
        if (fun_type->u.function.return_type->kind == TYPE_ARRAY) {
            fatal_error("A function cannot return an array");
        }
        Param *p = adjusted_type->u.function.params;
        while (p) {
            if (p->type->kind == TYPE_ARRAY) {
                Type *ptr             = new_type(TYPE_POINTER);
                ptr->u.pointer.target = clone_type(p->type->u.array.element);
                p->type               = ptr;
            } else if (p->type->kind == TYPE_VOID) {
                fatal_error("No void params allowed");
            }
            p = p->next;
        }
    } else {
        fatal_error("Function has non-function type");
    }
    bool has_body            = d->u.function.body != NULL;
    Param *params            = adjusted_type->u.function.params;
    bool all_params_complete = true;
    for (Param *p = params; p; p = p->next) {
        if (!is_complete(p->type)) {
            all_params_complete = false;
            break;
        }
    }
    if (has_body && (!is_complete(fun_type->u.function.return_type) || !all_params_complete)) {
        fatal_error("Can't define function with incomplete types");
    }
    bool global      = d->u.function.specifiers->storage != STORAGE_CLASS_STATIC;
    Symbol *existing = symtab_get_opt(d->u.function.name);
    bool defined     = has_body;
    if (existing) {
        if (existing->type->kind != fun_type->kind) {
            fatal_error("Redeclared function %s with different type", d->u.function.name);
        }
        if (existing->kind == SYM_FUNC) {
            if (existing->u.func.defined && has_body) {
                fatal_error("Defined function %s twice", d->u.function.name);
            }
            if (existing->u.func.global &&
                d->u.function.specifiers->storage == STORAGE_CLASS_STATIC) {
                fatal_error("Static function declaration follows non-static");
            }
            defined = has_body || existing->u.func.defined;
            global  = existing->u.func.global;
        }
    }
    symtab_add_fun(d->u.function.name, adjusted_type, global, defined);
    if (has_body) {
        Param *p = params;
        for (Declaration *param = d->u.function.param_decls; param && p;
             param = param->next, p = p->next) {
            symtab_add_automatic_var(param->u.var.declarators->name, p->type);
        }
        d->u.function.body =
            typecheck_statement(fun_type->u.function.return_type, d->u.function.body);
    }
    d->u.function.type = adjusted_type; // Update type in place
    //TODO: deallocate fun_type?
}

// Type-check a local declaration
void typecheck_local_decl(Declaration *d)
{
    switch (d->kind) {
    case DECL_VAR:
        typecheck_local_var_decl(d);
        break;
    case DECL_EMPTY:
        typecheck_struct_decl(d);
        break;
    default:
        fatal_error("Unsupported local declaration kind %d", d->kind);
    }
}

// Type-check a global variable declaration
void typecheck_file_scope_var_decl(Declaration *d)
{
    InitDeclarator *decl = d->u.var.declarators;
    const Type *var_type = decl->type;
    if (var_type->kind == TYPE_VOID) {
        fatal_error("Void variables not allowed");
    }
    validate_type(var_type);

    StorageClass storage = d->u.var.specifiers ? d->u.var.specifiers->storage : STORAGE_CLASS_NONE;
    bool global = storage != STORAGE_CLASS_STATIC;
    InitKind init_kind = (storage == STORAGE_CLASS_EXTERN) ? INIT_NONE : INIT_TENTATIVE;
    StaticInitializer *init_list = NULL;
    if (decl->init) {
        init_kind = INIT_INITIALIZED;
        init_list = to_static_init(var_type, decl->init);
    }
    if (!is_complete(var_type) && init_kind != INIT_NONE) {
        fatal_error("Can't define a variable with incomplete type");
    }
    Symbol *existing = symtab_get_opt(decl->name);
    if (existing) {
        if (existing->type->kind != var_type->kind) {
            fatal_error("Variable %s redeclared with different type", decl->name);
        }
        if (existing->kind == SYM_STATIC) {
            if (storage != STORAGE_CLASS_EXTERN && existing->u.static_var.global != global) {
                fatal_error("Conflicting variable linkage");
            }
            if (existing->u.static_var.init_kind == INIT_INITIALIZED &&
                init_kind == INIT_INITIALIZED) {
                fatal_error("Conflicting global variable definition");
            }
            init_kind = existing->u.static_var.init_kind == INIT_INITIALIZED
                            ? existing->u.static_var.init_kind
                            : init_kind;
            init_list = existing->u.static_var.init_kind == INIT_INITIALIZED
                            ? existing->u.static_var.init_list
                            : init_list;
            global    = storage == STORAGE_CLASS_EXTERN
                            ? existing->u.static_var.global
                            : global;
        }
    }
    symtab_add_static_var(decl->name, var_type, global, init_kind, init_list);
    decl->init = NULL; // Drop initializer
}

// Type-check a global declaration
void typecheck(ExternalDecl *d)
{
    if (d->kind == EXTERNAL_DECL_FUNCTION) {
        typecheck_fn_decl(d);
    } else {
        typecheck_file_scope_var_decl(d->u.declaration);
    }
}
