#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "internal.h"
#include "xalloc.h"

/* Forward declarations for recursive clone functions */
Type *clone_type(const Type *type);
TypeQualifier *clone_type_qualifier(const TypeQualifier *qual);
Field *clone_field(const Field *field);
Enumerator *clone_enumerator(const Enumerator *enumerator);
Param *clone_param(const Param *param);
Declaration *clone_declaration(const Declaration *decl);
DeclSpec *clone_decl_spec(const DeclSpec *spec);
StorageClass *clone_storage_class(const StorageClass *sc);
FunctionSpec *clone_function_spec(const FunctionSpec *fs);
AlignmentSpec *clone_alignment_spec(const AlignmentSpec *as);
InitDeclarator *clone_init_declarator(const InitDeclarator *init_decl);
Initializer *clone_initializer(const Initializer *init);
InitItem *clone_init_item(const InitItem *item);
Designator *clone_designator(const Designator *design);
Expr *clone_expression(const Expr *expr);
Literal *clone_literal(const Literal *lit);
UnaryOp *clone_unary_op(const UnaryOp *op);
BinaryOp *clone_binary_op(const BinaryOp *op);
AssignOp *clone_assign_op(const AssignOp *op);
GenericAssoc *clone_generic_assoc(const GenericAssoc *assoc);
Stmt *clone_stmt(const Stmt *stmt);
DeclOrStmt *clone_decl_or_stmt(const DeclOrStmt *ds);
ForInit *clone_for_init(const ForInit *fi);
ExternalDecl *clone_external_decl(const ExternalDecl *ext_decl);
TypeSpec *clone_type_spec(const TypeSpec *ts);
Declarator *clone_declarator(const Declarator *decl);
Pointer *clone_pointer(const Pointer *ptr);
DeclaratorSuffix *clone_declarator_suffix(const DeclaratorSuffix *suffix);

Program *clone_program(const Program *program)
{
    if (program == NULL)
        return NULL;
    Program *result = new_program();
    if (result == NULL)
        return NULL;
    result->decls = clone_external_decl(program->decls);
    return result;
}

Type *clone_type(const Type *type)
{
    if (type == NULL)
        return NULL;
    Type *result = new_type(type->kind);
    if (result == NULL)
        return NULL;
    switch (type->kind) {
    case TYPE_COMPLEX:
    case TYPE_IMAGINARY:
        result->u.complex.base = clone_type(type->u.complex.base);
        break;
    case TYPE_POINTER:
        result->u.pointer.target     = clone_type(type->u.pointer.target);
        result->u.pointer.qualifiers = clone_type_qualifier(type->u.pointer.qualifiers);
        break;
    case TYPE_ARRAY:
        result->u.array.element    = clone_type(type->u.array.element);
        result->u.array.size       = clone_expression(type->u.array.size);
        result->u.array.qualifiers = clone_type_qualifier(type->u.array.qualifiers);
        result->u.array.is_static  = type->u.array.is_static;
        break;
    case TYPE_FUNCTION:
        result->u.function.return_type = clone_type(type->u.function.return_type);
        result->u.function.params      = clone_param(type->u.function.params);
        result->u.function.variadic    = type->u.function.variadic;
        break;
    case TYPE_STRUCT:
    case TYPE_UNION:
        result->u.struct_t.name   = type->u.struct_t.name ? xstrdup(type->u.struct_t.name) : NULL;
        result->u.struct_t.fields = clone_field(type->u.struct_t.fields);
        break;
    case TYPE_ENUM:
        result->u.enum_t.name        = type->u.enum_t.name ? xstrdup(type->u.enum_t.name) : NULL;
        result->u.enum_t.enumerators = clone_enumerator(type->u.enum_t.enumerators);
        break;
    case TYPE_TYPEDEF_NAME:
        result->u.typedef_name.name =
            type->u.typedef_name.name ? xstrdup(type->u.typedef_name.name) : NULL;
        break;
    case TYPE_ATOMIC:
        result->u.atomic.base = clone_type(type->u.atomic.base);
        break;
    case TYPE_SIGNED:
    case TYPE_UNSIGNED:
        result->u.integer.signedness = type->u.integer.signedness;
        break;
    default:
        break; /* No nested fields for basic types */
    }
    result->qualifiers = clone_type_qualifier(type->qualifiers);
    return result;
}

TypeQualifier *clone_type_qualifier(const TypeQualifier *qual)
{
    if (qual == NULL)
        return NULL;
    TypeQualifier *result = new_type_qualifier(qual->kind);
    if (result == NULL)
        return NULL;
    result->next = clone_type_qualifier(qual->next);
    return result;
}

