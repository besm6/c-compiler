#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "ast.h"
#include "scanner.h"

/* Global lexer state */
static int current_token;
static int peek_token;
static char *current_lexeme;
static char lexeme_buffer[1024]; // Buffer for current lexeme

// Set manually to enable debug output
static int debug = 1;

/* Error handling */
static void fatal_error(const char *message, ...)
{
    fprintf(stderr, "Parse error: ");

    va_list ap;
    va_start(ap, message);
    vfprintf(stderr, message, ap);
    va_end(ap);

    if (current_lexeme) {
        fprintf(stderr, " (token: %d, lexeme: %s)\n", current_token, current_lexeme);
    } else {
        fprintf(stderr, " (token: %d)\n", current_token);
    }
    exit(1);
}

/* Token handling */
void advance_token()
{
    if (peek_token > 0) {
        current_token = peek_token;
        peek_token    = 0;
    } else {
        current_token = yylex();
    }
    current_lexeme = get_yytext();
}

// Does this token have something valuable in yytext?
static bool has_yytext(int token)
{
    return token == TOKEN_IDENTIFIER || token == TOKEN_I_CONSTANT ||
           token == TOKEN_F_CONSTANT || token == TOKEN_ENUMERATION_CONSTANT ||
           token == TOKEN_STRING_LITERAL || token == TOKEN_TYPEDEF_NAME;
}

// Peek next token, without advancing the parser.
static int next_token()
{
    if (peek_token == 0) {
        if (has_yytext(current_token)) {
            // Make a copy of current lexeme.
            strcpy(lexeme_buffer, current_lexeme);
            current_lexeme = lexeme_buffer;
        }
        peek_token = yylex();
    }
    return peek_token;
}

static void expect_token(int expected)
{
    if (current_token != expected) {
        fprintf(stderr, "Parse error: Expected token %d, got %d (lexeme: %s)\n",
                        expected, current_token, current_lexeme ? current_lexeme : "");
        exit(1);
    }
    advance_token();
}

// Is this token a start of a type name?
static bool is_type_name(int token)
{
    return token == TOKEN_VOID || token == TOKEN_CHAR ||
           token == TOKEN_SHORT || token == TOKEN_INT ||
           token == TOKEN_LONG || token == TOKEN_FLOAT ||
           token == TOKEN_DOUBLE || token == TOKEN_SIGNED ||
           token == TOKEN_UNSIGNED || token == TOKEN_BOOL ||
           token == TOKEN_COMPLEX || token == TOKEN_IMAGINARY ||
           token == TOKEN_STRUCT || token == TOKEN_UNION ||
           token == TOKEN_ENUM || token == TOKEN_TYPEDEF_NAME ||
           token == TOKEN_CONST || token == TOKEN_RESTRICT ||
           token == TOKEN_VOLATILE || token == TOKEN_ATOMIC;
}

/* Helper functions for AST construction */
static Type *new_type(TypeKind kind)
{
    Type *t                = malloc(sizeof(Type));
    t->kind                = kind;
    t->qualifiers          = NULL;
    t->u.integer.signedness = SIGNED_SIGNED; /* Default */
    return t;
}

static TypeQualifier *new_type_qualifier(TypeQualifierKind kind)
{
    TypeQualifier *q = malloc(sizeof(TypeQualifier));
    q->kind          = kind;
    q->next          = NULL;
    return q;
}

static Field *new_field(void)
{
    Field *f        = (Field *)malloc(sizeof(Field));
    f->next         = NULL;
    f->is_anonymous = false;
    return f;
}

static Enumerator *new_enumerator(Ident name, Expr *value)
{
    Enumerator *e = malloc(sizeof(Enumerator));
    e->name       = name;
    e->value      = value;
    e->next       = NULL;
    return e;
}

static Param *new_param()
{
    Param *p = malloc(sizeof(Param));
    p->name  = NULL;
    p->type  = NULL;
    p->next  = NULL;
    return p;
}

static ParamList *new_param_list()
{
    ParamList *pl = malloc(sizeof(ParamList));
    pl->is_empty  = true;
    pl->u.params  = NULL;
    return pl;
}

static Declaration *new_declaration(DeclarationKind kind)
{
    Declaration *d = malloc(sizeof(Declaration));
    d->kind        = kind;
    d->next        = NULL;
    return d;
}

static DeclSpec *new_decl_spec()
{
    DeclSpec *ds   = malloc(sizeof(DeclSpec));
    ds->storage    = NULL;
    ds->qualifiers = NULL;
    ds->base_type = NULL;
    ds->func_specs = NULL;
    ds->align_spec = NULL;
    return ds;
}

static StorageClass *new_storage_class(StorageClassKind kind)
{
    StorageClass *sc = malloc(sizeof(StorageClass));
    sc->kind         = kind;
    return sc;
}

static TypeSpec *new_type_spec(TypeSpecKind kind)
{
    TypeSpec *ts   = malloc(sizeof(TypeSpec));
    ts->kind       = kind;
    ts->qualifiers = NULL;
    ts->next       = NULL;
    return ts;
}

static FunctionSpec *new_function_spec(FunctionSpecKind kind)
{
    FunctionSpec *fs = malloc(sizeof(FunctionSpec));
    fs->kind         = kind;
    fs->next         = NULL;
    return fs;
}

static AlignmentSpec *new_alignment_spec(AlignmentSpecKind kind)
{
    AlignmentSpec *as = malloc(sizeof(AlignmentSpec));
    as->kind          = kind;
    return as;
}

static InitDeclarator *new_init_declarator(Declarator *declarator, Initializer *init)
{
    InitDeclarator *id = malloc(sizeof(InitDeclarator));
    id->declarator     = declarator;
    id->init           = init;
    id->next           = NULL;
    return id;
}

static Declarator *new_declarator(DeclaratorKind kind)
{
    Declarator *d = malloc(sizeof(Declarator));
    d->kind       = kind;
    d->next       = NULL;
    if (kind == DECLARATOR_NAMED) {
        d->u.named.name     = NULL;
        d->u.named.pointers = NULL;
        d->u.named.suffixes = NULL;
    } else {
        d->u.abstract.pointers = NULL;
        d->u.abstract.suffixes = NULL;
    }
    return d;
}

static Pointer *new_pointer()
{
    Pointer *p    = malloc(sizeof(Pointer));
    p->qualifiers = NULL;
    p->next       = NULL;
    return p;
}

static DeclaratorSuffix *new_declarator_suffix(DeclaratorSuffixKind kind)
{
    DeclaratorSuffix *ds = malloc(sizeof(DeclaratorSuffix));
    ds->kind             = kind;
    ds->next             = NULL;
    return ds;
}

static Initializer *new_initializer(InitializerKind kind)
{
    Initializer *i = malloc(sizeof(Initializer));
    i->kind        = kind;
    return i;
}

static InitItem *new_init_item(Designator *designators, Initializer *init)
{
    InitItem *ii    = malloc(sizeof(InitItem));
    ii->designators = designators;
    ii->init        = init;
    ii->next        = NULL;
    return ii;
}

static Designator *new_designator(DesignatorKind kind)
{
    Designator *d = malloc(sizeof(Designator));
    d->kind       = kind;
    d->next       = NULL;
    return d;
}

static Expr *new_expression(ExprKind kind)
{
    Expr *e = malloc(sizeof(Expr));
    e->kind = kind;
    e->type = NULL;
    e->next = NULL;
    return e;
}

static Literal *new_literal(LiteralKind kind)
{
    Literal *l = malloc(sizeof(Literal));
    l->kind    = kind;
    return l;
}

static UnaryOp *new_unary_op(UnaryOpKind kind)
{
    UnaryOp *op = malloc(sizeof(UnaryOp));
    op->kind    = kind;
    return op;
}

static BinaryOp *new_binary_op(BinaryOpKind kind)
{
    BinaryOp *op = malloc(sizeof(BinaryOp));
    op->kind     = kind;
    return op;
}

static AssignOp *new_assign_op(AssignOpKind kind)
{
    AssignOp *op = malloc(sizeof(AssignOp));
    op->kind     = kind;
    return op;
}

static GenericAssoc *new_generic_assoc(GenericAssocKind kind)
{
    GenericAssoc *ga = malloc(sizeof(GenericAssoc));
    ga->kind         = kind;
    ga->next         = NULL;
    return ga;
}

static Stmt *new_stmt(StmtKind kind)
{
    Stmt *s = malloc(sizeof(Stmt));
    s->kind = kind;
    return s;
}

static DeclOrStmt *new_decl_or_stmt(DeclOrStmtKind kind)
{
    DeclOrStmt *ds = malloc(sizeof(DeclOrStmt));
    ds->kind       = kind;
    ds->next       = NULL;
    return ds;
}

static ForInit *new_for_init(ForInitKind kind)
{
    ForInit *fi = malloc(sizeof(ForInit));
    fi->kind    = kind;
    return fi;
}

static ExternalDecl *new_external_decl(ExternalDeclKind kind)
{
    ExternalDecl *ed = malloc(sizeof(ExternalDecl));
    ed->kind         = kind;
    ed->next         = NULL;
    return ed;
}

static Program *new_program()
{
    Program *p = malloc(sizeof(Program));
    p->decls   = NULL;
    return p;
}

/* Append to linked list */
static void append_list(void *head_ptr, void *node_ptr)
{
    typedef struct List List;
    struct List {
        List *next; /* linked list */
    };

    if (!node_ptr)
        return;

    List **head = (List **) head_ptr;
    List *node = (List *) node_ptr;
    if (*head == NULL) {
        *head = node;
    } else {
        // Find tail.
        List *current = *head;
        while (current->next) {
            current = current->next;
        }
        current->next = node;
    }
    node->next = NULL;
}

/* Parser functions */
Expr *parse_primary_expression();
Expr *parse_constant();
Expr *parse_string();
Expr *parse_generic_selection();
GenericAssoc *parse_generic_assoc_list();
GenericAssoc *parse_generic_association();
Expr *parse_postfix_expression();
Expr *parse_argument_expression_list();
Expr *parse_unary_expression();
UnaryOp *parse_unary_operator();
Expr *parse_cast_expression();
Expr *parse_multiplicative_expression();
Expr *parse_additive_expression();
Expr *parse_shift_expression();
Expr *parse_relational_expression();
Expr *parse_equality_expression();
Expr *parse_and_expression();
Expr *parse_exclusive_or_expression();
Expr *parse_inclusive_or_expression();
Expr *parse_logical_and_expression();
Expr *parse_logical_or_expression();
Expr *parse_conditional_expression();
Expr *parse_assignment_expression();
AssignOp *parse_assignment_operator();
Expr *parse_expression();
Expr *parse_constant_expression();
Declaration *parse_declaration();
DeclSpec *parse_declaration_specifiers();
InitDeclarator *parse_init_declarator_list();
InitDeclarator *parse_init_declarator();
StorageClass *parse_storage_class_specifier();
TypeSpec *parse_type_specifier();
Type *parse_struct_or_union_specifier();
int parse_struct_or_union();
Field *parse_struct_declaration_list();
Field *parse_struct_declaration();
TypeSpec *parse_specifier_qualifier_list(TypeQualifier **qualifiers);
Declarator *parse_struct_declarator_list();
Declarator *parse_struct_declarator();
Type *parse_enum_specifier();
Enumerator *parse_enumerator_list();
Enumerator *parse_enumerator();
Type *parse_atomic_type_specifier();
TypeQualifier *parse_type_qualifier();
FunctionSpec *parse_function_specifier();
AlignmentSpec *parse_alignment_specifier();
Declarator *parse_declarator();
Declarator *parse_direct_declarator();
Pointer *parse_pointer();
TypeQualifier *parse_type_qualifier_list();
ParamList *parse_parameter_type_list();
Param *parse_parameter_list();
Param *parse_parameter_declaration();
Type *parse_type_name();
DeclaratorSuffix *parse_direct_abstract_declarator();
Initializer *parse_initializer();
InitItem *parse_initializer_list();
Designator *parse_designation();
Designator *parse_designator_list();
Designator *parse_designator();
Declaration *parse_static_assert_declaration();
Stmt *parse_statement();
Stmt *parse_labeled_statement();
Stmt *parse_compound_statement();
DeclOrStmt *parse_block_item_list();
DeclOrStmt *parse_block_item();
Stmt *parse_expression_statement();
Stmt *parse_selection_statement();
Stmt *parse_iteration_statement();
Stmt *parse_jump_statement();
Program *parse_translation_unit();
ExternalDecl *parse_external_declaration();
Declaration *parse_declaration_list();

