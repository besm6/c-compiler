#include <string.h>

#include "ast.h"

// Comparison functions
bool compare_type(const Type *a, const Type *b);
bool compare_type_qualifier(const TypeQualifier *a, const TypeQualifier *b);
bool compare_field(const Field *a, const Field *b);
bool compare_enumerator(const Enumerator *a, const Enumerator *b);
bool compare_param(const Param *a, const Param *b);
bool compare_declaration(const Declaration *a, const Declaration *b);
bool compare_decl_spec(const DeclSpec *a, const DeclSpec *b);
bool compare_function_spec(const FunctionSpec *a, const FunctionSpec *b);
bool compare_alignment_spec(const AlignmentSpec *a, const AlignmentSpec *b);
bool compare_init_declarator(const InitDeclarator *a, const InitDeclarator *b);
bool compare_initializer(const Initializer *a, const Initializer *b);
bool compare_init_item(const InitItem *a, const InitItem *b);
bool compare_designator(const Designator *a, const Designator *b);
bool compare_expr(const Expr *a, const Expr *b);
bool compare_literal(const Literal *a, const Literal *b);
bool compare_generic_assoc(const GenericAssoc *a, const GenericAssoc *b);
bool compare_stmt(const Stmt *a, const Stmt *b);
bool compare_decl_or_stmt(const DeclOrStmt *a, const DeclOrStmt *b);
bool compare_for_init(const ForInit *a, const ForInit *b);
bool compare_external_decl(const ExternalDecl *a, const ExternalDecl *b);
bool compare_program(const Program *a, const Program *b);

bool compare_ident(const char *a, const char *b)
{
    if (a == NULL && b == NULL)
        return true;
    if (a == NULL || b == NULL)
        return false;
    return strcmp(a, b) == 0;
}

