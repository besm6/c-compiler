#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ast.h"
#include "internal.h"
#include "tags.h"
#include "wio.h"

int import_debug; // Enable manually for debug

Type *import_type(WFILE *input);
TypeQualifier *import_type_qualifier(WFILE *input);
Field *import_field(WFILE *input);
Enumerator *import_enumerator(WFILE *input);
Param *import_param(WFILE *input);
Declaration *import_declaration(WFILE *input);
DeclSpec *import_decl_spec(WFILE *input);
FunctionSpec *import_function_spec(WFILE *input);
AlignmentSpec *import_alignment_spec(WFILE *input);
InitDeclarator *import_init_declarator(WFILE *input);
Initializer *import_initializer(WFILE *input);
InitItem *import_init_item(WFILE *input);
Designator *import_designator(WFILE *input);
Expr *import_expr(WFILE *input);
Literal *import_literal(WFILE *input);
GenericAssoc *import_generic_assoc(WFILE *input);
Stmt *import_stmt(WFILE *input);
DeclOrStmt *import_decl_or_stmt(WFILE *input);
ForInit *import_for_init(WFILE *input);
ExternalDecl *import_external_decl(WFILE *input);

static void check_input(const WFILE *input, const char *context)
{
    if (weof(input)) {
        fprintf(stderr, "Error: Premature EOF while reading %s\n", context);
        exit(1);
    }
    if (werror(input)) {
        fprintf(stderr, "Error: Input error while reading %s\n", context);
        exit(1);
    }
}

void ast_import_open(WFILE *input, int fileno)
{
    if (wdopen(input, fileno, "r") < 0) {
        fprintf(stderr, "Error importing AST: cannot open file descriptor #%d\n", fileno);
        exit(1);
    }
    lseek(fileno, 0L, SEEK_SET);
    size_t tag = wgetw(input);
    check_input(input, "program tag");
    if (tag != TAG_PROGRAM) {
        fprintf(stderr, "Error: Expected TAG_PROGRAM, got 0x%zx\n", tag);
        exit(1);
    }
}

Program *import_ast(int fileno)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    WFILE input;
    ast_import_open(&input, fileno);
    Program *program         = new_program();
    ExternalDecl **next_decl = &program->decls;
    for (;;) {
        *next_decl = import_external_decl(&input);
        if (!*next_decl)
            break;
        next_decl = &(*next_decl)->next;
    }
    wclose(&input);
    return program;
}

Type *import_type(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "type tag");
    if (tag < TAG_TYPE || tag > TAG_TYPE + TYPE_ATOMIC)
        return NULL;
    TypeKind kind = (TypeKind)(tag - TAG_TYPE);
    Type *type    = new_type(kind);
    switch (kind) {
    case TYPE_VOID:
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_LONG_LONG:
    case TYPE_SIGNED:
    case TYPE_UNSIGNED:
        type->u.integer.signedness = (Signedness)wgetw(input);
        check_input(input, "type signedness");
        break;
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
        break;
    case TYPE_COMPLEX:
    case TYPE_IMAGINARY:
        type->u.complex.base = import_type(input);
        break;
    case TYPE_POINTER:
        type->u.pointer.target    = import_type(input);
        TypeQualifier **next_qual = &type->u.pointer.qualifiers;
        for (;;) {
            *next_qual = import_type_qualifier(input);
            if (!*next_qual)
                break;
            next_qual = &(*next_qual)->next;
        }
        break;
    case TYPE_ARRAY:
        type->u.array.element           = import_type(input);
        type->u.array.size              = import_expr(input);
        TypeQualifier **next_qual_array = &type->u.array.qualifiers;
        for (;;) {
            *next_qual_array = import_type_qualifier(input);
            if (!*next_qual_array)
                break;
            next_qual_array = &(*next_qual_array)->next;
        }
        type->u.array.is_static = (bool)wgetw(input);
        check_input(input, "array is_static");
        break;
    case TYPE_FUNCTION:
        type->u.function.return_type = import_type(input);
        Param **next_param           = &type->u.function.params;
        for (;;) {
            *next_param = import_param(input);
            if (!*next_param)
                break;
            next_param = &(*next_param)->next;
        }
        type->u.function.variadic = (bool)wgetw(input);
        check_input(input, "function variadic");
        break;
    case TYPE_STRUCT:
    case TYPE_UNION:
        type->u.struct_t.name = wgetstr(input);
        check_input(input, "struct/union name");
        Field **next_field = &type->u.struct_t.fields;
        for (;;) {
            *next_field = import_field(input);
            if (!*next_field)
                break;
            next_field = &(*next_field)->next;
        }
        break;
    case TYPE_ENUM:
        type->u.enum_t.name = wgetstr(input);
        check_input(input, "enum name");
        Enumerator **next_enum = &type->u.enum_t.enumerators;
        for (;;) {
            *next_enum = import_enumerator(input);
            if (!*next_enum)
                break;
            next_enum = &(*next_enum)->next;
        }
        break;
    case TYPE_TYPEDEF_NAME:
        type->u.typedef_name.name = wgetstr(input);
        check_input(input, "typedef name");
        break;
    case TYPE_ATOMIC:
        type->u.atomic.base = import_type(input);
        break;
    }
    TypeQualifier **next_qual = &type->qualifiers;
    for (;;) {
        *next_qual = import_type_qualifier(input);
        if (!*next_qual)
            break;
        next_qual = &(*next_qual)->next;
    }
    return type;
}

