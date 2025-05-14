#ifndef AST_H
#define AST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* Forward declarations for recursive types */
typedef struct Type Type;
typedef struct TypeQualifier TypeQualifier;
typedef struct Field Field;
typedef struct Enumerator Enumerator;
typedef struct ParamList ParamList;
typedef struct Param Param;
typedef struct Declaration Declaration;
typedef struct DeclSpec DeclSpec;
typedef struct StorageClass StorageClass;
typedef struct TypeSpec TypeSpec;
typedef struct FunctionSpec FunctionSpec;
typedef struct AlignmentSpec AlignmentSpec;
typedef struct InitDeclarator InitDeclarator;
typedef struct Declarator Declarator;
typedef struct Pointer Pointer;
typedef struct DeclaratorSuffix DeclaratorSuffix;
typedef struct Initializer Initializer;
typedef struct InitItem InitItem;
typedef struct Designator Designator;
typedef struct Expr Expr;
typedef struct Literal Literal;
typedef struct UnaryOp UnaryOp;
typedef struct BinaryOp BinaryOp;
typedef struct AssignOp AssignOp;
typedef struct GenericAssoc GenericAssoc;
typedef struct Stmt Stmt;
typedef struct DeclOrStmt DeclOrStmt;
typedef struct ForInit ForInit;
typedef struct Program Program;
typedef struct ExternalDecl ExternalDecl;

/* Identifier */
typedef char *Ident;

/* Types */
typedef enum {
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_SHORT,
    TYPE_INT,
    TYPE_LONG,
    TYPE_LONG_LONG,
    TYPE_SIGNED,   // Internal for parser only
    TYPE_UNSIGNED, // Internal for parser only
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_LONG_DOUBLE,
    TYPE_COMPLEX,
    TYPE_IMAGINARY,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_FUNCTION,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,
    TYPE_TYPEDEF_NAME,
    TYPE_ATOMIC
} TypeKind;

typedef enum { SIGNED_SIGNED, SIGNED_UNSIGNED } Signedness;

struct Type {
    TypeKind kind;
    union {
        struct {
            Signedness signedness;
        } integer; /* for Char, Short, Int, Long */
        struct {
            Type *base;
        } complex; /* for Complex, Imaginary */
        struct {
            Type *target;
            TypeQualifier *qualifiers;
        } pointer;
        struct {
            Type *element;
            Expr *size;
            TypeQualifier *qualifiers;
        } array;
        struct {
            Type *returnType;
            ParamList *params;
            bool variadic;
        } function;
        struct {
            Ident name;
            Field *fields;
        } struct_t; /* optional name */
        struct {
            Ident name;
            Enumerator *enumerators;
        } enum_t;
        struct {
            Ident name;
        } typedef_name;
        struct {
            Type *base;
        } atomic;
    } u;
    TypeQualifier *qualifiers; /* attributes */
};

typedef enum {
    TYPE_QUALIFIER_CONST,
    TYPE_QUALIFIER_RESTRICT,
    TYPE_QUALIFIER_VOLATILE,
    TYPE_QUALIFIER_ATOMIC
} TypeQualifierKind;

struct TypeQualifier {
    TypeQualifier *next; /* linked list */
    TypeQualifierKind kind;
};

struct Field {
    Field *next; /* linked list */
    bool is_anonymous;
    union {
        struct {
            Ident name;
            Type *type;
            Expr *bitfield;
        } named; /* optional name, bitfield */
        struct {
            Type *type;
        } anonymous;
    } u;
};

struct Enumerator {
    Enumerator *next; /* linked list */
    Ident name;
    Expr *value;      /* optional */
};

/* Parameter List */
struct ParamList {
    bool is_empty;
    union {
        Param *params;
    } u;
};

struct Param {
    Param *next; /* linked list */
    Ident name; /* optional */
    Type *type;
};

/* Declarations */
typedef enum { DECL_VAR, DECL_STATIC_ASSERT, DECL_EMPTY } DeclarationKind;

struct Declaration {
    Declaration *next; /* linked list for declaration_list */
    DeclarationKind kind;
    union {
        struct {
            DeclSpec *specifiers;
            InitDeclarator *declarators;
        } var;
        struct {
            Expr *condition;
            char *message;
        } static_assrt;
    } u;
};

struct DeclSpec {
    StorageClass *storage; /* optional */
    TypeQualifier *qualifiers;
    TypeSpec *type_specs;
    FunctionSpec *func_specs;
    AlignmentSpec *align_spec; /* optional */
};

typedef enum {
    STORAGE_CLASS_TYPEDEF,
    STORAGE_CLASS_EXTERN,
    STORAGE_CLASS_STATIC,
    STORAGE_CLASS_THREAD_LOCAL,
    STORAGE_CLASS_AUTO,
    STORAGE_CLASS_REGISTER
} StorageClassKind;

struct StorageClass {
    StorageClassKind kind;
};

typedef enum {
    TYPE_SPEC_BASIC,
    TYPE_SPEC_STRUCT,
    TYPE_SPEC_UNION,
    TYPE_SPEC_ENUM,
    TYPE_SPEC_TYPEDEF_NAME,
    TYPE_SPEC_ATOMIC
} TypeSpecKind;