bool compare_type(const Type *a, const Type *b)
{
    if (!a && !b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    switch (a->kind) {
    case TYPE_VOID:
    case TYPE_BOOL:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
    case TYPE_SIGNED:
    case TYPE_UNSIGNED:
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_LONG_LONG:
        return a->u.integer.signedness == b->u.integer.signedness;
    case TYPE_COMPLEX:
    case TYPE_IMAGINARY:
        return compare_type(a->u.complex.base, b->u.complex.base);
    case TYPE_POINTER:
        if (!compare_type(a->u.pointer.target, b->u.pointer.target))
            return false;
        return compare_type_qualifier(a->u.pointer.qualifiers, b->u.pointer.qualifiers);
    case TYPE_ARRAY:
        if (!compare_type(a->u.array.element, b->u.array.element))
            return false;
        if (!compare_expr(a->u.array.size, b->u.array.size))
            return false;
        if (!compare_type_qualifier(a->u.array.qualifiers, b->u.array.qualifiers))
            return false;
        return a->u.array.is_static == b->u.array.is_static;
    case TYPE_FUNCTION:
        if (!compare_type(a->u.function.return_type, b->u.function.return_type))
            return false;
        if (!compare_param(a->u.function.params, b->u.function.params))
            return false;
        return a->u.function.variadic == b->u.function.variadic;
    case TYPE_STRUCT:
    case TYPE_UNION:
        if (!compare_ident(a->u.struct_t.name, b->u.struct_t.name))
            return false;
        return compare_field(a->u.struct_t.fields, b->u.struct_t.fields);
    case TYPE_ENUM:
        if (!compare_ident(a->u.enum_t.name, b->u.enum_t.name))
            return false;
        return compare_enumerator(a->u.enum_t.enumerators, b->u.enum_t.enumerators);
    case TYPE_TYPEDEF_NAME:
        return compare_ident(a->u.typedef_name.name, b->u.typedef_name.name);
    case TYPE_ATOMIC:
        return compare_type(a->u.atomic.base, b->u.atomic.base);
    }
    return compare_type_qualifier(a->qualifiers, b->qualifiers);
}

bool compare_type_qualifier(const TypeQualifier *a, const TypeQualifier *b)
{
    while (a && b) {
        if (a->kind != b->kind)
            return false;
        a = a->next;
        b = b->next;
    }
    return a == NULL && b == NULL;
}

bool compare_field(const Field *a, const Field *b)
{
    while (a && b) {
        if (!compare_type(a->type, b->type))
            return false;
        if (!compare_ident(a->name, b->name))
            return false;
        if (!compare_expr(a->bitfield, b->bitfield))
            return false;
        a = a->next;
        b = b->next;
    }
    return a == NULL && b == NULL;
}

bool compare_enumerator(const Enumerator *a, const Enumerator *b)
{
    while (a && b) {
        if (!compare_ident(a->name, b->name))
            return false;
        if (!compare_expr(a->value, b->value))
            return false;
        a = a->next;
        b = b->next;
    }
    return a == NULL && b == NULL;
}

bool compare_param(const Param *a, const Param *b)
{
    while (a && b) {
        if (!compare_ident(a->name, b->name))
            return false;
        if (!compare_type(a->type, b->type))
            return false;
        if (!compare_decl_spec(a->specifiers, b->specifiers))
            return false;
        a = a->next;
        b = b->next;
    }
    return a == NULL && b == NULL;
}

bool compare_declaration(const Declaration *a, const Declaration *b)
{
    if (!a && !b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    switch (a->kind) {
    case DECL_VAR:
        if (!compare_decl_spec(a->u.var.specifiers, b->u.var.specifiers))
            return false;
        return compare_init_declarator(a->u.var.declarators, b->u.var.declarators);
    case DECL_STATIC_ASSERT:
        if (!compare_expr(a->u.static_assrt.condition, b->u.static_assrt.condition))
            return false;
        return compare_ident(a->u.static_assrt.message, b->u.static_assrt.message);
    case DECL_EMPTY:
        if (!compare_decl_spec(a->u.empty.specifiers, b->u.empty.specifiers))
            return false;
        return compare_type(a->u.empty.type, b->u.empty.type);
    }
    return true;
}

bool compare_decl_spec(const DeclSpec *a, const DeclSpec *b)
{
    if (!a && !b)
        return true;
    if (!a || !b)
        return false;
    if (!compare_type_qualifier(a->qualifiers, b->qualifiers))
        return false;
    if (a->storage != b->storage)
        return false;
    if (!compare_function_spec(a->func_specs, b->func_specs))
        return false;
    return compare_alignment_spec(a->align_spec, b->align_spec);
}

bool compare_function_spec(const FunctionSpec *a, const FunctionSpec *b)
{
    while (a && b) {
        if (a->kind != b->kind)
            return false;
        a = a->next;
        b = b->next;
    }
    return a == NULL && b == NULL;
}

bool compare_alignment_spec(const AlignmentSpec *a, const AlignmentSpec *b)
{
    if (!a && !b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    switch (a->kind) {
    case ALIGN_SPEC_TYPE:
        return compare_type(a->u.type, b->u.type);
    case ALIGN_SPEC_EXPR:
        return compare_expr(a->u.expr, b->u.expr);
    }
    return true;
}

bool compare_init_declarator(const InitDeclarator *a, const InitDeclarator *b)
{
    while (a && b) {
        if (!compare_type(a->type, b->type))
            return false;
        if (!compare_ident(a->name, b->name))
            return false;
        if (!compare_initializer(a->init, b->init))
            return false;
        a = a->next;
        b = b->next;
    }
    return a == NULL && b == NULL;
}

bool compare_initializer(const Initializer *a, const Initializer *b)
{
    if (!a && !b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    switch (a->kind) {
    case INITIALIZER_SINGLE:
        return compare_expr(a->u.expr, b->u.expr);
    case INITIALIZER_COMPOUND:
        return compare_init_item(a->u.items, b->u.items);
    }
    return true;
}

bool compare_init_item(const InitItem *a, const InitItem *b)
{
    while (a && b) {
        if (!compare_designator(a->designators, b->designators))
            return false;
        if (!compare_initializer(a->init, b->init))
            return false;
        a = a->next;
        b = b->next;
    }
    return a == NULL && b == NULL;
}

bool compare_designator(const Designator *a, const Designator *b)
{
    while (a && b) {
        if (a->kind != b->kind)
            return false;
        switch (a->kind) {
        case DESIGNATOR_ARRAY:
            if (!compare_expr(a->u.expr, b->u.expr))
                return false;
            break;
        case DESIGNATOR_FIELD:
            if (!compare_ident(a->u.name, b->u.name))
                return false;
            break;
        }
        a = a->next;
        b = b->next;
    }
    return a == NULL && b == NULL;
}

bool compare_expr(const Expr *a, const Expr *b)
{
    if (!a && !b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    switch (a->kind) {
    case EXPR_LITERAL:
        return compare_literal(a->u.literal, b->u.literal);
    case EXPR_VAR:
        return compare_ident(a->u.var, b->u.var);
    case EXPR_UNARY_OP:
        if (a->u.unary_op.op != b->u.unary_op.op)
            return false;
        return compare_expr(a->u.unary_op.expr, b->u.unary_op.expr);
    case EXPR_BINARY_OP:
        if (a->u.binary_op.op != b->u.binary_op.op)
            return false;
        if (!compare_expr(a->u.binary_op.left, b->u.binary_op.left))
            return false;
        return compare_expr(a->u.binary_op.right, b->u.binary_op.right);
    case EXPR_ASSIGN:
        if (a->u.assign.op != b->u.assign.op)
            return false;
        if (!compare_expr(a->u.assign.target, b->u.assign.target))
            return false;
        return compare_expr(a->u.assign.value, b->u.assign.value);
    case EXPR_COND:
        if (!compare_expr(a->u.cond.condition, b->u.cond.condition))
            return false;
        if (!compare_expr(a->u.cond.then_expr, b->u.cond.then_expr))
            return false;
        return compare_expr(a->u.cond.else_expr, b->u.cond.else_expr);
    case EXPR_CAST:
        if (!compare_type(a->u.cast.type, b->u.cast.type))
            return false;
        return compare_expr(a->u.cast.expr, b->u.cast.expr);
    case EXPR_CALL:
        if (!compare_expr(a->u.call.func, b->u.call.func))
            return false;
        return compare_expr(a->u.call.args, b->u.call.args);
    case EXPR_COMPOUND:
        if (!compare_type(a->u.compound_literal.type, b->u.compound_literal.type))
            return false;
        return compare_init_item(a->u.compound_literal.init, b->u.compound_literal.init);
    case EXPR_FIELD_ACCESS:
        if (!compare_expr(a->u.field_access.expr, b->u.field_access.expr))
            return false;
        return compare_ident(a->u.field_access.field, b->u.field_access.field);
    case EXPR_PTR_ACCESS:
        if (!compare_expr(a->u.ptr_access.expr, b->u.ptr_access.expr))
            return false;
        return compare_ident(a->u.ptr_access.field, b->u.ptr_access.field);
    case EXPR_POST_INC:
        return compare_expr(a->u.post_inc, b->u.post_inc);
    case EXPR_POST_DEC:
        return compare_expr(a->u.post_dec, b->u.post_dec);
    case EXPR_SIZEOF_EXPR:
        return compare_expr(a->u.sizeof_expr, b->u.sizeof_expr);
    case EXPR_SIZEOF_TYPE:
        return compare_type(a->u.sizeof_type, b->u.sizeof_type);
    case EXPR_ALIGNOF:
        return compare_type(a->u.align_of, b->u.align_of);
    case EXPR_GENERIC:
        if (!compare_expr(a->u.generic.controlling_expr, b->u.generic.controlling_expr))
            return false;
        return compare_generic_assoc(a->u.generic.associations, b->u.generic.associations);
    }
    return compare_type(a->type, b->type);
}

bool compare_literal(const Literal *a, const Literal *b)
{
    if (!a && !b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    switch (a->kind) {
    case LITERAL_INT:
        return a->u.int_val == b->u.int_val;
    case LITERAL_FLOAT:
        return a->u.real_val == b->u.real_val;
    case LITERAL_CHAR:
        return a->u.char_val == b->u.char_val;
    case LITERAL_STRING:
        return compare_ident(a->u.string_val, b->u.string_val);
    case LITERAL_ENUM:
        return compare_ident(a->u.enum_const, b->u.enum_const);
    }
    return true;
}

bool compare_generic_assoc(const GenericAssoc *a, const GenericAssoc *b)
{
    while (a && b) {
        if (a->kind != b->kind)
            return false;
        switch (a->kind) {
        case GENERIC_ASSOC_TYPE:
            if (!compare_type(a->u.type_assoc.type, b->u.type_assoc.type))
                return false;
            if (!compare_expr(a->u.type_assoc.expr, b->u.type_assoc.expr))
                return false;
            break;
        case GENERIC_ASSOC_DEFAULT:
            if (!compare_expr(a->u.default_assoc, b->u.default_assoc))
                return false;
            break;
        }
        a = a->next;
        b = b->next;
    }
    return a == NULL && b == NULL;
}

bool compare_stmt(const Stmt *a, const Stmt *b)
{
    if (!a && !b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    switch (a->kind) {
    case STMT_EXPR:
        return compare_expr(a->u.expr, b->u.expr);
    case STMT_COMPOUND:
        return compare_decl_or_stmt(a->u.compound, b->u.compound);
    case STMT_IF:
        if (!compare_expr(a->u.if_stmt.condition, b->u.if_stmt.condition))
            return false;
        if (!compare_stmt(a->u.if_stmt.then_stmt, b->u.if_stmt.then_stmt))
            return false;
        return compare_stmt(a->u.if_stmt.else_stmt, b->u.if_stmt.else_stmt);
    case STMT_SWITCH:
        if (!compare_expr(a->u.switch_stmt.expr, b->u.switch_stmt.expr))
            return false;
        return compare_stmt(a->u.switch_stmt.body, b->u.switch_stmt.body);
    case STMT_WHILE:
        if (!compare_expr(a->u.while_stmt.condition, b->u.while_stmt.condition))
            return false;
        return compare_stmt(a->u.while_stmt.body, b->u.while_stmt.body);
    case STMT_DO_WHILE:
        if (!compare_stmt(a->u.do_while.body, b->u.do_while.body))
            return false;
        return compare_expr(a->u.do_while.condition, b->u.do_while.condition);
    case STMT_FOR:
        if (!compare_for_init(a->u.for_stmt.init, b->u.for_stmt.init))
            return false;
        if (!compare_expr(a->u.for_stmt.condition, b->u.for_stmt.condition))
            return false;
        if (!compare_expr(a->u.for_stmt.update, b->u.for_stmt.update))
            return false;
        return compare_stmt(a->u.for_stmt.body, b->u.for_stmt.body);
    case STMT_GOTO:
        return compare_ident(a->u.goto_label, b->u.goto_label);
    case STMT_CONTINUE:
    case STMT_BREAK:
        return true;
    case STMT_RETURN:
        return compare_expr(a->u.expr, b->u.expr);
    case STMT_LABELED:
        if (!compare_ident(a->u.labeled.label, b->u.labeled.label))
            return false;
        return compare_stmt(a->u.labeled.stmt, b->u.labeled.stmt);
    case STMT_CASE:
        if (!compare_expr(a->u.case_stmt.expr, b->u.case_stmt.expr))
            return false;
        return compare_stmt(a->u.case_stmt.stmt, b->u.case_stmt.stmt);
    case STMT_DEFAULT:
        return compare_stmt(a->u.default_stmt, b->u.default_stmt);
    }
    return true;
}

bool compare_decl_or_stmt(const DeclOrStmt *a, const DeclOrStmt *b)
{
    while (a && b) {
        if (a->kind != b->kind)
            return false;
        switch (a->kind) {
        case DECL_OR_STMT_DECL:
            if (!compare_declaration(a->u.decl, b->u.decl))
                return false;
            break;
        case DECL_OR_STMT_STMT:
            if (!compare_stmt(a->u.stmt, b->u.stmt))
                return false;
            break;
        }
        a = a->next;
        b = b->next;
    }
    return a == NULL && b == NULL;
}

bool compare_for_init(const ForInit *a, const ForInit *b)
{
    if (!a && !b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    switch (a->kind) {
    case FOR_INIT_EXPR:
        return compare_expr(a->u.expr, b->u.expr);
    case FOR_INIT_DECL:
        return compare_declaration(a->u.decl, b->u.decl);
    }
    return true;
}

bool compare_external_decl(const ExternalDecl *a, const ExternalDecl *b)
{
    while (a && b) {
        if (a->kind != b->kind)
            return false;
        switch (a->kind) {
        case EXTERNAL_DECL_FUNCTION:
            if (!compare_type(a->u.function.type, b->u.function.type))
                return false;
            if (!compare_ident(a->u.function.name, b->u.function.name))
                return false;
            if (!compare_decl_spec(a->u.function.specifiers, b->u.function.specifiers))
                return false;
            if (!compare_declaration(a->u.function.param_decls, b->u.function.param_decls))
                return false;
            if (!compare_stmt(a->u.function.body, b->u.function.body))
                return false;
            break;
        case EXTERNAL_DECL_DECLARATION:
            if (!compare_declaration(a->u.declaration, b->u.declaration))
                return false;
            break;
        }
        a = a->next;
        b = b->next;
    }
    return a == NULL && b == NULL;
}

bool compare_program(const Program *a, const Program *b)
{
    if (!a && !b)
        return true;
    if (!a || !b)
        return false;
    return compare_external_decl(a->decls, b->decls);
}
