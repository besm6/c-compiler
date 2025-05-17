//
// Internal types for parser, hidden from consumers of AST.
//
#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

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

void print_type_spec(FILE *fd, TypeSpec *spec, int indent);

#ifdef __cplusplus
}
#endif

#endif /* PARSER_INTERNAL_H */