struct TypeSpec {
    TypeSpec *next;            /* linked list */
    TypeSpecKind kind;
    union {
        Type *basic;
        struct {
            Ident name;
            Field *fields;
        } struct_spec; /* optional name */
        struct {
            Ident name;
            Enumerator *enumerators;
        } enum_spec;
        struct {
            Ident name;
        } typedef_name;
        struct {
            Type *type;
        } atomic;
    } u;
    TypeQualifier *qualifiers; /* attributes */
};

typedef enum { FUNC_SPEC_INLINE, FUNC_SPEC_NORETURN } FunctionSpecKind;

struct FunctionSpec {
    FunctionSpec *next; /* linked list */
    FunctionSpecKind kind;
};

typedef enum { ALIGN_SPEC_TYPE, ALIGN_SPEC_EXPR } AlignmentSpecKind;

struct AlignmentSpec {
    AlignmentSpecKind kind;
    union {
        Type *type;
        Expr *expr;
    } u;
};

struct InitDeclarator {
    InitDeclarator *next; /* linked list */
    Declarator *declarator;
    Initializer *init;    /* optional */
};

typedef enum { DECLARATOR_NAMED, DECLARATOR_ABSTRACT } DeclaratorKind;

struct Declarator {
    Declarator *next; /* linked list */
    DeclaratorKind kind;
    union {
        struct {
            Ident name;
            Pointer *pointers;
            DeclaratorSuffix *suffixes;
        } named;
        struct {
            Pointer *pointers;
            DeclaratorSuffix *suffixes;
        } abstract;
    } u;
};

struct Pointer {
    Pointer *next; /* linked list */
    TypeQualifier *qualifiers;
};

typedef enum { SUFFIX_ARRAY, SUFFIX_FUNCTION } DeclaratorSuffixKind;

struct DeclaratorSuffix {
    DeclaratorSuffix *next; /* linked list */
    DeclaratorSuffixKind kind;
    union {
        struct {
            Expr *size;
            TypeQualifier *qualifiers;
            bool is_static;
        } array;
        struct {
            ParamList *params;
            bool variadic;
        } function;
    } u;
};

typedef enum { INITIALIZER_SINGLE, INITIALIZER_COMPOUND } InitializerKind;

struct Initializer {
    InitializerKind kind;
    union {
        Expr *expr;
        InitItem *items;
    } u;
};

struct InitItem {
    InitItem *next; /* linked list */
    Designator *designators;
    Initializer *init;
};

typedef enum { DESIGNATOR_ARRAY, DESIGNATOR_FIELD } DesignatorKind;

struct Designator {
    Designator *next; /* linked list */
    DesignatorKind kind;
    union {
        Expr *expr; /* array index */
        Ident name; /* field name */
    } u;
};

/* Expressions */
typedef enum {
    EXPR_LITERAL,
    EXPR_VAR,
    EXPR_UNARY_OP,
    EXPR_BINARY_OP,
    EXPR_ASSIGN,
    EXPR_COND,
    EXPR_CAST,
    EXPR_CALL,
    EXPR_COMPOUND,
    EXPR_FIELD_ACCESS,
    EXPR_PTR_ACCESS,
    EXPR_POST_INC,
    EXPR_POST_DEC,
    EXPR_SIZEOF_EXPR,
    EXPR_SIZEOF_TYPE,
    EXPR_ALIGNOF,
    EXPR_GENERIC
} ExprKind;

struct Expr {
    Expr *next; /* linked list for argument lists */
    ExprKind kind;
    union {
        Literal *literal;
        Ident var;
        struct {
            UnaryOp *op;
            Expr *expr;
        } unary_op;
        struct {
            BinaryOp *op;
            Expr *left;
            Expr *right;
        } binary_op;
        struct {
            Expr *target;
            AssignOp *op;
            Expr *value;
        } assign;
        struct {
            Expr *condition;
            Expr *then_expr;
            Expr *else_expr;
        } cond;
        struct {
            Type *type;
            Expr *expr;
        } cast;
        struct {
            Expr *func;
            Expr *args;
        } call;
        struct {
            Type *type;
            InitItem *init;
        } compound_literal;
        struct {
            Expr *expr;
            Ident field;
        } field_access;
        struct {
            Expr *expr;
            Ident field;
        } ptr_access;
        Expr *post_inc;
        Expr *post_dec;
        Expr *sizeof_expr;
        Type *sizeof_type;
        Type * align_of;
        struct {
            Expr *controlling_expr;
            GenericAssoc *associations;
        } generic;
    } u;
    Type *type; /* attributes */
};

typedef enum { LITERAL_INT, LITERAL_FLOAT, LITERAL_CHAR, LITERAL_STRING, LITERAL_ENUM } LiteralKind;

struct Literal {
    LiteralKind kind;
    union {
        int int_val;
        float float_val;
        char char_val;
        char *string_val;
        Ident enum_const;
    } u;
};

