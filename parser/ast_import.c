#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "internal.h"
#include "tags.h"
#include "wio.h"

Type *import_type(WFILE *input);
TypeQualifier *import_type_qualifier(WFILE *input);
Field *import_field(WFILE *input);
Enumerator *import_enumerator(WFILE *input);
Param *import_param(WFILE *input);
Declaration *import_declaration(WFILE *input);
DeclSpec *import_decl_spec(WFILE *input);
StorageClass *import_storage_class(WFILE *input);
FunctionSpec *import_function_spec(WFILE *input);
AlignmentSpec *import_alignment_spec(WFILE *input);
InitDeclarator *import_init_declarator(WFILE *input);
Initializer *import_initializer(WFILE *input);
InitItem *import_init_item(WFILE *input);
Designator *import_designator(WFILE *input);
Expr *import_expr(WFILE *input);
Literal *import_literal(WFILE *input);
UnaryOp *import_unary_op(WFILE *input);
BinaryOp *import_binary_op(WFILE *input);
AssignOp *import_assign_op(WFILE *input);
GenericAssoc *import_generic_assoc(WFILE *input);
Stmt *import_stmt(WFILE *input);
DeclOrStmt *import_decl_or_stmt(WFILE *input);
ForInit *import_for_init(WFILE *input);
ExternalDecl *import_external_decl(WFILE *input);

static void check_input(WFILE *input, const char *context)
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

Program *import_ast(int fileno)
{
    WFILE *input = wdopen(fileno, "r");
    if (!input) {
        fprintf(stderr, "Error importing AST: cannot open file descriptor #%d\n", fileno);
        exit(1);
    }
    size_t tag = wgetw(input);
    check_input(input, "program tag");
    if (tag != TAG_PROGRAM) {
        fprintf(stderr, "Error: Expected TAG_PROGRAM, got 0x%zx\n", tag);
        exit(1);
    }
    Program *program         = new_program();
    ExternalDecl **next_decl = &program->decls;
    while ((tag = wgetw(input)) != TAG_EOL) {
        check_input(input, "external decl tag");
        *next_decl = import_external_decl(input);
        if (*next_decl)
            next_decl = &(*next_decl)->next;
    }
    check_input(input, "external decl list end");
    wclose(input);
    return program;
}

Type *import_type(WFILE *input)
{
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
        while ((tag = wgetw(input)) != TAG_EOL) {
            check_input(input, "pointer qualifier tag");
            *next_qual = import_type_qualifier(input);
            if (*next_qual)
                next_qual = &(*next_qual)->next;
        }
        check_input(input, "pointer qualifier list end");
        break;
    case TYPE_ARRAY:
        type->u.array.element           = import_type(input);
        type->u.array.size              = import_expr(input);
        TypeQualifier **next_qual_array = &type->u.array.qualifiers;
        while ((tag = wgetw(input)) != TAG_EOL) {
            check_input(input, "array qualifier tag");
            *next_qual_array = import_type_qualifier(input);
            if (*next_qual_array)
                next_qual_array = &(*next_qual_array)->next;
        }
        check_input(input, "array qualifier list end");
        type->u.array.is_static = (bool)wgetw(input);
        check_input(input, "array is_static");
        break;
    case TYPE_FUNCTION:
        type->u.function.return_type = import_type(input);
        Param **next_param           = &type->u.function.params;
        while ((tag = wgetw(input)) != TAG_EOL) {
            check_input(input, "function param tag");
            *next_param = import_param(input);
            if (*next_param)
                next_param = &(*next_param)->next;
        }
        check_input(input, "function param list end");
        type->u.function.variadic = (bool)wgetw(input);
        check_input(input, "function variadic");
        break;
    case TYPE_STRUCT:
    case TYPE_UNION:
        type->u.struct_t.name = wgetstr(input);
        check_input(input, "struct/union name");
        Field **next_field = &type->u.struct_t.fields;
        while ((tag = wgetw(input)) != TAG_EOL) {
            check_input(input, "struct field tag");
            *next_field = import_field(input);
            if (*next_field)
                next_field = &(*next_field)->next;
        }
        check_input(input, "struct field list end");
        break;
    case TYPE_ENUM:
        type->u.enum_t.name = wgetstr(input);
        check_input(input, "enum name");
        Enumerator **next_enum = &type->u.enum_t.enumerators;
        while ((tag = wgetw(input)) != TAG_EOL) {
            check_input(input, "enumerator tag");
            *next_enum = import_enumerator(input);
            if (*next_enum)
                next_enum = &(*next_enum)->next;
        }
        check_input(input, "enumerator list end");
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
    while ((tag = wgetw(input)) != TAG_EOL) {
        check_input(input, "type qualifier tag");
        *next_qual = import_type_qualifier(input);
        if (*next_qual)
            next_qual = &(*next_qual)->next;
    }
    check_input(input, "type qualifier list end");
    return type;
}