TypeQualifier *import_type_qualifier(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "type qualifier tag");
    if (tag == TAG_EOL)
         return NULL;
    if (tag < TAG_TYPEQUALIFIER || tag > TAG_TYPEQUALIFIER + TYPE_QUALIFIER_ATOMIC) {
        fprintf(stderr, "Error: Expected TAG_TYPEQUALIFIER, got 0x%zx\n", tag);
        exit(1);
    }
    TypeQualifierKind kind = (TypeQualifierKind)(tag - TAG_TYPEQUALIFIER);
    TypeQualifier *qual    = new_type_qualifier(kind);
    return qual;
}

Field *import_field(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "field tag");
    if (tag == TAG_EOL)
         return NULL;
    if (tag != TAG_FIELD) {
        fprintf(stderr, "Error: Expected TAG_FIELD, got 0x%zx\n", tag);
        exit(1);
    }
    Field *field = new_field();
    field->type  = import_type(input);
    field->name  = wgetstr(input);
    check_input(input, "field name");
    field->bitfield = import_expr(input);
    return field;
}

Enumerator *import_enumerator(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "enumerator tag");
    if (tag == TAG_EOL)
         return NULL;
    if (tag != TAG_ENUMERATOR) {
        fprintf(stderr, "Error: Expected TAG_ENUMERATOR, got 0x%zx\n", tag);
        exit(1);
    }
    Ident name = wgetstr(input);
    check_input(input, "enumerator name");
    Expr *value       = import_expr(input);
    Enumerator *enumr = new_enumerator(name, value);
    return enumr;
}

Param *import_param(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "param tag");
    if (tag == TAG_EOL)
         return NULL;
    if (tag != TAG_PARAM) {
        fprintf(stderr, "Error: Expected TAG_PARAM, got 0x%zx\n", tag);
        exit(1);
    }
    Param *param = new_param();
    param->name  = wgetstr(input);
    check_input(input, "param name");
    param->type = import_type(input);
    param->specifiers = import_decl_spec(input);
    return param;
}