Expr *parse_primary_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = NULL;
    switch (current_token) {
    case TOKEN_IDENTIFIER:
        expr        = new_expression(EXPR_VAR);
        expr->u.var = strdup(current_lexeme);
        advance_token();
        break;
    case TOKEN_I_CONSTANT:
    case TOKEN_F_CONSTANT:
    case TOKEN_ENUMERATION_CONSTANT:
        expr = parse_constant();
        break;
    case TOKEN_STRING_LITERAL:
    case TOKEN_FUNC_NAME:
        expr = parse_string();
        break;
    case TOKEN_LPAREN:
        advance_token();
        expr = parse_expression();
        expect_token(TOKEN_RPAREN);
        break;
    case TOKEN_GENERIC:
        expr = parse_generic_selection();
        break;
    default:
        fatal_error("Expected primary expression");
    }
    return expr;
}

Expr *parse_constant()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr      = new_expression(EXPR_LITERAL);
    expr->u.literal = new_literal(current_token == TOKEN_I_CONSTANT   ? LITERAL_INT
                                  : current_token == TOKEN_F_CONSTANT ? LITERAL_FLOAT
                                                                      : LITERAL_ENUM);
    switch (current_token) {
    case TOKEN_I_CONSTANT:
        expr->u.literal->u.int_val = atoi(current_lexeme);
        break;
    case TOKEN_F_CONSTANT:
        expr->u.literal->u.float_val = atof(current_lexeme);
        break;
    case TOKEN_ENUMERATION_CONSTANT:
        expr->u.literal->u.enum_const = strdup(current_lexeme);
        break;
    }
    advance_token();
    return expr;
}

Expr *parse_string()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr                    = new_expression(EXPR_LITERAL);
    expr->u.literal               = new_literal(LITERAL_STRING);
    expr->u.literal->u.string_val = strdup(current_lexeme);
    advance_token();
    return expr;
}

Expr *parse_generic_selection()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_GENERIC);
    expect_token(TOKEN_LPAREN);
    Expr *controlling_expr = parse_assignment_expression();
    expect_token(TOKEN_COMMA);
    GenericAssoc *associations = parse_generic_assoc_list();
    expect_token(TOKEN_RPAREN);
    Expr *expr                       = new_expression(EXPR_GENERIC);
    expr->u.generic.controlling_expr = controlling_expr;
    expr->u.generic.associations     = associations;
    return expr;
}

GenericAssoc *parse_generic_assoc_list()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    GenericAssoc *assoc = parse_generic_association();
    if (current_token == TOKEN_COMMA) {
        advance_token();
        assoc->next = parse_generic_assoc_list();
    }
    return assoc;
}

GenericAssoc *parse_generic_association()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    GenericAssoc *assoc;
    if (current_token == TOKEN_DEFAULT) {
        advance_token();
        expect_token(TOKEN_COLON);
        Expr *expr             = parse_assignment_expression();
        assoc                  = new_generic_assoc(GENERIC_ASSOC_DEFAULT);
        assoc->u.default_assoc = expr;
    } else {
        Type *type = parse_type_name();
        expect_token(TOKEN_COLON);
        Expr *expr               = parse_assignment_expression();
        assoc                    = new_generic_assoc(GENERIC_ASSOC_TYPE);
        assoc->u.type_assoc.type = type;
        assoc->u.type_assoc.expr = expr;
    }
    return assoc;
}

Expr *parse_postfix_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
#if 0
    // TODO: Try compound literal:
    // '(' type_name ')' '{' initializer_list '}' |
    // '(' type_name ')' '{' initializer_list ',' '}'
    if (current_token == TOKEN_LPAREN) {
        save_scanner_position();
        advance_token();
        if (/* current_token is type name? */) {
            Type *type = parse_type_name();
            expect_token(TOKEN_RPAREN);
            expect_token(TOKEN_LBRACE);

            InitItem *items = parse_initializer_list();
            if (current_token == TOKEN_COMMA)
                advance_token();
            expect_token(TOKEN_RBRACE);

            Expr *expr       = new_expression(EXPR_COMPOUND);
            expr->u.compound.type = type;
            expr->u.compound.init = init;
            return expr;
        }
        // Rewind if not a type name.
        restore_scanner_position();
        current_token = TOKEN_LPAREN;
    }
#endif
    Expr *expr = parse_primary_expression();
    while (1) {
        if (current_token == TOKEN_LBRACKET) {
            advance_token();
            Expr *index = parse_expression();
            expect_token(TOKEN_RBRACKET);
            Expr *new_expr              = new_expression(EXPR_BINARY_OP);
            new_expr->u.binary_op.op    = new_binary_op(BINARY_ADD);
            new_expr->u.binary_op.left  = expr;
            new_expr->u.binary_op.right = index;
            expr                        = new_expr;
        } else if (current_token == TOKEN_LPAREN) {
            advance_token();
            Expr *args = NULL;
            if (current_token != TOKEN_RPAREN) {
                args = parse_argument_expression_list();
            }
            expect_token(TOKEN_RPAREN);
            Expr *new_expr        = new_expression(EXPR_CALL);
            new_expr->u.call.func = expr;
            new_expr->u.call.args = args;
            expr                  = new_expr;
        } else if (current_token == TOKEN_DOT) {
            advance_token();
            Ident field = strdup(current_lexeme);
            expect_token(TOKEN_IDENTIFIER);
            Expr *new_expr                 = new_expression(EXPR_FIELD_ACCESS);
            new_expr->u.field_access.expr  = expr;
            new_expr->u.field_access.field = field;
            expr = new_expr;
        } else if (current_token == TOKEN_PTR_OP) {
            advance_token();
            Ident field = strdup(current_lexeme);
            expect_token(TOKEN_IDENTIFIER);
            Expr *new_expr               = new_expression(EXPR_PTR_ACCESS);
            new_expr->u.ptr_access.expr  = expr;
            new_expr->u.ptr_access.field = field;
            expr = new_expr;
        } else if (current_token == TOKEN_INC_OP) {
            advance_token();
            Expr *new_expr       = new_expression(EXPR_POST_INC);
            new_expr->u.post_inc = expr;
            expr                 = new_expr;
        } else if (current_token == TOKEN_DEC_OP) {
            advance_token();
            Expr *new_expr       = new_expression(EXPR_POST_DEC);
            new_expr->u.post_dec = expr;
            expr                 = new_expr;
        } else {
            break;
        }
    }
    return expr;
}

Expr *parse_argument_expression_list()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = parse_assignment_expression();
    if (current_token == TOKEN_COMMA) {
        advance_token();
        expr->next = parse_argument_expression_list();
    }
    return expr;
}

Expr *parse_unary_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_INC_OP) {
        advance_token();
        Expr *expr                = parse_unary_expression();
        Expr *new_expr            = new_expression(EXPR_UNARY_OP);
        new_expr->u.unary_op.op   = new_unary_op(UNARY_PRE_INC);
        new_expr->u.unary_op.expr = expr;
        return new_expr;
    } else if (current_token == TOKEN_DEC_OP) {
        advance_token();
        Expr *expr                = parse_unary_expression();
        Expr *new_expr            = new_expression(EXPR_UNARY_OP);
        new_expr->u.unary_op.op   = new_unary_op(UNARY_PRE_DEC);
        new_expr->u.unary_op.expr = expr;
        return new_expr;
    } else if (current_token == TOKEN_AMPERSAND || current_token == TOKEN_STAR ||
               current_token == TOKEN_PLUS || current_token == TOKEN_MINUS ||
               current_token == TOKEN_TILDE || current_token == TOKEN_NOT) {
        UnaryOp *op               = parse_unary_operator();
        Expr *expr                = parse_cast_expression();
        Expr *new_expr            = new_expression(EXPR_UNARY_OP);
        new_expr->u.unary_op.op   = op;
        new_expr->u.unary_op.expr = expr;
        return new_expr;
    } else if (current_token == TOKEN_SIZEOF) {
        advance_token();
        if (current_token == TOKEN_LPAREN && is_type_name(next_token())) {
            expect_token(TOKEN_LPAREN);
            Type *type = parse_type_name();
            expect_token(TOKEN_RPAREN);
            Expr *new_expr          = new_expression(EXPR_SIZEOF_TYPE);
            new_expr->u.sizeof_type = type;
            return new_expr;
        } else {
            Expr *expr              = parse_unary_expression();
            Expr *new_expr          = new_expression(EXPR_SIZEOF_EXPR);
            new_expr->u.sizeof_expr = expr;
            return new_expr;
        }
    } else if (current_token == TOKEN_ALIGNOF) {
        advance_token();
        expect_token(TOKEN_LPAREN);
        Type *type = parse_type_name();
        expect_token(TOKEN_RPAREN);
        Expr *new_expr       = new_expression(EXPR_ALIGNOF);
        new_expr->u.align_of = type;
        return new_expr;
    } else {
        return parse_postfix_expression();
    }
}

UnaryOp *parse_unary_operator()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    UnaryOp *op = new_unary_op(current_token == TOKEN_AMPERSAND ? UNARY_ADDRESS
                               : current_token == TOKEN_STAR    ? UNARY_DEREF
                               : current_token == TOKEN_PLUS    ? UNARY_PLUS
                               : current_token == TOKEN_MINUS   ? UNARY_NEG
                               : current_token == TOKEN_TILDE   ? UNARY_BIT_NOT
                                                                : UNARY_LOG_NOT);
    advance_token();
    return op;
}

Expr *parse_cast_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_LPAREN) {
        advance_token();
        Type *type = parse_type_name();
        expect_token(TOKEN_RPAREN);
        Expr *expr            = parse_cast_expression();
        Expr *new_expr        = new_expression(EXPR_CAST);
        new_expr->u.cast.type = type;
        new_expr->u.cast.expr = expr;
        return new_expr;
    }
    return parse_unary_expression();
}

Expr *parse_multiplicative_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = parse_cast_expression();
    while (current_token == TOKEN_STAR || current_token == TOKEN_SLASH ||
           current_token == TOKEN_PERCENT) {
        BinaryOpKind op_kind = current_token == TOKEN_STAR    ? BINARY_MUL
                               : current_token == TOKEN_SLASH ? BINARY_DIV
                                                              : BINARY_MOD;
        advance_token();
        Expr *right                 = parse_cast_expression();
        Expr *new_expr              = new_expression(EXPR_BINARY_OP);
        new_expr->u.binary_op.op    = new_binary_op(op_kind);
        new_expr->u.binary_op.left  = expr;
        new_expr->u.binary_op.right = right;
        expr                        = new_expr;
    }
    return expr;
}

Expr *parse_additive_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = parse_multiplicative_expression();
    while (current_token == TOKEN_PLUS || current_token == TOKEN_MINUS) {
        BinaryOpKind op_kind = current_token == TOKEN_PLUS ? BINARY_ADD : BINARY_SUB;
        advance_token();
        Expr *right                 = parse_multiplicative_expression();
        Expr *new_expr              = new_expression(EXPR_BINARY_OP);
        new_expr->u.binary_op.op    = new_binary_op(op_kind);
        new_expr->u.binary_op.left  = expr;
        new_expr->u.binary_op.right = right;
        expr                        = new_expr;
    }
    return expr;
}

