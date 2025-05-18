//
// Internal types for parser, hidden from consumers of AST.
//
#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

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

void print_type_spec(FILE *fd, TypeSpec *spec, int indent);
void print_declarator(FILE *fd, Declarator *decl, int indent);

void free_declarator(Declarator *decl);
void free_decl_spec(DeclSpec *spec);

Declarator *parse_declarator(void);

#ifdef __cplusplus
}
#endif

#endif /* PARSER_INTERNAL_H */
