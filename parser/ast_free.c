#include <stdlib.h>

#include "ast.h"
#include "internal.h"

// Forward declarations
void free_expression(Expr *expr);
void free_statement(Stmt *stmt);
static void free_declaration(Declaration *decl);
static void free_external_decl(ExternalDecl *ext);
static void free_initializer(Initializer *init);

// Free Type
void free_type(Type *type)
{
    if (!type)
        return;
    TypeQualifier *qual = type->qualifiers;
    while (qual) {
        TypeQualifier *next = qual->next;
        free(qual);
        qual = next;
    }
    free(type);
}

// Free Literal
static void free_literal(Literal *lit)
{
    if (!lit)
        return;
    if (lit->kind == LITERAL_STRING && lit->u.string_val) {
        free(lit->u.string_val);
    }
    free(lit);
}

// Free Designator
static void free_designator(Designator *designator)
{
    while (designator) {
        Designator *next = designator->next;
        if (designator->kind == DESIGNATOR_ARRAY) {
            free_expression(designator->u.expr);
        }
        free(designator);
        designator = next;
    }
}

// Free InitItem
static void free_init_item(InitItem *init_item)
{
    while (init_item) {
        InitItem *next = init_item->next;
        free_designator(init_item->designators);
        free_initializer(init_item->init);
        free(init_item);
        init_item = next;
    }
}

// Free Expr
void free_expression(Expr *expr)
{
    if (!expr)
        return;
    switch (expr->kind) {
    case EXPR_VAR:
        if (expr->u.var)
            free(expr->u.var);
        break;
    case EXPR_LITERAL:
        free_literal(expr->u.literal);
        break;
    case EXPR_BINARY_OP:
        free(expr->u.binary_op.op);
        free_expression(expr->u.binary_op.left);
        free_expression(expr->u.binary_op.right);
        break;
    case EXPR_UNARY_OP:
        free(expr->u.unary_op.op);
        free_expression(expr->u.unary_op.expr);
        break;
    case EXPR_POST_INC:
    case EXPR_POST_DEC:
        free_expression(expr->u.post_inc);
        break;
    case EXPR_CALL:
        free_expression(expr->u.call.func);
        free_expression(expr->u.call.args);
        break;
    case EXPR_CAST:
        free_type(expr->u.cast.type);
        free_expression(expr->u.cast.expr);
        break;
    case EXPR_COMPOUND:
        free_type(expr->u.compound_literal.type);
        free_init_item(expr->u.compound_literal.init);
        break;
    case EXPR_SIZEOF_EXPR:
        free_expression(expr->u.sizeof_expr);
        break;
    case EXPR_SIZEOF_TYPE:
        free_type(expr->u.sizeof_type);
        break;
    case EXPR_ALIGNOF:
        free_type(expr->u.align_of);
        break;
    case EXPR_GENERIC:
        free_expression(expr->u.generic.controlling_expr);
        GenericAssoc *assoc = expr->u.generic.associations;
        while (assoc) {
            GenericAssoc *next = assoc->next;
            if (assoc->kind == GENERIC_ASSOC_TYPE) {
                free_type(assoc->u.type_assoc.type);
                free_expression(assoc->u.type_assoc.expr);
            } else {
                free_expression(assoc->u.default_assoc);
            }
            free(assoc);
            assoc = next;
        }
        break;
    case EXPR_ASSIGN:
        free_expression(expr->u.assign.target);
        free_expression(expr->u.assign.value);
        break;
    case EXPR_COND:
        free_expression(expr->u.cond.condition);
        free_expression(expr->u.cond.then_expr);
        free_expression(expr->u.cond.else_expr);
        break;
    case EXPR_FIELD_ACCESS:
        free_expression(expr->u.field_access.expr);
        break;
    case EXPR_PTR_ACCESS:
        free_expression(expr->u.ptr_access.expr);
        break;
    }
    Expr *next = expr->next;
    free(expr);
    free_expression(next);
}