Expr *parse_shift_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = parse_additive_expression();
    while (current_token == TOKEN_LEFT_OP || current_token == TOKEN_RIGHT_OP) {
        BinaryOpKind op_kind =
            current_token == TOKEN_LEFT_OP ? BINARY_LEFT_SHIFT : BINARY_RIGHT_SHIFT;
        advance_token();
        Expr *right                 = parse_additive_expression();
        Expr *new_expr              = new_expression(EXPR_BINARY_OP);
        new_expr->u.binary_op.op    = new_binary_op(op_kind);
        new_expr->u.binary_op.left  = expr;
        new_expr->u.binary_op.right = right;
        expr                        = new_expr;
    }
    return expr;
}

Expr *parse_relational_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = parse_shift_expression();
    while (current_token == TOKEN_LT || current_token == TOKEN_GT || current_token == TOKEN_LE_OP ||
           current_token == TOKEN_GE_OP) {
        BinaryOpKind op_kind = current_token == TOKEN_LT      ? BINARY_LT
                               : current_token == TOKEN_GT    ? BINARY_GT
                               : current_token == TOKEN_LE_OP ? BINARY_LE
                                                              : BINARY_GE;
        advance_token();
        Expr *right                 = parse_shift_expression();
        Expr *new_expr              = new_expression(EXPR_BINARY_OP);
        new_expr->u.binary_op.op    = new_binary_op(op_kind);
        new_expr->u.binary_op.left  = expr;
        new_expr->u.binary_op.right = right;
        expr                        = new_expr;
    }
    return expr;
}

Expr *parse_equality_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = parse_relational_expression();
    while (current_token == TOKEN_EQ_OP || current_token == TOKEN_NE_OP) {
        BinaryOpKind op_kind = current_token == TOKEN_EQ_OP ? BINARY_EQ : BINARY_NE;
        advance_token();
        Expr *right                 = parse_relational_expression();
        Expr *new_expr              = new_expression(EXPR_BINARY_OP);
        new_expr->u.binary_op.op    = new_binary_op(op_kind);
        new_expr->u.binary_op.left  = expr;
        new_expr->u.binary_op.right = right;
        expr                        = new_expr;
    }
    return expr;
}

Expr *parse_and_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = parse_equality_expression();
    while (current_token == TOKEN_AMPERSAND) {
        advance_token();
        Expr *right                 = parse_equality_expression();
        Expr *new_expr              = new_expression(EXPR_BINARY_OP);
        new_expr->u.binary_op.op    = new_binary_op(BINARY_BIT_AND);
        new_expr->u.binary_op.left  = expr;
        new_expr->u.binary_op.right = right;
        expr                        = new_expr;
    }
    return expr;
}

Expr *parse_exclusive_or_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = parse_and_expression();
    while (current_token == TOKEN_CARET) {
        advance_token();
        Expr *right                 = parse_and_expression();
        Expr *new_expr              = new_expression(EXPR_BINARY_OP);
        new_expr->u.binary_op.op    = new_binary_op(BINARY_BIT_XOR);
        new_expr->u.binary_op.left  = expr;
        new_expr->u.binary_op.right = right;
        expr                        = new_expr;
    }
    return expr;
}

Expr *parse_inclusive_or_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = parse_exclusive_or_expression();
    while (current_token == TOKEN_PIPE) {
        advance_token();
        Expr *right                 = parse_exclusive_or_expression();
        Expr *new_expr              = new_expression(EXPR_BINARY_OP);
        new_expr->u.binary_op.op    = new_binary_op(BINARY_BIT_OR);
        new_expr->u.binary_op.left  = expr;
        new_expr->u.binary_op.right = right;
        expr                        = new_expr;
    }
    return expr;
}

Expr *parse_logical_and_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = parse_inclusive_or_expression();
    while (current_token == TOKEN_AND_OP) {
        advance_token();
        Expr *right                 = parse_inclusive_or_expression();
        Expr *new_expr              = new_expression(EXPR_BINARY_OP);
        new_expr->u.binary_op.op    = new_binary_op(BINARY_LOG_AND);
        new_expr->u.binary_op.left  = expr;
        new_expr->u.binary_op.right = right;
        expr                        = new_expr;
    }
    return expr;
}

Expr *parse_logical_or_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = parse_logical_and_expression();
    while (current_token == TOKEN_OR_OP) {
        advance_token();
        Expr *right                 = parse_logical_and_expression();
        Expr *new_expr              = new_expression(EXPR_BINARY_OP);
        new_expr->u.binary_op.op    = new_binary_op(BINARY_LOG_OR);
        new_expr->u.binary_op.left  = expr;
        new_expr->u.binary_op.right = right;
        expr                        = new_expr;
    }
    return expr;
}

Expr *parse_conditional_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = parse_logical_or_expression();
    if (current_token == TOKEN_QUESTION) {
        advance_token();
        Expr *then_expr = parse_expression();
        expect_token(TOKEN_COLON);
        Expr *else_expr            = parse_conditional_expression();
        Expr *new_expr             = new_expression(EXPR_COND);
        new_expr->u.cond.condition = expr;
        new_expr->u.cond.then_expr = then_expr;
        new_expr->u.cond.else_expr = else_expr;
        expr                       = new_expr;
    }
    return expr;
}

Expr *parse_assignment_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = parse_conditional_expression();
    if (current_token == TOKEN_ASSIGN || current_token == TOKEN_MUL_ASSIGN ||
        current_token == TOKEN_DIV_ASSIGN || current_token == TOKEN_MOD_ASSIGN ||
        current_token == TOKEN_ADD_ASSIGN || current_token == TOKEN_SUB_ASSIGN ||
        current_token == TOKEN_LEFT_ASSIGN || current_token == TOKEN_RIGHT_ASSIGN ||
        current_token == TOKEN_AND_ASSIGN || current_token == TOKEN_XOR_ASSIGN ||
        current_token == TOKEN_OR_ASSIGN) {
        AssignOp *op              = parse_assignment_operator();
        Expr *value               = parse_assignment_expression();
        Expr *new_expr            = new_expression(EXPR_ASSIGN);
        new_expr->u.assign.target = expr;
        new_expr->u.assign.op     = op;
        new_expr->u.assign.value  = value;
        expr                      = new_expr;
    }
    return expr;
}

AssignOp *parse_assignment_operator()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    AssignOp *op = new_assign_op(current_token == TOKEN_ASSIGN         ? ASSIGN_SIMPLE
                                 : current_token == TOKEN_MUL_ASSIGN   ? ASSIGN_MUL
                                 : current_token == TOKEN_DIV_ASSIGN   ? ASSIGN_DIV
                                 : current_token == TOKEN_MOD_ASSIGN   ? ASSIGN_MOD
                                 : current_token == TOKEN_ADD_ASSIGN   ? ASSIGN_ADD
                                 : current_token == TOKEN_SUB_ASSIGN   ? ASSIGN_SUB
                                 : current_token == TOKEN_LEFT_ASSIGN  ? ASSIGN_LEFT
                                 : current_token == TOKEN_RIGHT_ASSIGN ? ASSIGN_RIGHT
                                 : current_token == TOKEN_AND_ASSIGN   ? ASSIGN_AND
                                 : current_token == TOKEN_XOR_ASSIGN   ? ASSIGN_XOR
                                                                       : ASSIGN_OR);
    advance_token();
    return op;
}

Expr *parse_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = parse_assignment_expression();
    while (current_token == TOKEN_COMMA) {
        advance_token();
        Expr *next = parse_assignment_expression();
        expr->next = next;
    }
    return expr;
}

Expr *parse_constant_expression()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    return parse_conditional_expression();
}

