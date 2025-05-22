#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "internal.h"
#include "xalloc.h"

/* Clone TypeQualifier */
TypeQualifier *clone_type_qualifier(const TypeQualifier *qualifier)
{
    if (!qualifier)
        return NULL;
    TypeQualifier *new_qual = xmalloc(sizeof(TypeQualifier), __func__, __FILE__, __LINE__);
    if (!new_qual)
        return NULL;
    new_qual->kind = qualifier->kind;
    new_qual->next = clone_type_qualifier(qualifier->next);
    return new_qual;
}

/* Clone Param */
Param *clone_param(const Param *param)
{
    if (!param)
        return NULL;
    Param *new_param = xmalloc(sizeof(Param), __func__, __FILE__, __LINE__);
    if (!new_param)
        return NULL;
    new_param->name = xstrdup(param->name);
    new_param->type = clone_type(param->type);
    new_param->next = clone_param(param->next);
    return new_param;
}

/* Clone Field */
Field *clone_field(const Field *field)
{
    if (!field)
        return NULL;
    Field *new_field = xmalloc(sizeof(Field), __func__, __FILE__, __LINE__);
    if (!new_field)
        return NULL;
    new_field->type     = clone_type(field->type);
    new_field->name     = xstrdup(field->name);
    new_field->bitfield = clone_expression(field->bitfield);
    new_field->next     = clone_field(field->next);
    return new_field;
}

/* Clone Enumerator */
Enumerator *clone_enumerator(const Enumerator *enumerator)
{
    if (!enumerator)
        return NULL;
    Enumerator *new_enum = xmalloc(sizeof(Enumerator), __func__, __FILE__, __LINE__);
    if (!new_enum)
        return NULL;
    new_enum->name  = xstrdup(enumerator->name);
    new_enum->value = clone_expression(enumerator->value);
    new_enum->next  = clone_enumerator(enumerator->next);
    return new_enum;
}

/* Clone Type */
Type *clone_type(const Type *type)
{
    if (!type)
        return NULL;
    Type *new_type = xmalloc(sizeof(Type), __func__, __FILE__, __LINE__);
    if (!new_type)
        return NULL;
    new_type->kind       = type->kind;
    new_type->qualifiers = clone_type_qualifier(type->qualifiers);

    switch (type->kind) {
    case TYPE_INT:
        new_type->u.integer.signedness = type->u.integer.signedness;
        break;
    case TYPE_COMPLEX:
    case TYPE_IMAGINARY:
        new_type->u.complex.base = clone_type(type->u.complex.base);
        break;
    case TYPE_POINTER:
        new_type->u.pointer.target     = clone_type(type->u.pointer.target);
        new_type->u.pointer.qualifiers = clone_type_qualifier(type->u.pointer.qualifiers);
        break;
    case TYPE_ARRAY:
        new_type->u.array.element    = clone_type(type->u.array.element);
        new_type->u.array.size       = clone_expression(type->u.array.size);
        new_type->u.array.qualifiers = clone_type_qualifier(type->u.array.qualifiers);
        new_type->u.array.is_static  = type->u.array.is_static;
        break;
    case TYPE_FUNCTION:
        new_type->u.function.return_type = clone_type(type->u.function.return_type);
        new_type->u.function.params      = clone_param(type->u.function.params);
        new_type->u.function.variadic    = type->u.function.variadic;
        break;
    case TYPE_STRUCT:
    case TYPE_UNION:
        new_type->u.struct_t.name   = xstrdup(type->u.struct_t.name);
        new_type->u.struct_t.fields = clone_field(type->u.struct_t.fields);
        break;
    case TYPE_ENUM:
        new_type->u.enum_t.name        = xstrdup(type->u.enum_t.name);
        new_type->u.enum_t.enumerators = clone_enumerator(type->u.enum_t.enumerators);
        break;
    case TYPE_TYPEDEF_NAME:
        new_type->u.typedef_name.name = xstrdup(type->u.typedef_name.name);
        break;
    case TYPE_ATOMIC:
        new_type->u.atomic.base = clone_type(type->u.atomic.base);
        break;
    default:
        break;
    }
    return new_type;
}

/* Clone Literal */
Literal *clone_literal(const Literal *literal)
{
    if (!literal)
        return NULL;
    Literal *new_literal = xmalloc(sizeof(Literal), __func__, __FILE__, __LINE__);
    if (!new_literal)
        return NULL;
    new_literal->kind = literal->kind;
    switch (literal->kind) {
    case LITERAL_INT:
        new_literal->u.int_val = literal->u.int_val;
        break;
    case LITERAL_FLOAT:
        new_literal->u.real_val = literal->u.real_val;
        break;
    case LITERAL_CHAR:
        new_literal->u.char_val = literal->u.char_val;
        break;
    case LITERAL_STRING:
        new_literal->u.string_val = xstrdup(literal->u.string_val);
        break;
    case LITERAL_ENUM:
        new_literal->u.enum_const = xstrdup(literal->u.enum_const);
        break;
    }
    return new_literal;
}