// Free Pointer
void free_pointer(Pointer *pointer)
{
    while (pointer) {
        TypeQualifier *qual = pointer->qualifiers;
        while (qual) {
            TypeQualifier *next = qual->next;
            free(qual);
            qual = next;
        }
        Pointer *next = pointer->next;
        free(pointer);
        pointer = next;
    }
}

// Free DeclaratorSuffix
void free_declarator_suffix(DeclaratorSuffix *suffix)
{
    while (suffix) {
        DeclaratorSuffix *next = suffix->next;
        switch (suffix->kind) {
        case SUFFIX_ARRAY:
            free_expression(suffix->u.array.size);
            break;
        case SUFFIX_FUNCTION: {
            Param *param = suffix->u.function.params;
            while (param) {
                Param *next_param = param->next;
                free_type(param->type);
                if (param->name)
                    free(param->name);
                free(param);
                param = next_param;
            }
            break;
        }
        case SUFFIX_POINTER:
            free_pointer(suffix->u.pointer.pointers);
            free_declarator_suffix(suffix->u.pointer.suffix);
            break;
        }
        free(suffix);
        suffix = next;
    }
}

// Free Declarator
void free_declarator(Declarator *decl)
{
    if (!decl)
        return;
    if (decl->name)
        free(decl->name);
    free_pointer(decl->pointers);
    free_declarator_suffix(decl->suffixes);
    free(decl);
}

// Free Initializer
static void free_initializer(Initializer *init)
{
    if (!init)
        return;
    if (init->kind == INITIALIZER_SINGLE) {
        free_expression(init->u.expr);
    } else {
        free_init_item(init->u.items);
    }
    free(init);
}

// Free TypeSpec
void free_type_spec(TypeSpec *ts)
{
    if (!ts)
        return;
    switch (ts->kind) {
    case TYPE_SPEC_BASIC:
        free_type(ts->u.basic);
        break;
    case TYPE_SPEC_STRUCT:
    case TYPE_SPEC_UNION:
        if (ts->u.struct_spec.name)
            free(ts->u.struct_spec.name);
        Field *field = ts->u.struct_spec.fields;
        while (field) {
            Field *next = field->next;
            free_type(field->type);
            if (field->name) {
                free(field->name);
            }
            free_expression(field->bitfield);
            free(field);
            field = next;
        }
        break;
    case TYPE_SPEC_ENUM:
        if (ts->u.enum_spec.name)
            free(ts->u.enum_spec.name);
        Enumerator *e = ts->u.enum_spec.enumerators;
        while (e) {
            Enumerator *next = e->next;
            if (e->name)
                free(e->name);
            free_expression(e->value);
            free(e);
            e = next;
        }
        break;
    case TYPE_SPEC_TYPEDEF_NAME:
        if (ts->u.typedef_name.name)
            free(ts->u.typedef_name.name);
        break;
    case TYPE_SPEC_ATOMIC:
        free_type(ts->u.atomic.type);
        break;
    }
    free(ts);
}

// Free DeclSpec
static void free_decl_spec(DeclSpec *spec)
{
    if (!spec)
        return;
    if (spec->storage)
        free(spec->storage);
    if (spec->base_type) {
        free_type(spec->base_type);
    }
    TypeQualifier *qual = spec->qualifiers;
    while (qual) {
        TypeQualifier *next = qual->next;
        free(qual);
        qual = next;
    }
    if (spec->func_specs)
        free(spec->func_specs);
    if (spec->align_spec) {
        if (spec->align_spec->kind == ALIGN_SPEC_TYPE) {
            free_type(spec->align_spec->u.type);
        } else {
            free_expression(spec->align_spec->u.expr);
        }
        free(spec->align_spec);
    }
    free(spec);
}

// Free InitDeclarator
static void free_init_declarator(InitDeclarator *id)
{
    if (!id)
        return;
    free_declarator(id->declarator);
    free_initializer(id->init);
    InitDeclarator *next = id->next;
    free(id);
    free_init_declarator(next);
}