//
// Fuse TypeSpec list into a single Type
//
Type *fuse_type_specifiers(TypeSpec *specs)
{
    if (!specs) {
        fatal_error("Empty type specifier list");
    }

    /* State for tracking type specifiers */
    TypeKind base_kind     = -1; /* Unset */
    Signedness signedness  = -1; /* Unset */
    int int_count          = 0;  /* For int */
    int long_count         = 0;  /* For long, long long */
    bool is_complex        = false;
    bool is_imaginary      = false;
    bool is_atomic         = false;
    TypeSpec *struct_spec  = NULL;
    TypeSpec *union_spec   = NULL;
    TypeSpec *enum_spec    = NULL;
    TypeSpec *typedef_spec = NULL;
    int specifier_count    = 0;

    /* Collect specifiers */
    for (TypeSpec *s = specs; s; s = s->next) {
        specifier_count++;
        if (s->kind == TYPE_SPEC_BASIC) {
            switch (s->u.basic->kind) {
            case TYPE_VOID:
                if (base_kind != -1) {
                    fatal_error("void cannot combine with other types");
                }
                base_kind = TYPE_VOID;
                break;
            case TYPE_BOOL:
                if (base_kind != -1) {
                    fatal_error("_Bool cannot combine with other types");
                }
                base_kind = TYPE_BOOL;
                break;
            case TYPE_CHAR:
                if (base_kind != -1) {
                    fatal_error("char cannot combine with %s", type_kind_str[base_kind]);
                }
                base_kind = TYPE_CHAR;
                break;
            case TYPE_SHORT:
                if (base_kind != -1 && base_kind != TYPE_INT) {
                    fatal_error("short cannot combine with %s", type_kind_str[base_kind]);
                }
                base_kind = TYPE_SHORT;
                break;
            case TYPE_INT:
                if (base_kind != -1 && base_kind != TYPE_SHORT && base_kind != TYPE_LONG) {
                    fatal_error("int cannot combine with %s", type_kind_str[base_kind]);
                }
                if (int_count > 0) {
                    fatal_error("multiple int specifiers");
                }
                int_count++;
                if (base_kind == -1) {
                    base_kind = TYPE_INT;
                }
                break;
            case TYPE_LONG:
                if (base_kind != -1 && base_kind != TYPE_INT && base_kind != TYPE_LONG &&
                    base_kind != TYPE_DOUBLE) {
                    fatal_error("long cannot combine with %s", type_kind_str[base_kind]);
                }
                if (long_count > 2) {
                    fatal_error("too many long specifiers");
                }
                long_count++;
                if (base_kind == TYPE_DOUBLE) {
                    base_kind = TYPE_LONG_DOUBLE;
                } else if (long_count == 2) {
                    base_kind = TYPE_LONG_LONG;
                } else {
                    base_kind = TYPE_LONG;
                }
                break;
            case TYPE_FLOAT:
                if (base_kind != -1 && base_kind != TYPE_COMPLEX && base_kind != TYPE_IMAGINARY) {
                    fatal_error("float cannot combine with %s", type_kind_str[base_kind]);
                }
                base_kind = TYPE_FLOAT;
                break;
            case TYPE_DOUBLE:
                if (base_kind != -1 && base_kind != TYPE_LONG && base_kind != TYPE_COMPLEX &&
                    base_kind != TYPE_IMAGINARY) {
                    fatal_error("double cannot combine with %s", type_kind_str[base_kind]);
                }
                base_kind = TYPE_DOUBLE;
                break;
            case TYPE_SIGNED:
                if (base_kind != -1 && base_kind != TYPE_CHAR && base_kind != TYPE_SHORT &&
                    base_kind != TYPE_INT && base_kind != TYPE_LONG) {
                    fatal_error("signed cannot combine with %s", type_kind_str[base_kind]);
                }
                signedness = SIGNED_SIGNED;
                break;
            case TYPE_UNSIGNED:
                if (base_kind != -1 && base_kind != TYPE_CHAR && base_kind != TYPE_SHORT &&
                    base_kind != TYPE_INT && base_kind != TYPE_LONG) {
                    fatal_error("unsigned cannot combine with %s", type_kind_str[base_kind]);
                }
                signedness = SIGNED_UNSIGNED;
                break;
            case TYPE_COMPLEX:
                if (base_kind != -1 && base_kind != TYPE_FLOAT && base_kind != TYPE_DOUBLE) {
                    fatal_error("_Complex cannot combine with %s", type_kind_str[base_kind]);
                }
                is_complex = true;
                if (base_kind == -1)
                    base_kind = TYPE_DOUBLE; /* Default for _Complex */
                break;
            case TYPE_IMAGINARY:
                if (base_kind != -1 && base_kind != TYPE_FLOAT && base_kind != TYPE_DOUBLE) {
                    fatal_error("_Imaginary cannot combine with %s", type_kind_str[base_kind]);
                }
                is_imaginary = true;
                if (base_kind == -1)
                    base_kind = TYPE_DOUBLE; /* Default for _Imaginary */
                break;
            default:
                fatal_error("Unknown basic type specifier");
            }
        } else if (s->kind == TYPE_SPEC_STRUCT) {
            if (struct_spec || union_spec || enum_spec || typedef_spec || base_kind != -1) {
                fatal_error("struct cannot combine with other distinct types");
            }
            struct_spec = s;
        } else if (s->kind == TYPE_SPEC_UNION) {
            if (struct_spec || union_spec || enum_spec || typedef_spec || base_kind != -1) {
                fatal_error("union cannot combine with other distinct types");
            }
            union_spec = s;
        } else if (s->kind == TYPE_SPEC_ENUM) {
            if (struct_spec || union_spec || enum_spec || typedef_spec || base_kind != -1) {
                fatal_error("enum cannot combine with other distinct types");
            }
            enum_spec = s;
        } else if (s->kind == TYPE_SPEC_TYPEDEF_NAME) {
            if (struct_spec || union_spec || enum_spec || typedef_spec || base_kind != -1) {
                fatal_error("typedef name cannot combine with other distinct types");
            }
            typedef_spec = s;
        } else if (s->kind == TYPE_SPEC_ATOMIC) {
            fatal_error("Atomic() is not supported");
        }
    }

    /* Validate and construct Type */
    Type *result = NULL;

    if (struct_spec) {
        result                    = new_type(TYPE_STRUCT);
        result->u.struct_t.name   = struct_spec->u.struct_spec.name;
        result->u.struct_t.fields = struct_spec->u.struct_spec.fields;
    } else if (union_spec) {
        result                    = new_type(TYPE_UNION);
        result->u.struct_t.name   = union_spec->u.struct_spec.name;
        result->u.struct_t.fields = union_spec->u.struct_spec.fields;
    } else if (enum_spec) {
        result                       = new_type(TYPE_ENUM);
        result->u.enum_t.name        = enum_spec->u.enum_spec.name;
        result->u.enum_t.enumerators = enum_spec->u.enum_spec.enumerators;
    } else if (typedef_spec) {
        result                      = new_type(TYPE_TYPEDEF_NAME);
        result->u.typedef_name.name = typedef_spec->u.typedef_name.name;
    } else {
        /* Handle basic types */
        if (base_kind == -1) {
            if (signedness == -1) {
                fatal_error("No valid type specifier provided");
            }
            // Signed/unsigned defaults to int.
            base_kind = TYPE_INT;
        }
        if (is_complex && is_imaginary) {
            fatal_error("_Complex and _Imaginary cannot combine");
        }
        if ((is_complex || is_imaginary) && (base_kind != TYPE_FLOAT && base_kind != TYPE_DOUBLE)) {
            fatal_error("_Complex/_Imaginary require float or double");
        }
        if ((signedness == SIGNED_UNSIGNED || long_count > 0) &&
            (base_kind == TYPE_FLOAT || base_kind == TYPE_DOUBLE)) {
            fatal_error("signed/unsigned/long cannot combine with float/double");
        }
        if (base_kind == TYPE_VOID || base_kind == TYPE_BOOL) {
            if (long_count > 0 || signedness == SIGNED_UNSIGNED || is_complex || is_imaginary) {
                fatal_error("void/_Bool cannot combine with modifiers");
            }
        }

        /* Create Type based on base_kind */
        if (is_complex) {
            result                 = new_type(TYPE_COMPLEX);
            result->u.complex.base = new_type(base_kind);
        } else if (is_imaginary) {
            result                 = new_type(TYPE_IMAGINARY);
            result->u.complex.base = new_type(base_kind);
        } else {
            result = new_type(base_kind);
            if (base_kind == TYPE_CHAR || base_kind == TYPE_SHORT || base_kind == TYPE_INT ||
                base_kind == TYPE_LONG) {
                if (signedness == -1) {
                    signedness = SIGNED_SIGNED; // Default
                }
                result->u.integer.signedness = signedness;
            }
        }
    }
    return result;
}

Declaration *parse_declaration()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_STATIC_ASSERT) {
        return parse_static_assert_declaration();
    }
    DeclSpec *specifiers = parse_declaration_specifiers();
    if (current_token == TOKEN_SEMICOLON) {
        advance_token();
        Declaration *decl      = new_declaration(DECL_EMPTY);
        decl->u.var.specifiers = specifiers;
        return decl;
    }
    InitDeclarator *declarators = parse_init_declarator_list();
    expect_token(TOKEN_SEMICOLON);
    Declaration *decl       = new_declaration(DECL_VAR);
    decl->u.var.specifiers  = specifiers;
    decl->u.var.declarators = declarators;
    return decl;
}

DeclSpec *parse_declaration_specifiers()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    DeclSpec *ds = new_decl_spec();
    TypeSpec *type_specs = NULL;
    while (1) {
        if (current_token == TOKEN_TYPEDEF || current_token == TOKEN_EXTERN ||
            current_token == TOKEN_STATIC || current_token == TOKEN_THREAD_LOCAL ||
            current_token == TOKEN_AUTO || current_token == TOKEN_REGISTER) {
            ds->storage = parse_storage_class_specifier();
        } else if (current_token == TOKEN_VOID || current_token == TOKEN_CHAR ||
                   current_token == TOKEN_SHORT || current_token == TOKEN_INT ||
                   current_token == TOKEN_LONG || current_token == TOKEN_FLOAT ||
                   current_token == TOKEN_DOUBLE || current_token == TOKEN_SIGNED ||
                   current_token == TOKEN_UNSIGNED || current_token == TOKEN_BOOL ||
                   current_token == TOKEN_COMPLEX || current_token == TOKEN_IMAGINARY ||
                   current_token == TOKEN_STRUCT || current_token == TOKEN_UNION ||
                   current_token == TOKEN_ENUM || current_token == TOKEN_TYPEDEF_NAME ||
                   (current_token == TOKEN_ATOMIC && next_token() == TOKEN_LPAREN)) {
            TypeSpec *ts = parse_type_specifier();
            append_list(&type_specs, ts);
        } else if (current_token == TOKEN_CONST || current_token == TOKEN_RESTRICT ||
                   current_token == TOKEN_VOLATILE || current_token == TOKEN_ATOMIC) {
            TypeQualifier *q = parse_type_qualifier();
            append_list(&ds->qualifiers, q);
        } else if (current_token == TOKEN_INLINE || current_token == TOKEN_NORETURN) {
            FunctionSpec *fs = parse_function_specifier();
            append_list(&ds->func_specs, fs);
        } else if (current_token == TOKEN_ALIGNAS) {
            ds->align_spec = parse_alignment_specifier();
        } else {
            break;
        }
    }
    ds->base_type = fuse_type_specifiers(type_specs);
    return ds;
}

InitDeclarator *parse_init_declarator_list()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    InitDeclarator *decl = parse_init_declarator();
    if (current_token == TOKEN_COMMA) {
        advance_token();
        decl->next = parse_init_declarator_list();
    }
    return decl;
}

InitDeclarator *parse_init_declarator()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Declarator *declarator = parse_declarator();
    Initializer *init      = NULL;
    if (current_token == TOKEN_ASSIGN) {
        advance_token();
        init = parse_initializer();
    }
    return new_init_declarator(declarator, init);
}

StorageClass *parse_storage_class_specifier()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    StorageClassKind kind = current_token == TOKEN_TYPEDEF        ? STORAGE_CLASS_TYPEDEF
                            : current_token == TOKEN_EXTERN       ? STORAGE_CLASS_EXTERN
                            : current_token == TOKEN_STATIC       ? STORAGE_CLASS_STATIC
                            : current_token == TOKEN_THREAD_LOCAL ? STORAGE_CLASS_THREAD_LOCAL
                            : current_token == TOKEN_AUTO         ? STORAGE_CLASS_AUTO
                                                                  : STORAGE_CLASS_REGISTER;
    advance_token();
    return new_storage_class(kind);
}

TypeSpec *parse_type_specifier()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    TypeSpec *ts;
    if (current_token == TOKEN_VOID) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_VOID);
        advance_token();
    } else if (current_token == TOKEN_CHAR) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_CHAR);
        advance_token();
    } else if (current_token == TOKEN_SHORT) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_SHORT);
        advance_token();
    } else if (current_token == TOKEN_INT) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_INT);
        advance_token();
    } else if (current_token == TOKEN_LONG) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_LONG);
        advance_token();
    } else if (current_token == TOKEN_FLOAT) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_FLOAT);
        advance_token();
    } else if (current_token == TOKEN_DOUBLE) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_DOUBLE);
        advance_token();
    } else if (current_token == TOKEN_SIGNED) {
        ts                               = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic                      = new_type(TYPE_SIGNED);
        ts->u.basic->u.integer.signedness = SIGNED_SIGNED;
        advance_token();
    } else if (current_token == TOKEN_UNSIGNED) {
        ts                               = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic                      = new_type(TYPE_UNSIGNED);
        ts->u.basic->u.integer.signedness = SIGNED_UNSIGNED;
        advance_token();
    } else if (current_token == TOKEN_BOOL) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_BOOL);
        advance_token();
    } else if (current_token == TOKEN_COMPLEX) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_COMPLEX);
        advance_token();
    } else if (current_token == TOKEN_IMAGINARY) {
        ts          = new_type_spec(TYPE_SPEC_BASIC);
        ts->u.basic = new_type(TYPE_IMAGINARY);
        advance_token();
    } else if (current_token == TOKEN_ATOMIC && next_token() == TOKEN_LPAREN) {
        ts                = new_type_spec(TYPE_SPEC_ATOMIC);
        ts->u.atomic.type = parse_atomic_type_specifier();
    } else if (current_token == TOKEN_STRUCT || current_token == TOKEN_UNION) {
        Type *type = parse_struct_or_union_specifier();
        ts         = new_type_spec(type->kind == TYPE_STRUCT ? TYPE_SPEC_STRUCT : TYPE_SPEC_UNION);
        ts->u.struct_spec.name   = type->u.struct_t.name;
        ts->u.struct_spec.fields = type->u.struct_t.fields;
    } else if (current_token == TOKEN_ENUM) {
        Type *type                  = parse_enum_specifier();
        ts                          = new_type_spec(TYPE_SPEC_ENUM);
        ts->u.enum_spec.name        = type->u.enum_t.name;
        ts->u.enum_spec.enumerators = type->u.enum_t.enumerators;
    } else if (current_token == TOKEN_TYPEDEF_NAME) {
        ts                      = new_type_spec(TYPE_SPEC_TYPEDEF_NAME);
        ts->u.typedef_name.name = strdup(current_lexeme);
        advance_token();
    } else {
        fatal_error("Expected type specifier");
    }
    return ts;
}

Type *parse_struct_or_union_specifier()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    int su     = parse_struct_or_union();
    Type *type = new_type(su == TOKEN_STRUCT ? TYPE_STRUCT : TYPE_UNION);
    if (current_token == TOKEN_IDENTIFIER) {
        type->u.struct_t.name = strdup(current_lexeme);
        advance_token();
    }
    if (current_token == TOKEN_LBRACE) {
        advance_token();
        type->u.struct_t.fields = parse_struct_declaration_list();
        expect_token(TOKEN_RBRACE);
    }
    return type;
}

