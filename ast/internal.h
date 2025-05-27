//
// Internal types for parser, hidden from consumers of AST.
//
#ifndef AST_INTERNAL_H
#define AST_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "ast.h"

/* Forward declarations for recursive types */
typedef struct Declarator Declarator;
typedef struct DeclaratorSuffix DeclaratorSuffix;
typedef struct Pointer Pointer;
typedef struct TypeSpec TypeSpec;

typedef enum {
    TYPE_SPEC_BASIC,
    TYPE_SPEC_STRUCT,
    TYPE_SPEC_UNION,
    TYPE_SPEC_ENUM,
    TYPE_SPEC_TYPEDEF_NAME,
    TYPE_SPEC_ATOMIC
} TypeSpecKind; // Internal for parser only

struct TypeSpec { // Internal for parser only
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

struct Declarator {
    Declarator *next; /* linked list */
    Ident name;       /* NULL for abstract declarator */
    Pointer *pointers;
    DeclaratorSuffix *suffixes;
};

struct Pointer {
    Pointer *next; /* linked list */
    TypeQualifier *qualifiers;
};

typedef enum { SUFFIX_ARRAY, SUFFIX_FUNCTION, SUFFIX_POINTER } DeclaratorSuffixKind;

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
            Param *params;
            bool variadic;
        } function;
        struct {
            Pointer *pointers;
            DeclaratorSuffix *suffix;
        } pointer;
    } u;
};

//
// Print
//
void print_type_spec(FILE *fd, const TypeSpec *spec, int indent);
void print_declarator(FILE *fd, Declarator *decl, int indent);

//
// Allocate
//
Type *new_type(TypeKind kind);
TypeQualifier *new_type_qualifier(TypeQualifierKind kind);
Field *new_field(void);
Enumerator *new_enumerator(Ident name, Expr *value);
Param *new_param(void);
Declaration *new_declaration(DeclarationKind kind);
DeclSpec *new_decl_spec(void);
TypeSpec *new_type_spec(TypeSpecKind kind);
FunctionSpec *new_function_spec(FunctionSpecKind kind);
AlignmentSpec *new_alignment_spec(AlignmentSpecKind kind);
InitDeclarator *new_init_declarator(void);
Declarator *new_declarator(void);
Pointer *new_pointer(void);
DeclaratorSuffix *new_declarator_suffix(DeclaratorSuffixKind kind);
Initializer *new_initializer(InitializerKind kind);
InitItem *new_init_item(Designator *designators, Initializer *init);
Designator *new_designator(DesignatorKind kind);
Expr *new_expression(ExprKind kind);
Literal *new_literal(LiteralKind kind);
BinaryOp *new_binary_op(BinaryOpKind kind);
AssignOp *new_assign_op(AssignOpKind kind);
GenericAssoc *new_generic_assoc(GenericAssocKind kind);
Stmt *new_stmt(StmtKind kind);
DeclOrStmt *new_decl_or_stmt(DeclOrStmtKind kind);
ForInit *new_for_init(ForInitKind kind);
ExternalDecl *new_external_decl(ExternalDeclKind kind);
Program *new_program(void);

//
// Deallocate
//
void free_declarator(Declarator *decl);
void free_decl_spec(DeclSpec *spec);
void free_type_spec(TypeSpec *ts);
void free_pointer(Pointer *pointer);
void free_declarator_suffix(DeclaratorSuffix *suffix);

//
// Clone
//
Type *clone_type(const Type *type);
TypeQualifier *clone_type_qualifier(const TypeQualifier *qualifier);
Param *clone_param(const Param *param);
Expr *clone_expression(const Expr *expression);
Literal *clone_literal(const Literal *literal);
BinaryOp *clone_binary_op(const BinaryOp *op);
AssignOp *clone_assign_op(const AssignOp *op);
GenericAssoc *clone_generic_assoc(const GenericAssoc *assoc);
InitItem *clone_init_item(const InitItem *item);
TypeSpec *clone_type_spec(const TypeSpec *ts);
Field *clone_field(const Field *field);
Enumerator *clone_enumerator(const Enumerator *enumerator);

#ifdef __cplusplus
}
#endif

#endif /* AST_INTERNAL_H */