// Free Declaration
static void free_declaration(Declaration *decl)
{
    if (!decl)
        return;
    switch (decl->kind) {
    case DECL_VAR:
        free_decl_spec(decl->u.var.specifiers);
        free_init_declarator(decl->u.var.declarators);
        break;
    case DECL_STATIC_ASSERT:
        free_expression(decl->u.static_assrt.condition);
        if (decl->u.static_assrt.message)
            free(decl->u.static_assrt.message);
        break;
    case DECL_EMPTY:
        free_decl_spec(decl->u.var.specifiers);
        break;
    }
    free(decl);
}

// Free Stmt
void free_statement(Stmt *stmt)
{
    if (!stmt)
        return;
    switch (stmt->kind) {
    case STMT_EXPR:
        free_expression(stmt->u.expr);
        break;
    case STMT_IF:
        free_expression(stmt->u.if_stmt.condition);
        free_statement(stmt->u.if_stmt.then_stmt);
        free_statement(stmt->u.if_stmt.else_stmt);
        break;
    case STMT_SWITCH:
        free_expression(stmt->u.switch_stmt.expr);
        free_statement(stmt->u.switch_stmt.body);
        break;
    case STMT_WHILE:
        free_expression(stmt->u.while_stmt.condition);
        free_statement(stmt->u.while_stmt.body);
        break;
    case STMT_DO_WHILE:
        free_statement(stmt->u.do_while.body);
        free_expression(stmt->u.do_while.condition);
        break;
    case STMT_FOR:
        if (stmt->u.for_stmt.init) {
            if (stmt->u.for_stmt.init->kind == FOR_INIT_EXPR) {
                free_expression(stmt->u.for_stmt.init->u.expr);
            } else {
                free_declaration(stmt->u.for_stmt.init->u.decl);
            }
            free(stmt->u.for_stmt.init);
        }
        free_expression(stmt->u.for_stmt.condition);
        free_expression(stmt->u.for_stmt.update);
        free_statement(stmt->u.for_stmt.body);
        break;
    case STMT_GOTO:
        if (stmt->u.goto_label)
            free(stmt->u.goto_label);
        break;
    case STMT_CONTINUE:
    case STMT_BREAK:
        break;
    case STMT_RETURN:
        free_expression(stmt->u.expr);
        break;
    case STMT_LABELED:
        if (stmt->u.labeled.label)
            free(stmt->u.labeled.label);
        free_statement(stmt->u.labeled.stmt);
        break;
    case STMT_CASE:
        free_expression(stmt->u.case_stmt.expr);
        free_statement(stmt->u.case_stmt.stmt);
        break;
    case STMT_DEFAULT:
        free_statement(stmt->u.default_stmt);
        break;
    case STMT_COMPOUND: {
        DeclOrStmt *item = stmt->u.compound;
        while (item) {
            DeclOrStmt *next = item->next;
            if (item->kind == DECL_OR_STMT_DECL) {
                free_declaration(item->u.decl);
            } else {
                free_statement(item->u.stmt);
            }
            free(item);
            item = next;
        }
        break;
    }
    }
    free(stmt);
}

// Free ExternalDecl
static void free_external_decl(ExternalDecl *ext)
{
    if (!ext)
        return;
    switch (ext->kind) {
    case EXTERNAL_DECL_FUNCTION:
        free_decl_spec(ext->u.function.specifiers);
        free_declarator(ext->u.function.declarator);
        free_statement(ext->u.function.body);
        break;
    case EXTERNAL_DECL_DECLARATION:
        free_declaration(ext->u.declaration);
        break;
    }
    ExternalDecl *next = ext->next;
    free(ext);
    free_external_decl(next);
}

// Main free function
void free_program(Program *program)
{
    if (!program)
        return;
    free_external_decl(program->decls);
    free(program);
}