Declaration *import_declaration(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "declaration tag");
    if (tag == TAG_EOL)
         return NULL;
    if (tag < TAG_DECLARATION || tag > TAG_DECLARATION + DECL_EMPTY) {
        fprintf(stderr, "Error: Expected TAG_DECLARATION, got 0x%zx\n", tag);
        exit(1);
    }
    DeclarationKind kind = (DeclarationKind)(tag - TAG_DECLARATION);
    Declaration *decl    = new_declaration(kind);
    switch (kind) {
    case DECL_VAR:
        decl->u.var.specifiers      = import_decl_spec(input);
        InitDeclarator **next_idecl = &decl->u.var.declarators;
        for (;;) {
            *next_idecl = import_init_declarator(input);
            if (!*next_idecl)
                break;
            next_idecl = &(*next_idecl)->next;
        }
        break;
    case DECL_STATIC_ASSERT:
        decl->u.static_assrt.condition = import_expr(input);
        decl->u.static_assrt.message   = wgetstr(input);
        check_input(input, "static assert message");
        break;
    case DECL_EMPTY:
        decl->u.empty.specifiers = import_decl_spec(input);
        decl->u.empty.type       = import_type(input);
        break;
    }
    return decl;
}

DeclSpec *import_decl_spec(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "decl spec tag");
    if (tag == TAG_EOL)
        return NULL;
    if (tag != TAG_DECLSPEC) {
        fprintf(stderr, "Error: Expected TAG_DECLSPEC, got 0x%zx\n", tag);
        exit(1);
    }
    DeclSpec *spec            = new_decl_spec();
    TypeQualifier **next_qual = &spec->qualifiers;
    for (;;) {
        *next_qual = import_type_qualifier(input);
        if (!*next_qual)
            break;
        next_qual = &(*next_qual)->next;
    }
    spec->storage             = wgetw(input);
    FunctionSpec **next_fspec = &spec->func_specs;
    for (;;) {
        *next_fspec = import_function_spec(input);
        if (!*next_fspec)
            break;
        next_fspec = &(*next_fspec)->next;
    }
    spec->align_spec = import_alignment_spec(input);
    return spec;
}

FunctionSpec *import_function_spec(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "function spec tag");
    if (tag == TAG_EOL)
        return NULL;
    if (tag < TAG_FUNCTIONSPEC || tag > TAG_FUNCTIONSPEC + FUNC_SPEC_NORETURN) {
        fprintf(stderr, "Error: Expected TAG_FUNCTIONSPEC, got 0x%zx\n", tag);
        exit(1);
    }
    FunctionSpecKind kind = (FunctionSpecKind)(tag - TAG_FUNCTIONSPEC);
    FunctionSpec *fspec   = new_function_spec(kind);
    return fspec;
}

AlignmentSpec *import_alignment_spec(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "alignment spec tag");
    if (tag < TAG_ALIGNMENTSPEC || tag > TAG_ALIGNMENTSPEC + ALIGN_SPEC_EXPR)
        return NULL;
    AlignmentSpecKind kind = (AlignmentSpecKind)(tag - TAG_ALIGNMENTSPEC);
    AlignmentSpec *aspec   = new_alignment_spec(kind);
    switch (kind) {
    case ALIGN_SPEC_TYPE:
        aspec->u.type = import_type(input);
        break;
    case ALIGN_SPEC_EXPR:
        aspec->u.expr = import_expr(input);
        break;
    }
    return aspec;
}

InitDeclarator *import_init_declarator(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "init declarator tag");
    if (tag == TAG_EOL)
        return NULL;
    if (tag != TAG_INITDECLARATOR) {
        fprintf(stderr, "Error: Expected TAG_INITDECLARATOR, got 0x%zx\n", tag);
        exit(1);
    }
    InitDeclarator *idecl = new_init_declarator();
    idecl->type           = import_type(input);
    idecl->name           = wgetstr(input);
    check_input(input, "init declarator name");
    idecl->init = import_initializer(input);
    return idecl;
}

Initializer *import_initializer(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "initializer tag");
    if (tag < TAG_INITIALIZER || tag > TAG_INITIALIZER + INITIALIZER_COMPOUND)
        return NULL;
    InitializerKind kind = (InitializerKind)(tag - TAG_INITIALIZER);
    Initializer *init    = new_initializer(kind);
    switch (kind) {
    case INITIALIZER_SINGLE:
        init->u.expr = import_expr(input);
        break;
    case INITIALIZER_COMPOUND: {
        InitItem **next_item = &init->u.items;
        for (;;) {
            *next_item = import_init_item(input);
            if (!*next_item)
                break;
            next_item = &(*next_item)->next;
        }
        break;
    }
    }
    return init;
}

