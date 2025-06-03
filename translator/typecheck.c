#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symtab.h"
#include "typetab.h"

// Assume utility functions from Type_utils and Const_convert
bool is_complete(Type *t);
int get_size(Type *t);
int get_alignment(Type *t);
bool is_scalar(Type *t);
bool is_arithmetic(Type *t);
bool is_integer(Type *t);
bool is_character(Type *t);
bool is_pointer(Type *t);
bool is_complete_pointer(Type *t);
bool is_signed(Type *t);
Type *const_convert(Type *target, Literal *c);
Type *new_char_type(void);  // Helper for string literals
Type *new_long_type(void);  // Helper for TYPE_LONG
Type *new_ulong_type(void); // Helper for TYPE_LONG_LONG (unsigned)

// Assume Rounding module function
int round_away_from_zero(int alignment, int size);

// Forward declarations
void validate_type(Type *t);
Expr *typecheck_and_convert(Expr *e);
Expr *typecheck_scalar(Expr *e);
Initializer *typecheck_init(Type *target_type, Initializer *init);
StaticInitializer *static_init_helper(Type *var_type, Initializer *init);

// Check if an expression is an lvalue
bool is_lvalue(Expr *e)
{
    switch (e->kind) {
    case EXPR_DEREF:
    case EXPR_SUBSCRIPT:
    case EXPR_VAR:
    case EXPR_FIELD_ACCESS:
    case EXPR_PTR_ACCESS:
        return true;
    case EXPR_BINARY_OP:
        if (e->u.binary_op.op == BINARY_LOG_AND || e->u.binary_op.op == BINARY_LOG_OR) {
            return false;
        }
        return is_lvalue(e->u.binary_op.left);
    default:
        return false;
    }
}

// Validate a type (recursive)
void validate_type(Type *t)
{
    if (!t)
        return;
    switch (t->kind) {
    case TYPE_ARRAY:
        if (!is_complete(t->u.array.element)) {
            fprintf(stderr, "Array of incomplete type\n");
            exit(1);
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
        fprintf(stderr, "Unsupported type kind %d\n", t->kind);
        exit(1);
    }
}

// Validate a struct definition
void validate_struct_definition(char *tag, Field *members)
{
    if (typetab_mem(tag)) {
        fprintf(stderr, "Structure %s was already declared\n", tag);
        exit(1);
    }
    // Check for duplicate member names
    struct NameSet {
        char *name;
        struct NameSet *next;
    } *names = NULL;
    for (Field *m = members; m; m = m->next) {
        for (struct NameSet *n = names; n; n = n->next) {
            if (strcmp(n->name, m->name) == 0) {
                fprintf(stderr, "Duplicate member %s in structure %s\n", m->name, tag);
                exit(1);
            }
        }
        struct NameSet *new_name = malloc(sizeof(struct NameSet));
        new_name->name           = m->name;
        new_name->next           = names;
        names                    = new_name;
        validate_type(m->type);
        if (m->type->kind == TYPE_FUNCTION) {
            fprintf(stderr, "Can't declare structure member with function type\n");
            exit(1);
        }
        if (!is_complete(m->type)) {
            fprintf(stderr, "Cannot declare structure member with incomplete type\n");
            exit(1);
        }
    }
    // Free name set
    while (names) {
        struct NameSet *next = names->next;
        free(names);
        names = next;
    }
}

// Type-check a struct declaration
void typecheck_struct_decl(Declaration *d)
{
    if (!d->u.var.declarators)
        return; // Ignore forward declarations
    validate_struct_definition(d->u.var.specifiers->type->u.struct_t.name,
                               d->u.var.specifiers->type->u.struct_t.fields);
    // Build member definitions
    int current_size = 0, current_alignment = 1, member_count = 0;
    for (Field *f = d->u.var.specifiers->type->u.struct_t.fields; f; f = f->next)
        member_count++;
    TypeMember *members = malloc(member_count * sizeof(TypeMember));
    int i               = 0;
    for (Field *f = d->u.var.specifiers->type->u.struct_t.fields; f; f = f->next) {
        int member_alignment = get_alignment(f->type);
        int offset           = round_away_from_zero(member_alignment, current_size);
        members[i].name      = strdup(f->name);
        members[i].type      = f->type;
        members[i].offset    = offset;
        current_alignment =
            current_alignment > member_alignment ? current_alignment : member_alignment;
        current_size = offset + get_size(f->type);
        i++;
    }
    int size = round_away_from_zero(current_alignment, current_size);
    typetab_add_struct_definition(d->u.var.specifiers->type->u.struct_t.name, current_alignment,
                                  size, members, member_count);
    free(members); // typetab owns the copy
}

// Convert an expression to a target type
Expr *convert_to(Expr *e, Type *target_type)
{
    if (e->type->kind == target_type->kind &&
        (!is_pointer(e->type) ||
         e->type->u.pointer.target->kind == target_type->u.pointer.target->kind))
        return e; // Avoid unnecessary casts

    Expr *cast = new_expression(EXPR_CAST);
    cast.u.cast = { target_type, e };
    cast.type = clone_type(target_type);
    return cast;
}

// Get common type for arithmetic operations
Type *get_common_type(Type *t1, Type *t2)
{
    Type *int_type    = new_type(TYPE_INT);
    Type *double_type = new_type(TYPE_DOUBLE);
    if (is_character(t1))
        t1 = int_type;
    if (is_character(t2))
        t2 = int_type;
    if (t1->kind == t2->kind)
        return t1;
    if (t1->kind == TYPE_DOUBLE || t2->kind == TYPE_DOUBLE)
        return double_type;
    if (get_size(t1) == get_size(t2))
        return is_signed(t1) ? t2 : t1;
    return get_size(t1) > get_size(t2) ? t1 : t2;
}

// Check if a constant is a zero integer
bool is_zero_int(Literal *c)
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
bool is_null_pointer_constant(Expr *e)
{
    return e->kind == EXPR_LITERAL && is_zero_int(e->u.literal);
}

// Get common pointer type
Type *get_common_pointer_type(Expr *e1, Expr *e2)
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
    fprintf(stderr, "Incompatible pointer types\n");
    exit(1);
}