Field *clone_field(const Field *field)
{
    if (field == NULL)
        return NULL;
    Field *result = new_field();
    if (result == NULL)
        return NULL;
    result->type     = clone_type(field->type);
    result->name     = field->name ? xstrdup(field->name) : NULL;
    result->bitfield = clone_expression(field->bitfield);
    result->next     = clone_field(field->next);
    return result;
}

Enumerator *clone_enumerator(const Enumerator *enumerator)
{
    if (enumerator == NULL)
        return NULL;
    Enumerator *result = new_enumerator(enumerator->name ? xstrdup(enumerator->name) : NULL,
                                        clone_expression(enumerator->value));
    if (result == NULL)
        return NULL;
    result->next = clone_enumerator(enumerator->next);
    return result;
}

Param *clone_param(const Param *param)
{
    if (param == NULL)
        return NULL;
    Param *result = new_param();
    if (result == NULL)
        return NULL;
    result->name = param->name ? xstrdup(param->name) : NULL;
    result->type = clone_type(param->type);
    result->next = clone_param(param->next);
    return result;
}

Declaration *clone_declaration(const Declaration *decl)
{
    if (decl == NULL)
        return NULL;
    Declaration *result = new_declaration(decl->kind);
    if (result == NULL)
        return NULL;
    switch (decl->kind) {
    case DECL_VAR:
        result->u.var.specifiers  = clone_decl_spec(decl->u.var.specifiers);
        result->u.var.declarators = clone_init_declarator(decl->u.var.declarators);
        break;
    case DECL_STATIC_ASSERT:
        result->u.static_assrt.condition = clone_expression(decl->u.static_assrt.condition);
        result->u.static_assrt.message =
            decl->u.static_assrt.message ? xstrdup(decl->u.static_assrt.message) : NULL;
        break;
    case DECL_EMPTY:
        result->u.empty.specifiers = clone_decl_spec(decl->u.empty.specifiers);
        result->u.empty.type       = clone_type(decl->u.empty.type);
        break;
    }
    result->next = clone_declaration(decl->next);
    return result;
}

DeclSpec *clone_decl_spec(const DeclSpec *spec)
{
    if (spec == NULL)
        return NULL;
    DeclSpec *result = new_decl_spec();
    if (result == NULL)
        return NULL;
    result->qualifiers = clone_type_qualifier(spec->qualifiers);
    result->storage    = clone_storage_class(spec->storage);
    result->func_specs = clone_function_spec(spec->func_specs);
    result->align_spec = clone_alignment_spec(spec->align_spec);
    return result;
}

StorageClass *clone_storage_class(const StorageClass *sc)
{
    if (sc == NULL)
        return NULL;
    StorageClass *result = new_storage_class(sc->kind);
    return result;
}

FunctionSpec *clone_function_spec(const FunctionSpec *fs)
{
    if (fs == NULL)
        return NULL;
    FunctionSpec *result = new_function_spec(fs->kind);
    if (result == NULL)
        return NULL;
    result->next = clone_function_spec(fs->next);
    return result;
}

AlignmentSpec *clone_alignment_spec(const AlignmentSpec *as)
{
    if (as == NULL)
        return NULL;
    AlignmentSpec *result = new_alignment_spec(as->kind);
    if (result == NULL)
        return NULL;
    if (as->kind == ALIGN_SPEC_TYPE) {
        result->u.type = clone_type(as->u.type);
    } else {
        result->u.expr = clone_expression(as->u.expr);
    }
    return result;
}

InitDeclarator *clone_init_declarator(const InitDeclarator *init_decl)
{
    if (init_decl == NULL)
        return NULL;
    InitDeclarator *result = new_init_declarator();
    if (result == NULL)
        return NULL;
    result->type = clone_type(init_decl->type);
    result->name = init_decl->name ? xstrdup(init_decl->name) : NULL;
    result->init = clone_initializer(init_decl->init);
    result->next = clone_init_declarator(init_decl->next);
    return result;
}

Initializer *clone_initializer(const Initializer *init)
{
    if (init == NULL)
        return NULL;
    Initializer *result = new_initializer(init->kind);
    if (result == NULL)
        return NULL;
    switch (init->kind) {
    case INITIALIZER_SINGLE:
        result->u.expr = clone_expression(init->u.expr);
        break;
    case INITIALIZER_COMPOUND:
        result->u.items = clone_init_item(init->u.items);
        break;
    }
    return result;
}

InitItem *clone_init_item(const InitItem *item)
{
    if (item == NULL)
        return NULL;
    InitItem *result =
        new_init_item(clone_designator(item->designators), clone_initializer(item->init));
    if (result == NULL)
        return NULL;
    result->next = clone_init_item(item->next);
    return result;
}