/* Clone UnaryOp */
UnaryOp *clone_unary_op(const UnaryOp *op)
{
    if (!op)
        return NULL;
    UnaryOp *new_op = xmalloc(sizeof(UnaryOp), __func__, __FILE__, __LINE__);
    if (!new_op)
        return NULL;
    new_op->kind = op->kind;
    return new_op;
}

/* Clone BinaryOp */
BinaryOp *clone_binary_op(const BinaryOp *op)
{
    if (!op)
        return NULL;
    BinaryOp *new_op = xmalloc(sizeof(BinaryOp), __func__, __FILE__, __LINE__);
    if (!new_op)
        return NULL;
    new_op->kind = op->kind;
    return new_op;
}

/* Clone AssignOp */
AssignOp *clone_assign_op(const AssignOp *op)
{
    if (!op)
        return NULL;
    AssignOp *new_op = xmalloc(sizeof(AssignOp), __func__, __FILE__, __LINE__);
    if (!new_op)
        return NULL;
    new_op->kind = op->kind;
    return new_op;
}

/* Clone GenericAssoc */
GenericAssoc *clone_generic_assoc(const GenericAssoc *assoc)
{
    if (!assoc)
        return NULL;
    GenericAssoc *new_assoc = xmalloc(sizeof(GenericAssoc), __func__, __FILE__, __LINE__);
    if (!new_assoc)
        return NULL;
    new_assoc->kind = assoc->kind;
    if (assoc->kind == GENERIC_ASSOC_TYPE) {
        new_assoc->u.type_assoc.type = clone_type(assoc->u.type_assoc.type);
        new_assoc->u.type_assoc.expr = clone_expression(assoc->u.type_assoc.expr);
    } else {
        new_assoc->u.default_assoc = clone_expression(assoc->u.default_assoc);
    }
    new_assoc->next = clone_generic_assoc(assoc->next);
    return new_assoc;
}

/* Clone Designator */
Designator *clone_designator(const Designator *designator)
{
    if (!designator)
        return NULL;
    Designator *new_designator = xmalloc(sizeof(Designator), __func__, __FILE__, __LINE__);
    if (!new_designator)
        return NULL;
    new_designator->kind = designator->kind;
    if (designator->kind == DESIGNATOR_ARRAY) {
        new_designator->u.expr = clone_expression(designator->u.expr);
    } else {
        new_designator->u.name = xstrdup(designator->u.name);
    }
    new_designator->next = clone_designator(designator->next);
    return new_designator;
}

/* Clone Initializer */
Initializer *clone_initializer(const Initializer *init)
{
    if (!init)
        return NULL;
    Initializer *new_init = xmalloc(sizeof(Initializer), __func__, __FILE__, __LINE__);
    if (!new_init)
        return NULL;
    new_init->kind = init->kind;
    if (init->kind == INITIALIZER_SINGLE) {
        new_init->u.expr = clone_expression(init->u.expr);
    } else {
        new_init->u.items = clone_init_item(init->u.items);
    }
    return new_init;
}

/* Clone InitItem */
InitItem *clone_init_item(const InitItem *item)
{
    if (!item)
        return NULL;
    InitItem *new_item = xmalloc(sizeof(InitItem), __func__, __FILE__, __LINE__);
    if (!new_item)
        return NULL;
    new_item->designators = clone_designator(item->designators);
    new_item->init        = clone_initializer(item->init);
    new_item->next        = clone_init_item(item->next);
    return new_item;
}

