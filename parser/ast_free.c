#include <stdlib.h>

#include "ast.h"

// Forward declarations
static void free_expr(Expr *expr);
static void free_stmt(Stmt *stmt);
static void free_declaration(Declaration *decl);
static void free_external_decl(ExternalDecl *ext);

// Free Type
static void free_type(Type *type)
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

// Free Expr
static void free_expr(Expr *expr)
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
        free_expr(expr->u.binary_op.left);
        free_expr(expr->u.binary_op.right);
        break;
    case EXPR_UNARY_OP:
        free(expr->u.unary_op.op);
        free_expr(expr->u.unary_op.expr);
        break;
    case EXPR_POST_INC:
    case EXPR_POST_DEC:
        free_expr(expr->u.post_inc);
        break;
    case EXPR_CALL:
        free_expr(expr->u.call.func);
        Expr *arg = expr->u.call.args;
        while (arg) {
            Expr *next = arg->next;
            free_expr(arg);
            arg = next;
        }
        break;
    case EXPR_CAST:
        free_type(expr->u.cast.type);
        free_expr(expr->u.cast.expr);
        break;
    case EXPR_COMPOUND:
        Initializer *init = expr->u.compound;
        while (init) {
            Initializer *next = init->next;
            if (init->kind == INITIALIZER_SINGLE) {
                free_expr(init->u.expr);
            }
            free(init);
            init = next;
        }
        break;
    case EXPR_SIZEOF_EXPR:
        free_expr(expr->u.sizeof_expr);
        break;
    case EXPR_SIZEOF_TYPE:
        free_type(expr->u.sizeof_type);
        break;
    case EXPR_ALIGNOF:
        free_type(expr->u.alignof);
        break;
    case EXPR_GENERIC:
        free_expr(expr->u.generic.control);
        GenericAssoc *assoc = expr->u.generic.assocs;
        while (assoc) {
            GenericAssoc *next = assoc->next;
            free_type(assoc->type);
            free_expr(assoc->expr);
            free(assoc);
            assoc = next;
        }
        break;
    }
    Expr *next = expr->next;
    free(expr);
    free_expr(next);
}

// Free Declarator
static void free_declarator(Declarator *decl)
{
    if (!decl)
        return;
    if (decl->kind == DECLARATOR_NAMED) {
        if (decl->u.named.name)
            free(decl->u.named.name);
        DeclSuffix *suffix = decl->u.named.suffixes;
        while (suffix) {
            DeclSuffix *next = suffix->next;
            if (suffix->kind == SUFFIX_ARRAY) {
                free_expr(suffix->u.array.size);
            } else if (suffix->kind == SUFFIX_FUNCTION) {
                ParamList *params = suffix->u.function.params;
                if (!params->is_empty) {
                    Param *param = params->u.params;
                    while (param) {
                        Param *next_param = param->next;
                        free_type(param->type);
                        if (param->name)
                            free(param->name);
                        free(param);
                        param = next_param;
                    }
                }
                free(params);
            }
            free(suffix);
            suffix = next;
        }
    }
    free(decl);
}

// Free Initializer
static void free_initializer(Initializer *init)
{
    if (!init)
        return;
    if (init->kind == INITIALIZER_SINGLE) {
        free_expr(init->u.expr);
    }
    free(init);
}