// Convert by assignment
Expr *convert_by_assignment(Expr *e, Type *target_type)
{
    if (e->type->kind == target_type->kind &&
        (!is_pointer(e->type) ||
         e->type->u.pointer.target->kind == target_type->u.pointer.target->kind))
        return e;
    if (is_arithmetic(e->type) && is_arithmetic(target_type))
        return convert_to(e, target_type);
    if (is_null_pointer_constant(e) && is_pointer(target_type))
        return convert_to(e, target_type);
    if ((target_type->kind == TYPE_POINTER && target_type->u.pointer.target->kind == TYPE_VOID &&
         is_pointer(e->type)) ||
        (is_pointer(target_type) && e->type->kind == TYPE_POINTER &&
         e->type->u.pointer.target->kind == TYPE_VOID)) {
        return convert_to(e, target_type);
    }
    fprintf(stderr, "Cannot convert type for assignment\n");
    exit(1);
}

// Type-check a variable
Expr *typecheck_var(Expr *e)
{
    Symbol *sym = symtab_get(e->u.var);
    if (sym->type->kind == TYPE_FUNCTION) {
        fprintf(stderr, "Tried to use function name as variable\n");
        exit(1);
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
        e->type = new_char_type();
        break;
    case LITERAL_FLOAT:
        e->type = new_type(TYPE_DOUBLE);
        break;
    case LITERAL_STRING: {
        Type *array            = new_type(TYPE_ARRAY);
        array->u.array.element = new_char_type();
        array->u.array.size    = (Expr *)(size_t)(strlen(e->u.literal->u.string_val) + 1);
        e->type                = array;
        break;
    }
    case LITERAL_ENUM:
        e->type = new_type(TYPE_INT);
        break;
    default:
        fprintf(stderr, "Unsupported literal kind %d\n", e->u.literal->kind);
        exit(1);
    }
    return e;
}