int parse_struct_or_union()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token != TOKEN_STRUCT && current_token != TOKEN_UNION) {
        fatal_error("Expected struct or union");
    }
    int su = current_token;
    advance_token();
    return su;
}

Field *parse_struct_declaration_list()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Field *fields = parse_struct_declaration();
    if (current_token != TOKEN_RBRACE) {
        fields->next = parse_struct_declaration_list();
    }
    return fields;
}

Field *parse_struct_declaration()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_STATIC_ASSERT) {
        parse_static_assert_declaration(); // TODO: Ignore for now
        return NULL;
    }

    /* Parse specifier_qualifier_list */
    TypeQualifier *qualifiers = NULL;
    TypeSpec *type_specs      = parse_specifier_qualifier_list(&qualifiers);
    if (!type_specs)
        return NULL;

    /* Check for anonymous struct/union */
    if (current_token == TOKEN_SEMICOLON) {
        Field *field            = new_field();
        field->is_anonymous     = true;
        field->u.anonymous.type = parse_type_name(); // Wrong! use type_specs
        if (!field->u.anonymous.type) {
            free(field);
            return NULL;
        }
        advance_token(); /* Consume ';' */
        return field;
    }

    /* Parse struct_declarator_list */
    Field *fields = NULL, **fields_tail = &fields;
    do {
        Field *field            = new_field();
        field->is_anonymous     = false;
        field->u.named.name     = (current_token == TOKEN_IDENTIFIER) ? strdup(current_lexeme) : NULL;
        field->u.named.bitfield = NULL;

        /* Parse field type */
        field->u.named.type = parse_type_name(); // Wrong! use type_specs
        if (!field->u.named.type) {
            free(field);
            return NULL;
        }

        if (field->u.named.name)
            advance_token();

        /* Handle bitfield */
        if (current_token == TOKEN_COLON) {
            advance_token();
            field->u.named.bitfield = parse_constant_expression();
            if (!field->u.named.bitfield) {
                free(field);
                return NULL;
            }
        }

        *fields_tail = field;
        fields_tail  = &field->next;

        if (current_token == TOKEN_COMMA) {
            advance_token();
        } else {
            break;
        }
    } while (current_token != TOKEN_SEMICOLON);

    if (current_token != TOKEN_SEMICOLON) {
        /* Free allocated fields on error */
        while (fields) {
            Field *next = fields->next;
            free(fields->u.named.name);
            free(fields);
            fields = next;
        }
        return NULL;
    }

    advance_token(); /* Consume ';' */
    return fields;
}

TypeSpec *parse_specifier_qualifier_list(TypeQualifier **qualifiers)
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    TypeSpec *type_specs = NULL;
    *qualifiers          = NULL;

    while (1) {
        if (current_token == TOKEN_CONST || current_token == TOKEN_RESTRICT ||
            current_token == TOKEN_VOLATILE ||
            (current_token == TOKEN_ATOMIC && next_token() != TOKEN_LPAREN)) {
            /* Parse type_qualifier */
            TypeQualifierKind q_kind;
            switch (current_token) {
            case TOKEN_CONST:
                q_kind = TYPE_QUALIFIER_CONST;
                break;
            case TOKEN_RESTRICT:
                q_kind = TYPE_QUALIFIER_RESTRICT;
                break;
            case TOKEN_VOLATILE:
                q_kind = TYPE_QUALIFIER_VOLATILE;
                break;
            case TOKEN_ATOMIC:
                q_kind = TYPE_QUALIFIER_ATOMIC;
                break;
            default:
                return NULL; /* Unreachable */
            }
            append_list(qualifiers, new_type_qualifier(q_kind));
            advance_token();
        } else if (current_token == TOKEN_VOID || current_token == TOKEN_CHAR ||
                   current_token == TOKEN_SHORT || current_token == TOKEN_INT ||
                   current_token == TOKEN_LONG || current_token == TOKEN_FLOAT ||
                   current_token == TOKEN_DOUBLE || current_token == TOKEN_SIGNED ||
                   current_token == TOKEN_UNSIGNED || current_token == TOKEN_BOOL ||
                   current_token == TOKEN_COMPLEX || current_token == TOKEN_IMAGINARY ||
                   current_token == TOKEN_STRUCT || current_token == TOKEN_UNION ||
                   current_token == TOKEN_ENUM || current_token == TOKEN_TYPEDEF_NAME ||
                   (current_token == TOKEN_ATOMIC && next_token() == TOKEN_LPAREN)) {
            /* Parse type_specifier */
            TypeSpec *ts = NULL;
            if (current_token == TOKEN_VOID) {
                ts          = new_type_spec(TYPE_SPEC_BASIC);
                ts->u.basic = new_type(TYPE_VOID);
                advance_token();
            } else if (current_token == TOKEN_CHAR) {
                ts                               = new_type_spec(TYPE_SPEC_BASIC);
                ts->u.basic                      = new_type(TYPE_CHAR);
                ts->u.basic->u.integer.signedness = SIGNED_SIGNED;
                advance_token();
            } else if (current_token == TOKEN_SHORT) {
                ts                               = new_type_spec(TYPE_SPEC_BASIC);
                ts->u.basic                      = new_type(TYPE_SHORT);
                ts->u.basic->u.integer.signedness = SIGNED_SIGNED;
                advance_token();
            } else if (current_token == TOKEN_INT) {
                ts                               = new_type_spec(TYPE_SPEC_BASIC);
                ts->u.basic                      = new_type(TYPE_INT);
                ts->u.basic->u.integer.signedness = SIGNED_SIGNED;
                advance_token();
            } else if (current_token == TOKEN_LONG) {
                ts                               = new_type_spec(TYPE_SPEC_BASIC);
                ts->u.basic                      = new_type(TYPE_LONG);
                ts->u.basic->u.integer.signedness = SIGNED_SIGNED;
                advance_token();
            } else if (current_token == TOKEN_FLOAT) {
                ts          = new_type_spec(TYPE_SPEC_BASIC);
                ts->u.basic = new_type(TYPE_FLOAT);
                advance_token();
            } else if (current_token == TOKEN_DOUBLE) {
                ts          = new_type_spec(TYPE_SPEC_BASIC);
                ts->u.basic = new_type(TYPE_DOUBLE);
                advance_token();
            } else if (current_token == TOKEN_SIGNED) {
                ts                               = new_type_spec(TYPE_SPEC_BASIC);
                ts->u.basic                      = new_type(TYPE_SIGNED); // Later fused to int
                ts->u.basic->u.integer.signedness = SIGNED_SIGNED;
                advance_token();
            } else if (current_token == TOKEN_UNSIGNED) {
                ts                               = new_type_spec(TYPE_SPEC_BASIC);
                ts->u.basic                      = new_type(TYPE_UNSIGNED); // Later fused to int
                ts->u.basic->u.integer.signedness = SIGNED_UNSIGNED;
                advance_token();
            } else if (current_token == TOKEN_BOOL) {
                ts          = new_type_spec(TYPE_SPEC_BASIC);
                ts->u.basic = new_type(TYPE_BOOL);
                advance_token();
            } else if (current_token == TOKEN_COMPLEX) {
                ts                          = new_type_spec(TYPE_SPEC_BASIC);
                ts->u.basic                 = new_type(TYPE_COMPLEX);
                ts->u.basic->u.complex.base = new_type(TYPE_DOUBLE); /* Default */
                advance_token();
            } else if (current_token == TOKEN_IMAGINARY) {
                ts                          = new_type_spec(TYPE_SPEC_BASIC);
                ts->u.basic                 = new_type(TYPE_IMAGINARY);
                ts->u.basic->u.complex.base = new_type(TYPE_DOUBLE); /* Default */
                advance_token();
            } else if (current_token == TOKEN_TYPEDEF_NAME) {
                ts                      = new_type_spec(TYPE_SPEC_TYPEDEF_NAME);
                ts->u.typedef_name.name = strdup(current_lexeme);
                advance_token();
            } else if (current_token == TOKEN_ATOMIC) {
                advance_token();
                if (current_token != TOKEN_LPAREN)
                    return NULL; /* Expected '(' */
                advance_token();
                Type *base = parse_type_name();
                if (!base)
                    return NULL;
                if (current_token != TOKEN_RPAREN)
                    return NULL; /* Expected ')' */
                advance_token();
                ts                = new_type_spec(TYPE_SPEC_ATOMIC);
                ts->u.atomic.type = base;
            } else if (current_token == TOKEN_STRUCT || current_token == TOKEN_UNION) {
                bool is_struct = (current_token == TOKEN_STRUCT);
                advance_token();
                ts = new_type_spec(is_struct ? TYPE_SPEC_STRUCT : TYPE_SPEC_UNION);
                if (current_token == TOKEN_IDENTIFIER) {
                    ts->u.struct_spec.name = strdup(current_lexeme);
                    advance_token();
                } else {
                    ts->u.struct_spec.name = NULL;
                }
                if (current_token == TOKEN_LBRACE) {
                    advance_token();
                    /* Parse struct_declaration_list */
                    Field *fields = NULL, **fields_tail = &fields;
                    while (current_token != TOKEN_RBRACE) {
                        if (current_token == TOKEN_STATIC_ASSERT) {
                            /* Skip static_assert_declaration */
                            advance_token();
                            if (current_token != TOKEN_LPAREN)
                                return NULL;
                            advance_token();
                            Expr *cond = parse_constant_expression(); // TODO: check
                            if (!cond || current_token != TOKEN_COMMA)
                                return NULL;
                            advance_token();
                            if (current_token != TOKEN_STRING_LITERAL)
                                return NULL;
                            advance_token();
                            if (current_token != TOKEN_RPAREN || next_token() != TOKEN_SEMICOLON)
                                return NULL;
                            advance_token();
                            advance_token();
                        } else {
                            TypeQualifier *field_quals = NULL;
                            TypeSpec *field_specs = parse_specifier_qualifier_list(&field_quals);
                            if (!field_specs)
                                return NULL;
                            if (current_token == TOKEN_SEMICOLON) {
                                /* Anonymous struct/union */
                                Field *field            = (Field *)malloc(sizeof(Field));
                                field->is_anonymous     = true;
                                field->u.anonymous.type = fuse_type_specifiers(field_specs);
                                if (!field->u.anonymous.type) {
                                    fatal_error("Incorrect type of anonymous field");
                                }
                                field->u.anonymous.type->qualifiers = field_quals;
                                field->next  = NULL;
                                *fields_tail = field;
                                fields_tail  = &field->next;
                                advance_token();
                            } else {
                                /* struct_declarator_list */
                                do {
                                    Field *field            = (Field *)malloc(sizeof(Field));
                                    field->is_anonymous     = false;
                                    field->u.named.name     = (current_token == TOKEN_IDENTIFIER)
                                                                  ? strdup(current_lexeme)
                                                                  : NULL;
                                    field->u.named.bitfield = NULL;
                                    field->u.named.type = fuse_type_specifiers(field_specs);
                                    if (!field->u.named.type) {
                                        fatal_error("Incorrect type of named field");
                                    }
                                    field->u.named.type->qualifiers = field_quals;
                                    field->next = NULL;
                                    if (field->u.named.name)
                                        advance_token();
                                    if (current_token == TOKEN_COLON) {
                                        advance_token();
                                        field->u.named.bitfield = parse_constant_expression(); // TODO: check
                                        if (!field->u.named.bitfield)
                                            return NULL;
                                    }
                                    *fields_tail = field;
                                    fields_tail  = &field->next;
                                    if (current_token == TOKEN_COMMA)
                                        advance_token();
                                    else
                                        break;
                                } while (current_token != TOKEN_SEMICOLON);
                                if (current_token != TOKEN_SEMICOLON)
                                    return NULL;
                                advance_token();
                            }
                        }
                    }
                    ts->u.struct_spec.fields = fields;
                    advance_token(); /* Consume '}' */
                }
            } else if (current_token == TOKEN_ENUM) {
                advance_token();
                ts = new_type_spec(TYPE_SPEC_ENUM);
                if (current_token == TOKEN_IDENTIFIER) {
                    ts->u.enum_spec.name = strdup(current_lexeme);
                    advance_token();
                } else {
                    ts->u.enum_spec.name = NULL;
                }
                if (current_token == TOKEN_LBRACE) {
                    advance_token();
                    Enumerator *enums = NULL, **enums_tail = &enums;
                    do {
                        if (current_token != TOKEN_IDENTIFIER)
                            return NULL;
                        Enumerator *e = (Enumerator *)malloc(sizeof(Enumerator));
                        e->name       = strdup(current_lexeme);
                        e->value      = NULL;
                        e->next       = NULL;
                        advance_token();
                        if (current_token == TOKEN_ASSIGN) {
                            advance_token();
                            e->value = parse_constant_expression(); // TODO: check
                            if (!e->value)
                                return NULL;
                        }
                        *enums_tail = e;
                        enums_tail  = &e->next;
                        if (current_token == TOKEN_COMMA)
                            advance_token();
                        else
                            break;
                    } while (current_token != TOKEN_RBRACE);
                    if (current_token != TOKEN_RBRACE)
                        return NULL;
                    advance_token();
                    ts->u.enum_spec.enumerators = enums;
                }
            }
            append_list(&type_specs, ts);
        } else {
            break; /* End of specifier_qualifier_list */
        }
    }
    return type_specs;
}

