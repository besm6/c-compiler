#include <stdlib.h>

#include "ast.h"
#include "internal.h"

/* Helper functions for AST construction */
Type *new_type(TypeKind kind)
{
    Type *t                = malloc(sizeof(Type));
    t->kind                = kind;
    t->qualifiers          = NULL;
    t->u.integer.signedness = SIGNED_SIGNED; /* Default */
    return t;
}

TypeQualifier *new_type_qualifier(TypeQualifierKind kind)
{
    TypeQualifier *q = malloc(sizeof(TypeQualifier));
    q->kind          = kind;
    q->next          = NULL;
    return q;
}

Field *new_field(void)
{
    Field *f    = (Field *)malloc(sizeof(Field));
    f->next     = NULL;
    f->type     = NULL;
    f->name     = NULL;
    f->bitfield = NULL;
    return f;
}

Enumerator *new_enumerator(Ident name, Expr *value)
{
    Enumerator *e = malloc(sizeof(Enumerator));
    e->name       = name;
    e->value      = value;
    e->next       = NULL;
    return e;
}

Param *new_param()
{
    Param *p = malloc(sizeof(Param));
    p->name  = NULL;
    p->type  = NULL;
    p->next  = NULL;
    return p;
}

Declaration *new_declaration(DeclarationKind kind)
{
    Declaration *d = malloc(sizeof(Declaration));
    d->kind        = kind;
    d->next        = NULL;
    return d;
}

DeclSpec *new_decl_spec()
{
    DeclSpec *ds   = malloc(sizeof(DeclSpec));
    ds->storage    = NULL;
    ds->qualifiers = NULL;
    ds->func_specs = NULL;
    ds->align_spec = NULL;
    return ds;
}

StorageClass *new_storage_class(StorageClassKind kind)
{
    StorageClass *sc = malloc(sizeof(StorageClass));
    sc->kind         = kind;
    return sc;
}

TypeSpec *new_type_spec(TypeSpecKind kind)
{
    TypeSpec *ts   = malloc(sizeof(TypeSpec));
    ts->kind       = kind;
    ts->qualifiers = NULL;
    ts->next       = NULL;
    return ts;
}

FunctionSpec *new_function_spec(FunctionSpecKind kind)
{
    FunctionSpec *fs = malloc(sizeof(FunctionSpec));
    fs->kind         = kind;
    fs->next         = NULL;
    return fs;
}

AlignmentSpec *new_alignment_spec(AlignmentSpecKind kind)
{
    AlignmentSpec *as = malloc(sizeof(AlignmentSpec));
    as->kind          = kind;
    return as;
}

InitDeclarator *new_init_declarator()
{
    InitDeclarator *id = malloc(sizeof(InitDeclarator));
    id->init           = NULL;
    id->next           = NULL;
    id->type           = NULL;
    id->name           = NULL;
    return id;
}

Declarator *new_declarator()
{
    Declarator *d = malloc(sizeof(Declarator));
    d->next       = NULL;
    d->name       = NULL;
    d->pointers   = NULL;
    d->suffixes   = NULL;
    return d;
}

Pointer *new_pointer()
{
    Pointer *p    = malloc(sizeof(Pointer));
    p->qualifiers = NULL;
    p->next       = NULL;
    return p;
}

DeclaratorSuffix *new_declarator_suffix(DeclaratorSuffixKind kind)
{
    DeclaratorSuffix *ds = malloc(sizeof(DeclaratorSuffix));
    ds->kind             = kind;
    ds->next             = NULL;
    return ds;
}

Initializer *new_initializer(InitializerKind kind)
{
    Initializer *i = malloc(sizeof(Initializer));
    i->kind        = kind;
    return i;
}

InitItem *new_init_item(Designator *designators, Initializer *init)
{
    InitItem *ii    = malloc(sizeof(InitItem));
    ii->designators = designators;
    ii->init        = init;
    ii->next        = NULL;
    return ii;
}

Designator *new_designator(DesignatorKind kind)
{
    Designator *d = malloc(sizeof(Designator));
    d->kind       = kind;
    d->next       = NULL;
    return d;
}

Expr *new_expression(ExprKind kind)
{
    Expr *e = malloc(sizeof(Expr));
    e->kind = kind;
    e->type = NULL;
    e->next = NULL;
    return e;
}

Literal *new_literal(LiteralKind kind)
{
    Literal *l = malloc(sizeof(Literal));
    l->kind    = kind;
    return l;
}

UnaryOp *new_unary_op(UnaryOpKind kind)
{
    UnaryOp *op = malloc(sizeof(UnaryOp));
    op->kind    = kind;
    return op;
}

BinaryOp *new_binary_op(BinaryOpKind kind)
{
    BinaryOp *op = malloc(sizeof(BinaryOp));
    op->kind     = kind;
    return op;
}

AssignOp *new_assign_op(AssignOpKind kind)
{
    AssignOp *op = malloc(sizeof(AssignOp));
    op->kind     = kind;
    return op;
}

GenericAssoc *new_generic_assoc(GenericAssocKind kind)
{
    GenericAssoc *ga = malloc(sizeof(GenericAssoc));
    ga->kind         = kind;
    ga->next         = NULL;
    return ga;
}

Stmt *new_stmt(StmtKind kind)
{
    Stmt *s = malloc(sizeof(Stmt));
    s->kind = kind;
    return s;
}

DeclOrStmt *new_decl_or_stmt(DeclOrStmtKind kind)
{
    DeclOrStmt *ds = malloc(sizeof(DeclOrStmt));
    ds->kind       = kind;
    ds->next       = NULL;
    return ds;
}

ForInit *new_for_init(ForInitKind kind)
{
    ForInit *fi = malloc(sizeof(ForInit));
    fi->kind    = kind;
    return fi;
}

ExternalDecl *new_external_decl(ExternalDeclKind kind)
{
    ExternalDecl *ed = malloc(sizeof(ExternalDecl));
    ed->kind         = kind;
    ed->next         = NULL;
    return ed;
}

Program *new_program()
{
    Program *p = malloc(sizeof(Program));
    p->decls   = NULL;
    return p;
}