// Type-check a string literal
Expr *typecheck_string(Expr *e)
{
    Type *array            = new_type(TYPE_ARRAY);
    array->u.array.element = new_char_type();
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
            fprintf(stderr, "Cannot cast between pointer and double\n");
            exit(1);
        }
        if (e->u.cast.type->kind == TYPE_VOID) {
            e->type        = clone_type(e->u.cast.type);
            e->u.cast.expr = inner;
            return e;
        }
        if (!is_scalar(e->u.cast.type) || !is_scalar(inner->type)) {
            fprintf(stderr, "Can only cast scalar types\n");
            exit(1);
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
                fprintf(stderr, "Bitwise complement only valid for integer types\n");
                exit(1);
            }
            if (is_character(inner->type))
                inner = convert_to(inner, new_type(TYPE_INT));
            e->type            = clone_type(inner->type);
            e->u.unary_op.expr = inner;
            return e;
        }
        case UNARY_NEG: {
            Expr *inner = typecheck_and_convert(e->u.unary_op.expr);
            if (!is_arithmetic(inner->type)) {
                fprintf(stderr, "Can only negate arithmetic types\n");
                exit(1);
            }
            if (is_character(inner->type))
                inner = convert_to(inner, new_type(TYPE_INT));
            e->type            = clone_type(inner->type);
            e->u.unary_op.expr = inner;
            return e;
        }
        case UNARY_DEREF: {
            Expr *inner = typecheck_and_convert(e->u.unary_op.expr);
            if (!is_pointer(inner->type)) {
                fprintf(stderr, "Tried to dereference non-pointer\n");
                exit(1);
            }
            if (inner->type->u.pointer.target->kind == TYPE_VOID) {
                fprintf(stderr, "Can't dereference pointer to void\n");
                exit(1);
            }
            e->type            = clone_type(inner->type->u.pointer.target);
            e->u.unary_op.expr = inner;
            return e;
        }
        case UNARY_ADDRESS: {
            Expr *inner = typecheck_exp(e->u.unary_op.expr);
            if (!is_lvalue(inner)) {
                fprintf(stderr, "Cannot take address of non-lvalue\n");
                exit(1);
            }
            Type *ptr             = new_type(TYPE_POINTER);
            ptr->u.pointer.target = clone_type(inner->type);
            e->type               = ptr;
            e->u.unary_op.expr    = inner;
            return e;
        }
        default:
            fprintf(stderr, "Unsupported unary op %d\n", e->u.unary_op.op);
            exit(1);
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
                Type *common = get_common_type(e1->type, e2->type);
                e1           = convert_to(e1, common);
                e2           = convert_to(e2, common);
                e->type      = clone_type(common);
            } else if (is_complete_pointer(e1->type) && is_integer(e2->type)) {
                e2      = convert_to(e2, new_long_type());
                e->type = clone_type(e1->type);
            } else if (is_complete_pointer(e2->type) && is_integer(e1->type)) {
                e1      = convert_to(e1, new_long_type());
                e->type = clone_type(e2->type);
            } else {
                fprintf(stderr, "Invalid operands for addition\n");
                exit(1);
            }
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_SUB: {
            e1 = typecheck_and_convert(e1);
            e2 = typecheck_and_convert(e2);
            if (is_arithmetic(e1->type) && is_arithmetic(e2->type)) {
                Type *common = get_common_type(e1->type, e2->type);
                e1           = convert_to(e1, common);
                e2           = convert_to(e2, common);
                e->type      = clone_type(common);
            } else if (is_complete_pointer(e1->type) && is_integer(e2->type)) {
                e2      = convert_to(e2, new_long_type());
                e->type = clone_type(e1->type);
            } else if (is_complete_pointer(e1->type) && e1->type->kind == e2->type->kind) {
                e->type = new_long_type();
            } else {
                fprintf(stderr, "Invalid operands for subtraction\n");
                exit(1);
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
                fprintf(stderr, "Can only multiply arithmetic types\n");
                exit(1);
            }
            Type *common = get_common_type(e1->type, e2->type);
            e1           = convert_to(e1, common);
            e2           = convert_to(e2, common);
            if (e->u.binary_op.op == BINARY_MOD && common->kind == TYPE_DOUBLE) {
                fprintf(stderr, "Can't apply %% to double\n");
                exit(1);
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
            Type *common         = is_pointer(e1->type) || is_pointer(e2->type)
                                       ? get_common_pointer_type(e1, e2)
                                       : get_common_type(e1->type, e2->type);
            e1                   = convert_to(e1, common);
            e2                   = convert_to(e2, common);
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
            Type *common = is_arithmetic(e1->type) && is_arithmetic(e2->type)
                               ? get_common_type(e1->type, e2->type)
                               : (e1->type->kind == e2->type->kind ? e1->type : NULL);
            if (!common) {
                fprintf(stderr, "Invalid types for comparison\n");
                exit(1);
            }
            e1                   = convert_to(e1, common);
            e2                   = convert_to(e2, common);
            e->type              = new_type(TYPE_INT);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        default:
            fprintf(stderr, "Unsupported binary op %d\n", e->u.binary_op.op);
            exit(1);
        }
    }
    case EXPR_ASSIGN: {
        Expr *lhs = typecheck_and_convert(e->u.assign.target);
        if (!is_lvalue(lhs)) {
            fprintf(stderr, "Left hand side of assignment is invalid lvalue\n");
            exit(1);
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
        Type *result_type;
        if (then_expr->type->kind == TYPE_VOID && else_expr->type->kind == TYPE_VOID) {
            result_type = new_type(TYPE_VOID);
        } else if (is_pointer(then_expr->type) || is_pointer(else_expr->type)) {
            result_type = get_common_pointer_type(then_expr, else_expr);
        } else if (is_arithmetic(then_expr->type) && is_arithmetic(else_expr->type)) {
            result_type = get_common_type(then_expr->type, else_expr->type);
        } else if (then_expr->type->kind == else_expr->type->kind) {
            result_type = then_expr->type;
        } else {
            fprintf(stderr, "Invalid operands for conditional\n");
            exit(1);
        }
        e->type             = clone_type(result_type);
        e->u.cond.condition = cond;
        e->u.cond.then_expr = convert_to(then_expr, result_type);
        e->u.cond.else_expr = convert_to(else_expr, result_type);
        return e;
    }
    case EXPR_CALL: {
        Expr *func = e->u.call.func;
        if (func->kind != EXPR_VAR) {
            fprintf(stderr, "Function call requires variable name\n");
            exit(1);
        }
        Symbol *sym = symtab_get(func->u.var);
        if (sym->type->kind != TYPE_FUNCTION) {
            fprintf(stderr, "Tried to use variable as function name\n");
            exit(1);
        }
        Param *params   = sym->type->u.function.params;
        int param_count = 0, arg_count = 0;
        for (Param *p = params; p; p = p->next)
            param_count++;
        for (Expr *a = e->u.call.args; a; a = a->next)
            arg_count++;
        if (param_count != arg_count) {
            fprintf(stderr, "Function called with wrong number of arguments\n");
            exit(1);
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
        Expr *ptr   = typecheck_and_convert(e->u.binary_op.left);
        Expr *index = typecheck_and_convert(e->u.binary_op.right);
        Type *result_type;
        if (is_complete_pointer(ptr->type) && is_integer(index->type)) {
            result_type = ptr->type->u.pointer.target;
            index       = convert_to(index, new_long_type());
        } else if (is_complete_pointer(index->type) && is_integer(ptr->type)) {
            result_type = index->type->u.pointer.target;
            ptr         = convert_to(ptr, new_long_type());
        } else {
            fprintf(stderr, "Invalid types for subscript operation\n");
            exit(1);
        }
        e->type              = clone_type(result_type);
        e->u.binary_op.left  = ptr;
        e->u.binary_op.right = index;
        return e;
    }
    case EXPR_SIZEOF_EXPR: {
        Expr *inner = typecheck_exp(e->u.sizeof_expr);
        if (!is_complete(inner->type)) {
            fprintf(stderr, "Can't apply sizeof to incomplete type\n");
            exit(1);
        }
        e->type          = new_ulong_type();
        e->u.sizeof_expr = inner;
        return e;
    }
    case EXPR_SIZEOF_TYPE: {
        validate_type(e->u.sizeof_type);
        if (!is_complete(e->u.sizeof_type)) {
            fprintf(stderr, "Can't apply sizeof to incomplete type\n");
            exit(1);
        }
        e->type = new_ulong_type();
        return e;
    }
    case EXPR_FIELD_ACCESS: {
        Expr *strct = typecheck_and_convert(e->u.field_access.expr);
        if (strct->type->kind != TYPE_STRUCT) {
            fprintf(stderr, "Dot operator requires structure type\n");
            exit(1);
        }
        TypeEntry *entry   = typetab_find(strct->type->u.struct_t.name);
        TypeMember *member = NULL;
        for (int i = 0; i < entry->member_count; i++) {
            if (strcmp(entry->members[i].name, e->u.field_access.field) == 0) {
                member = &entry->members[i];
                break;
            }
        }
        if (!member) {
            fprintf(stderr, "Struct %s has no member %s\n", strct->type->u.struct_t.name,
                    e->u.field_access.field);
            exit(1);
        }
        e->type                = clone_type(member->type);
        e->u.field_access.expr = strct;
        return e;
    }
    case EXPR_PTR_ACCESS: {
        Expr *strct_ptr = typecheck_and_convert(e->u.ptr_access.expr);
        if (!is_pointer(strct_ptr->type) ||
            strct_ptr->type->u.pointer.target->kind != TYPE_STRUCT) {
            fprintf(stderr, "Arrow operator requires pointer to structure\n");
            exit(1);
        }
        TypeEntry *entry   = typetab_find(strct_ptr->type->u.pointer.target->u.struct_t.name);
        TypeMember *member = NULL;
        for (int i = 0; i < entry->member_count; i++) {
            if (strcmp(entry->members[i].name, e->u.ptr_access.field) == 0) {
                member = &entry->members[i];
                break;
            }
        }
        if (!member) {
            fprintf(stderr, "Struct %s has no member %s\n",
                    strct_ptr->type->u.pointer.target->u.struct_t.name, e->u.ptr_access.field);
            exit(1);
        }
        e->type              = clone_type(member->type);
        e->u.ptr_access.expr = strct_ptr;
        return e;
    }
    default:
        fprintf(stderr, "Unsupported expression kind %d\n", e->kind);
        exit(1);
    }
}