Declarator *parse_struct_declarator_list()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Declarator *decl = parse_struct_declarator();
    if (current_token == TOKEN_COMMA) {
        advance_token();
        decl->next = parse_struct_declarator_list();
    }
    return decl;
}

Declarator *parse_struct_declarator()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_COLON) {
        advance_token();
        Expr *bitfield = parse_constant_expression();
        return new_declarator(DECLARATOR_NAMED); /* Placeholder */
    }
    Declarator *decl = parse_declarator();
    if (current_token == TOKEN_COLON) {
        advance_token();
        Expr *bitfield = parse_constant_expression();
        /* Update decl with bitfield */
    }
    return decl;
}

Type *parse_enum_specifier()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_ENUM);
    Type *type = new_type(TYPE_ENUM);
    if (current_token == TOKEN_IDENTIFIER) {
        type->u.enum_t.name = strdup(current_lexeme);
        advance_token();
    }
    if (current_token == TOKEN_LBRACE) {
        advance_token();
        type->u.enum_t.enumerators = parse_enumerator_list();
        if (current_token == TOKEN_COMMA)
            advance_token();
        expect_token(TOKEN_RBRACE);
    }
    return type;
}

Enumerator *parse_enumerator_list()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Enumerator *enumr = parse_enumerator();
    if (current_token == TOKEN_COMMA && next_token() != TOKEN_RBRACE) {
        advance_token();
        enumr->next = parse_enumerator_list();
    }
    return enumr;
}

Enumerator *parse_enumerator()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Ident name = strdup(current_lexeme);
    expect_token(TOKEN_IDENTIFIER);
    Expr *value = NULL;
    if (current_token == TOKEN_ASSIGN) {
        advance_token();
        value = parse_constant_expression();
    }
    return new_enumerator(name, value);
}

Type *parse_atomic_type_specifier()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_ATOMIC);
    expect_token(TOKEN_LPAREN);
    Type *type = parse_type_name();
    expect_token(TOKEN_RPAREN);
    Type *atomic          = new_type(TYPE_ATOMIC);
    atomic->u.atomic.base = type;
    return atomic;
}

TypeQualifier *parse_type_qualifier()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    TypeQualifierKind kind = current_token == TOKEN_CONST      ? TYPE_QUALIFIER_CONST
                             : current_token == TOKEN_RESTRICT ? TYPE_QUALIFIER_RESTRICT
                             : current_token == TOKEN_VOLATILE ? TYPE_QUALIFIER_VOLATILE
                                                               : TYPE_QUALIFIER_ATOMIC;
    advance_token();
    return new_type_qualifier(kind);
}

FunctionSpec *parse_function_specifier()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    FunctionSpecKind kind = current_token == TOKEN_INLINE ? FUNC_SPEC_INLINE : FUNC_SPEC_NORETURN;
    advance_token();
    return new_function_spec(kind);
}

AlignmentSpec *parse_alignment_specifier()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_ALIGNAS);
    expect_token(TOKEN_LPAREN);
    AlignmentSpec *as;
    if (current_token == TOKEN_VOID || current_token == TOKEN_CHAR || /* Type check simplified */
        current_token == TOKEN_STRUCT || current_token == TOKEN_UNION ||
        current_token == TOKEN_ENUM || current_token == TOKEN_TYPEDEF_NAME) {
        Type *type = parse_type_name();
        as         = new_alignment_spec(ALIGN_SPEC_TYPE);
        as->u.type = type;
    } else {
        Expr *expr = parse_constant_expression();
        as         = new_alignment_spec(ALIGN_SPEC_EXPR);
        as->u.expr = expr;
    }
    expect_token(TOKEN_RPAREN);
    return as;
}

Declarator *parse_declarator()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Pointer *pointers = NULL;
    if (current_token == TOKEN_STAR) {
        pointers = parse_pointer();
    }
    Declarator *decl = parse_direct_declarator();
    if (pointers) {
        append_list(&decl->u.named.pointers, pointers);
    }
    return decl;
}

Declarator *parse_direct_declarator()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Declarator *decl;
    if (current_token == TOKEN_IDENTIFIER) {
        decl               = new_declarator(DECLARATOR_NAMED);
        decl->u.named.name = strdup(current_lexeme);
        advance_token();
    } else if (current_token == TOKEN_LPAREN) {
        advance_token();
        decl = parse_declarator();
        expect_token(TOKEN_RPAREN);
    } else {
        fatal_error("Expected identifier or '('");
    }
    while (1) {
        if (current_token == TOKEN_LBRACKET) {
            advance_token();
            DeclaratorSuffix *suffix = new_declarator_suffix(SUFFIX_ARRAY);
            if (current_token == TOKEN_STATIC) {
                advance_token();
                suffix->u.array.is_static = true;
            }
            TypeQualifier *qualifiers = NULL;
            if (current_token == TOKEN_CONST || current_token == TOKEN_RESTRICT ||
                current_token == TOKEN_VOLATILE ||
                (current_token == TOKEN_ATOMIC && next_token() != TOKEN_LPAREN)) {
                qualifiers = parse_type_qualifier_list();
            }
            Expr *size = NULL;
            if (current_token != TOKEN_RBRACKET && current_token != TOKEN_STAR) {
                size = parse_assignment_expression();
            }
            if (current_token == TOKEN_STAR) {
                advance_token();
            }
            expect_token(TOKEN_RBRACKET);
            suffix->u.array.qualifiers = qualifiers;
            suffix->u.array.size       = size;
            append_list(&decl->u.named.suffixes, suffix);
        } else if (current_token == TOKEN_LPAREN) {
            advance_token();
            DeclaratorSuffix *suffix = new_declarator_suffix(SUFFIX_FUNCTION);
            ParamList *params        = NULL;
            if (current_token != TOKEN_RPAREN) {
                params = parse_parameter_type_list();
            } else {
                params = new_param_list();
            }
            expect_token(TOKEN_RPAREN);
            suffix->u.function.params   = params;
            suffix->u.function.variadic = params->u.params && params->u.params->next;
            append_list(&decl->u.named.suffixes, suffix);
        } else {
            break;
        }
    }
    return decl;
}

Pointer *parse_pointer()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Pointer *pointers = NULL, **pointers_tail = &pointers;
    while (current_token == TOKEN_STAR) {
        Pointer *p = new_pointer();
        advance_token();
        p->qualifiers  = parse_type_qualifier_list();
        *pointers_tail = p;
        pointers_tail  = &p->next;
    }
    return pointers;
}

TypeQualifier *parse_type_qualifier_list()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    TypeQualifier *qualifiers = NULL, **qualifiers_tail = &qualifiers;
    while (current_token == TOKEN_CONST || current_token == TOKEN_RESTRICT ||
           current_token == TOKEN_VOLATILE ||
           (current_token == TOKEN_ATOMIC && next_token() != TOKEN_LPAREN)) {
        TypeQualifier *q = new_type_qualifier(TYPE_QUALIFIER_CONST); /* Default */
        switch (current_token) {
        case TOKEN_CONST:
            q->kind = TYPE_QUALIFIER_CONST;
            break;
        case TOKEN_RESTRICT:
            q->kind = TYPE_QUALIFIER_RESTRICT;
            break;
        case TOKEN_VOLATILE:
            q->kind = TYPE_QUALIFIER_VOLATILE;
            break;
        case TOKEN_ATOMIC:
            q->kind = TYPE_QUALIFIER_ATOMIC;
            break;
        default:
            return NULL; /* Unreachable */
        }
        advance_token();
        *qualifiers_tail = q;
        qualifiers_tail  = &q->next;
    }
    return qualifiers;
}

ParamList *parse_parameter_type_list()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_RPAREN) {
        ParamList *pl     = (ParamList *)malloc(sizeof(ParamList));
        pl->is_empty      = true;
        return pl;
    }
    ParamList *pl     = (ParamList *)malloc(sizeof(ParamList));
    pl->is_empty      = false;
    Param *params = NULL, **params_tail = &params;
    do {
        Param *param = (Param *)malloc(sizeof(Param));
        param->type = parse_type_name();
        if (!param->type) {
            fatal_error("Incorrect parameter type");
        }
        param->name = (current_token == TOKEN_IDENTIFIER) ? strdup(current_lexeme) : NULL;
        param->next = NULL;
        if (param->name)
            advance_token();
        *params_tail = param;
        params_tail  = &param->next;
        if (current_token == TOKEN_COMMA) {
            advance_token();
            if (current_token == TOKEN_ELLIPSIS) {
                advance_token();
                pl->u.params = params;
                return pl; /* Variadic */
            }
        } else {
            break;
        }
    } while (current_token != TOKEN_RPAREN);
    pl->u.params = params;
    return pl;
}

Param *parse_parameter_list()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Param *param = parse_parameter_declaration();
    if (current_token == TOKEN_COMMA && next_token() != TOKEN_ELLIPSIS) {
        advance_token();
        param->next = parse_parameter_list();
    }
    return param;
}

