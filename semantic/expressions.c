//
// Type-checking for expressions.
//
#include <stdio.h>
#include <string.h>

#include "semantic.h"
#include "structtab.h"
#include "symtab.h"
#include "typecheck.h"
#include "typetab.h"
#include "xalloc.h"

// Check if an expression is an lvalue.
static bool is_lvalue(const Expr *e)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    switch (e->kind) {
    case EXPR_VAR:
    case EXPR_FIELD_ACCESS:
    case EXPR_PTR_ACCESS:
    case EXPR_SUBSCRIPT:
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

// Type-check a variable reference.
static Expr *typecheck_var(Expr *e)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    const Symbol *sym = symtab_get(e->u.var);

    free_type(e->type);
    e->type = clone_type(sym->type, __func__, __FILE__, __LINE__);
    return e;
}

// Type-check a string literal.
Expr *typecheck_string(Expr *e)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    Type *array            = new_type(TYPE_ARRAY, __func__, __FILE__, __LINE__);
    array->u.array.element = new_type(TYPE_CHAR, __func__, __FILE__, __LINE__);
    set_array_size(array, strlen(e->u.literal->u.string_val) + 1);
    free_type(e->type);
    e->type = array;
    return e;
}

// Type-check a constant literal.
static Expr *typecheck_literal(Expr *e)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    free_type(e->type);
    e->type = NULL; // prevent double-free: typecheck_string also calls free_type(e->type)
    switch (e->u.literal->kind) {
    case LITERAL_INT:
        e->type = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
        break;
    case LITERAL_CHAR:
        e->type = new_type(TYPE_CHAR, __func__, __FILE__, __LINE__);
        break;
    case LITERAL_FLOAT:
        e->type = new_type(TYPE_DOUBLE, __func__, __FILE__, __LINE__);
        break;
    case LITERAL_STRING: {
        e = typecheck_string(e);
        break;
    }
    case LITERAL_ENUM: {
        const Symbol *sym = symtab_get(e->u.literal->u.enum_const);
        int val           = sym->u.enum_val;
        xfree(e->u.literal->u.enum_const);
        e->u.literal->kind      = LITERAL_INT;
        e->u.literal->u.int_val = val;
        e->type                 = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
        break;
    }
    default:
        fatal_error("Unsupported literal kind %d", e->u.literal->kind);
    }
    return e;
}