TypeQualifier *import_type_qualifier(WFILE *input)
{
    size_t tag = wgetw(input);
    check_input(input, "type qualifier tag");
    if (tag < TAG_TYPEQUALIFIER || tag > TAG_TYPEQUALIFIER + TYPE_QUALIFIER_ATOMIC)
        return NULL;
    TypeQualifierKind kind = (TypeQualifierKind)(tag - TAG_TYPEQUALIFIER);
    TypeQualifier *qual    = new_type_qualifier(kind);
    return qual;
}

Field *import_field(WFILE *input)
{
    size_t tag = wgetw(input);
    check_input(input, "field tag");
    if (tag != TAG_FIELD)
        return NULL;
    Field *field = new_field();
    field->type  = import_type(input);
    field->name  = wgetstr(input);
    check_input(input, "field name");
    field->bitfield = import_expr(input);
    return field;
}

Enumerator *import_enumerator(WFILE *input)
{
    size_t tag = wgetw(input);
    check_input(input, "enumerator tag");
    if (tag != TAG_ENUMERATOR)
        return NULL;
    Ident name = wgetstr(input);
    check_input(input, "enumerator name");
    Expr *value       = import_expr(input);
    Enumerator *enumr = new_enumerator(name, value);
    return enumr;
}

Param *import_param(WFILE *input)
{
    size_t tag = wgetw(input);
    check_input(input, "param tag");
    if (tag != TAG_PARAM)
        return NULL;
    Param *param = new_param();
    param->name  = wgetstr(input);
    check_input(input, "param name");
    param->type = import_type(input);
    return param;
}