DeclaratorSuffix *parse_direct_abstract_declarator()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    DeclaratorSuffix *suffix = NULL;
    DeclaratorSuffix **tail  = &suffix; // Pointer to the last suffix's next field

    while (1) {
        if (current_token == TOKEN_LPAREN) {
            // Handle '(' abstract_declarator ')' or '(' parameter_type_list ')' or '(' ')'
            advance_token(); // Consume '('
            if (current_token == TOKEN_RPAREN) {
                // Case: '(' ')'
                advance_token(); // Consume ')'
                DeclaratorSuffix *new_suffix                 = new_declarator_suffix(SUFFIX_FUNCTION);
                new_suffix->u.function.params                = new_param_list();
                new_suffix->u.function.params->is_empty      = true;
                new_suffix->u.function.variadic              = false;
                new_suffix->next                             = NULL;
                *tail                                        = new_suffix;
                tail                                         = &new_suffix->next;
            } else if (current_token == TOKEN_STAR) {
                // Case: '(' abstract_declarator ')'
                DeclaratorSuffix *new_suffix   = new_declarator_suffix(SUFFIX_POINTER);
                new_suffix->u.pointer.pointers = parse_pointer();
                new_suffix->u.pointer.suffix   = parse_direct_abstract_declarator();
                new_suffix->next               = NULL;
                expect_token(TOKEN_RPAREN); // Consume ')'
                *tail = new_suffix;
                tail  = &new_suffix->next;
            } else {
                // Case: '(' parameter_type_list ')'
                ParamList *params = parse_parameter_type_list();
                if (!params) {
                    fatal_error("Expected parameter_type_list in parentheses");
                }
                expect_token(TOKEN_RPAREN); // Consume ')'
                DeclaratorSuffix *new_suffix    = new_declarator_suffix(SUFFIX_FUNCTION);
                new_suffix->u.function.params   = params;
                new_suffix->u.function.variadic = false;
                new_suffix->next                = NULL;
                *tail                           = new_suffix;
                tail                            = &new_suffix->next;
            }
        } else if (current_token == TOKEN_LBRACKET) {
            // Handle array-related cases
            advance_token(); // Consume '['
            DeclaratorSuffix *new_suffix   = new_declarator_suffix(SUFFIX_ARRAY);
            new_suffix->u.array.size       = NULL;
            new_suffix->u.array.qualifiers = NULL;
            new_suffix->u.array.is_static  = false;
            new_suffix->next               = NULL;

            if (current_token == TOKEN_RBRACKET) {
                // Case: '[' ']'
                advance_token(); // Consume ']'
            } else if (current_token == TOKEN_STAR) {
                // Case: '[' '*' ']'
                advance_token();                 // Consume '*'
                expect_token(TOKEN_RBRACKET);    // Consume ']'
                new_suffix->u.array.size = NULL; // VLA with *
            } else if (current_token == TOKEN_STATIC) {
                // Cases: '[' STATIC ... ']'
                advance_token(); // Consume STATIC
                new_suffix->u.array.is_static = true;
                if (current_token == TOKEN_CONST || current_token == TOKEN_RESTRICT ||
                    current_token == TOKEN_VOLATILE || current_token == TOKEN_ATOMIC) {
                    // Case: '[' STATIC type_qualifier_list assignment_expression ']'
                    new_suffix->u.array.qualifiers = parse_type_qualifier_list();
                } else {
                    // Case: '[' STATIC assignment_expression ']'
                }
                new_suffix->u.array.size = parse_assignment_expression();
                if (!new_suffix->u.array.size) {
                    fatal_error("Invalid array size");
                }
                expect_token(TOKEN_RBRACKET); // Consume ']'
            } else if (current_token == TOKEN_CONST || current_token == TOKEN_RESTRICT ||
                       current_token == TOKEN_VOLATILE || current_token == TOKEN_ATOMIC) {
                // Cases: '[' type_qualifier_list ... ']'
                new_suffix->u.array.qualifiers = parse_type_qualifier_list();
                if (current_token == TOKEN_STATIC) {
                    // Case: '[' type_qualifier_list STATIC assignment_expression ']'
                    advance_token(); // Consume STATIC
                    new_suffix->u.array.is_static = true;
                    new_suffix->u.array.size      = parse_assignment_expression();
                    if (!new_suffix->u.array.size) {
                        fatal_error("Invalid array size");
                    }
                } else {
                    // Case: '[' type_qualifier_list assignment_expression ']'
                    //    or '[' type_qualifier_list ']'
                    if (current_token != TOKEN_RBRACKET) {
                        new_suffix->u.array.size = parse_assignment_expression();
                        if (!new_suffix->u.array.size) {
                            fatal_error("Invalid array size");
                        }
                    }
                }
                expect_token(TOKEN_RBRACKET); // Consume ']'
            } else {
                // Case: '[' assignment_expression ']'
                new_suffix->u.array.size = parse_assignment_expression();
                if (!new_suffix->u.array.size) {
                    fatal_error("Invalid array size");
                }
                expect_token(TOKEN_RBRACKET); // Consume ']'
            }
            *tail = new_suffix;
            tail  = &new_suffix->next;
        } else {
            // No more suffixes to parse
            break;
        }
    }
    return suffix;
}

Type *type_apply_pointers(Type *type, Pointer *pointers)
{
    for (Pointer *p = pointers; p; p = p->next) {
        Type *ptr                 = new_type(TYPE_POINTER);
        ptr->u.pointer.target     = type;
        ptr->u.pointer.qualifiers = p->qualifiers;
        ptr->qualifiers           = NULL;
        type                      = ptr;
    }
    return type;
}

Type *type_apply_suffixes(Type *type, DeclaratorSuffix *suffixes)
{
    for (DeclaratorSuffix *s = suffixes; s; s = s->next) {
        switch (s->kind) {
        case SUFFIX_ARRAY: {
            Type *array               = new_type(TYPE_ARRAY);
            array->u.array.element    = type;
            array->u.array.size       = s->u.array.size;
            array->u.array.qualifiers = s->u.array.qualifiers;
            array->u.array.is_static  = s->u.array.is_static;
            array->qualifiers         = NULL;
            type                      = array;
            break;
        }
        case SUFFIX_FUNCTION: {
            Type *func                  = new_type(TYPE_FUNCTION);
            func->u.function.returnType = type;
            func->u.function.params     = s->u.function.params;
            func->u.function.variadic   = s->u.function.variadic;
            func->qualifiers            = NULL;
            type                        = func;
            break;
        }
        case SUFFIX_POINTER:
            type = type_apply_suffixes(type, s->next);
            type = type_apply_pointers(type, s->u.pointer.pointers);
            return type_apply_suffixes(type, s->u.pointer.suffix);
        }
    }
    return type;
}

Param *parse_parameter_declaration()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Param *param = new_param();

    /* Parse declaration_specifiers */
    TypeQualifier *qualifiers = NULL;
    TypeSpec *type_specs      = parse_specifier_qualifier_list(&qualifiers);
    if (!type_specs) {
        free(param);
        return NULL;
    }

    /* Check for declarator or abstract_declarator */
    if (current_token == TOKEN_IDENTIFIER || current_token == TOKEN_LPAREN ||
        current_token == TOKEN_STAR || current_token == TOKEN_LBRACKET) {
        bool is_declarator = (current_token == TOKEN_IDENTIFIER || current_token == TOKEN_LPAREN);
        char *name         = NULL;
        Pointer *pointers  = NULL;
        DeclaratorSuffix *suffixes = NULL;

        if (is_declarator && current_token == TOKEN_IDENTIFIER) {
            name = strdup(current_lexeme);
            advance_token();
            if (current_token == TOKEN_STAR || current_token == TOKEN_LBRACKET ||
                current_token == TOKEN_LPAREN) {
                // Parse abstract_declarator.
                pointers = parse_pointer();
                suffixes = parse_direct_abstract_declarator();
            }
        } else {
            // Parse abstract_declarator.
            pointers = parse_pointer();
            suffixes = parse_direct_abstract_declarator();
        }

        /* Construct base type from type_specs */
        Type *base_type = fuse_type_specifiers(type_specs);
        if (!base_type) {
            free(param);
            free(name);
            return NULL;
        }

        /* Apply pointers and suffixes */
        param->type = type_apply_suffixes(type_apply_pointers(base_type, pointers), suffixes);
        param->name = name;
    } else {
        /* Only declaration_specifiers (unnamed parameter) */
        param->type = parse_type_name();
        if (!param->type) {
            free(param);
            return NULL;
        }
    }

    return param;
}

//
// type_name : specifier_qualifier_list abstract_declarator
//           | specifier_qualifier_list
//           ;
Type *parse_type_name()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    TypeQualifier *qualifiers = NULL;
    TypeSpec *type_specs      = parse_specifier_qualifier_list(&qualifiers);
    if (!type_specs)
        return NULL;

    /* Construct base Type from type_specs (simplified to first basic type) */
    Type *base_type = fuse_type_specifiers(type_specs);
    if (!base_type) {
        fatal_error("Incorrect type");
    }
    base_type->qualifiers = qualifiers;

    /* Parse optional abstract_declarator */
    Pointer *pointers          = NULL;
    DeclaratorSuffix *suffixes = NULL;
    if (current_token == TOKEN_STAR || current_token == TOKEN_LPAREN ||
        current_token == TOKEN_LBRACKET) {
        // Parse abstract_declarator
        pointers = parse_pointer();
        suffixes = parse_direct_abstract_declarator();
    }

    /* Apply pointers and suffixes to construct the final type */
    return type_apply_suffixes(type_apply_pointers(base_type, pointers), suffixes);
}

Initializer *parse_initializer()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_LBRACE) {
        advance_token();
        InitItem *items = parse_initializer_list();
        if (current_token == TOKEN_COMMA)
            advance_token();
        expect_token(TOKEN_RBRACE);
        Initializer *init = new_initializer(INITIALIZER_COMPOUND);
        init->u.items     = items;
        return init;
    }
    Initializer *init = new_initializer(INITIALIZER_SINGLE);
    init->u.expr      = parse_assignment_expression();
    return init;
}

InitItem *parse_initializer_list()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Designator *designation = NULL;
    if (current_token == TOKEN_LBRACKET || current_token == TOKEN_DOT) {
        designation = parse_designation();
    }
    Initializer *init = parse_initializer();
    InitItem *item    = new_init_item(designation, init);
    if (current_token == TOKEN_COMMA && next_token() != TOKEN_RBRACE) {
        advance_token();
        item->next = parse_initializer_list();
    }
    return item;
}

Designator *parse_designation()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Designator *designators = parse_designator_list();
    expect_token(TOKEN_ASSIGN);
    return designators;
}

Designator *parse_designator_list()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Designator *designator = parse_designator();
    if (current_token == TOKEN_LBRACKET || current_token == TOKEN_DOT) {
        designator->next = parse_designator_list();
    }
    return designator;
}

Designator *parse_designator()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_LBRACKET) {
        advance_token();
        Expr *expr = parse_constant_expression();
        expect_token(TOKEN_RBRACKET);
        Designator *d = new_designator(DESIGNATOR_ARRAY);
        d->u.expr     = expr;
        return d;
    }
    if (current_token != TOKEN_DOT) {
        fatal_error("Expected designator");
    }
    advance_token();
    Ident name = strdup(current_lexeme);
    expect_token(TOKEN_IDENTIFIER);
    Designator *d = new_designator(DESIGNATOR_FIELD);
    d->u.name     = name;
    return d;
}

Declaration *parse_static_assert_declaration()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_STATIC_ASSERT);
    expect_token(TOKEN_LPAREN);
    Expr *condition = parse_constant_expression();
    expect_token(TOKEN_COMMA);
    expect_token(TOKEN_STRING_LITERAL);
    char *message = strdup(current_lexeme);
    advance_token();
    expect_token(TOKEN_RPAREN);
    expect_token(TOKEN_SEMICOLON);
    Declaration *decl               = new_declaration(DECL_STATIC_ASSERT);
    decl->u.static_assrt.condition = condition;
    decl->u.static_assrt.message   = message;
    return decl;
}

Stmt *parse_statement()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_IDENTIFIER && next_token() == TOKEN_COLON) {
        return parse_labeled_statement();
    } else if (current_token == TOKEN_CASE || current_token == TOKEN_DEFAULT) {
        return parse_labeled_statement();
    } else if (current_token == TOKEN_LBRACE) {
        return parse_compound_statement();
    } else if (current_token == TOKEN_IF || current_token == TOKEN_SWITCH) {
        return parse_selection_statement();
    } else if (current_token == TOKEN_WHILE || current_token == TOKEN_DO ||
               current_token == TOKEN_FOR) {
        return parse_iteration_statement();
    } else if (current_token == TOKEN_GOTO || current_token == TOKEN_CONTINUE ||
               current_token == TOKEN_BREAK || current_token == TOKEN_RETURN) {
        return parse_jump_statement();
    } else {
        return parse_expression_statement();
    }
}

