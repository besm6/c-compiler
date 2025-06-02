#ifndef TYPETAB_H
#define TYPETAB_H

#include "ast.h"

// Structure for a struct member entry
typedef struct {
    char *name; // Member name (Ident, owned copy)
    Type *type; // Member type (Type* from ast.h)
    int offset; // Offset within the struct (in bytes)
} TypeMember;

// Structure for a struct type entry
typedef struct {
    char *tag;           // Struct tag (Ident, owned copy)
    int alignment;       // Alignment requirement (in bytes)
    int size;            // Total size of the struct (in bytes)
    TypeMember *members; // Array of members
    int member_count;    // Number of members
    // Optional: Hash table for member name lookup (e.g., uthash)
    // struct MemberMap *member_map;
} TypeEntry;

// Initialize the type table (create an empty table)
void typetab_init(void);
// Postcondition: Type table is empty and ready for use.

// Destroy the type table (free all memory)
void typetab_destroy(void);
// Postcondition: All TypeEntry and TypeMember memory is freed, table is invalid.

// Add a struct definition
void typetab_add_struct_definition(char *tag, int alignment, int size, TypeMember *members,
                                   int member_count);
// Precondition: tag is a non-null string, members is a valid array of member_count elements or NULL
// if member_count is 0. Postcondition: A TypeEntry with tag, alignment, size, and copied members is
// added/replaced in typetab.

// Check if a struct tag exists
bool typetab_mem(char *tag);
// Precondition: tag is a non-null string.
// Postcondition: Returns true if tag exists in typetab, else false.

// Get a struct definition by tag (fails if not found)
TypeEntry *typetab_find(char *tag);
// Precondition: tag is a non-null string.
// Postcondition: Returns non-null TypeEntry* if found, else terminates with error.
// Example for `typetab_find`:
//    TypeEntry *typetab_find(char *tag) {
//        TypeEntry *entry;
//        HASH_FIND_STR(typetab, tag, entry);
//        if (!entry) {
//            fprintf(stderr, "Struct %s not found\n", tag);
//            exit(1);
//        }
//        return entry;
//    }

// Get members of a struct, sorted by offset
TypeMember *typetab_get_members(char *tag, int *count);
// Precondition: tag is a non-null string, exists in typetab, count is a valid pointer.
// Postcondition: Sets *count to number of members, returns a new array of TypeMember sorted by
// offset (caller must free). Example:
//    TypeMember *typetab_get_members(char *tag, int *count) {
//        TypeEntry *entry = typetab_find(tag);
//        *count = entry->member_count;
//        TypeMember *result = malloc(*count * sizeof(TypeMember));
//        memcpy(result, entry->members, *count * sizeof(TypeMember));
//        qsort(result, *count, sizeof(TypeMember),
//              [](const void *a, const void *b) {
//                  return ((TypeMember*)a)->offset - ((TypeMember*)b)->offset;
//              });
//        return result;
//    }

// Get member types of a struct
Type **typetab_get_member_types(char *tag, int *count);
// Precondition: tag is a non-null string, exists in typetab, count is a valid pointer.
// Postcondition: Sets *count to number of members, returns a new array of Type* (caller must free).

// Get the number of entries (for testing or debugging)
size_t typetab_size(void);
// Postcondition: Returns the number of entries in the table.

#endif