Designator *clone_designator(const Designator *design)
{
    if (design == NULL)
        return NULL;
    Designator *result = new_designator(design->kind);
    if (result == NULL)
        return NULL;
    if (design->kind == DESIGNATOR_ARRAY) {
        result->u.expr = clone_expression(design->u.expr);
    } else {
        result->u.name = design->u.name ? xstrdup(design->u.name) : NULL;
    }
    result->next = clone_designator(design->next);
    return result;
}

Expr *clone_expression(const Expr *expr)
{
    if (expr == NULL)
        return NULL;
    Expr *result = new_expression(expr->kind);
    if (result == NULL)
        return NULL;
    switch (expr->kind) {
    case EXPR_LITERAL:
        result->u.literal = clone_literal(expr->u.literal);
        break;
    case EXPR_VAR:
        result->u.var = expr->u.var ? xstrdup(expr->u.var) : NULL;
        break;
    case EXPR_UNARY_OP:
        result->u.unary_op.op   = clone_unary_op(expr->u.unary_op.op);
        result->u.unary_op.expr = clone_expression(expr->u.unary_op.expr);
        break;
    case EXPR_BINARY_OP:
        result->u.binary_op.op    = clone_binary_op(expr->u.binary_op.op);
        result->u.binary_op.left  = clone_expression(expr->u.binary_op.left);
        result->u.binary_op.right = clone_expression(expr->u.binary_op.right);
        break;
    case EXPR_ASSIGN:
        result->u.assign.target = clone_expression(expr->u.assign.target);
        result->u.assign.op     = clone_assign_op(expr->u.assign.op);
        result->u.assign.value  = clone_expression(expr->u.assign.value);
        break;
    case EXPR_COND:
        result->u.cond.condition = clone_expression(expr->u.cond.condition);
        result->u.cond.then_expr = clone_expression(expr->u.cond.then_expr);
        result->u.cond.else_expr = clone_expression(expr->u.cond.else_expr);
        break;
    case EXPR_CAST:
        result->u.cast.type = clone_type(expr->u.cast.type);
        result->u.cast.expr = clone_expression(expr->u.cast.expr);
        break;
    case EXPR_CALL:
        result->u.call.func = clone_expression(expr->u.call.func);
        result->u.call.args = clone_expression(expr->u.call.args);
        break;
    case EXPR_COMPOUND:
        result->u.compound_literal.type = clone_type(expr->u.compound_literal.type);
        result->u.compound_literal.init = clone_init_item(expr->u.compound_literal.init);
        break;
    case EXPR_FIELD_ACCESS:
    case EXPR_PTR_ACCESS:
        result->u.field_access.expr = clone_expression(expr->u.field_access.expr);
        result->u.field_access.field =
            expr->u.field_access.field ? xstrdup(expr->u.field_access.field) : NULL;
        break;
    case EXPR_POST_INC:
    case EXPR_POST_DEC:
        result->u.post_inc = clone_expression(expr->u.post_inc);
        break;
    case EXPR_SIZEOF_EXPR:
        result->u.sizeof_expr = clone_expression(expr->u.sizeof_expr);
        break;
    case EXPR_SIZEOF_TYPE:
    case EXPR_ALIGNOF:
        result->u.sizeof_type = clone_type(expr->u.sizeof_type);
        break;
    case EXPR_GENERIC:
        result->u.generic.controlling_expr = clone_expression(expr->u.generic.controlling_expr);
        result->u.generic.associations     = clone_generic_assoc(expr->u.generic.associations);
        break;
    }
    result->type = clone_type(expr->type);
    result->next = clone_expression(expr->next);
    return result;
}

Literal *clone_literal(const Literal *lit)
{
    if (lit == NULL)
        return NULL;
    Literal *result = new_literal(lit->kind);
    if (result == NULL)
        return NULL;
    switch (lit->kind) {
    case LITERAL_INT:
        result->u.int_val = lit->u.int_val;
        break;
    case LITERAL_FLOAT:
        result->u.real_val = lit->u.real_val;
        break;
    case LITERAL_CHAR:
        result->u.char_val = lit->u.char_val;
        break;
    case LITERAL_STRING:
        result->u.string_val = lit->u.string_val ? xstrdup(lit->u.string_val) : NULL;
        break;
    case LITERAL_ENUM:
        result->u.enum_const = lit->u.enum_const ? xstrdup(lit->u.enum_const) : NULL;
        break;
    }
    return result;
}