// Free DeclSpec
static void free_decl_spec(DeclSpec *spec)
{
    if (!spec)
        return;
    if (spec->storage)
        free(spec->storage);
    if (spec->type_specs) {
        switch (spec->type_specs->kind) {
        case TYPE_SPEC_BASIC:
            free_type(spec->type_specs->u.basic);
            break;
        case TYPE_SPEC_STRUCT:
        case TYPE_SPEC_UNION:
            if (spec->type_specs->u.struct_spec.name)
                free(spec->type_specs->u.struct_spec.name);
            StructField *field = spec->type_specs->u.struct_spec.fields;
            while (field) {
                StructField *next = field->next;
                if (!field->is_anonymous) {
                    if (field->u.named.name)
                        free(field->u.named.name);
                    free_type(field->u.named.type);
                }
                free(field);
                field = next;
            }
            break;
        case TYPE_SPEC_ENUM:
            if (spec->type_specs->u.enum_spec.name)
                free(spec->type_specs->u.enum_spec.name);
            Enumerator *e = spec->type_specs->u.enum_spec.enumerators;
            while (e) {
                Enumerator *next = e->next;
                if (e->name)
                    free(e->name);
                free_expr(e->value);
                free(e);
                e = next;
            }
            break;
        case TYPE_SPEC_TYPEDEF_NAME:
            if (spec->type_specs->u.typedef_name)
                free(spec->type_specs->u.typedef_name);
            break;
        case TYPE_SPEC_ATOMIC:
            free_type(spec->type_specs->u.atomic->u.atomic.base);
            free(spec->type_specs->u.atomic);
            break;
        }
        free(spec->type_specs);
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
            free_expr(spec->align_spec->u.expr);
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
        free_expr(decl->u.static_assert.condition);
        if (decl->u.static_assert.message)
            free(decl->u.static_assert.message);
        break;
    case DECL_EMPTY:
        free_decl_spec(decl->u.var.specifiers);
        break;
    }
    free(decl);
}

// Free Stmt
static void free_stmt(Stmt *stmt)
{
    if (!stmt)
        return;
    switch (stmt->kind) {
    case STMT_EXPR:
        free_expr(stmt->u.expr);
        break;
    case STMT_IF:
        free_expr(stmt->u.if_stmt.condition);
        free_stmt(stmt->u.if_stmt.then_stmt);
        free_stmt(stmt->u.if_stmt.else_stmt);
        break;
    case STMT_SWITCH:
        free_expr(stmt->u.switch_stmt.expr);
        free_stmt(stmt->u.switch_stmt.body);
        break;
    case STMT_WHILE:
        free_expr(stmt->u.while_stmt.condition);
        free_stmt(stmt->u.while_stmt.body);
        break;
    case STMT_DO_WHILE:
        free_stmt(stmt->u.do_while.body);
        free_expr(stmt->u.do_while.condition);
        break;
    case STMT_FOR:
        if (stmt->u.for_stmt.init) {
            if (stmt->u.for_stmt.init->kind == FOR_INIT_EXPR) {
                free_expr(stmt->u.for_stmt.init->u.expr);
            } else {
                free_declaration(stmt->u.for_stmt.init->u.decl);
            }
            free(stmt->u.for_stmt.init);
        }
        free_expr(stmt->u.for_stmt.condition);
        free_expr(stmt->u.for_stmt.update);
        free_stmt(stmt->u.for_stmt.body);
        break;
    case STMT_GOTO:
        if (stmt->u.goto_label)
            free(stmt->u.goto_label);
        break;
    case STMT_CONTINUE:
    case STMT_BREAK:
        break;
    case STMT_RETURN:
        free_expr(stmt->u.expr);
        break;
    case STMT_LABELED:
        if (stmt->u.labeled.label)
            free(stmt->u.labeled.label);
        free_stmt(stmt->u.labeled.stmt);
        break;
    case STMT_CASE:
        free_expr(stmt->u.case_stmt.expr);
        free_stmt(stmt->u.case_stmt.stmt);
        break;
    case STMT_DEFAULT:
        free_stmt(stmt->u.default_stmt);
        break;
    case STMT_COMPOUND:
        DeclOrStmt *item = stmt->u.compound;
        while (item) {
            DeclOrStmt *next = item->next;
            if (item->kind == DECL_OR_STMT_DECL) {
                free_declaration(item->u.decl);
            } else {
                free_stmt(item->u.stmt);
            }
            free(item);
            item = next;
        }
        break;
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
        free_stmt(ext->u.function.body);
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
void free_ast(Program *program)
{
    if (!program)
        return;
    free_external_decl(program->decls);
    free(program);
}