InitItem *import_init_item(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "init item tag");
    if (tag == TAG_EOL)
        return NULL;
    if (tag != TAG_INITITEM) {
        fprintf(stderr, "Error: Expected TAG_INITITEM, got 0x%zx\n", tag);
        exit(1);
    }
    Designator *designators = NULL;
    Designator **next_desg  = &designators;
    for (;;) {
        *next_desg = import_designator(input);
        if (!*next_desg)
            break;
        next_desg = &(*next_desg)->next;
    }
    Initializer *init = import_initializer(input);
    InitItem *item    = new_init_item(designators, init);
    return item;
}

Designator *import_designator(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "designator tag");
    if (tag == TAG_EOL)
        return NULL;
    if (tag < TAG_DESIGNATOR || tag > TAG_DESIGNATOR + DESIGNATOR_FIELD) {
        fprintf(stderr, "Error: Expected TAG_DESIGNATOR, got 0x%zx\n", tag);
        exit(1);
    }
    DesignatorKind kind = (DesignatorKind)(tag - TAG_DESIGNATOR);
    Designator *desg    = new_designator(kind);
    switch (kind) {
    case DESIGNATOR_ARRAY:
        desg->u.expr = import_expr(input);
        break;
    case DESIGNATOR_FIELD:
        desg->u.name = wgetstr(input);
        check_input(input, "designator field name");
        break;
    }
    return desg;
}

Expr *import_expr(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "expr tag");
    if (tag == TAG_EOL)
        return NULL;
    if (tag < TAG_EXPR || tag > TAG_EXPR + EXPR_GENERIC) {
        fprintf(stderr, "Error: Expected TAG_EXPR, got 0x%zx\n", tag);
        exit(1);
    }
    ExprKind kind = (ExprKind)(tag - TAG_EXPR);
    Expr *expr    = new_expression(kind);
    switch (kind) {
    case EXPR_LITERAL:
        expr->u.literal = import_literal(input);
        break;
    case EXPR_VAR:
        expr->u.var = wgetstr(input);
        check_input(input, "expr var name");
        break;
    case EXPR_UNARY_OP:
        expr->u.unary_op.op   = wgetw(input);
        expr->u.unary_op.expr = import_expr(input);
        break;
    case EXPR_BINARY_OP:
        expr->u.binary_op.op    = wgetw(input);
        expr->u.binary_op.left  = import_expr(input);
        expr->u.binary_op.right = import_expr(input);
        break;
    case EXPR_ASSIGN:
        expr->u.assign.op     = wgetw(input);
        expr->u.assign.target = import_expr(input);
        expr->u.assign.value  = import_expr(input);
        break;
    case EXPR_COND:
        expr->u.cond.condition = import_expr(input);
        expr->u.cond.then_expr = import_expr(input);
        expr->u.cond.else_expr = import_expr(input);
        break;
    case EXPR_CAST:
        expr->u.cast.type = import_type(input);
        expr->u.cast.expr = import_expr(input);
        break;
    case EXPR_CALL:
        expr->u.call.func = import_expr(input);
        Expr **next_arg   = &expr->u.call.args;
        for (;;) {
            *next_arg = import_expr(input);
            if (!*next_arg)
                break;
            next_arg = &(*next_arg)->next;
        }
        break;
    case EXPR_COMPOUND:
        expr->u.compound_literal.type = import_type(input);
        InitItem **next_item          = &expr->u.compound_literal.init;
        for (;;) {
            *next_item = import_init_item(input);
            if (!*next_item)
                break;
            next_item = &(*next_item)->next;
        }
        break;
    case EXPR_FIELD_ACCESS:
        expr->u.field_access.expr  = import_expr(input);
        expr->u.field_access.field = wgetstr(input);
        check_input(input, "field access name");
        break;
    case EXPR_PTR_ACCESS:
        expr->u.ptr_access.expr  = import_expr(input);
        expr->u.ptr_access.field = wgetstr(input);
        check_input(input, "ptr access name");
        break;
    case EXPR_POST_INC:
        expr->u.post_inc = import_expr(input);
        break;
    case EXPR_POST_DEC:
        expr->u.post_dec = import_expr(input);
        break;
    case EXPR_SIZEOF_EXPR:
        expr->u.sizeof_expr = import_expr(input);
        break;
    case EXPR_SIZEOF_TYPE:
        expr->u.sizeof_type = import_type(input);
        break;
    case EXPR_ALIGNOF:
        expr->u.align_of = import_type(input);
        break;
    case EXPR_GENERIC:
        expr->u.generic.controlling_expr = import_expr(input);
        GenericAssoc **next_gasc         = &expr->u.generic.associations;
        for (;;) {
            *next_gasc = import_generic_assoc(input);
            if (!*next_gasc)
                break;
            next_gasc = &(*next_gasc)->next;
        }
        break;
    }
    expr->type = import_type(input);
    return expr;
}