UnaryOp *clone_unary_op(const UnaryOp *op)
{
    if (op == NULL)
        return NULL;
    UnaryOp *result = new_unary_op(op->kind);
    return result;
}

BinaryOp *clone_binary_op(const BinaryOp *op)
{
    if (op == NULL)
        return NULL;
    BinaryOp *result = new_binary_op(op->kind);
    return result;
}

AssignOp *clone_assign_op(const AssignOp *op)
{
    if (op == NULL)
        return NULL;
    AssignOp *result = new_assign_op(op->kind);
    return result;
}

GenericAssoc *clone_generic_assoc(const GenericAssoc *assoc)
{
    if (assoc == NULL)
        return NULL;
    GenericAssoc *result = new_generic_assoc(assoc->kind);
    if (result == NULL)
        return NULL;
    if (assoc->kind == GENERIC_ASSOC_TYPE) {
        result->u.type_assoc.type = clone_type(assoc->u.type_assoc.type);
        result->u.type_assoc.expr = clone_expression(assoc->u.type_assoc.expr);
    } else {
        result->u.default_assoc = clone_expression(assoc->u.default_assoc);
    }
    result->next = clone_generic_assoc(assoc->next);
    return result;
}

Stmt *clone_stmt(const Stmt *stmt)
{
    if (stmt == NULL)
        return NULL;
    Stmt *result = new_stmt(stmt->kind);
    if (result == NULL)
        return NULL;
    switch (stmt->kind) {
    case STMT_EXPR:
        result->u.expr = clone_expression(stmt->u.expr);
        break;
    case STMT_COMPOUND:
        result->u.compound = clone_decl_or_stmt(stmt->u.compound);
        break;
    case STMT_IF:
        result->u.if_stmt.condition = clone_expression(stmt->u.if_stmt.condition);
        result->u.if_stmt.then_stmt = clone_stmt(stmt->u.if_stmt.then_stmt);
        result->u.if_stmt.else_stmt = clone_stmt(stmt->u.if_stmt.else_stmt);
        break;
    case STMT_SWITCH:
        result->u.switch_stmt.expr = clone_expression(stmt->u.switch_stmt.expr);
        result->u.switch_stmt.body = clone_stmt(stmt->u.switch_stmt.body);
        break;
    case STMT_WHILE:
        result->u.while_stmt.condition = clone_expression(stmt->u.while_stmt.condition);
        result->u.while_stmt.body      = clone_stmt(stmt->u.while_stmt.body);
        break;
    case STMT_DO_WHILE:
        result->u.do_while.body      = clone_stmt(stmt->u.do_while.body);
        result->u.do_while.condition = clone_expression(stmt->u.do_while.condition);
        break;
    case STMT_FOR:
        result->u.for_stmt.init      = clone_for_init(stmt->u.for_stmt.init);
        result->u.for_stmt.condition = clone_expression(stmt->u.for_stmt.condition);
        result->u.for_stmt.update    = clone_expression(stmt->u.for_stmt.update);
        result->u.for_stmt.body      = clone_stmt(stmt->u.for_stmt.body);
        break;
    case STMT_GOTO:
        result->u.goto_label = stmt->u.goto_label ? xstrdup(stmt->u.goto_label) : NULL;
        break;
    case STMT_CONTINUE:
    case STMT_BREAK:
        break; /* No fields to clone */
    case STMT_RETURN:
        result->u.expr = clone_expression(stmt->u.expr);
        break;
    case STMT_LABELED:
        result->u.labeled.label = stmt->u.labeled.label ? xstrdup(stmt->u.labeled.label) : NULL;
        result->u.labeled.stmt  = clone_stmt(stmt->u.labeled.stmt);
        break;
    case STMT_CASE:
        result->u.case_stmt.expr = clone_expression(stmt->u.case_stmt.expr);
        result->u.case_stmt.stmt = clone_stmt(stmt->u.case_stmt.stmt);
        break;
    case STMT_DEFAULT:
        result->u.default_stmt = clone_stmt(stmt->u.default_stmt);
        break;
    }
    return result;
}

DeclOrStmt *clone_decl_or_stmt(const DeclOrStmt *ds)
{
    if (ds == NULL)
        return NULL;
    DeclOrStmt *result = new_decl_or_stmt(ds->kind);
    if (result == NULL)
        return NULL;
    if (ds->kind == DECL_OR_STMT_DECL) {
        result->u.decl = clone_declaration(ds->u.decl);
    } else {
        result->u.stmt = clone_stmt(ds->u.stmt);
    }
    result->next = clone_decl_or_stmt(ds->next);
    return result;
}

