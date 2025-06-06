//
// Definitions of struct and union.
//
#ifndef TYPETAB_H
#define TYPETAB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ast.h"

// Structure for a struct member entry
typedef struct FieldDef {
    struct FieldDef *next; // Next member in list
    char *name;            // Member name (Ident, owned copy)
    Type *type;            // Member type (Type* from ast.h)
    int offset;            // Offset within the struct (in bytes)
} FieldDef;

// Structure for a struct type entry
typedef struct {
    char *tag;         // Struct tag (Ident, owned copy)
    int alignment;     // Alignment requirement (in bytes)
    int size;          // Total size of the struct (in bytes)
    FieldDef *members; // List of members, sorted by offset
} StructDef;

// Initialize the type table (create an empty table)
void typetab_init(void);
// Postcondition: Type table is empty and ready for use.

// Destroy the type table (free all memory)
void typetab_destroy(void);
// Postcondition: All StructDef and FieldDef memory is freed, table is empty.

// Add a struct definition
void typetab_add_struct(const char *tag, int alignment, int size, FieldDef *members);
// Precondition: tag is a non-null string, members is a valid list of elements or NULL.
// Postcondition: A StructDef with tag, alignment, size, and copied members is added/replaced in typetab.

// Check if a struct tag exists
bool typetab_exists(const char *tag);
// Precondition: tag is a non-null string.
// Postcondition: Returns true if tag exists in typetab, else false.

// Get a struct definition by tag (fails if not found)
StructDef *typetab_find(const char *tag);
// Precondition: tag is a non-null string.
// Postcondition: Returns non-null StructDef* if found, else terminates with error.

// Print all types.
void typetab_print(void);

// Allocate a FieldDef
FieldDef *new_member(const char *name, Type *type, int offset);

#ifdef __cplusplus
}
#endif

#endif /* TYPETAB_H */