// Type-check an expression.
static Expr *typecheck_expr(Expr *e)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!e)
        return NULL;
    switch (e->kind) {
    case EXPR_VAR:
        return typecheck_var(e);
    case EXPR_LITERAL:
        return typecheck_literal(e);
    case EXPR_CAST: {
        validate_type(e->u.cast.type);
        Expr *inner = typecheck_and_decay(e->u.cast.expr);
        if ((e->u.cast.type->kind == TYPE_DOUBLE && is_pointer(inner->type)) ||
            (is_pointer(e->u.cast.type) && inner->type->kind == TYPE_DOUBLE)) {
            fatal_error("Cannot cast between pointer and double");
        }
        if (e->u.cast.type->kind == TYPE_VOID) {
            free_type(e->type);
            e->type        = clone_type(e->u.cast.type, __func__, __FILE__, __LINE__);
            e->u.cast.expr = inner;
            return e;
        }
        if (!is_scalar(e->u.cast.type) || !is_scalar(inner->type)) {
            fatal_error("Can only cast scalar types");
        }
        free_type(e->type);
        e->type        = clone_type(e->u.cast.type, __func__, __FILE__, __LINE__);
        e->u.cast.expr = inner;
        return e;
    }
    case EXPR_UNARY_OP: {
        switch (e->u.unary_op.op) {
        case UNARY_LOG_NOT: {
            free_type(e->type);
            Expr *inner        = typecheck_scalar(e->u.unary_op.expr);
            e->type            = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
            e->u.unary_op.expr = inner;
            return e;
        }
        case UNARY_BIT_NOT: {
            Expr *inner = typecheck_and_decay(e->u.unary_op.expr);
            if (!is_integer(inner->type)) {
                fatal_error("Bitwise complement only valid for integer types");
            }
            if (is_character(inner->type))
                inner = convert_to_kind(inner, TYPE_INT);
            free_type(e->type);
            e->type            = clone_type(inner->type, __func__, __FILE__, __LINE__);
            e->u.unary_op.expr = inner;
            return e;
        }
        case UNARY_PLUS:
        case UNARY_NEG: {
            Expr *inner = typecheck_and_decay(e->u.unary_op.expr);
            if (!is_arithmetic(inner->type)) {
                fatal_error("Can only apply unary +/- to arithmetic types");
            }
            if (is_character(inner->type))
                inner = convert_to_kind(inner, TYPE_INT);
            free_type(e->type);
            e->type            = clone_type(inner->type, __func__, __FILE__, __LINE__);
            e->u.unary_op.expr = inner;
            return e;
        }
        case UNARY_DEREF: {
            Expr *inner = typecheck_and_decay(e->u.unary_op.expr);
            if (!is_pointer(inner->type)) {
                fatal_error("Tried to dereference non-pointer");
            }
            if (inner->type->u.pointer.target->kind == TYPE_VOID) {
                fatal_error("Can't dereference pointer to void");
            }
            free_type(e->type);
            e->type = clone_type(inner->type->u.pointer.target, __func__, __FILE__, __LINE__);
            e->u.unary_op.expr = inner;
            return e;
        }
        case UNARY_ADDRESS: {
            Expr *inner = typecheck_expr(e->u.unary_op.expr);
            if (!is_lvalue(inner)) {
                fatal_error("Cannot take address of non-lvalue");
            }
            Type *ptr             = new_type(TYPE_POINTER, __func__, __FILE__, __LINE__);
            ptr->u.pointer.target = clone_type(inner->type, __func__, __FILE__, __LINE__);
            free_type(e->type);
            e->type            = ptr;
            e->u.unary_op.expr = inner;
            return e;
        }
        case UNARY_PRE_INC:
        case UNARY_PRE_DEC: {
            Expr *inner = typecheck_and_decay(e->u.unary_op.expr);
            if (!is_lvalue(inner)) {
                fatal_error("Operand of pre-increment/decrement must be a modifiable lvalue");
            }
            if (!is_scalar(inner->type)) {
                fatal_error("Operand of pre-increment/decrement must be a scalar type");
            }
            free_type(e->type);
            e->type            = clone_type(inner->type, __func__, __FILE__, __LINE__);
            e->u.unary_op.expr = inner;
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
            e1 = typecheck_scalar(e1);
            e2 = typecheck_scalar(e2);
            free_type(e->type);
            e->type              = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_ADD: {
            e1 = typecheck_and_decay(e1);
            e2 = typecheck_and_decay(e2);
            free_type(e->type);
            if (is_arithmetic(e1->type) && is_arithmetic(e2->type)) {
                const Type *common = get_common_type(e1->type, e2->type);
                e1                 = convert_to_type(e1, common);
                e2                 = convert_to_type(e2, common);
                e->type            = clone_type(common, __func__, __FILE__, __LINE__);
            } else if (is_complete_pointer(e1->type) && is_integer(e2->type)) {
                e2      = convert_to_kind(e2, TYPE_LONG);
                e->type = clone_type(e1->type, __func__, __FILE__, __LINE__);
            } else if (is_complete_pointer(e2->type) && is_integer(e1->type)) {
                e1      = convert_to_kind(e1, TYPE_LONG);
                e->type = clone_type(e2->type, __func__, __FILE__, __LINE__);
            } else {
                fatal_error("Invalid operands for addition");
            }
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_SUB: {
            e1 = typecheck_and_decay(e1);
            e2 = typecheck_and_decay(e2);
            free_type(e->type);
            if (is_arithmetic(e1->type) && is_arithmetic(e2->type)) {
                const Type *common = get_common_type(e1->type, e2->type);
                e1                 = convert_to_type(e1, common);
                e2                 = convert_to_type(e2, common);
                e->type            = clone_type(common, __func__, __FILE__, __LINE__);
            } else if (is_complete_pointer(e1->type) && is_integer(e2->type)) {
                e2      = convert_to_kind(e2, TYPE_LONG);
                e->type = clone_type(e1->type, __func__, __FILE__, __LINE__);
            } else if (is_complete_pointer(e1->type) && e1->type->kind == e2->type->kind) {
                e->type = new_type(TYPE_LONG, __func__, __FILE__, __LINE__);
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
            e1 = typecheck_and_decay(e1);
            e2 = typecheck_and_decay(e2);
            if (!is_arithmetic(e1->type) || !is_arithmetic(e2->type)) {
                fatal_error("Can only multiply arithmetic types");
            }
            const Type *common = get_common_type(e1->type, e2->type);
            e1                 = convert_to_type(e1, common);
            e2                 = convert_to_type(e2, common);
            if (e->u.binary_op.op == BINARY_MOD && common->kind == TYPE_DOUBLE) {
                fatal_error("Can't apply %% to double");
            }
            free_type(e->type);
            e->type              = clone_type(common, __func__, __FILE__, __LINE__);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_EQ:
        case BINARY_NE: {
            e1                 = typecheck_and_decay(e1);
            e2                 = typecheck_and_decay(e2);
            const Type *common = is_pointer(e1->type) || is_pointer(e2->type)
                                     ? common_pointer_type(e1, e2)
                                     : get_common_type(e1->type, e2->type);
            e1                 = convert_to_type(e1, common);
            e2                 = convert_to_type(e2, common);
            free_type(e->type);
            e->type              = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_LT:
        case BINARY_GT:
        case BINARY_LE:
        case BINARY_GE: {
            e1                 = typecheck_and_decay(e1);
            e2                 = typecheck_and_decay(e2);
            const Type *common = is_arithmetic(e1->type) && is_arithmetic(e2->type)
                                     ? get_common_type(e1->type, e2->type)
                                     : (e1->type->kind == e2->type->kind ? e1->type : NULL);
            if (!common) {
                fatal_error("Invalid types for comparison");
            }
            e1 = convert_to_type(e1, common);
            e2 = convert_to_type(e2, common);
            free_type(e->type);
            e->type              = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_BIT_AND:
        case BINARY_BIT_XOR:
        case BINARY_BIT_OR: {
            e1 = typecheck_and_decay(e1);
            e2 = typecheck_and_decay(e2);
            if (!is_integer(e1->type) || !is_integer(e2->type)) {
                fatal_error("Bitwise operators require integer operands");
            }
            const Type *common = get_common_type(e1->type, e2->type);
            e1                 = convert_to_type(e1, common);
            e2                 = convert_to_type(e2, common);
            free_type(e->type);
            e->type              = clone_type(common, __func__, __FILE__, __LINE__);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_LEFT_SHIFT:
        case BINARY_RIGHT_SHIFT: {
            e1 = typecheck_and_decay(e1);
            e2 = typecheck_and_decay(e2);
            if (!is_integer(e1->type) || !is_integer(e2->type)) {
                fatal_error("Shift operators require integer operands");
            }
            if (is_character(e1->type)) {
                e1 = convert_to_kind(e1, TYPE_INT);
            }
            if (is_character(e2->type)) {
                e2 = convert_to_kind(e2, TYPE_INT);
            }
            free_type(e->type);
            e->type              = clone_type(e1->type, __func__, __FILE__, __LINE__);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        default:
            fatal_error("Unsupported binary op %d", e->u.binary_op.op);
        }
    }
    case EXPR_ASSIGN: {
        Expr *lhs = typecheck_and_decay(e->u.assign.target);
        if (!is_lvalue(lhs)) {
            fatal_error("Left hand side of assignment is invalid lvalue");
        }
        Expr *rhs = typecheck_and_decay(e->u.assign.value);
        rhs       = coerce_for_assignment(rhs, lhs->type);
        free_type(e->type);
        e->type            = clone_type(lhs->type, __func__, __FILE__, __LINE__);
        e->u.assign.target = lhs;
        e->u.assign.value  = rhs;
        return e;
    }
    case EXPR_COND: {
        Expr *cond      = typecheck_scalar(e->u.cond.condition);
        Expr *then_expr = typecheck_and_decay(e->u.cond.then_expr);
        Expr *else_expr = typecheck_and_decay(e->u.cond.else_expr);
        const Type *result_type;
        if (then_expr->type->kind == TYPE_VOID && else_expr->type->kind == TYPE_VOID) {
            result_type = new_type(TYPE_VOID, __func__, __FILE__, __LINE__);
        } else if (is_pointer(then_expr->type) || is_pointer(else_expr->type)) {
            result_type = common_pointer_type(then_expr, else_expr);
        } else if (is_arithmetic(then_expr->type) && is_arithmetic(else_expr->type)) {
            result_type = get_common_type(then_expr->type, else_expr->type);
        } else if (then_expr->type->kind == else_expr->type->kind) {
            result_type = then_expr->type;
        } else {
            fatal_error("Invalid operands for conditional");
        }
        free_type(e->type);
        e->type             = clone_type(result_type, __func__, __FILE__, __LINE__);
        e->u.cond.condition = cond;
        e->u.cond.then_expr = convert_to_type(then_expr, result_type);
        e->u.cond.else_expr = convert_to_type(else_expr, result_type);
        return e;
    }
    case EXPR_CALL: {
        Expr       *func    = e->u.call.func;
        const Type *fn_type;
        if (func->kind == EXPR_VAR) {
            const Symbol *sym = symtab_get(func->u.var);
            fn_type           = sym->type;
            if (fn_type->kind == TYPE_POINTER)
                fn_type = fn_type->u.pointer.target; // function pointer decay
            if (fn_type->kind != TYPE_FUNCTION)
                fatal_error("Tried to use variable as function name");
        } else {
            func    = typecheck_and_decay(func);
            fn_type = func->type;
            if (fn_type->kind == TYPE_POINTER)
                fn_type = fn_type->u.pointer.target;
            if (fn_type->kind != TYPE_FUNCTION)
                fatal_error("Expression is not a function or function pointer");
            e->u.call.func = func;
        }
        const Param *params = fn_type->u.function.params;
        int param_count = 0, arg_count = 0;
        for (const Param *p = params; p; p = p->next)
            param_count++;
        for (const Expr *a = e->u.call.args; a; a = a->next)
            arg_count++;
        if (param_count != arg_count) {
            fatal_error("Function called with wrong number of arguments");
        }
        Expr *arg = e->u.call.args, *prev = NULL, *new_args = NULL;
        const Param *p = params;
        while (arg && p) {
            Expr *new_arg = coerce_for_assignment(typecheck_and_decay(arg), p->type);
            if (!new_args)
                new_args = new_arg;
            if (prev)
                prev->next = new_arg;
            prev = new_arg;
            arg  = arg->next;
            p    = p->next;
        }
        free_type(e->type);
        e->type        = clone_type(fn_type->u.function.return_type, __func__, __FILE__, __LINE__);
        e->u.call.args = new_args;
        return e;
    }
    case EXPR_SUBSCRIPT: {
        Expr *ptr   = typecheck_and_decay(e->u.subscript.left);
        Expr *index = typecheck_and_decay(e->u.subscript.right);
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
        free_type(e->type);
        e->type              = clone_type(result_type, __func__, __FILE__, __LINE__);
        e->u.subscript.left  = ptr;
        e->u.subscript.right = index;
        return e;
    }
    case EXPR_SIZEOF_EXPR: {
        Expr *inner = typecheck_expr(e->u.sizeof_expr);
        if (!is_complete(inner->type)) {
            fatal_error("Can't apply sizeof to incomplete type");
        }
        free_type(e->type);
        e->type          = new_type(TYPE_ULONG, __func__, __FILE__, __LINE__);
        e->u.sizeof_expr = inner;
        return e;
    }
    case EXPR_SIZEOF_TYPE: {
        validate_type(e->u.sizeof_type);
        if (!is_complete(e->u.sizeof_type)) {
            fatal_error("Can't apply sizeof to incomplete type");
        }
        free_type(e->type);
        e->type = new_type(TYPE_ULONG, __func__, __FILE__, __LINE__);
        return e;
    }
    case EXPR_ALIGNOF: {
        validate_type(e->u.align_of);
        if (!is_complete(e->u.align_of)) {
            fatal_error("Can't apply _Alignof to incomplete type");
        }
        free_type(e->type);
        e->type = new_type(TYPE_ULONG, __func__, __FILE__, __LINE__);
        return e;
    }
    case EXPR_FIELD_ACCESS: {
        Expr *strct = typecheck_and_decay(e->u.field_access.expr);
        if (strct->type->kind != TYPE_STRUCT && strct->type->kind != TYPE_UNION) {
            fatal_error("Dot operator requires structure or union type");
        }
        const StructDef *entry = structtab_find(strct->type->u.struct_t.name);
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
        free_type(e->type);
        e->type                 = clone_type(member->type, __func__, __FILE__, __LINE__);
        e->u.field_access.offset = member->offset;
        e->u.field_access.expr  = strct;
        return e;
    }
    case EXPR_PTR_ACCESS: {
        Expr *strct_ptr = typecheck_and_decay(e->u.ptr_access.expr);
        const Type *ptr_type = strct_ptr->type;
        if (ptr_type->kind == TYPE_TYPEDEF_NAME)
            ptr_type = typetab_resolve(ptr_type->u.typedef_name.name);
        if (!is_pointer(ptr_type) ||
            (ptr_type->u.pointer.target->kind != TYPE_STRUCT &&
             ptr_type->u.pointer.target->kind != TYPE_UNION)) {
            fatal_error("Arrow operator requires pointer to structure or union");
        }
        const Type *target_type = ptr_type->u.pointer.target;
        if (target_type->kind == TYPE_TYPEDEF_NAME)
            target_type = typetab_resolve(target_type->u.typedef_name.name);
        const StructDef *entry = structtab_find(target_type->u.struct_t.name);
        const FieldDef *member = entry->members;
        for (; member; member = member->next) {
            if (strcmp(member->name, e->u.ptr_access.field) == 0) {
                break;
            }
        }
        if (!member) {
            fatal_error("Struct %s has no member %s",
                        target_type->u.struct_t.name,
                        e->u.ptr_access.field);
        }
        free_type(e->type);
        e->type                = clone_type(member->type, __func__, __FILE__, __LINE__);
        e->u.ptr_access.offset = member->offset;
        e->u.ptr_access.expr   = strct_ptr;
        return e;
    }
    case EXPR_POST_INC: {
        Expr *inner = typecheck_and_decay(e->u.post_inc);
        if (!is_lvalue(inner)) {
            fatal_error("Operand of post-increment must be a modifiable lvalue");
        }
        if (!is_scalar(inner->type)) {
            fatal_error("Operand of post-increment must be a scalar type");
        }
        free_type(e->type);
        e->type       = clone_type(inner->type, __func__, __FILE__, __LINE__);
        e->u.post_inc = inner;
        return e;
    }
    case EXPR_POST_DEC: {
        Expr *inner = typecheck_and_decay(e->u.post_dec);
        if (!is_lvalue(inner)) {
            fatal_error("Operand of post-decrement must be a modifiable lvalue");
        }
        if (!is_scalar(inner->type)) {
            fatal_error("Operand of post-decrement must be a scalar type");
        }
        free_type(e->type);
        e->type       = clone_type(inner->type, __func__, __FILE__, __LINE__);
        e->u.post_dec = inner;
        return e;
    }
    case EXPR_GENERIC: {
        // Controlling expression is not evaluated; only its type is used for matching.
        const Expr *ctrl      = typecheck_and_decay(e->u.generic.controlling_expr);
        const Type *ctrl_type = ctrl->type;

        GenericAssoc *selected      = NULL;
        GenericAssoc *default_assoc = NULL;
        for (GenericAssoc *ga = e->u.generic.associations; ga; ga = ga->next) {
            if (ga->kind == GENERIC_ASSOC_TYPE) {
                validate_type(ga->u.type_assoc.type);
                ga->u.type_assoc.expr = typecheck_and_decay(ga->u.type_assoc.expr);
                if (!selected && compare_type(ctrl_type, ga->u.type_assoc.type)) {
                    selected = ga;
                }
            } else {
                if (default_assoc)
                    fatal_error("Multiple default associations in _Generic");
                ga->u.default_assoc = typecheck_and_decay(ga->u.default_assoc);
                default_assoc       = ga;
            }
        }

        GenericAssoc *match = selected ? selected : default_assoc;
        if (!match)
            fatal_error("No matching association in _Generic expression");

        const Expr *match_expr = (match->kind == GENERIC_ASSOC_TYPE)
                                     ? match->u.type_assoc.expr
                                     : match->u.default_assoc;
        free_type(e->type);
        e->type = clone_type(match_expr->type, __func__, __FILE__, __LINE__);

        // Prune to the selected association so TAC lowering sees exactly one branch.
        for (GenericAssoc *ga = e->u.generic.associations, *nxt; ga; ga = nxt) {
            nxt = ga->next;
            if (ga == match) {
                match->next = NULL;
                continue;
            }
            if (ga->kind == GENERIC_ASSOC_TYPE) {
                free_type(ga->u.type_assoc.type);
                free_expression(ga->u.type_assoc.expr);
            } else {
                free_expression(ga->u.default_assoc);
            }
            xfree(ga);
        }
        e->u.generic.associations     = match;
        free_expression(e->u.generic.controlling_expr);
        e->u.generic.controlling_expr = NULL;
        return e;
    }
    case EXPR_COMPOUND: {
        Type *lit_type = e->u.compound_literal.type;
        validate_type(lit_type);
        if (!is_complete(lit_type)) {
            fatal_error("Compound literal must have a complete type");
        }
        if (lit_type->kind == TYPE_ARRAY || lit_type->kind == TYPE_STRUCT) {
            // Wrap InitItem list in a temporary INITIALIZER_COMPOUND to reuse typecheck_init.
            Initializer *wrap = new_initializer(INITIALIZER_COMPOUND);
            wrap->u.items     = e->u.compound_literal.init;
            Initializer *result = typecheck_init(lit_type, wrap);
            // Detach the type-checked items and free the wrapper shell.
            e->u.compound_literal.init = result->u.items;
            result->u.items            = NULL;
            free_initializer(result);
        } else {
            // Scalar: C11 allows {expr} for a scalar type; typecheck the single item.
            InitItem *item = e->u.compound_literal.init;
            if (!item || item->next) {
                fatal_error("Scalar compound literal must have exactly one initializer");
            }
            item->init = typecheck_init(lit_type, item->init);
        }
        free_type(e->type);
        e->type = clone_type(lit_type, __func__, __FILE__, __LINE__);
        return e;
    }
    default:
        fatal_error("Unsupported expression kind %d", e->kind);
    }
}

// Type-check an expression and apply array-to-pointer decay.
Expr *typecheck_and_decay(Expr *e)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!e)
        return NULL;
    Expr *typed = typecheck_expr(e);
    if (typed->type->kind == TYPE_STRUCT && !is_complete(typed->type)) {
        fatal_error("Incomplete structure type not permitted");
    }
    if (typed->type->kind == TYPE_ARRAY) {
        Type *ptr = new_type(TYPE_POINTER, __func__, __FILE__, __LINE__);
        ptr->u.pointer.target =
            clone_type(typed->type->u.array.element, __func__, __FILE__, __LINE__);
        free_type(typed->type);
        typed->type = ptr; // Modify in place
    }
    if (typed->type->kind == TYPE_FUNCTION) {
        Type *ptr             = new_type(TYPE_POINTER, __func__, __FILE__, __LINE__);
        ptr->u.pointer.target = clone_type(typed->type, __func__, __FILE__, __LINE__);
        free_type(typed->type);
        typed->type = ptr;
    }
    return typed;
}

// Type-check an expression and require it to be scalar.
Expr *typecheck_scalar(Expr *e)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *typed = typecheck_and_decay(e);
    if (!is_scalar(typed->type)) {
        fatal_error("A scalar operand is required");
    }
    return typed;
}
