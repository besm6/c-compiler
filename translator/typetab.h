#ifndef TYPETAB_H
#define TYPETAB_H

#include "ast.h"

// Structure for a struct member entry
typedef struct TypeMember {
    struct TypeMember *next; // Next member in list
    char *name;              // Member name (Ident, owned copy)
    Type *type;              // Member type (Type* from ast.h)
    int offset;              // Offset within the struct (in bytes)
} TypeMember;

// Structure for a struct type entry
typedef struct {
    char *tag;           // Struct tag (Ident, owned copy)
    int alignment;       // Alignment requirement (in bytes)
    int size;            // Total size of the struct (in bytes)
    TypeMember *members; // List of members, sorted by offset
} TypeEntry;

// Initialize the type table (create an empty table)
void typetab_init(void);
// Postcondition: Type table is empty and ready for use.

// Destroy the type table (free all memory)
void typetab_destroy(void);
// Postcondition: All TypeEntry and TypeMember memory is freed, table is invalid.

// Add a struct definition
void typetab_add_struct_definition(char *tag, int alignment, int size, TypeMember *members);
// Precondition: tag is a non-null string, members is a valid list of elements or NULL.
// Postcondition: A TypeEntry with tag, alignment, size, and copied members is added/replaced in typetab.

// Check if a struct tag exists
bool typetab_exists(char *tag);
// Precondition: tag is a non-null string.
// Postcondition: Returns true if tag exists in typetab, else false.

// Get a struct definition by tag (fails if not found)
TypeEntry *typetab_find(char *tag);
// Precondition: tag is a non-null string.
// Postcondition: Returns non-null TypeEntry* if found, else terminates with error.

#endif