ForInit *clone_for_init(const ForInit *fi)
{
    if (fi == NULL)
        return NULL;
    ForInit *result = new_for_init(fi->kind);
    if (result == NULL)
        return NULL;
    if (fi->kind == FOR_INIT_EXPR) {
        result->u.expr = clone_expression(fi->u.expr);
    } else {
        result->u.decl = clone_declaration(fi->u.decl);
    }
    return result;
}

ExternalDecl *clone_external_decl(const ExternalDecl *ext_decl)
{
    if (ext_decl == NULL)
        return NULL;
    ExternalDecl *result = new_external_decl(ext_decl->kind);
    if (result == NULL)
        return NULL;
    if (ext_decl->kind == EXTERNAL_DECL_FUNCTION) {
        result->u.function.type = clone_type(ext_decl->u.function.type);
        result->u.function.name =
            ext_decl->u.function.name ? xstrdup(ext_decl->u.function.name) : NULL;
        result->u.function.specifiers  = clone_decl_spec(ext_decl->u.function.specifiers);
        result->u.function.param_decls = clone_declaration(ext_decl->u.function.param_decls);
        result->u.function.body        = clone_stmt(ext_decl->u.function.body);
    } else {
        result->u.declaration = clone_declaration(ext_decl->u.declaration);
    }
    result->next = clone_external_decl(ext_decl->next);
    return result;
}

TypeSpec *clone_type_spec(const TypeSpec *ts)
{
    if (ts == NULL)
        return NULL;
    TypeSpec *result = new_type_spec(ts->kind);
    if (result == NULL)
        return NULL;
    switch (ts->kind) {
    case TYPE_SPEC_BASIC:
        result->u.basic = clone_type(ts->u.basic);
        break;
    case TYPE_SPEC_STRUCT:
    case TYPE_SPEC_UNION:
        result->u.struct_spec.name = ts->u.struct_spec.name ? xstrdup(ts->u.struct_spec.name) : NULL;
        result->u.struct_spec.fields = clone_field(ts->u.struct_spec.fields);
        break;
    case TYPE_SPEC_ENUM:
        result->u.enum_spec.name = ts->u.enum_spec.name ? xstrdup(ts->u.enum_spec.name) : NULL;
        result->u.enum_spec.enumerators = clone_enumerator(ts->u.enum_spec.enumerators);
        break;
    case TYPE_SPEC_TYPEDEF_NAME:
        result->u.typedef_name.name =
            ts->u.typedef_name.name ? xstrdup(ts->u.typedef_name.name) : NULL;
        break;
    case TYPE_SPEC_ATOMIC:
        result->u.atomic.type = clone_type(ts->u.atomic.type);
        break;
    }
    result->qualifiers = clone_type_qualifier(ts->qualifiers);
    result->next       = clone_type_spec(ts->next);
    return result;
}

Declarator *clone_declarator(const Declarator *decl)
{
    if (decl == NULL)
        return NULL;
    Declarator *result = new_declarator();
    if (result == NULL)
        return NULL;
    result->name     = decl->name ? xstrdup(decl->name) : NULL;
    result->pointers = clone_pointer(decl->pointers);
    result->suffixes = clone_declarator_suffix(decl->suffixes);
    result->next     = clone_declarator(decl->next);
    return result;
}

Pointer *clone_pointer(const Pointer *ptr)
{
    if (ptr == NULL)
        return NULL;
    Pointer *result = new_pointer();
    if (result == NULL)
        return NULL;
    result->qualifiers = clone_type_qualifier(ptr->qualifiers);
    result->next       = clone_pointer(ptr->next);
    return result;
}

DeclaratorSuffix *clone_declarator_suffix(const DeclaratorSuffix *suffix)
{
    if (suffix == NULL)
        return NULL;
    DeclaratorSuffix *result = new_declarator_suffix(suffix->kind);
    if (result == NULL)
        return NULL;
    switch (suffix->kind) {
    case SUFFIX_ARRAY:
        result->u.array.size       = clone_expression(suffix->u.array.size);
        result->u.array.qualifiers = clone_type_qualifier(suffix->u.array.qualifiers);
        result->u.array.is_static  = suffix->u.array.is_static;
        break;
    case SUFFIX_FUNCTION:
        result->u.function.params   = clone_param(suffix->u.function.params);
        result->u.function.variadic = suffix->u.function.variadic;
        break;
    case SUFFIX_POINTER:
        result->u.pointer.pointers = clone_pointer(suffix->u.pointer.pointers);
        result->u.pointer.suffix   = clone_declarator_suffix(suffix->u.pointer.suffix);
        break;
    }
    result->next = clone_declarator_suffix(suffix->next);
    return result;
}