Declaration *import_declaration(WFILE *input)
{
    size_t tag = wgetw(input);
    check_input(input, "declaration tag");
    if (tag < TAG_DECLARATION || tag > TAG_DECLARATION + DECL_EMPTY)
        return NULL;
    DeclarationKind kind = (DeclarationKind)(tag - TAG_DECLARATION);
    Declaration *decl    = new_declaration(kind);
    switch (kind) {
    case DECL_VAR:
        decl->u.var.specifiers      = import_decl_spec(input);
        InitDeclarator **next_idecl = &decl->u.var.declarators;
        while ((tag = wgetw(input)) != TAG_EOL) {
            check_input(input, "init declarator tag");
            *next_idecl = import_init_declarator(input);
            if (*next_idecl)
                next_idecl = &(*next_idecl)->next;
        }
        check_input(input, "init declarator list end");
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
    size_t tag = wgetw(input);
    check_input(input, "decl spec tag");
    if (tag != TAG_DECLSPEC)
        return NULL;
    DeclSpec *spec            = new_decl_spec();
    TypeQualifier **next_qual = &spec->qualifiers;
    while ((tag = wgetw(input)) != TAG_EOL) {
        check_input(input, "decl spec qualifier tag");
        *next_qual = import_type_qualifier(input);
        if (*next_qual)
            next_qual = &(*next_qual)->next;
    }
    check_input(input, "decl spec qualifier list end");
    spec->storage             = import_storage_class(input);
    FunctionSpec **next_fspec = &spec->func_specs;
    while ((tag = wgetw(input)) != TAG_EOL) {
        check_input(input, "function spec tag");
        *next_fspec = import_function_spec(input);
        if (*next_fspec)
            next_fspec = &(*next_fspec)->next;
    }
    check_input(input, "function spec list end");
    spec->align_spec = import_alignment_spec(input);
    return spec;
}

StorageClass *import_storage_class(WFILE *input)
{
    size_t tag = wgetw(input);
    check_input(input, "storage class tag");
    if (tag < TAG_STORAGECLASS || tag > TAG_STORAGECLASS + STORAGE_CLASS_REGISTER)
        return NULL;
    StorageClassKind kind = (StorageClassKind)(tag - TAG_STORAGECLASS);
    StorageClass *stor    = new_storage_class(kind);
    return stor;
}

FunctionSpec *import_function_spec(WFILE *input)
{
    size_t tag = wgetw(input);
    check_input(input, "function spec tag");
    if (tag < TAG_FUNCTIONSPEC || tag > TAG_FUNCTIONSPEC + FUNC_SPEC_NORETURN)
        return NULL;
    FunctionSpecKind kind = (FunctionSpecKind)(tag - TAG_FUNCTIONSPEC);
    FunctionSpec *fspec   = new_function_spec(kind);
    return fspec;
}

AlignmentSpec *import_alignment_spec(WFILE *input)
{
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
    size_t tag = wgetw(input);
    check_input(input, "init declarator tag");
    if (tag != TAG_INITDECLARATOR)
        return NULL;
    InitDeclarator *idecl = new_init_declarator();
    idecl->type           = import_type(input);
    idecl->name           = wgetstr(input);
    check_input(input, "init declarator name");
    idecl->init = import_initializer(input);
    return idecl;
}

Initializer *import_initializer(WFILE *input)
{
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
        while ((tag = wgetw(input)) != TAG_EOL) {
            check_input(input, "init item tag");
            *next_item = import_init_item(input);
            if (*next_item)
                next_item = &(*next_item)->next;
        }
        check_input(input, "init item list end");
        break;
    }
    }
    return init;
}

InitItem *import_init_item(WFILE *input)
{
    size_t tag = wgetw(input);
    check_input(input, "init item tag");
    if (tag != TAG_INITITEM)
        return NULL;
    Designator *designators = NULL;
    Designator **next_desg  = &designators;
    while ((tag = wgetw(input)) != TAG_EOL) {
        check_input(input, "designator tag");
        *next_desg = import_designator(input);
        if (*next_desg)
            next_desg = &(*next_desg)->next;
    }
    check_input(input, "designator list end");
    Initializer *init = import_initializer(input);
    InitItem *item    = new_init_item(designators, init);
    return item;
}

Designator *import_designator(WFILE *input)
{
    size_t tag = wgetw(input);
    check_input(input, "designator tag");
    if (tag < TAG_DESIGNATOR || tag > TAG_DESIGNATOR + DESIGNATOR_FIELD)
        return NULL;
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
    size_t tag = wgetw(input);
    check_input(input, "expr tag");
    if (tag < TAG_EXPR || tag > TAG_EXPR + EXPR_GENERIC)
        return NULL;
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
        expr->u.unary_op.op   = import_unary_op(input);
        expr->u.unary_op.expr = import_expr(input);
        break;
    case EXPR_BINARY_OP:
        expr->u.binary_op.op    = import_binary_op(input);
        expr->u.binary_op.left  = import_expr(input);
        expr->u.binary_op.right = import_expr(input);
        break;
    case EXPR_ASSIGN:
        expr->u.assign.target = import_expr(input);
        expr->u.assign.op     = import_assign_op(input);
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
        while ((tag = wgetw(input)) != TAG_EOL) {
            check_input(input, "call arg tag");
            *next_arg = import_expr(input);
            if (*next_arg)
                next_arg = &(*next_arg)->next;
        }
        check_input(input, "call arg list end");
        break;
    case EXPR_COMPOUND:
        expr->u.compound_literal.type = import_type(input);
        InitItem **next_item          = &expr->u.compound_literal.init;
        while ((tag = wgetw(input)) != TAG_EOL) {
            check_input(input, "compound init item tag");
            *next_item = import_init_item(input);
            if (*next_item)
                next_item = &(*next_item)->next;
        }
        check_input(input, "compound init list end");
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
        while ((tag = wgetw(input)) != TAG_EOL) {
            check_input(input, "generic assoc tag");
            *next_gasc = import_generic_assoc(input);
            if (*next_gasc)
                next_gasc = &(*next_gasc)->next;
        }
        check_input(input, "generic assoc list end");
        break;
    }
    expr->type = import_type(input);
    return expr;
}