Stmt *parse_labeled_statement()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_IDENTIFIER) {
        Ident label = strdup(current_lexeme);
        advance_token();
        expect_token(TOKEN_COLON);
        Stmt *stmt               = parse_statement();
        Stmt *labeled            = new_stmt(STMT_LABELED);
        labeled->u.labeled.label = label;
        labeled->u.labeled.stmt  = stmt;
        return labeled;
    } else if (current_token == TOKEN_CASE) {
        advance_token();
        Expr *expr = parse_constant_expression();
        expect_token(TOKEN_COLON);
        Stmt *stmt                  = parse_statement();
        Stmt *case_stmt             = new_stmt(STMT_CASE);
        case_stmt->u.case_stmt.expr = expr;
        case_stmt->u.case_stmt.stmt = stmt;
        return case_stmt;
    } else if (current_token != TOKEN_DEFAULT) {
        fatal_error("Expected labeled statement");
    }
    advance_token();
    expect_token(TOKEN_COLON);
    Stmt *stmt                   = parse_statement();
    Stmt *default_stmt           = new_stmt(STMT_DEFAULT);
    default_stmt->u.default_stmt = stmt;
    return default_stmt;
}

Stmt *parse_compound_statement()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_LBRACE);
    DeclOrStmt *items = NULL;
    if (current_token != TOKEN_RBRACE) {
        items = parse_block_item_list();
    }
    expect_token(TOKEN_RBRACE);
    Stmt *stmt       = new_stmt(STMT_COMPOUND);
    stmt->u.compound = items;
    return stmt;
}

DeclOrStmt *parse_block_item_list()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    DeclOrStmt *item = parse_block_item();
    if (current_token != TOKEN_RBRACE) {
        item->next = parse_block_item_list();
    }
    return item;
}

DeclOrStmt *parse_block_item()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_TYPEDEF || current_token == TOKEN_EXTERN ||
        current_token == TOKEN_STATIC || current_token == TOKEN_THREAD_LOCAL ||
        current_token == TOKEN_AUTO || current_token == TOKEN_REGISTER ||
        current_token == TOKEN_VOID || current_token == TOKEN_CHAR ||
        current_token == TOKEN_SHORT || current_token == TOKEN_INT || current_token == TOKEN_LONG ||
        current_token == TOKEN_FLOAT || current_token == TOKEN_DOUBLE ||
        current_token == TOKEN_SIGNED || current_token == TOKEN_UNSIGNED ||
        current_token == TOKEN_BOOL || current_token == TOKEN_COMPLEX ||
        current_token == TOKEN_IMAGINARY || current_token == TOKEN_STRUCT ||
        current_token == TOKEN_UNION || current_token == TOKEN_ENUM ||
        current_token == TOKEN_TYPEDEF_NAME || current_token == TOKEN_ATOMIC ||
        current_token == TOKEN_CONST || current_token == TOKEN_RESTRICT ||
        current_token == TOKEN_VOLATILE || current_token == TOKEN_INLINE ||
        current_token == TOKEN_NORETURN || current_token == TOKEN_ALIGNAS ||
        current_token == TOKEN_STATIC_ASSERT) {
        Declaration *decl = parse_declaration();
        DeclOrStmt *ds    = new_decl_or_stmt(DECL_OR_STMT_DECL);
        ds->u.decl        = decl;
        return ds;
    }
    Stmt *stmt     = parse_statement();
    DeclOrStmt *ds = new_decl_or_stmt(DECL_OR_STMT_STMT);
    ds->u.stmt     = stmt;
    return ds;
}

Stmt *parse_expression_statement()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = NULL;
    if (current_token != TOKEN_SEMICOLON) {
        expr = parse_expression();
    }
    expect_token(TOKEN_SEMICOLON);
    Stmt *stmt   = new_stmt(STMT_EXPR);
    stmt->u.expr = expr;
    return stmt;
}

Stmt *parse_selection_statement()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_IF) {
        advance_token();
        expect_token(TOKEN_LPAREN);
        Expr *condition = parse_expression();
        expect_token(TOKEN_RPAREN);
        Stmt *then_stmt = parse_statement();
        Stmt *else_stmt = NULL;
        if (current_token == TOKEN_ELSE) {
            advance_token();
            else_stmt = parse_statement();
        }
        Stmt *stmt                = new_stmt(STMT_IF);
        stmt->u.if_stmt.condition = condition;
        stmt->u.if_stmt.then_stmt = then_stmt;
        stmt->u.if_stmt.else_stmt = else_stmt;
        return stmt;
    } else if (current_token != TOKEN_SWITCH) {
        fatal_error("Expected if or switch");
    }
    advance_token();
    expect_token(TOKEN_LPAREN);
    Expr *expr = parse_expression();
    expect_token(TOKEN_RPAREN);
    Stmt *body               = parse_statement();
    Stmt *stmt               = new_stmt(STMT_SWITCH);
    stmt->u.switch_stmt.expr = expr;
    stmt->u.switch_stmt.body = body;
    return stmt;
}

Stmt *parse_iteration_statement()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_WHILE) {
        advance_token();
        expect_token(TOKEN_LPAREN);
        Expr *condition = parse_expression();
        expect_token(TOKEN_RPAREN);
        Stmt *body                   = parse_statement();
        Stmt *stmt                   = new_stmt(STMT_WHILE);
        stmt->u.while_stmt.condition = condition;
        stmt->u.while_stmt.body      = body;
        return stmt;
    } else if (current_token == TOKEN_DO) {
        advance_token();
        Stmt *body = parse_statement();
        expect_token(TOKEN_WHILE);
        expect_token(TOKEN_LPAREN);
        Expr *condition = parse_expression();
        expect_token(TOKEN_RPAREN);
        expect_token(TOKEN_SEMICOLON);
        Stmt *stmt                 = new_stmt(STMT_DO_WHILE);
        stmt->u.do_while.body      = body;
        stmt->u.do_while.condition = condition;
        return stmt;
    } else if (current_token != TOKEN_FOR) {
        fatal_error("Expected while, do, or for");
    }
    advance_token();
    expect_token(TOKEN_LPAREN);
    ForInit *init   = NULL;
    Expr *condition = NULL;
    Expr *update    = NULL;
    if (current_token == TOKEN_TYPEDEF || current_token == TOKEN_EXTERN ||
        current_token == TOKEN_STATIC || current_token == TOKEN_THREAD_LOCAL ||
        current_token == TOKEN_AUTO || current_token == TOKEN_REGISTER ||
        current_token == TOKEN_VOID || current_token == TOKEN_CHAR ||
        current_token == TOKEN_SHORT || current_token == TOKEN_INT ||
        current_token == TOKEN_LONG || current_token == TOKEN_FLOAT ||
        current_token == TOKEN_DOUBLE || current_token == TOKEN_SIGNED ||
        current_token == TOKEN_UNSIGNED || current_token == TOKEN_BOOL ||
        current_token == TOKEN_COMPLEX || current_token == TOKEN_IMAGINARY ||
        current_token == TOKEN_STRUCT || current_token == TOKEN_UNION ||
        current_token == TOKEN_ENUM || current_token == TOKEN_TYPEDEF_NAME ||
        current_token == TOKEN_ATOMIC || current_token == TOKEN_CONST ||
        current_token == TOKEN_RESTRICT || current_token == TOKEN_VOLATILE ||
        current_token == TOKEN_INLINE || current_token == TOKEN_NORETURN ||
        current_token == TOKEN_ALIGNAS || current_token == TOKEN_STATIC_ASSERT) {
        Declaration *decl = parse_declaration();
        init              = new_for_init(FOR_INIT_DECL);
        init->u.decl      = decl;
    } else {
        Stmt *expr_stmt = parse_expression_statement();
        init            = new_for_init(FOR_INIT_EXPR);
        init->u.expr    = expr_stmt->u.expr;
    }
    Stmt *cond_stmt = parse_expression_statement();
    condition       = cond_stmt->u.expr;
    if (current_token != TOKEN_RPAREN) {
        update = parse_expression();
    }
    expect_token(TOKEN_RPAREN);
    Stmt *body                 = parse_statement();
    Stmt *stmt                 = new_stmt(STMT_FOR);
    stmt->u.for_stmt.init      = init;
    stmt->u.for_stmt.condition = condition;
    stmt->u.for_stmt.update    = update;
    stmt->u.for_stmt.body      = body;
    return stmt;
}

Stmt *parse_jump_statement()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_GOTO) {
        advance_token();
        Ident label = strdup(current_lexeme);
        expect_token(TOKEN_IDENTIFIER);
        expect_token(TOKEN_SEMICOLON);
        Stmt *stmt         = new_stmt(STMT_GOTO);
        stmt->u.goto_label = label;
        return stmt;
    } else if (current_token == TOKEN_CONTINUE) {
        advance_token();
        expect_token(TOKEN_SEMICOLON);
        return new_stmt(STMT_CONTINUE);
    } else if (current_token == TOKEN_BREAK) {
        advance_token();
        expect_token(TOKEN_SEMICOLON);
        return new_stmt(STMT_BREAK);
    } else if (current_token != TOKEN_RETURN) {
        fatal_error("Expected jump statement");
    }
    advance_token();
    Expr *expr = NULL;
    if (current_token != TOKEN_SEMICOLON) {
        expr = parse_expression();
    }
    expect_token(TOKEN_SEMICOLON);
    Stmt *stmt   = new_stmt(STMT_RETURN);
    stmt->u.expr = expr;
    return stmt;
}

Program *parse_translation_unit()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Program *program = new_program();
    while (current_token != TOKEN_EOF) {
        ExternalDecl *decl = parse_external_declaration();
        append_list(&program->decls, decl);
    }
    return program;
}

ExternalDecl *parse_external_declaration()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_STATIC_ASSERT) {

        // Static assert.
        ExternalDecl *ed  = new_external_decl(EXTERNAL_DECL_DECLARATION);
        ed->u.declaration = parse_static_assert_declaration();
        return ed;
    }
    DeclSpec *spec = parse_declaration_specifiers();
    if (current_token == TOKEN_SEMICOLON) {

        // Empty declaration.
        advance_token();
        Declaration *decl      = new_declaration(DECL_EMPTY);
        decl->u.var.specifiers = spec;

        ExternalDecl *ed  = new_external_decl(EXTERNAL_DECL_DECLARATION);
        ed->u.declaration = decl;
        return ed;
    }
    if (current_token == TOKEN_IDENTIFIER && (next_token() == TOKEN_SEMICOLON ||
        next_token() == TOKEN_COMMA || next_token() == TOKEN_ASSIGN)) {

        // Declaration of variables.
        InitDeclarator *declarators = parse_init_declarator_list();
        expect_token(TOKEN_SEMICOLON);
        Declaration *decl       = new_declaration(DECL_VAR);
        decl->u.var.specifiers  = spec;
        decl->u.var.declarators = declarators;

        ExternalDecl *ed  = new_external_decl(EXTERNAL_DECL_DECLARATION);
        ed->u.declaration = decl;
        return ed;
    }

    // Function definition.
    Declarator *decl   = parse_declarator();
    Declaration *decls = NULL;
    if (current_token != TOKEN_LBRACE) {
        decls = parse_declaration_list();
    }
    Stmt *body = parse_compound_statement();

    ExternalDecl *ed          = new_external_decl(EXTERNAL_DECL_FUNCTION);
    ed->u.function.specifiers = spec;
    ed->u.function.declarator = decl;
    ed->u.function.decls      = decls;
    ed->u.function.body       = body;
    return ed;
}

Declaration *parse_declaration_list()
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    Declaration *decl = parse_declaration();
    if (current_token != TOKEN_LBRACE) {
        decl->next = parse_declaration_list();
    }
    return decl;
}

/* Main parsing function */
Program *parse(FILE *input)
{
    if (debug) {
        printf("--- %s()\n", __func__);
    }
    init_scanner(input);
    advance_token();
    Program *program = parse_translation_unit();
    if (current_token != TOKEN_EOF) {
        fatal_error("Expected end of file");
    }
    return program;
}