typedef enum {
    UNARY_ADDRESS,
    UNARY_DEREF,
    UNARY_PLUS,
    UNARY_NEG,
    UNARY_BIT_NOT,
    UNARY_LOG_NOT,
    UNARY_PRE_INC,
    UNARY_PRE_DEC
} UnaryOpKind;

struct UnaryOp {
    UnaryOpKind kind;
};

typedef enum {
    BINARY_MUL,
    BINARY_DIV,
    BINARY_MOD,
    BINARY_ADD,
    BINARY_SUB,
    BINARY_LEFT_SHIFT,
    BINARY_RIGHT_SHIFT,
    BINARY_LT,
    BINARY_GT,
    BINARY_LE,
    BINARY_GE,
    BINARY_EQ,
    BINARY_NE,
    BINARY_BIT_AND,
    BINARY_BIT_XOR,
    BINARY_BIT_OR,
    BINARY_LOG_AND,
    BINARY_LOG_OR
} BinaryOpKind;

struct BinaryOp {
    BinaryOpKind kind;
};

typedef enum {
    ASSIGN_SIMPLE,
    ASSIGN_MUL,
    ASSIGN_DIV,
    ASSIGN_MOD,
    ASSIGN_ADD,
    ASSIGN_SUB,
    ASSIGN_LEFT,
    ASSIGN_RIGHT,
    ASSIGN_AND,
    ASSIGN_XOR,
    ASSIGN_OR
} AssignOpKind;

struct AssignOp {
    AssignOpKind kind;
};

typedef enum { GENERIC_ASSOC_TYPE, GENERIC_ASSOC_DEFAULT } GenericAssocKind;

struct GenericAssoc {
    GenericAssoc *next; /* linked list */
    GenericAssocKind kind;
    union {
        struct {
            Type *type;
            Expr *expr;
        } type_assoc;
        Expr *default_assoc;
    } u;
};

/* Statements */
typedef enum {
    STMT_EXPR,
    STMT_COMPOUND,
    STMT_IF,
    STMT_SWITCH,
    STMT_WHILE,
    STMT_DO_WHILE,
    STMT_FOR,
    STMT_GOTO,
    STMT_CONTINUE,
    STMT_BREAK,
    STMT_RETURN,
    STMT_LABELED,
    STMT_CASE,
    STMT_DEFAULT
} StmtKind;

struct Stmt {
    StmtKind kind;
    union {
        Expr *expr; /* optional */
        DeclOrStmt *compound;
        struct {
            Expr *condition;
            Stmt *then_stmt;
            Stmt *else_stmt;
        } if_stmt;
        struct {
            Expr *expr;
            Stmt *body;
        } switch_stmt;
        struct {
            Expr *condition;
            Stmt *body;
        } while_stmt;
        struct {
            Stmt *body;
            Expr *condition;
        } do_while;
        struct {
            ForInit *init;
            Expr *condition;
            Expr *update;
            Stmt *body;
        } for_stmt;
        Ident goto_label;
        struct {
            Ident label;
            Stmt *stmt;
        } labeled;
        struct {
            Expr *expr;
            Stmt *stmt;
        } case_stmt;
        Stmt *default_stmt;
    } u;
};

typedef enum { DECL_OR_STMT_DECL, DECL_OR_STMT_STMT } DeclOrStmtKind;

struct DeclOrStmt {
    DeclOrStmt *next; /* linked list */
    DeclOrStmtKind kind;
    union {
        Declaration *decl;
        Stmt *stmt;
    } u;
};

typedef enum { FOR_INIT_EXPR, FOR_INIT_DECL } ForInitKind;

struct ForInit {
    ForInitKind kind;
    union {
        Expr *expr;
        Declaration *decl;
    } u;
};

/* Program Structure */
struct Program {
    ExternalDecl *decls;
};

typedef enum { EXTERNAL_DECL_FUNCTION, EXTERNAL_DECL_DECLARATION } ExternalDeclKind;

struct ExternalDecl {
    ExternalDecl *next; /* linked list */
    ExternalDeclKind kind;
    union {
        struct {
            DeclSpec *specifiers;
            Declarator *declarator;
            Declaration *decls;
            Stmt *body;
        } function;
        Declaration *declaration;
    } u;
};

Program *parse(FILE *input);
void free_program(Program* program);

void print_program(FILE *fd, Program *program);
void print_declarator(FILE *fd, Declarator *decl, int indent);
void print_expression(FILE *fd, Expr *expr, int indent);
void print_statement(FILE *fd, Stmt *stmt, int indent);
void print_type_spec(FILE *fd, TypeSpec *spec, int indent);
void print_type(FILE *fd, Type *type, int indent);
void print_type_qualifiers(FILE *fd, TypeQualifier *qualifiers, int indent);
extern const char *type_kind_str[];

void free_declarator(Declarator *decl);
void free_expression(Expr *expr);
void free_statement(Stmt *stmt);
void free_type(Type *type);

#ifdef GTEST_API_
void advance_token(void);
Declarator *parse_declarator(void);
Expr *parse_primary_expression(void);
Expr *parse_expression(void);
Stmt *parse_statement(void);
Type *parse_type_name(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* AST_H */