Literal *import_literal(WFILE *input)
{
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

UnaryOp *import_unary_op(WFILE *input)
{
    size_t tag = wgetw(input);
    check_input(input, "unary op tag");
    if (tag < TAG_UNARYOP || tag > TAG_UNARYOP + UNARY_PRE_DEC)
        return NULL;
    UnaryOpKind kind = (UnaryOpKind)(tag - TAG_UNARYOP);
    UnaryOp *uop     = new_unary_op(kind);
    return uop;
}

BinaryOp *import_binary_op(WFILE *input)
{
    size_t tag = wgetw(input);
    check_input(input, "binary op tag");
    if (tag < TAG_BINARYOP || tag > TAG_BINARYOP + BINARY_LOG_OR)
        return NULL;
    BinaryOpKind kind = (BinaryOpKind)(tag - TAG_BINARYOP);
    BinaryOp *bop     = new_binary_op(kind);
    return bop;
}

AssignOp *import_assign_op(WFILE *input)
{
    size_t tag = wgetw(input);
    check_input(input, "assign op tag");
    if (tag < TAG_ASSIGNOP || tag > TAG_ASSIGNOP + ASSIGN_OR)
        return NULL;
    AssignOpKind kind = (AssignOpKind)(tag - TAG_ASSIGNOP);
    AssignOp *aop     = new_assign_op(kind);
    return aop;
}

GenericAssoc *import_generic_assoc(WFILE *input)
{
    size_t tag = wgetw(input);
    check_input(input, "generic assoc tag");
    if (tag < TAG_GENERICASSOC || tag > TAG_GENERICASSOC + GENERIC_ASSOC_DEFAULT)
        return NULL;
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
    size_t tag = wgetw(input);
    check_input(input, "stmt tag");
    if (tag < TAG_STMT || tag > TAG_STMT + STMT_DEFAULT)
        return NULL;
    StmtKind kind = (StmtKind)(tag - TAG_STMT);
    Stmt *stmt    = new_stmt(kind);
    switch (kind) {
    case STMT_EXPR:
        stmt->u.expr = import_expr(input);
        break;
    case STMT_COMPOUND: {
        DeclOrStmt **next_dost = &stmt->u.compound;
        while ((tag = wgetw(input)) != TAG_EOL) {
            check_input(input, "decl or stmt tag");
            *next_dost = import_decl_or_stmt(input);
            if (*next_dost)
                next_dost = &(*next_dost)->next;
        }
        check_input(input, "compound decl or stmt list end");
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
    size_t tag = wgetw(input);
    check_input(input, "decl or stmt tag");
    if (tag < TAG_DECLORSTMT || tag > TAG_DECLORSTMT + DECL_OR_STMT_STMT)
        return NULL;
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
        while ((tag = wgetw(input)) != TAG_EOL) {
            check_input(input, "param decl tag");
            *next_decl = import_declaration(input);
            if (*next_decl)
                next_decl = &(*next_decl)->next;
        }
        check_input(input, "param decl list end");
        exdecl->u.function.body = import_stmt(input);
        break;
    case EXTERNAL_DECL_DECLARATION:
        exdecl->u.declaration = import_declaration(input);
        break;
    }
    return exdecl;
}