/* Main function to clone an expression */
Expr *clone_expression(const Expr *expression)
{
    if (!expression)
        return NULL;
    Expr *new_expr = xmalloc(sizeof(Expr), __func__, __FILE__, __LINE__);
    if (!new_expr)
        return NULL;
    new_expr->kind = expression->kind;
    new_expr->next = clone_expression(expression->next);
    new_expr->type = clone_type(expression->type);

    switch (expression->kind) {
    case EXPR_LITERAL:
        new_expr->u.literal = clone_literal(expression->u.literal);
        break;
    case EXPR_VAR:
        new_expr->u.var = xstrdup(expression->u.var);
        break;
    case EXPR_UNARY_OP:
        new_expr->u.unary_op.op   = clone_unary_op(expression->u.unary_op.op);
        new_expr->u.unary_op.expr = clone_expression(expression->u.unary_op.expr);
        break;
    case EXPR_BINARY_OP:
        new_expr->u.binary_op.op    = clone_binary_op(expression->u.binary_op.op);
        new_expr->u.binary_op.left  = clone_expression(expression->u.binary_op.left);
        new_expr->u.binary_op.right = clone_expression(expression->u.binary_op.right);
        break;
    case EXPR_ASSIGN:
        new_expr->u.assign.op     = clone_assign_op(expression->u.assign.op);
        new_expr->u.assign.target = clone_expression(expression->u.assign.target);
        new_expr->u.assign.value  = clone_expression(expression->u.assign.value);
        break;
    case EXPR_COND:
        new_expr->u.cond.condition = clone_expression(expression->u.cond.condition);
        new_expr->u.cond.then_expr = clone_expression(expression->u.cond.then_expr);
        new_expr->u.cond.else_expr = clone_expression(expression->u.cond.else_expr);
        break;
    case EXPR_CAST:
        new_expr->u.cast.type = clone_type(expression->u.cast.type);
        new_expr->u.cast.expr = clone_expression(expression->u.cast.expr);
        break;
    case EXPR_CALL:
        new_expr->u.call.func = clone_expression(expression->u.call.func);
        new_expr->u.call.args = clone_expression(expression->u.call.args);
        break;
    case EXPR_COMPOUND:
        new_expr->u.compound_literal.type = clone_type(expression->u.compound_literal.type);
        new_expr->u.compound_literal.init = clone_init_item(expression->u.compound_literal.init);
        break;
    case EXPR_FIELD_ACCESS:
        new_expr->u.field_access.expr  = clone_expression(expression->u.field_access.expr);
        new_expr->u.field_access.field = xstrdup(expression->u.field_access.field);
        break;
    case EXPR_PTR_ACCESS:
        new_expr->u.ptr_access.expr  = clone_expression(expression->u.ptr_access.expr);
        new_expr->u.ptr_access.field = xstrdup(expression->u.ptr_access.field);
        break;
    case EXPR_POST_INC:
        new_expr->u.post_inc = clone_expression(expression->u.post_inc);
        break;
    case EXPR_POST_DEC:
        new_expr->u.post_dec = clone_expression(expression->u.post_dec);
        break;
    case EXPR_SIZEOF_EXPR:
        new_expr->u.sizeof_expr = clone_expression(expression->u.sizeof_expr);
        break;
    case EXPR_SIZEOF_TYPE:
        new_expr->u.sizeof_type = clone_type(expression->u.sizeof_type);
        break;
    case EXPR_ALIGNOF:
        new_expr->u.align_of = clone_type(expression->u.align_of);
        break;
    case EXPR_GENERIC:
        new_expr->u.generic.controlling_expr =
            clone_expression(expression->u.generic.controlling_expr);
        new_expr->u.generic.associations = clone_generic_assoc(expression->u.generic.associations);
        break;
    }
    return new_expr;
}

/* Clone TypeSpec */
TypeSpec *clone_type_spec(const TypeSpec *ts)
{
    if (!ts)
        return NULL;
    TypeSpec *new_ts = xmalloc(sizeof(TypeSpec), __func__, __FILE__, __LINE__);
    if (!new_ts)
        return NULL;
    new_ts->kind       = ts->kind;
    new_ts->next       = clone_type_spec(ts->next);
    new_ts->qualifiers = clone_type_qualifier(ts->qualifiers);

    switch (ts->kind) {
    case TYPE_SPEC_BASIC:
        new_ts->u.basic = clone_type(ts->u.basic);
        break;
    case TYPE_SPEC_STRUCT:
    case TYPE_SPEC_UNION:
        new_ts->u.struct_spec.name   = xstrdup(ts->u.struct_spec.name);
        new_ts->u.struct_spec.fields = clone_field(ts->u.struct_spec.fields);
        break;
    case TYPE_SPEC_ENUM:
        new_ts->u.enum_spec.name        = xstrdup(ts->u.enum_spec.name);
        new_ts->u.enum_spec.enumerators = clone_enumerator(ts->u.enum_spec.enumerators);
        break;
    case TYPE_SPEC_TYPEDEF_NAME:
        new_ts->u.typedef_name.name = xstrdup(ts->u.typedef_name.name);
        break;
    case TYPE_SPEC_ATOMIC:
        new_ts->u.atomic.type = clone_type(ts->u.atomic.type);
        break;
    }
    return new_ts;
}