Literal *import_literal(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "literal tag");
    if (tag < TAG_LITERAL || tag > TAG_LITERAL + LITERAL_ENUM)
        return NULL;
    LiteralKind kind = (LiteralKind)(tag - TAG_LITERAL);
    Literal *lit     = new_literal(kind);
    switch (kind) {
    case LITERAL_INT:
        lit->u.int_val = (int)wgetw(input);
        check_input(input, "literal int");
        break;
    case LITERAL_FLOAT:
        lit->u.real_val = wgetd(input);
        check_input(input, "literal float");
        break;
    case LITERAL_CHAR:
        lit->u.char_val = (char)wgetw(input);
        check_input(input, "literal char");
        break;
    case LITERAL_STRING:
        lit->u.string_val = wgetstr(input);
        check_input(input, "literal string");
        break;
    case LITERAL_ENUM:
        lit->u.enum_const = wgetstr(input);
        check_input(input, "literal enum");
        break;
    }
    return lit;
}

GenericAssoc *import_generic_assoc(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "generic assoc tag");
    if (tag == TAG_EOL)
        return NULL;
    if (tag < TAG_GENERICASSOC || tag > TAG_GENERICASSOC + GENERIC_ASSOC_DEFAULT) {
        fprintf(stderr, "Error: Expected TAG_STMT, got 0x%zx\n", tag);
        exit(1);
    }
    GenericAssocKind kind = (GenericAssocKind)(tag - TAG_GENERICASSOC);
    GenericAssoc *gasc    = new_generic_assoc(kind);
    switch (kind) {
    case GENERIC_ASSOC_TYPE:
        gasc->u.type_assoc.type = import_type(input);
        gasc->u.type_assoc.expr = import_expr(input);
        break;
    case GENERIC_ASSOC_DEFAULT:
        gasc->u.default_assoc = import_expr(input);
        break;
    }
    return gasc;
}