// Type-check and convert an expression
Expr *typecheck_and_convert(Expr *e)
{
    if (!e)
        return NULL;
    Expr *typed = typecheck_exp(e);
    if (typed->type->kind == TYPE_STRUCT && !is_complete(typed->type)) {
        fprintf(stderr, "Incomplete structure type not permitted\n");
        exit(1);
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
        fprintf(stderr, "A scalar operand is required\n");
        exit(1);
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

        InitItem **tail = &init->u.items;
        int count;
        TypeMember *members = typetab_get_members(t->u.struct_t.name, &count);
        for (int i = 0; i < count; i++) {
            InitItem *item    = new_init_item(NULL, make_zero_init(members[i].type));
            *tail             = item;
            tail              = &item->next;
        }
        free(members);
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
        fprintf(stderr, "Unsupported type for zero init: %d\n", t->kind);
        exit(1);
    }
    return init;
}

// Convert an initializer to a StaticInitializer list
StaticInitializer *static_init_helper(Type *var_type, Initializer *init)
{
    if (!init) {
        StaticInitializer *zero = malloc(sizeof(StaticInitializer));
        zero->kind              = INIT_ZERO;
        zero->u.zero_bytes      = get_size(var_type);
        zero->next              = NULL;
        return zero;
    }
    if (var_type->kind == TYPE_ARRAY && init->kind == INITIALIZER_SINGLE &&
        init->u.expr->kind == EXPR_LITERAL && init->u.expr->u.literal->kind == LITERAL_STRING) {
        if (!is_character(var_type->u.array.element)) {
            fprintf(stderr, "Can't initialize array of non-character type with string literal\n");
            exit(1);
        }
        char *s                              = init->u.expr->u.literal->u.string_val;
        size_t len                           = strlen(s);
        size_t array_size                    = (size_t)var_type->u.array.size;
        StaticInitializer *result            = malloc(sizeof(StaticInitializer));
        result->kind                         = INIT_STRING;
        result->u.string_val.str             = strdup(s);
        result->u.string_val.null_terminated = (array_size >= len + 1);
        result->next                         = NULL;
        if (array_size > len + 1) {
            StaticInitializer *zero = malloc(sizeof(StaticInitializer));
            zero->kind              = INIT_ZERO;
            zero->u.zero_bytes = (array_size - (len + 1)) * get_size(var_type->u.array.element);
            zero->next         = NULL;
            result->next       = zero;
        }
        return result;
    }
    if (var_type->kind == TYPE_ARRAY && init->kind == INITIALIZER_SINGLE) {
        fprintf(stderr, "Can't initialize array from scalar value\n");
        exit(1);
    }
    if (var_type->kind == TYPE_POINTER && init->kind == INITIALIZER_SINGLE &&
        init->u.expr->kind == EXPR_LITERAL && init->u.expr->u.literal->kind == LITERAL_STRING) {
        if (var_type->u.pointer.target->kind != TYPE_CHAR) {
            fprintf(stderr, "String literal can only initialize char *\n");
            exit(1);
        }
        char *s                   = init->u.expr->u.literal->u.string_val;
        char *str_id              = symtab_add_string(s);
        StaticInitializer *result = malloc(sizeof(StaticInitializer));
        result->kind              = INIT_POINTER;
        result->u.ptr_id          = strdup(str_id);
        result->next              = NULL;
        return result;
    }
    if (init->kind == INITIALIZER_SINGLE && init->u.expr->kind == EXPR_LITERAL) {
        Literal *c = init->u.expr->u.literal;
        if (is_zero_int(c)) {
            StaticInitializer *zero = malloc(sizeof(StaticInitializer));
            zero->kind              = INIT_ZERO;
            zero->u.zero_bytes      = get_size(var_type);
            zero->next              = NULL;
            return zero;
        }
        if (is_arithmetic(var_type)) {
            Type *converted_type      = const_convert(var_type, c);
            StaticInitializer *result = malloc(sizeof(StaticInitializer));
            result->next              = NULL;
            switch (converted_type->kind) {
            case TYPE_CHAR:
                result->kind       = INIT_CHAR;
                result->u.char_val = c->u.char_val;
                break;
            case TYPE_INT:
                result->kind      = INIT_INT;
                result->u.int_val = c->u.int_val;
                break;
            case TYPE_LONG:
                result->kind       = INIT_LONG;
                result->u.long_val = c->u.int_val; // Simplified
                break;
            case TYPE_DOUBLE:
                result->kind         = INIT_DOUBLE;
                result->u.double_val = c->u.real_val;
                break;
            default:
                fprintf(stderr, "Unsupported constant type for initializer\n");
                exit(1);
            }
            return result;
        }
        fprintf(stderr, "Invalid static initializer for type %d\n", var_type->kind);
        exit(1);
    }
    if (var_type->kind == TYPE_STRUCT && init->kind == INITIALIZER_COMPOUND) {
        TypeEntry *entry = typetab_find(var_type->u.struct_t.name);
        int init_count   = 0;
        for (InitItem *item = init->u.items; item; item = item->next)
            init_count++;
        if (init_count > entry->member_count) {
            fprintf(stderr, "Too many elements in struct initializer\n");
            exit(1);
        }
        StaticInitializer *result = NULL, **tail = &result;
        int current_offset = 0;
        for (int i = 0; i < init_count; i++) {
            TypeMember *memb = &entry->members[i];
            if (current_offset < memb->offset) {
                StaticInitializer *zero = malloc(sizeof(StaticInitializer));
                zero->kind              = INIT_ZERO;
                zero->u.zero_bytes      = memb->offset - current_offset;
                zero->next              = NULL;
                *tail                   = zero;
                tail                    = &zero->next;
            }
            StaticInitializer *member_init = static_init_helper(memb->type, init->u.items[i].init);
            *tail                          = member_init;
            while (*tail)
                tail = &(*tail)->next;
            current_offset = memb->offset + get_size(memb->type);
        }
        if (current_offset < entry->size) {
            StaticInitializer *zero = malloc(sizeof(StaticInitializer));
            zero->kind              = INIT_ZERO;
            zero->u.zero_bytes      = entry->size - current_offset;
            zero->next              = NULL;
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
            fprintf(stderr, "Too many values in static initializer\n");
            exit(1);
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
            StaticInitializer *zero = malloc(sizeof(StaticInitializer));
            zero->kind              = INIT_ZERO;
            zero->u.zero_bytes =
                ((int)array_size - init_count) * get_size(var_type->u.array.element);
            zero->next = NULL;
            *tail      = zero;
        }
        return result;
    }
    fprintf(stderr, "Invalid static initializer for type %d\n", var_type->kind);
    exit(1);
}

