#include <stdlib.h>

#include "ast.h"
#include "internal.h"
#include "xalloc.h"

/* Helper functions for AST construction */
Type *new_type(TypeKind kind)
{
    Type *t                 = xmalloc(sizeof(Type), __func__, __FILE__, __LINE__);
    t->kind                 = kind;
    t->u.integer.signedness = SIGNED_SIGNED; /* Default */
    return t;
}

TypeQualifier *new_type_qualifier(TypeQualifierKind kind)
{
    TypeQualifier *q = xmalloc(sizeof(TypeQualifier), __func__, __FILE__, __LINE__);
    q->kind          = kind;
    return q;
}

Field *new_field(void)
{
    Field *f = (Field *)xmalloc(sizeof(Field), __func__, __FILE__, __LINE__);
    return f;
}

Enumerator *new_enumerator(Ident name, Expr *value)
{
    Enumerator *e = xmalloc(sizeof(Enumerator), __func__, __FILE__, __LINE__);
    e->name       = name;
    e->value      = value;
    return e;
}

Param *new_param()
{
    Param *p = xmalloc(sizeof(Param), __func__, __FILE__, __LINE__);
    return p;
}

Declaration *new_declaration(DeclarationKind kind)
{
    Declaration *d = xmalloc(sizeof(Declaration), __func__, __FILE__, __LINE__);
    d->kind        = kind;
    return d;
}

DeclSpec *new_decl_spec()
{
    DeclSpec *ds = xmalloc(sizeof(DeclSpec), __func__, __FILE__, __LINE__);
    return ds;
}

TypeSpec *new_type_spec(TypeSpecKind kind)
{
    TypeSpec *ts = xmalloc(sizeof(TypeSpec), __func__, __FILE__, __LINE__);
    ts->kind     = kind;
    return ts;
}

FunctionSpec *new_function_spec(FunctionSpecKind kind)
{
    FunctionSpec *fs = xmalloc(sizeof(FunctionSpec), __func__, __FILE__, __LINE__);
    fs->kind         = kind;
    return fs;
}

AlignmentSpec *new_alignment_spec(AlignmentSpecKind kind)
{
    AlignmentSpec *as = xmalloc(sizeof(AlignmentSpec), __func__, __FILE__, __LINE__);
    as->kind          = kind;
    return as;
}

InitDeclarator *new_init_declarator()
{
    InitDeclarator *id = xmalloc(sizeof(InitDeclarator), __func__, __FILE__, __LINE__);
    return id;
}

Declarator *new_declarator()
{
    Declarator *d = xmalloc(sizeof(Declarator), __func__, __FILE__, __LINE__);
    return d;
}

Pointer *new_pointer()
{
    Pointer *p = xmalloc(sizeof(Pointer), __func__, __FILE__, __LINE__);
    return p;
}

DeclaratorSuffix *new_declarator_suffix(DeclaratorSuffixKind kind)
{
    DeclaratorSuffix *ds = xmalloc(sizeof(DeclaratorSuffix), __func__, __FILE__, __LINE__);
    ds->kind             = kind;
    return ds;
}

Initializer *new_initializer(InitializerKind kind)
{
    Initializer *i = xmalloc(sizeof(Initializer), __func__, __FILE__, __LINE__);
    i->kind        = kind;
    return i;
}

InitItem *new_init_item(Designator *designators, Initializer *init)
{
    InitItem *ii    = xmalloc(sizeof(InitItem), __func__, __FILE__, __LINE__);
    ii->designators = designators;
    ii->init        = init;
    return ii;
}

Designator *new_designator(DesignatorKind kind)
{
    Designator *d = xmalloc(sizeof(Designator), __func__, __FILE__, __LINE__);
    d->kind       = kind;
    return d;
}

Expr *new_expression(ExprKind kind)
{
    Expr *e = xmalloc(sizeof(Expr), __func__, __FILE__, __LINE__);
    e->kind = kind;
    return e;
}

Literal *new_literal(LiteralKind kind)
{
    Literal *l = xmalloc(sizeof(Literal), __func__, __FILE__, __LINE__);
    l->kind    = kind;
    return l;
}

UnaryOp *new_unary_op(UnaryOpKind kind)
{
    UnaryOp *op = xmalloc(sizeof(UnaryOp), __func__, __FILE__, __LINE__);
    op->kind    = kind;
    return op;
}

BinaryOp *new_binary_op(BinaryOpKind kind)
{
    BinaryOp *op = xmalloc(sizeof(BinaryOp), __func__, __FILE__, __LINE__);
    op->kind     = kind;
    return op;
}

AssignOp *new_assign_op(AssignOpKind kind)
{
    AssignOp *op = xmalloc(sizeof(AssignOp), __func__, __FILE__, __LINE__);
    op->kind     = kind;
    return op;
}

GenericAssoc *new_generic_assoc(GenericAssocKind kind)
{
    GenericAssoc *ga = xmalloc(sizeof(GenericAssoc), __func__, __FILE__, __LINE__);
    ga->kind         = kind;
    return ga;
}

Stmt *new_stmt(StmtKind kind)
{
    Stmt *s = xmalloc(sizeof(Stmt), __func__, __FILE__, __LINE__);
    s->kind = kind;
    return s;
}

DeclOrStmt *new_decl_or_stmt(DeclOrStmtKind kind)
{
    DeclOrStmt *ds = xmalloc(sizeof(DeclOrStmt), __func__, __FILE__, __LINE__);
    ds->kind       = kind;
    return ds;
}

ForInit *new_for_init(ForInitKind kind)
{
    ForInit *fi = xmalloc(sizeof(ForInit), __func__, __FILE__, __LINE__);
    fi->kind    = kind;
    return fi;
}

ExternalDecl *new_external_decl(ExternalDeclKind kind)
{
    ExternalDecl *ed = xmalloc(sizeof(ExternalDecl), __func__, __FILE__, __LINE__);
    ed->kind         = kind;
    return ed;
}

Program *new_program()
{
    Program *p = xmalloc(sizeof(Program), __func__, __FILE__, __LINE__);
    return p;
}
