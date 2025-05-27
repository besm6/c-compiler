#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "parser.h"
#include "scanner.h"
#include "internal.h"
#include "xalloc.h"

/* Global lexer state */
static int current_token;
static int peek_token;
static const char *current_lexeme;
static char lexeme_buffer[1024]; // Buffer for current lexeme

// Enable debug output
int parser_debug;

static int scope_level;

/* Error handling */
static void _Noreturn fatal_error(const char *message, ...)
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

//
// In case of identifier, look in the parser's symbol table.
// When it's a previously defined typedef - return TOKEN_TYPEDEF_NAME.
// When a previously defined enumerator - return TOKEN_ENUMERATION_CONSTANT.
//
static int token_translation(int token)
{
    // Check identifier type
    if (token == TOKEN_IDENTIFIER) {
        token = symtab_find(get_yytext());
        if (! token) {
            token = TOKEN_IDENTIFIER;
        }
    }
    if (parser_debug) {
        printf("--- token %d '%s'\n", token, get_yytext());
    }
    return token;
}

/* Token handling */
void advance_token()
{
    if (peek_token > 0) {
        current_token = peek_token;
        peek_token    = 0;
    } else {
        current_token = token_translation(yylex());
    }
    current_lexeme = get_yytext();
}

// Is current token valid but different from the given one?
static inline bool current_token_is_not(int token)
{
    return (current_token != TOKEN_EOF) && (current_token != token);
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
        peek_token = token_translation(yylex());
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

// Is this token a type specifier?
static bool is_type_specifier(int token)
{
    return token == TOKEN_VOID || token == TOKEN_CHAR ||
           token == TOKEN_SHORT || token == TOKEN_INT ||
           token == TOKEN_LONG || token == TOKEN_FLOAT ||
           token == TOKEN_DOUBLE || token == TOKEN_SIGNED ||
           token == TOKEN_UNSIGNED || token == TOKEN_BOOL ||
           token == TOKEN_COMPLEX || token == TOKEN_IMAGINARY ||
           token == TOKEN_STRUCT || token == TOKEN_UNION ||
           token == TOKEN_ENUM || token == TOKEN_TYPEDEF_NAME;
}

// Is this token a type qualifier?
// Except _Atomic.
static bool is_type_qualifier(int token)
{
    return token == TOKEN_CONST || token == TOKEN_RESTRICT ||
           token == TOKEN_VOLATILE;
}

// Is this token a storage class specifier?
static bool is_storage_class_specifier(int token)
{
    return token == TOKEN_TYPEDEF || token == TOKEN_EXTERN ||
           token == TOKEN_STATIC || token == TOKEN_THREAD_LOCAL ||
           token == TOKEN_AUTO || token == TOKEN_REGISTER;
}

//
// Append to linked list.
// The first field of a struct must be the *next pointer.
//
static void append_list(void *head, void *node)
{
    if (!node)
        return;
    *(void**)node = NULL;

    // Find tail.
    void **tail = (void**) head;
    while (*tail) {
        tail = (void**) *tail;
    }
    *tail = node;
}

// Checks if an expression is a constant expression according to C language rules.
static bool is_constant_expression(const Expr *expression)
{
    if (!expression)
        return false;

    switch (expression->kind) {
    case EXPR_LITERAL:
        /* All literals are constant, including enum constants */
        return true;

    case EXPR_VAR:
        /* Variables are not constant, even if they are enum constants
         * (handled in EXPR_LITERAL with LITERAL_ENUM) */
        return false;

    case EXPR_UNARY_OP: {
        /* Check which unary operations are allowed */
        UnaryOpKind op_kind = expression->u.unary_op.op->kind;
        const Expr *operand = expression->u.unary_op.expr;
        switch (op_kind) {
        case UNARY_PLUS:
        case UNARY_NEG:
        case UNARY_BIT_NOT:
        case UNARY_LOG_NOT:
            /* These operators are allowed if operand is constant */
            return is_constant_expression(operand);
        case UNARY_ADDRESS:
        case UNARY_DEREF:
        case UNARY_PRE_INC:
        case UNARY_PRE_DEC:
            /* These involve addresses or modification, not constant */
            return false;
        }
        return false; /* Unreachable, but for safety */
    }

    case EXPR_BINARY_OP: {
        /* Arithmetic, bitwise, relational, and logical ops are allowed */
        const Expr *left  = expression->u.binary_op.left;
        const Expr *right = expression->u.binary_op.right;
        return is_constant_expression(left) && is_constant_expression(right);
    }

    case EXPR_ASSIGN:
        /* Assignments are not constant */
        return false;

    case EXPR_COND: {
        /* Ternary operator is allowed if all operands are constant */
        const Expr *cond      = expression->u.cond.condition;
        const Expr *then_expr = expression->u.cond.then_expr;
        const Expr *else_expr = expression->u.cond.else_expr;
        return is_constant_expression(cond) && is_constant_expression(then_expr) &&
               is_constant_expression(else_expr);
    }

    case EXPR_CAST: {
        /* Casts are allowed if the operand is constant and target is scalar */
        const Expr *operand     = expression->u.cast.expr;
        const Type *target_type = expression->u.cast.type;
        /* Check if target type is scalar (void, arithmetic, pointer) */
        bool is_scalar =
            target_type &&
            (target_type->kind == TYPE_VOID || target_type->kind == TYPE_BOOL ||
             target_type->kind == TYPE_CHAR || target_type->kind == TYPE_SHORT ||
             target_type->kind == TYPE_INT || target_type->kind == TYPE_LONG ||
             target_type->kind == TYPE_LONG_LONG || target_type->kind == TYPE_FLOAT ||
             target_type->kind == TYPE_DOUBLE || target_type->kind == TYPE_LONG_DOUBLE ||
             target_type->kind == TYPE_COMPLEX || target_type->kind == TYPE_IMAGINARY ||
             target_type->kind == TYPE_POINTER);
        return is_scalar && is_constant_expression(operand);
    }

    case EXPR_CALL:
        /* Function calls are not constant */
        return false;

    case EXPR_COMPOUND:
        /* Compound literals are not constant (may involve runtime init) */
        return false;

    case EXPR_FIELD_ACCESS:
    case EXPR_PTR_ACCESS:
        /* Member access involves addresses, not constant */
        return false;

    case EXPR_POST_INC:
    case EXPR_POST_DEC:
        /* Increment/decrement are not constant */
        return false;

    case EXPR_SIZEOF_EXPR:
        /* sizeof(expr) is constant if the expression's type is known at compile-time.
         * In C, sizeof doesn't evaluate the expression, so we don't need to check
         * if the operand is constant. */
        return true;

    case EXPR_SIZEOF_TYPE:
        /* sizeof(type) is always constant */
        return true;

    case EXPR_ALIGNOF:
        /* _Alignof(type) is always constant */
        return true;

    case EXPR_GENERIC:
        /* _Generic is not constant (type selection may be runtime-dependent) */
        return false;
    }

    return false; /* Unreachable, but for safety */
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
DeclSpec *parse_declaration_specifiers(Type **base_type);
InitDeclarator *parse_init_declarator_list(Declarator *first, const Type *base_type);
InitDeclarator *parse_init_declarator(Declarator *decl, const Type *base_type);
StorageClass *parse_storage_class_specifier();
TypeSpec *parse_type_specifier();
TypeSpec *parse_struct_or_union_specifier();
int parse_struct_or_union();
Field *parse_struct_declaration_list();
Field *parse_struct_declaration();
TypeSpec *parse_specifier_qualifier_list(TypeQualifier **qualifiers);
Declarator *parse_struct_declarator_list();
Declarator *parse_struct_declarator();
TypeSpec *parse_enum_specifier();
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
Param *parse_parameter_type_list(bool *variadic_flag);
Param *parse_parameter_list();
Param *parse_parameter_declaration();
Type *parse_type_name();
DeclaratorSuffix *parse_direct_abstract_declarator(Ident *name_out);
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

//
// primary_expression
//     : IDENTIFIER
//     | constant
//     | string
//     | '(' expression ')'
//     | generic_selection
//     ;
//
Expr *parse_primary_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = NULL;
    switch (current_token) {
    case TOKEN_IDENTIFIER:
        expr        = new_expression(EXPR_VAR);
        expr->u.var = xstrdup(current_lexeme);
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

//
// constant
//     : I_CONSTANT             /* includes character_constant */
//     | F_CONSTANT
//     | ENUMERATION_CONSTANT   /* after it has been defined as such */
//     ;
//
Expr *parse_constant()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr      = new_expression(EXPR_LITERAL);
    expr->u.literal = new_literal(current_token == TOKEN_I_CONSTANT   ? LITERAL_INT
                                  : current_token == TOKEN_F_CONSTANT ? LITERAL_FLOAT
                                                                      : LITERAL_ENUM);
    switch (current_token) {
    case TOKEN_I_CONSTANT:
        expr->u.literal->u.int_val = strtoul(current_lexeme, NULL, 0); // TODO: suffixes
        break;
    case TOKEN_F_CONSTANT:
        expr->u.literal->u.real_val = strtod(current_lexeme, NULL); // TODO: suffixes
        break;
    case TOKEN_ENUMERATION_CONSTANT:
        expr->u.literal->u.enum_const = xstrdup(current_lexeme);
        break;
    }
    advance_token();
    return expr;
}

//
// string
//     : STRING_LITERAL
//     | FUNC_NAME
//     ;
//
Expr *parse_string()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr                    = new_expression(EXPR_LITERAL);
    expr->u.literal               = new_literal(LITERAL_STRING);
    expr->u.literal->u.string_val = xstrdup(current_lexeme);
    advance_token();
    return expr;
}

//
// generic_selection
//     : GENERIC '(' assignment_expression ',' generic_assoc_list ')'
//     ;
//
Expr *parse_generic_selection()
{
    if (parser_debug) {
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

//
// generic_assoc_list
//     : generic_association
//     | generic_assoc_list ',' generic_association
//     ;
//
GenericAssoc *parse_generic_assoc_list()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    GenericAssoc *assoc = parse_generic_association();
    if (current_token == TOKEN_COMMA) {
        advance_token();
        assoc->next = parse_generic_assoc_list();
    }
    return assoc;
}

//
// generic_association
//     : type_name ':' assignment_expression
//     | DEFAULT ':' assignment_expression
//     ;
//
GenericAssoc *parse_generic_association()
{
    if (parser_debug) {
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

//
// postfix_expression
//     : primary_expression
//     | postfix_expression '[' expression ']'
//     | postfix_expression '(' ')'
//     | postfix_expression '(' argument_expression_list ')'
//     | postfix_expression '.' IDENTIFIER
//     | postfix_expression PTR_OP IDENTIFIER
//     | postfix_expression INC_OP
//     | postfix_expression DEC_OP
//     | '(' type_name ')' '{' initializer_list '}'
//     | '(' type_name ')' '{' initializer_list ',' '}'
//     ;
//
Expr *parse_postfix_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
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
            if (current_token_is_not(TOKEN_RPAREN)) {
                args = parse_argument_expression_list();
            }
            expect_token(TOKEN_RPAREN);
            Expr *new_expr        = new_expression(EXPR_CALL);
            new_expr->u.call.func = expr;
            new_expr->u.call.args = args;
            expr                  = new_expr;
        } else if (current_token == TOKEN_DOT) {
            advance_token();
            Ident field = xstrdup(current_lexeme);
            expect_token(TOKEN_IDENTIFIER);
            Expr *new_expr                 = new_expression(EXPR_FIELD_ACCESS);
            new_expr->u.field_access.expr  = expr;
            new_expr->u.field_access.field = field;
            expr = new_expr;
        } else if (current_token == TOKEN_PTR_OP) {
            advance_token();
            Ident field = xstrdup(current_lexeme);
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

//
// argument_expression_list
//     : assignment_expression
//     | argument_expression_list ',' assignment_expression
//     ;
//
Expr *parse_argument_expression_list()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = parse_assignment_expression();
    if (current_token == TOKEN_COMMA) {
        advance_token();
        expr->next = parse_argument_expression_list();
    }
    return expr;
}

//
// unary_expression
//     : postfix_expression
//     | INC_OP unary_expression
//     | DEC_OP unary_expression
//     | unary_operator cast_expression
//     | SIZEOF unary_expression
//     | SIZEOF '(' type_name ')'
//     | ALIGNOF '(' type_name ')'
//     ;
//
Expr *parse_unary_expression()
{
    if (parser_debug) {
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
        if (current_token == TOKEN_LPAREN &&
            (is_type_specifier(next_token()) || is_type_qualifier(next_token()) ||
             next_token() == TOKEN_ATOMIC)) {
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

//
// unary_operator
//     : '&'
//     | '*'
//     | '+'
//     | '-'
//     | '~'
//     | '!'
//     ;
//
UnaryOp *parse_unary_operator()
{
    if (parser_debug) {
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

//
// cast_expression
//     : unary_expression
//     | '(' type_name ')' cast_expression
//     ;
//
Expr *parse_cast_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_LPAREN && is_type_specifier(next_token())) {
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

//
// multiplicative_expression
//     : cast_expression
//     | multiplicative_expression '*' cast_expression
//     | multiplicative_expression '/' cast_expression
//     | multiplicative_expression '%' cast_expression
//     ;
//
Expr *parse_multiplicative_expression()
{
    if (parser_debug) {
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

//
// additive_expression
//     : multiplicative_expression
//     | additive_expression '+' multiplicative_expression
//     | additive_expression '-' multiplicative_expression
//     ;
//
Expr *parse_additive_expression()
{
    if (parser_debug) {
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

//
// shift_expression
//     : additive_expression
//     | shift_expression LEFT_OP additive_expression
//     | shift_expression RIGHT_OP additive_expression
//     ;
//
Expr *parse_shift_expression()
{
    if (parser_debug) {
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

//
// relational_expression
//     : shift_expression
//     | relational_expression '<' shift_expression
//     | relational_expression '>' shift_expression
//     | relational_expression LE_OP shift_expression
//     | relational_expression GE_OP shift_expression
//     ;
//
Expr *parse_relational_expression()
{
    if (parser_debug) {
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

//
// equality_expression
//     : relational_expression
//     | equality_expression EQ_OP relational_expression
//     | equality_expression NE_OP relational_expression
//     ;
//
Expr *parse_equality_expression()
{
    if (parser_debug) {
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

//
// and_expression
//     : equality_expression
//     | and_expression '&' equality_expression
//     ;
//
Expr *parse_and_expression()
{
    if (parser_debug) {
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

//
// exclusive_or_expression
//     : and_expression
//     | exclusive_or_expression '^' and_expression
//     ;
//
Expr *parse_exclusive_or_expression()
{
    if (parser_debug) {
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

//
// inclusive_or_expression
//     : exclusive_or_expression
//     | inclusive_or_expression '|' exclusive_or_expression
//     ;
//
Expr *parse_inclusive_or_expression()
{
    if (parser_debug) {
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

//
// logical_and_expression
//     : inclusive_or_expression
//     | logical_and_expression AND_OP inclusive_or_expression
//     ;
//
Expr *parse_logical_and_expression()
{
    if (parser_debug) {
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

//
// logical_or_expression
//     : logical_and_expression
//     | logical_or_expression OR_OP logical_and_expression
//     ;
//
Expr *parse_logical_or_expression()
{
    if (parser_debug) {
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

//
// conditional_expression
//     : logical_or_expression
//     | logical_or_expression '?' expression ':' conditional_expression
//     ;
//
Expr *parse_conditional_expression()
{
    if (parser_debug) {
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

//
// assignment_expression
//     : conditional_expression
//     | unary_expression assignment_operator assignment_expression
//     ;
//
Expr *parse_assignment_expression()
{
    if (parser_debug) {
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

//
// assignment_operator
//     : '='
//     | MUL_ASSIGN
//     | DIV_ASSIGN
//     | MOD_ASSIGN
//     | ADD_ASSIGN
//     | SUB_ASSIGN
//     | LEFT_ASSIGN
//     | RIGHT_ASSIGN
//     | AND_ASSIGN
//     | XOR_ASSIGN
//     | OR_ASSIGN
//     ;
//
AssignOp *parse_assignment_operator()
{
    if (parser_debug) {
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

//
// expression
//     : assignment_expression
//     | expression ',' assignment_expression
//     ;
//
Expr *parse_expression()
{
    if (parser_debug) {
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

//
// constant_expression
//     : conditional_expression    /* with constraints */
//     ;
// Returns non-NULL value.
//
Expr *parse_constant_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expression = parse_conditional_expression();
    if (!is_constant_expression(expression)) {
        fatal_error("Expected constant expression");
    }
    return expression;
}

//
// Fuse TypeSpec list into a single Type.
// Returns non-NULL value.
//
Type *fuse_type_specifiers(const TypeSpec *specs)
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
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
    int specifier_count    = 0;
    const TypeSpec *struct_spec  = NULL;
    const TypeSpec *union_spec   = NULL;
    const TypeSpec *enum_spec    = NULL;
    const TypeSpec *typedef_spec = NULL;
    const TypeSpec *atomic_spec  = NULL;

    /* Collect specifiers */
    for (const TypeSpec *s = specs; s; s = s->next) {
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
            if (struct_spec || union_spec || enum_spec || typedef_spec || atomic_spec || base_kind != -1) {
                fatal_error("struct cannot combine with other distinct types");
            }
            struct_spec = s;
        } else if (s->kind == TYPE_SPEC_UNION) {
            if (struct_spec || union_spec || enum_spec || typedef_spec || atomic_spec || base_kind != -1) {
                fatal_error("union cannot combine with other distinct types");
            }
            union_spec = s;
        } else if (s->kind == TYPE_SPEC_ENUM) {
            if (struct_spec || union_spec || enum_spec || typedef_spec || atomic_spec || base_kind != -1) {
                fatal_error("enum cannot combine with other distinct types");
            }
            enum_spec = s;
        } else if (s->kind == TYPE_SPEC_TYPEDEF_NAME) {
            if (struct_spec || union_spec || enum_spec || typedef_spec || atomic_spec || base_kind != -1) {
                fatal_error("typedef name cannot combine with other distinct types");
            }
            typedef_spec = s;
        } else if (s->kind == TYPE_SPEC_ATOMIC) {
            if (struct_spec || union_spec || enum_spec || typedef_spec || atomic_spec || base_kind != -1) {
                fatal_error("_Atomic(type) cannot combine with other distinct types");
            }
            atomic_spec = s;
        }
    }

    /* Validate and construct Type */
    Type *result = NULL;

    if (struct_spec) {
        result                    = new_type(TYPE_STRUCT);
        result->u.struct_t.name   = xstrdup(struct_spec->u.struct_spec.name);
        result->u.struct_t.fields = clone_field(struct_spec->u.struct_spec.fields);
    } else if (union_spec) {
        result                    = new_type(TYPE_UNION);
        result->u.struct_t.name   = xstrdup(union_spec->u.struct_spec.name);
        result->u.struct_t.fields = clone_field(union_spec->u.struct_spec.fields);
    } else if (enum_spec) {
        result                       = new_type(TYPE_ENUM);
        result->u.enum_t.name        = xstrdup(enum_spec->u.enum_spec.name);
        result->u.enum_t.enumerators = clone_enumerator(enum_spec->u.enum_spec.enumerators);
    } else if (typedef_spec) {
        result                      = new_type(TYPE_TYPEDEF_NAME);
        result->u.typedef_name.name = xstrdup(typedef_spec->u.typedef_name.name);
    } else if (atomic_spec) {
        result                = new_type(TYPE_ATOMIC);
        result->u.atomic.base = clone_type(atomic_spec->u.atomic.type);
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

Type *type_apply_pointers(Type *type, const Pointer *pointers)
{
    for (const Pointer *p = pointers; p; p = p->next) {
        Type *ptr                 = new_type(TYPE_POINTER);
        ptr->u.pointer.target     = type;
        ptr->u.pointer.qualifiers = clone_type_qualifier(p->qualifiers);
        ptr->qualifiers           = NULL;
        type                      = ptr;
    }
    return type;
}

Type *type_apply_suffixes(Type *type, const DeclaratorSuffix *suffixes)
{
    for (const DeclaratorSuffix *s = suffixes; s; s = s->next) {
        switch (s->kind) {
        case SUFFIX_ARRAY: {
            Type *array               = new_type(TYPE_ARRAY);
            array->u.array.element    = type;
            array->u.array.size       = clone_expression(s->u.array.size);
            array->u.array.qualifiers = clone_type_qualifier(s->u.array.qualifiers);
            array->u.array.is_static  = s->u.array.is_static;
            array->qualifiers         = NULL;
            type                      = array;
            break;
        }
        case SUFFIX_FUNCTION: {
            Type *func                   = new_type(TYPE_FUNCTION);
            func->u.function.return_type = type;
            func->u.function.params      = clone_param(s->u.function.params);
            func->u.function.variadic    = s->u.function.variadic;
            func->qualifiers             = NULL;
            type                         = func;
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

//
// Is this a typedef?
//
static bool is_typedef(DeclSpec *specifiers)
{
    if (!specifiers || !specifiers->storage)
        return false;
    return specifiers->storage->kind == STORAGE_CLASS_TYPEDEF;
}

//
// Define all names in this declarator as typedef.
//
static void define_typedef(InitDeclarator *decl)
{
    for (; decl; decl = decl->next) {
        if (!symtab_define(decl->name, TOKEN_TYPEDEF_NAME, scope_level)) {
            fatal_error("Typedef %s redefined", decl->name);
        }
    }
}

//
// declaration
//     : declaration_specifiers ';'
//     | declaration_specifiers init_declarator_list ';'
//     | static_assert_declaration
//     ;
//
Declaration *parse_declaration()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_STATIC_ASSERT) {
        return parse_static_assert_declaration();
    }
    Type *base_type = NULL;
    DeclSpec *specifiers = parse_declaration_specifiers(&base_type);
    if (current_token == TOKEN_SEMICOLON) {
        advance_token();
        Declaration *decl        = new_declaration(DECL_EMPTY);
        decl->u.empty.specifiers = specifiers;
        decl->u.empty.type       = base_type;
        return decl;
    }
    InitDeclarator *declarators = parse_init_declarator_list(NULL, base_type);
    expect_token(TOKEN_SEMICOLON);
    free_type(base_type);
    Declaration *decl       = new_declaration(DECL_VAR);
    decl->u.var.specifiers  = specifiers;
    decl->u.var.declarators = declarators;
    if (is_typedef(specifiers)) {
        define_typedef(declarators);
    }
    return decl;
}

//
// declaration_specifiers
//     : storage_class_specifier declaration_specifiers
//     | storage_class_specifier
//     | type_specifier declaration_specifiers
//     | type_specifier
//     | type_qualifier declaration_specifiers
//     | type_qualifier
//     | function_specifier declaration_specifiers
//     | function_specifier
//     | alignment_specifier declaration_specifiers
//     | alignment_specifier
//     ;
// Stores base type by provided pointer.
// Returns DeclSpec object when it's not empty.
// Otherwise returns NULL.
//
DeclSpec *parse_declaration_specifiers(Type **base_type_result)
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    DeclSpec *ds = new_decl_spec();
    TypeSpec *type_specs = NULL;
    while (1) {
        if (is_storage_class_specifier(current_token)) {
            ds->storage = parse_storage_class_specifier();
        } else if (is_type_specifier(current_token) ||
                   (current_token == TOKEN_ATOMIC && next_token() == TOKEN_LPAREN)) {
            TypeSpec *ts = parse_type_specifier();
            append_list(&type_specs, ts);
        } else if (is_type_qualifier(current_token) || current_token == TOKEN_ATOMIC) {
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
    *base_type_result = fuse_type_specifiers(type_specs);
    free_type_spec(type_specs);
    if (!ds->storage && !ds->qualifiers && !ds->func_specs && !ds->align_spec) {
        free_decl_spec(ds);
        return NULL;
    }
    return ds;
}

//
// init_declarator_list
//     : init_declarator
//     | init_declarator_list ',' init_declarator
//     ;
//
InitDeclarator *parse_init_declarator_list(Declarator *first, const Type *base_type)
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    InitDeclarator *decl = parse_init_declarator(first, base_type);
    if (current_token == TOKEN_COMMA) {
        advance_token();
        decl->next = parse_init_declarator_list(NULL, base_type);
    }
    return decl;
}

//
// init_declarator
//     : declarator '=' initializer
//     | declarator
//     ;
//
InitDeclarator *parse_init_declarator(Declarator *decl, const Type *base_type)
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
        if (base_type)
            print_type(stdout, base_type, 4);
    }
    if (!decl) {
        decl = parse_declarator();
    }
    InitDeclarator *init_decl = new_init_declarator();
    init_decl->name = decl->name;
    decl->name = NULL;
    if (current_token == TOKEN_ASSIGN) {
        advance_token();
        init_decl->init = parse_initializer();
    }
    init_decl->type = type_apply_suffixes(type_apply_pointers(clone_type(base_type), decl->pointers), decl->suffixes);
    free_declarator(decl);
    return init_decl;
}

//
// storage_class_specifier
//     : TYPEDEF   /* identifiers must be flagged as TYPEDEF_NAME */
//     | EXTERN
//     | STATIC
//     | THREAD_LOCAL
//     | AUTO
//     | REGISTER
//     ;
//
StorageClass *parse_storage_class_specifier()
{
    if (parser_debug) {
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

//
// type_specifier
//     : VOID
//     | CHAR
//     | SHORT
//     | INT
//     | LONG
//     | FLOAT
//     | DOUBLE
//     | SIGNED
//     | UNSIGNED
//     | BOOL
//     | COMPLEX
//     | IMAGINARY     /* non-mandated extension */
//     | atomic_type_specifier
//     | struct_or_union_specifier
//     | enum_specifier
//     | TYPEDEF_NAME      /* after it has been defined as such */
//     ;
// Returns non-NULL value.
//
TypeSpec *parse_type_specifier()
{
    if (parser_debug) {
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
        ts = parse_struct_or_union_specifier();
    } else if (current_token == TOKEN_ENUM) {
        ts = parse_enum_specifier();
    } else if (current_token == TOKEN_TYPEDEF_NAME) {
        ts                      = new_type_spec(TYPE_SPEC_TYPEDEF_NAME);
        ts->u.typedef_name.name = xstrdup(current_lexeme);
        advance_token();
    } else {
        fatal_error("Expected type specifier");
    }
    return ts;
}

//
// struct_or_union_specifier
//     : struct_or_union '{' struct_declaration_list '}'
//     | struct_or_union IDENTIFIER '{' struct_declaration_list '}'
//     | struct_or_union IDENTIFIER
//     ;
// struct_or_union
//     : STRUCT
//     | UNION
//     ;
//
TypeSpec *parse_struct_or_union_specifier()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token != TOKEN_STRUCT && current_token != TOKEN_UNION) {
        fatal_error("Expected struct or union");
    }
    TypeSpec *ts = new_type_spec(current_token == TOKEN_STRUCT ? TYPE_SPEC_STRUCT : TYPE_SPEC_UNION);
    advance_token();
    if (current_token == TOKEN_IDENTIFIER) {
        ts->u.struct_spec.name = xstrdup(current_lexeme);
        advance_token();
    }
    if (current_token == TOKEN_LBRACE) {
        advance_token();
        ts->u.struct_spec.fields = parse_struct_declaration_list();
        expect_token(TOKEN_RBRACE);
    }
    return ts;
}

//
// struct_declaration_list
//     : struct_declaration
//     | struct_declaration_list struct_declaration
//     ;
//
Field *parse_struct_declaration_list()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Field *fields = parse_struct_declaration();
    if (current_token_is_not(TOKEN_RBRACE)) {
        fields->next = parse_struct_declaration_list();
    }
    return fields;
}

//
// struct_declaration
//     : specifier_qualifier_list ';'  /* for anonymous struct/union */
//     | specifier_qualifier_list struct_declarator_list ';'
//     | static_assert_declaration
//     ;
//
Field *parse_struct_declaration()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_STATIC_ASSERT) {
        parse_static_assert_declaration(); // TODO: implement static assert as a special kind of Field
        return NULL;
    }

    /* Parse specifier_qualifier_list */
    TypeQualifier *qualifiers = NULL;
    TypeSpec *type_specs      = parse_specifier_qualifier_list(&qualifiers);

    /* Construct base Type from type_specs (simplified to first basic type) */
    Type *base_type = fuse_type_specifiers(type_specs);
    free_type_spec(type_specs);
    base_type->qualifiers = qualifiers;

    /* Parse struct_declarator_list */
    Field *fields = NULL, **fields_tail = &fields;
    for (;;) {
        Field *field = new_field();
        field->type = clone_type(base_type);

        if (current_token != TOKEN_COLON && current_token != TOKEN_SEMICOLON) {
            Declarator *declarator = parse_declarator();
            field->name            = declarator->name;
            declarator->name       = NULL;
            if (declarator->pointers) {
                field->type = type_apply_pointers(field->type, declarator->pointers);
            }
            if (declarator->suffixes) {
                field->type = type_apply_suffixes(field->type, declarator->suffixes);
            }
            free_declarator(declarator);
        }
        if (current_token == TOKEN_COLON) {
            advance_token();
            field->bitfield = parse_constant_expression();
        }

        *fields_tail = field;
        fields_tail  = &field->next;

        if (current_token == TOKEN_SEMICOLON) {
            break;
        }
        expect_token(TOKEN_COMMA);
    }
    expect_token(TOKEN_SEMICOLON);
    free_type(base_type);
    return fields;
}

//
// specifier_qualifier_list
//     : type_specifier specifier_qualifier_list
//     | type_specifier
//     | type_qualifier specifier_qualifier_list
//     | type_qualifier
//     ;
// Returns non-NULL value.
//
TypeSpec *parse_specifier_qualifier_list(TypeQualifier **qualifiers)
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    TypeSpec *type_specs = NULL;
    *qualifiers          = NULL;

    while (1) {
        if (current_token == TOKEN_CONST || current_token == TOKEN_RESTRICT ||
            current_token == TOKEN_VOLATILE ||
            (current_token == TOKEN_ATOMIC && next_token() != TOKEN_LPAREN)) {
            TypeQualifier *q = parse_type_qualifier();
            append_list(qualifiers, q);
        } else if (is_type_specifier(current_token) || is_type_qualifier(current_token) ||
                   (current_token == TOKEN_ATOMIC && next_token() == TOKEN_LPAREN)) {
            TypeSpec *ts = parse_type_specifier();
            append_list(&type_specs, ts);
        } else {
            break; /* End of specifier_qualifier_list */
        }
    }
    if (!type_specs) {
        fatal_error("Expected type specifier");
    }
    return type_specs;
}

//
// enum_specifier
//     : ENUM '{' enumerator_list '}'
//     | ENUM '{' enumerator_list ',' '}'
//     | ENUM IDENTIFIER '{' enumerator_list '}'
//     | ENUM IDENTIFIER '{' enumerator_list ',' '}'
//     | ENUM IDENTIFIER
//     ;
//
TypeSpec *parse_enum_specifier()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_ENUM);
    TypeSpec *ts = new_type_spec(TYPE_SPEC_ENUM);
    if (current_token == TOKEN_IDENTIFIER) {
        ts->u.enum_spec.name = xstrdup(current_lexeme);
        advance_token();
    }
    if (current_token == TOKEN_LBRACE) {
        advance_token();
        ts->u.enum_spec.enumerators = parse_enumerator_list();
        if (current_token == TOKEN_COMMA)
            advance_token();
        expect_token(TOKEN_RBRACE);
    }
    return ts;
}

//
// enumerator_list
//     : enumerator
//     | enumerator_list ',' enumerator
//     ;
//
Enumerator *parse_enumerator_list()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Enumerator *enumr = parse_enumerator();
    if (current_token == TOKEN_COMMA && next_token() != TOKEN_RBRACE) {
        advance_token();
        enumr->next = parse_enumerator_list();
    }
    return enumr;
}

//
// enumerator  /* identifiers must be flagged as ENUMERATION_CONSTANT */
//     : enumeration_constant '=' constant_expression
//     | enumeration_constant
//     ;
//
Enumerator *parse_enumerator()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Ident name = xstrdup(current_lexeme);
    expect_token(TOKEN_IDENTIFIER);
    Expr *value = NULL;
    if (current_token == TOKEN_ASSIGN) {
        advance_token();
        value = parse_constant_expression();
    }
    if (!symtab_define(name, TOKEN_ENUMERATION_CONSTANT, scope_level)) {
        fatal_error("Enumerator %s redefined", name);
    }
    return new_enumerator(name, value);
}

//
// atomic_type_specifier
//     : ATOMIC '(' type_name ')'
//     ;
//
Type *parse_atomic_type_specifier()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_ATOMIC);
    expect_token(TOKEN_LPAREN);
    Type *type = parse_type_name();
    expect_token(TOKEN_RPAREN);
    return type;
}

//
// type_qualifier
//     : CONST
//     | RESTRICT
//     | VOLATILE
//     | ATOMIC
//     ;
// Returns non-NULL value.
//
TypeQualifier *parse_type_qualifier()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    switch (current_token) {
    case TOKEN_CONST:
        advance_token();
        return new_type_qualifier(TYPE_QUALIFIER_CONST);
    case TOKEN_RESTRICT:
        advance_token();
        return new_type_qualifier(TYPE_QUALIFIER_RESTRICT);
    case TOKEN_VOLATILE:
        advance_token();
        return new_type_qualifier(TYPE_QUALIFIER_VOLATILE);
    case TOKEN_ATOMIC:
        advance_token();
        return new_type_qualifier(TYPE_QUALIFIER_ATOMIC);
    default:
        fatal_error("Expected type qualifier");
    }
}

//
// function_specifier
//     : INLINE
//     | NORETURN
//     ;
//
FunctionSpec *parse_function_specifier()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    FunctionSpecKind kind = current_token == TOKEN_INLINE ? FUNC_SPEC_INLINE : FUNC_SPEC_NORETURN;
    advance_token();
    return new_function_spec(kind);
}

//
// alignment_specifier
//     : ALIGNAS '(' type_name ')'
//     | ALIGNAS '(' constant_expression ')'
//     ;
//
AlignmentSpec *parse_alignment_specifier()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_ALIGNAS);
    expect_token(TOKEN_LPAREN);
    AlignmentSpec *as;
    if (is_type_specifier(current_token)) {
        as         = new_alignment_spec(ALIGN_SPEC_TYPE);
        as->u.type = parse_type_name();
    } else {
        as         = new_alignment_spec(ALIGN_SPEC_EXPR);
        as->u.expr = parse_constant_expression();
    }
    expect_token(TOKEN_RPAREN);
    return as;
}

//
// declarator
//    : pointer direct_declarator
//    | direct_declarator
//    ;
//
Declarator *parse_declarator()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Pointer *pointers = NULL;
    if (current_token == TOKEN_STAR) {
        pointers = parse_pointer();
    }
    Declarator *decl = parse_direct_declarator();
    if (pointers) {
        append_list(&decl->pointers, pointers);
    }
    return decl;
}

//
// direct_declarator
//     : IDENTIFIER
//     | '(' declarator ')'
//     | direct_declarator '[' ']'
//     | direct_declarator '[' '*' ']'
//     | direct_declarator '[' STATIC type_qualifier_list assignment_expression ']'
//     | direct_declarator '[' STATIC assignment_expression ']'
//     | direct_declarator '[' type_qualifier_list '*' ']'
//     | direct_declarator '[' type_qualifier_list STATIC assignment_expression ']'
//     | direct_declarator '[' type_qualifier_list assignment_expression ']'
//     | direct_declarator '[' type_qualifier_list ']'
//     | direct_declarator '[' assignment_expression ']'
//     | direct_declarator '(' parameter_type_list ')'
//     | direct_declarator '(' ')'
//     | direct_declarator '(' identifier_list ')'
//     ;
//
Declarator *parse_direct_declarator()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Declarator *decl;
    if (current_token == TOKEN_IDENTIFIER) {
        decl       = new_declarator();
        decl->name = xstrdup(current_lexeme);
        advance_token();
    } else if (current_token == TOKEN_LPAREN) {
        advance_token();
        decl = parse_declarator();
        expect_token(TOKEN_RPAREN);
    } else {
        fatal_error("Expected identifier or '('");
    }
    while (1) {
        DeclaratorSuffix *suffix = NULL;

        if (current_token == TOKEN_LBRACKET) {
            advance_token();
            suffix = new_declarator_suffix(SUFFIX_ARRAY);
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
            if (current_token == TOKEN_STAR) {
                advance_token();
            } else if (current_token_is_not(TOKEN_RBRACKET)) {
                size = parse_assignment_expression();
            }
            expect_token(TOKEN_RBRACKET);
            suffix->u.array.qualifiers = qualifiers;
            suffix->u.array.size       = size;
        } else if (current_token == TOKEN_LPAREN) {
            advance_token();
            suffix = new_declarator_suffix(SUFFIX_FUNCTION);
            if (current_token_is_not(TOKEN_RPAREN)) {
                suffix->u.function.params = parse_parameter_type_list(&suffix->u.function.variadic);
            }
            expect_token(TOKEN_RPAREN);
        } else {
            break;
        }
        append_list(&decl->suffixes, suffix);
    }
    return decl;
}

//
// pointer
//     : '*' type_qualifier_list pointer
//     | '*' type_qualifier_list
//     | '*' pointer
//     | '*'
//     ;
//
Pointer *parse_pointer()
{
    if (parser_debug) {
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

//
// type_qualifier_list
//     : type_qualifier
//     | type_qualifier_list type_qualifier
//     ;
//
TypeQualifier *parse_type_qualifier_list()
{
    if (parser_debug) {
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

//
// parameter_type_list
//     : parameter_list ',' ELLIPSIS
//     | parameter_list
//     ;
// Return a linked list of parameters.
// Return NULL for empty parameter list.
//
Param *parse_parameter_type_list(bool *variadic_flag)
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    // Assume non-variadic by default.
    *variadic_flag = false;
    if (current_token == TOKEN_RPAREN) {
        return NULL;
    }
    if (current_token == TOKEN_ELLIPSIS) {
        fatal_error("Variadic function must have at least one parameter");
    }
    Param *params = parse_parameter_list();
    if (current_token == TOKEN_COMMA && next_token() == TOKEN_ELLIPSIS) {
        advance_token();
        advance_token();
        *variadic_flag = true;
    }
    return params;
}

//
// parameter_list
//     : parameter_declaration
//     | parameter_list ',' parameter_declaration
//     ;
//
Param *parse_parameter_list()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Param *param = parse_parameter_declaration();
    if (current_token == TOKEN_COMMA && next_token() != TOKEN_ELLIPSIS) {
        advance_token();
        param->next = parse_parameter_list();
    }
    return param;
}

//
// abstract_declarator
//     : pointer direct_abstract_declarator
//     | pointer
//     | direct_abstract_declarator
//     ;
//
// direct_abstract_declarator
//     : '(' abstract_declarator ')'
//     | '[' ']'
//     | '[' '*' ']'
//     | '[' STATIC type_qualifier_list assignment_expression ']'
//     | '[' STATIC assignment_expression ']'
//     | '[' type_qualifier_list STATIC assignment_expression ']'
//     | '[' type_qualifier_list assignment_expression ']'
//     | '[' type_qualifier_list ']'
//     | '[' assignment_expression ']'
//     | direct_abstract_declarator '[' ']'
//     | direct_abstract_declarator '[' '*' ']'
//     | direct_abstract_declarator '[' STATIC type_qualifier_list assignment_expression ']'
//     | direct_abstract_declarator '[' STATIC assignment_expression ']'
//     | direct_abstract_declarator '[' type_qualifier_list assignment_expression ']'
//     | direct_abstract_declarator '[' type_qualifier_list STATIC assignment_expression ']'
//     | direct_abstract_declarator '[' type_qualifier_list ']'
//     | direct_abstract_declarator '[' assignment_expression ']'
//     | '(' ')'
//     | '(' parameter_type_list ')'
//     | direct_abstract_declarator '(' ')'
//     | direct_abstract_declarator '(' parameter_type_list ')'
//     ;
//
DeclaratorSuffix *parse_direct_abstract_declarator(Ident *name_out)
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    DeclaratorSuffix *suffix = NULL;
    DeclaratorSuffix **tail  = &suffix; // Pointer to the last suffix's next field

    while (1) {
        if (name_out && current_token == TOKEN_IDENTIFIER) {
            // Pass name to caller.
            *name_out = xstrdup(current_lexeme);
            advance_token();
        } else if (current_token == TOKEN_LPAREN) {
            // Handle '(' abstract_declarator ')' or '(' parameter_type_list ')' or '(' ')'
            advance_token(); // Consume '('
            if (current_token == TOKEN_RPAREN) {
                // Case: '(' ')'
                advance_token(); // Consume ')'
                DeclaratorSuffix *new_suffix    = new_declarator_suffix(SUFFIX_FUNCTION);
                new_suffix->u.function.params   = NULL;
                new_suffix->u.function.variadic = false;
                *tail                           = new_suffix;
                tail                            = &new_suffix->next;
            } else if (current_token == TOKEN_STAR) {
                // Case: '(' abstract_declarator ')'
                DeclaratorSuffix *new_suffix   = new_declarator_suffix(SUFFIX_POINTER);
                new_suffix->u.pointer.pointers = parse_pointer();
                new_suffix->u.pointer.suffix   = parse_direct_abstract_declarator(name_out);
                expect_token(TOKEN_RPAREN); // Consume ')'
                *tail = new_suffix;
                tail  = &new_suffix->next;
            } else if (current_token == TOKEN_ELLIPSIS) {
                fatal_error("Variadic function must have at least one parameter");
            } else {
                // Case: '(' parameter_type_list ')'
                DeclaratorSuffix *new_suffix  = new_declarator_suffix(SUFFIX_FUNCTION);
                new_suffix->u.function.params = parse_parameter_type_list(&new_suffix->u.function.variadic);
                expect_token(TOKEN_RPAREN); // Consume ')'
                *tail = new_suffix;
                tail  = &new_suffix->next;
            }
        } else if (current_token == TOKEN_LBRACKET) {
            // Handle array-related cases
            advance_token(); // Consume '['
            DeclaratorSuffix *new_suffix   = new_declarator_suffix(SUFFIX_ARRAY);
            new_suffix->u.array.size       = NULL;
            new_suffix->u.array.qualifiers = NULL;
            new_suffix->u.array.is_static  = false;

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
                    if (current_token_is_not(TOKEN_RBRACKET)) {
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

//
// parameter_declaration
//     : declaration_specifiers declarator
//     | declaration_specifiers abstract_declarator
//     | declaration_specifiers
//     ;
//
Param *parse_parameter_declaration()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Param *param = new_param();

    /* Parse declaration_specifiers */
    param->specifiers = parse_declaration_specifiers(&param->type);

    /* Check for declarator or abstract_declarator */
    if (current_token == TOKEN_IDENTIFIER) {
        param->name = xstrdup(current_lexeme);
        advance_token();
    }
    if (current_token == TOKEN_STAR || current_token == TOKEN_LBRACKET ||
        current_token == TOKEN_LPAREN) {
        Pointer *pointers = parse_pointer();
        DeclaratorSuffix *suffixes = parse_direct_abstract_declarator(param->name ? NULL : &param->name);

        /* Apply pointers and suffixes */
        param->type = type_apply_suffixes(type_apply_pointers(param->type, pointers), suffixes);
        free_pointer(pointers);
        free_declarator_suffix(suffixes);
    }
    return param;
}

//
// type_name : specifier_qualifier_list abstract_declarator
//           | specifier_qualifier_list
//           ;
// Returns non-NULL value.
//
Type *parse_type_name()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    TypeQualifier *qualifiers = NULL;
    TypeSpec *type_specs      = parse_specifier_qualifier_list(&qualifiers);

    /* Construct base Type from type_specs (simplified to first basic type) */
    Type *base_type = fuse_type_specifiers(type_specs);
    free_type_spec(type_specs);
    base_type->qualifiers = qualifiers;

    /* Parse optional abstract_declarator */
    Pointer *pointers          = NULL;
    DeclaratorSuffix *suffixes = NULL;
    if (current_token == TOKEN_STAR || current_token == TOKEN_LPAREN ||
        current_token == TOKEN_LBRACKET) {
        // Parse abstract_declarator
        pointers = parse_pointer();
        suffixes = parse_direct_abstract_declarator(NULL);
    }

    /* Apply pointers and suffixes to construct the final type */
    Type *type = type_apply_suffixes(type_apply_pointers(base_type, pointers), suffixes);
    free_pointer(pointers);
    free_declarator_suffix(suffixes);
    return type;
}

//
// initializer
//     : '{' initializer_list '}'
//     | '{' initializer_list ',' '}'
//     | assignment_expression
//     ;
//
Initializer *parse_initializer()
{
    if (parser_debug) {
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

//
// initializer_list
//     : designation initializer
//     | initializer
//     | initializer_list ',' designation initializer
//     | initializer_list ',' initializer
//     ;
//
InitItem *parse_initializer_list()
{
    if (parser_debug) {
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

//
// designation
//     : designator_list '='
//     ;
//
Designator *parse_designation()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Designator *designators = parse_designator_list();
    expect_token(TOKEN_ASSIGN);
    return designators;
}

//
// designator_list
//     : designator
//     | designator_list designator
//     ;
//
Designator *parse_designator_list()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Designator *designator = parse_designator();
    if (current_token == TOKEN_LBRACKET || current_token == TOKEN_DOT) {
        designator->next = parse_designator_list();
    }
    return designator;
}

//
// designator
//     : '[' constant_expression ']'
//     | '.' IDENTIFIER
//     ;
//
Designator *parse_designator()
{
    if (parser_debug) {
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
    Ident name = xstrdup(current_lexeme);
    expect_token(TOKEN_IDENTIFIER);
    Designator *d = new_designator(DESIGNATOR_FIELD);
    d->u.name     = name;
    return d;
}

//
// static_assert_declaration
//     : STATIC_ASSERT '(' constant_expression ',' STRING_LITERAL ')' ';'
//     ;
//
Declaration *parse_static_assert_declaration()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_STATIC_ASSERT);
    expect_token(TOKEN_LPAREN);
    Expr *condition = parse_constant_expression();
    expect_token(TOKEN_COMMA);
    char *message = xstrdup(current_lexeme);
    expect_token(TOKEN_STRING_LITERAL);
    expect_token(TOKEN_RPAREN);
    expect_token(TOKEN_SEMICOLON);

    Declaration *decl              = new_declaration(DECL_STATIC_ASSERT);
    decl->u.static_assrt.condition = condition;
    decl->u.static_assrt.message   = message;
    return decl;
}

//
// statement
//     : labeled_statement
//     | compound_statement
//     | expression_statement
//     | selection_statement
//     | iteration_statement
//     | jump_statement
//     ;
//
Stmt *parse_statement()
{
    if (parser_debug) {
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

//
// labeled_statement
//     : IDENTIFIER ':' statement
//     | CASE constant_expression ':' statement
//     | DEFAULT ':' statement
//     ;
//
Stmt *parse_labeled_statement()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_IDENTIFIER) {
        Ident label = xstrdup(current_lexeme);
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

//
// compound_statement
//     : '{' '}'
//     | '{'  block_item_list '}'
//     ;
//
Stmt *parse_compound_statement()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_LBRACE);
    scope_level++;

    DeclOrStmt *items = NULL;
    if (current_token_is_not(TOKEN_RBRACE)) {
        items = parse_block_item_list();
    }
    expect_token(TOKEN_RBRACE);
    scope_level--;
    symtab_purge(scope_level);

    Stmt *stmt       = new_stmt(STMT_COMPOUND);
    stmt->u.compound = items;
    return stmt;
}

//
// block_item_list
//     : block_item
//     | block_item_list block_item
//     ;
//
DeclOrStmt *parse_block_item_list()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    DeclOrStmt *item = parse_block_item();
    if (current_token_is_not(TOKEN_RBRACE)) {
        item->next = parse_block_item_list();
    }
    return item;
}

//
// block_item
//     : declaration
//     | statement
//     ;
//
DeclOrStmt *parse_block_item()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (is_storage_class_specifier(current_token) ||
        is_type_specifier(current_token) || is_type_qualifier(current_token) ||
        current_token == TOKEN_ATOMIC || current_token == TOKEN_INLINE ||
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

//
// expression_statement
//     : ';'
//     | expression ';'
//     ;
//
Stmt *parse_expression_statement()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = NULL;
    if (current_token_is_not(TOKEN_SEMICOLON)) {
        expr = parse_expression();
    }
    expect_token(TOKEN_SEMICOLON);
    Stmt *stmt   = new_stmt(STMT_EXPR);
    stmt->u.expr = expr;
    return stmt;
}

//
// selection_statement
//     : IF '(' expression ')' statement ELSE statement
//     | IF '(' expression ')' statement
//     | SWITCH '(' expression ')' statement
//     ;
//
Stmt *parse_selection_statement()
{
    if (parser_debug) {
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

//
// iteration_statement
//     : WHILE '(' expression ')' statement
//     | DO statement WHILE '(' expression ')' ';'
//     | FOR '(' expression_statement expression_statement ')' statement
//     | FOR '(' expression_statement expression_statement expression ')' statement
//     | FOR '(' declaration expression_statement ')' statement
//     | FOR '(' declaration expression_statement expression ')' statement
//     ;
//
Stmt *parse_iteration_statement()
{
    if (parser_debug) {
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
    if (is_storage_class_specifier(current_token) ||
        is_type_specifier(current_token) || is_type_qualifier(current_token) ||
        current_token == TOKEN_ATOMIC || current_token == TOKEN_INLINE || current_token == TOKEN_NORETURN ||
        current_token == TOKEN_ALIGNAS || current_token == TOKEN_STATIC_ASSERT) {
        Declaration *decl = parse_declaration();
        init              = new_for_init(FOR_INIT_DECL);
        init->u.decl      = decl;
    } else {
        Stmt *expr_stmt = parse_expression_statement();
        init            = new_for_init(FOR_INIT_EXPR);
        init->u.expr    = expr_stmt->u.expr;
        expr_stmt->u.expr = NULL;
        free_statement(expr_stmt);
    }
    Stmt *cond_stmt = parse_expression_statement();
    condition       = cond_stmt->u.expr;
    if (current_token_is_not(TOKEN_RPAREN)) {
        update = parse_expression();
    }
    cond_stmt->u.expr = NULL;
    free_statement(cond_stmt);
    expect_token(TOKEN_RPAREN);
    Stmt *body                 = parse_statement();
    Stmt *stmt                 = new_stmt(STMT_FOR);
    stmt->u.for_stmt.init      = init;
    stmt->u.for_stmt.condition = condition;
    stmt->u.for_stmt.update    = update;
    stmt->u.for_stmt.body      = body;
    return stmt;
}

//
// jump_statement
//     : GOTO IDENTIFIER ';'
//     | CONTINUE ';'
//     | BREAK ';'
//     | RETURN ';'
//     | RETURN expression ';'
//     ;
//
Stmt *parse_jump_statement()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_GOTO) {
        advance_token();
        Ident label = xstrdup(current_lexeme);
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
    if (current_token_is_not(TOKEN_SEMICOLON)) {
        expr = parse_expression();
    }
    expect_token(TOKEN_SEMICOLON);
    Stmt *stmt   = new_stmt(STMT_RETURN);
    stmt->u.expr = expr;
    return stmt;
}

//
// translation_unit
//     : external_declaration
//     | translation_unit external_declaration
//     ;
//
Program *parse_translation_unit()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Program *program = new_program();
    while (current_token != TOKEN_EOF) {
        ExternalDecl *decl = parse_external_declaration();
        append_list(&program->decls, decl);
    }
    return program;
}

//
// external_declaration
//     : function_definition
//     | declaration
//     ;
//
ExternalDecl *parse_external_declaration()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_STATIC_ASSERT) {

        // Static assert.
        ExternalDecl *ed  = new_external_decl(EXTERNAL_DECL_DECLARATION);
        ed->u.declaration = parse_static_assert_declaration();
        return ed;
    }

    // Parse declaration_specifiers (common to both function_definition and declaration).
    Type *base_type = NULL;
    DeclSpec *spec = parse_declaration_specifiers(&base_type);

    // Check if it's a declaration (ends with ';').
    if (current_token == TOKEN_SEMICOLON) {

        // Empty declaration.
        advance_token();
        Declaration *decl        = new_declaration(DECL_EMPTY);
        decl->u.empty.specifiers = spec;
        decl->u.empty.type       = base_type;

        ExternalDecl *ed  = new_external_decl(EXTERNAL_DECL_DECLARATION);
        ed->u.declaration = decl;
        return ed;
    }

    // Parse declarator (required for both function_definition and declaration with
    // init_declarator_list).
    Declarator *decl = parse_declarator();

    // Check if it's a declaration with init_declarator_list.
    if (current_token == TOKEN_SEMICOLON || current_token == TOKEN_COMMA ||
        current_token == TOKEN_ASSIGN) {

        // Declaration of variables.
        ExternalDecl *ed  = new_external_decl(EXTERNAL_DECL_DECLARATION);
        ed->u.declaration = new_declaration(DECL_VAR);

        ed->u.declaration->u.var.specifiers  = spec;
        ed->u.declaration->u.var.declarators = parse_init_declarator_list(decl, base_type);
        if (is_typedef(spec)) {
            define_typedef(ed->u.declaration->u.var.declarators);
        }
        expect_token(TOKEN_SEMICOLON);
        free_type(base_type);
        return ed;
    }

    // Must be a function_definition.
    Declaration *decl_list = NULL;
    if (current_token_is_not(TOKEN_LBRACE)) {
        decl_list = parse_declaration_list();
    }

    ExternalDecl *ed           = new_external_decl(EXTERNAL_DECL_FUNCTION);
    ed->u.function.specifiers  = spec;
    ed->u.function.name        = xstrdup(decl->name);
    ed->u.function.type        = type_apply_suffixes(type_apply_pointers(base_type, decl->pointers), decl->suffixes);
    ed->u.function.param_decls = decl_list;
    ed->u.function.body        = parse_compound_statement();
    free_declarator(decl);
    return ed;
}

//
// declaration_list
//     : declaration
//     | declaration_list declaration
//     ;
//
Declaration *parse_declaration_list()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Declaration *decl = parse_declaration();
    if (current_token_is_not(TOKEN_LBRACE)) {
        decl->next = parse_declaration_list();
    }
    return decl;
}

/* Main parsing function */
Program *parse(FILE *input)
{
    if (parser_debug) {
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