// Convert initializer to static initializer
StaticInitializer *to_static_init(Type *var_type, Initializer *init)
{
    return static_init_helper(var_type, init);
}

// Type-check an initializer
Initializer *typecheck_init(Type *target_type, Initializer *init)
{
    if (!init)
        return NULL;
    init->type = clone_type(target_type);
    if (target_type->kind == TYPE_ARRAY && init->kind == INITIALIZER_SINGLE &&
        init->u.expr->kind == EXPR_LITERAL && init->u.expr->u.literal->kind == LITERAL_STRING) {
        if (!is_character(target_type->u.array.element)) {
            fprintf(stderr, "Can't initialize non-character type with string literal\n");
            exit(1);
        }
        size_t len = strlen(init->u.expr->u.literal->u.string_val);
        if (len > (size_t)target_type->u.array.size) {
            fprintf(stderr, "Too many characters in string literal\n");
            exit(1);
        }
        init->u.expr = typecheck_string(init->u.expr);
        return init;
    }
    if (target_type->kind == TYPE_STRUCT && init->kind == INITIALIZER_COMPOUND) {
        int member_count;
        TypeMember *members = typetab_get_members(target_type->u.struct_t.name, &member_count);
        int init_count      = 0;
        for (InitItem *item = init->u.items; item; item = item->next)
            init_count++;
        if (init_count > member_count) {
            fprintf(stderr, "Too many elements in structure initializer\n");
            exit(1);
        }
        InitItem *items = NULL, **tail = &items;
        for (int i = 0; i < init_count; i++) {
            InitItem *new_item    = malloc(sizeof(InitItem));
            new_item->designators = NULL;
            new_item->init        = typecheck_init(members[i].type, init->u.items[i].init);
            new_item->next        = NULL;
            *tail                 = new_item;
            tail                  = &new_item->next;
        }
        for (int i = init_count; i < member_count; i++) {
            InitItem *new_item    = malloc(sizeof(InitItem));
            new_item->designators = NULL;
            new_item->init        = make_zero_init(members[i].type);
            new_item->next        = NULL;
            *tail                 = new_item;
            tail                  = &new_item->next;
        }
        free(members);
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
            fprintf(stderr, "Too many values in initializer\n");
            exit(1);
        }
        InitItem *items = NULL, **tail = &items;
        for (int i = 0; i < init_count; i++) {
            InitItem *new_item    = malloc(sizeof(InitItem));
            new_item->designators = NULL;
            new_item->init = typecheck_init(target_type->u.array.element, init->u.items[i].init);
            new_item->next = NULL;
            *tail          = new_item;
            tail           = &new_item->next;
        }
        for (int i = init_count; i < (int)array_size; i++) {
            InitItem *new_item    = malloc(sizeof(InitItem));
            new_item->designators = NULL;
            new_item->init        = make_zero_init(target_type->u.array.element);
            new_item->next        = NULL;
            *tail                 = new_item;
            tail                  = &new_item->next;
        }
        init->u.items = items;
        return init;
    }
    fprintf(stderr, "Can't initialize scalar value from compound initializer\n");
    exit(1);
}