Stmt *import_stmt(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "stmt tag");
    if (tag == TAG_EOL)
        return NULL;
    if (tag < TAG_STMT || tag > TAG_STMT + STMT_DEFAULT) {
        fprintf(stderr, "Error: Expected TAG_STMT, got 0x%zx\n", tag);
        exit(1);
    }
    StmtKind kind = (StmtKind)(tag - TAG_STMT);
    Stmt *stmt    = new_stmt(kind);
    switch (kind) {
    case STMT_EXPR:
        stmt->u.expr = import_expr(input);
        break;
    case STMT_COMPOUND: {
        DeclOrStmt **next_dost = &stmt->u.compound;
        for (;;) {
            *next_dost = import_decl_or_stmt(input);
            if (!*next_dost)
                break;
            next_dost = &(*next_dost)->next;
        }
        break;
    }
    case STMT_IF:
        stmt->u.if_stmt.condition = import_expr(input);
        stmt->u.if_stmt.then_stmt = import_stmt(input);
        stmt->u.if_stmt.else_stmt = import_stmt(input);
        break;
    case STMT_SWITCH:
        stmt->u.switch_stmt.expr = import_expr(input);
        stmt->u.switch_stmt.body = import_stmt(input);
        break;
    case STMT_WHILE:
        stmt->u.while_stmt.condition = import_expr(input);
        stmt->u.while_stmt.body      = import_stmt(input);
        break;
    case STMT_DO_WHILE:
        stmt->u.do_while.body      = import_stmt(input);
        stmt->u.do_while.condition = import_expr(input);
        break;
    case STMT_FOR:
        stmt->u.for_stmt.init      = import_for_init(input);
        stmt->u.for_stmt.condition = import_expr(input);
        stmt->u.for_stmt.update    = import_expr(input);
        stmt->u.for_stmt.body      = import_stmt(input);
        break;
    case STMT_GOTO:
        stmt->u.goto_label = wgetstr(input);
        check_input(input, "goto label");
        break;
    case STMT_CONTINUE:
    case STMT_BREAK:
        break;
    case STMT_RETURN:
        stmt->u.expr = import_expr(input);
        break;
    case STMT_LABELED:
        stmt->u.labeled.label = wgetstr(input);
        check_input(input, "labeled stmt label");
        stmt->u.labeled.stmt = import_stmt(input);
        break;
    case STMT_CASE:
        stmt->u.case_stmt.expr = import_expr(input);
        stmt->u.case_stmt.stmt = import_stmt(input);
        break;
    case STMT_DEFAULT:
        stmt->u.default_stmt = import_stmt(input);
        break;
    }
    return stmt;
}

DeclOrStmt *import_decl_or_stmt(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "decl or stmt tag");
    if (tag == TAG_EOL)
        return NULL;
    if (tag < TAG_DECLORSTMT || tag > TAG_DECLORSTMT + DECL_OR_STMT_STMT) {
        fprintf(stderr, "Error: Expected TAG_DECLORSTMT, got 0x%zx\n", tag);
        exit(1);
    }
    DeclOrStmtKind kind = (DeclOrStmtKind)(tag - TAG_DECLORSTMT);
    DeclOrStmt *dost    = new_decl_or_stmt(kind);
    switch (kind) {
    case DECL_OR_STMT_DECL:
        dost->u.decl = import_declaration(input);
        break;
    case DECL_OR_STMT_STMT:
        dost->u.stmt = import_stmt(input);
        break;
    }
    return dost;
}

ForInit *import_for_init(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "for init tag");
    if (tag < TAG_FORINIT || tag > TAG_FORINIT + FOR_INIT_DECL)
        return NULL;
    ForInitKind kind = (ForInitKind)(tag - TAG_FORINIT);
    ForInit *finit   = new_for_init(kind);
    switch (kind) {
    case FOR_INIT_EXPR:
        finit->u.expr = import_expr(input);
        break;
    case FOR_INIT_DECL:
        finit->u.decl = import_declaration(input);
        break;
    }
    return finit;
}

ExternalDecl *import_external_decl(WFILE *input)
{
    if (import_debug) {
        printf("--- %s()\n", __func__);
    }
    size_t tag = wgetw(input);
    check_input(input, "external decl tag");
    if (tag < TAG_EXTERNALDECL || tag > TAG_EXTERNALDECL + EXTERNAL_DECL_DECLARATION)
        return NULL;
    ExternalDeclKind kind = (ExternalDeclKind)(tag - TAG_EXTERNALDECL);
    ExternalDecl *exdecl  = new_external_decl(kind);
    switch (kind) {
    case EXTERNAL_DECL_FUNCTION:
        exdecl->u.function.type = import_type(input);
        exdecl->u.function.name = wgetstr(input);
        check_input(input, "function name");
        exdecl->u.function.specifiers = import_decl_spec(input);
        Declaration **next_decl       = &exdecl->u.function.param_decls;
        for (;;) {
            *next_decl = import_declaration(input);
            if (!*next_decl)
                break;
            next_decl = &(*next_decl)->next;
        }
        exdecl->u.function.body = import_stmt(input);
        break;
    case EXTERNAL_DECL_DECLARATION:
        exdecl->u.declaration = import_declaration(input);
        break;
    }
    return exdecl;
}