// Type-check a block
DeclOrStmt *typecheck_block(Type *ret_type)
{
    for (DeclOrStmt *stmt = d->u.function.body->u.compound; stmt; stmt = stmt->next) {
        if (stmt->kind == DECL_OR_STMT_STMT) {
            stmt->u.stmt = typecheck_stmt(ret_type, stmt->u.stmt);
        } else {
            typecheck_decl(stmt->u.decl);
        }
    }
    return d->u.function.body->u.compound;
}

// Type-check a statement
Stmt *typecheck_statement(Type *ret_type, Stmt *s)
{
    if (!s)
        return NULL;
    switch (s->kind) {
    case STMT_RETURN:
        if (s->u.expr) {
            if (ret_type->kind == TYPE_VOID) {
                fprintf(stderr, "Void function cannot return a value\n");
                exit(1);
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
            if (s->u.for_stmt.init->u.decl->u.var.specifiers->storage != STORAGE_CLASS_NONE) {
                fprintf(stderr, "Storage class not permitted in for loop header\n");
                exit(1);
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
        fprintf(stderr, "Unsupported statement kind %d\n", s->kind);
        exit(1);
    }
}

// Type-check a local variable declaration
void typecheck_local_var_decl(Declaration *d)
{
    Type *var_type       = d->u.var.specifiers->type;
    InitDeclarator *decl = d->u.var.declarators;
    if (var_type->kind == TYPE_VOID) {
        fprintf(stderr, "No void declarations\n");
        exit(1);
    }
    validate_type(var_type);
    if (d->u.var.specifiers->storage == STORAGE_CLASS_EXTERN) {
        if (decl->init) {
            fprintf(stderr, "Initializer on local extern declaration\n");
            exit(1);
        }
        Symbol *existing = symtab_get_opt(decl->name);
        if (existing && existing->type->kind != var_type->kind) {
            fprintf(stderr, "Variable %s redeclared with different type\n", decl->name);
            exit(1);
        }
        if (!existing) {
            symtab_add_static_var(decl->name, clone_type(var_type), true, INIT_NONE, NULL);
        }
        return;
    }
    if (!is_complete(var_type)) {
        fprintf(stderr, "Cannot define a variable with incomplete type\n");
        exit(1);
    }
    if (d->u.var.specifiers->storage == STORAGE_CLASS_STATIC) {
        StaticInitializer *static_init =
            decl->init ? to_static_init(var_type, decl->init) : static_init_helper(var_type, NULL);
        symtab_add_static_var(decl->name, clone_type(var_type), false, INIT_INITIALIZED,
                              static_init);
        decl->init = NULL; // Drop initializer
        return;
    }
    symtab_add_automatic_var(decl->name, clone_type(var_type));
    decl->init = typecheck_init(var_type, decl->init);
}

// Type-check a function declaration
void typecheck_fn_decl(ExternalDecl *d)
{
    Type *fun_type = d->u.function.type;
    validate_type(fun_type);
    Type *adjusted_type = clone_type(fun_type);
    if (fun_type->kind == TYPE_FUNCTION) {
        if (fun_type->u.function.return_type->kind == TYPE_ARRAY) {
            fprintf(stderr, "A function cannot return an array\n");
            exit(1);
        }
        Param *p = adjusted_type->u.function.params;
        while (p) {
            if (p->type->kind == TYPE_ARRAY) {
                Type *ptr             = new_type(TYPE_POINTER);
                ptr->u.pointer.target = clone_type(p->type->u.array.element);
                p->type               = ptr;
            } else if (p->type->kind == TYPE_VOID) {
                fprintf(stderr, "No void params allowed\n");
                exit(1);
            }
            p = p->next;
        }
    } else {
        fprintf(stderr, "Function has non-function type\n");
        exit(1);
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
        fprintf(stderr, "Can't define function with incomplete types\n");
        exit(1);
    }
    bool global      = d->u.function.specifiers->storage != STORAGE_CLASS_STATIC;
    Symbol *existing = symtab_get_opt(d->u.function.name);
    bool defined     = has_body;
    if (existing) {
        if (existing->type->kind != fun_type->kind) {
            fprintf(stderr, "Redeclared function %s with different type\n", d->u.function.name);
            exit(1);
        }
        if (existing->kind == SYM_FUNC) {
            if (existing->u.func.defined && has_body) {
                fprintf(stderr, "Defined function %s twice\n", d->u.function.name);
                exit(1);
            }
            if (existing->u.func.global &&
                d->u.function.specifiers->storage == STORAGE_CLASS_STATIC) {
                fprintf(stderr, "Static function declaration follows non-static\n");
                exit(1);
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
            symtab_add_automatic_var(param->u.var.declarators->name, clone_type(p->type));
        }
        d->u.function.body =
            typecheck_statement(fun_type->u.function.return_type, d->u.function.body);
    }
    d->u.function.type = adjusted_type; // Update type in place
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
        fprintf(stderr, "Unsupported local declaration kind %d\n", d->kind);
        exit(1);
    }
}

// Type-check a global variable declaration
void typecheck_file_scope_var_decl(Declaration *d)
{
    Type *var_type       = d->u.var.specifiers->type;
    InitDeclarator *decl = d->u.var.declarators;
    if (var_type->kind == TYPE_VOID) {
        fprintf(stderr, "Void variables not allowed\n");
        exit(1);
    }
    validate_type(var_type);
    bool global = d->u.var.specifiers->storage != STORAGE_CLASS_STATIC;
    InitKind init_kind =
        d->u.var.specifiers->storage == STORAGE_CLASS_EXTERN ? INIT_NONE : INIT_TENTATIVE;
    StaticInitializer *init_list = NULL;
    if (decl->init) {
        init_kind = INIT_INITIALIZED;
        init_list = to_static_init(var_type, decl->init);
    }
    if (!is_complete(var_type) && init_kind != INIT_NONE) {
        fprintf(stderr, "Can't define a variable with incomplete type\n");
        exit(1);
    }
    Symbol *existing = symtab_get_opt(decl->name);
    if (existing) {
        if (existing->type->kind != var_type->kind) {
            fprintf(stderr, "Variable %s redeclared with different type\n", decl->name);
            exit(1);
        }
        if (existing->kind == SYM_STATIC) {
            if (d->u.var.specifiers->storage != STORAGE_CLASS_EXTERN &&
                existing->u.static_var.global != global) {
                fprintf(stderr, "Conflicting variable linkage\n");
                exit(1);
            }
            if (existing->u.static_var.init_kind == INIT_INITIALIZED &&
                init_kind == INIT_INITIALIZED) {
                fprintf(stderr, "Conflicting global variable definition\n");
                exit(1);
            }
            init_kind = existing->u.static_var.init_kind == INIT_INITIALIZED
                            ? existing->u.static_var.init_kind
                            : init_kind;
            init_list = existing->u.static_var.init_kind == INIT_INITIALIZED
                            ? existing->u.static_var.init_list
                            : init_list;
            global    = d->u.var.specifiers->storage == STORAGE_CLASS_EXTERN
                            ? existing->u.static_var.global
                            : global;
        }
    }
    symtab_add_static_var(decl->name, clone_type(var_type), global, init_kind, init_list);
    decl->init = NULL; // Drop initializer
}

// Type-check a global declaration
void typecheck_global_decl(ExternalDecl *d)
{
    if (d->kind == EXTERNAL_DECL_FUNCTION) {
        typecheck_fn_decl(d);
    } else {
        typecheck_file_scope_var_decl(d->u.declaration);
    }
}

// Type-check a program
void typecheck(Program *p)
{
    symtab_init();
    typetab_init();
    for (ExternalDecl *d = p->decls; d; d = d->next) {
        typecheck_global_decl(d);
    }
}
