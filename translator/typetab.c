#include "typetab.h"

#include <stdint.h>
// #include <string.h>

#include "ast.h"
#include "string_map.h"
#include "translator.h"
#include "xalloc.h"

StringMap typetab;

//
// Allocate a FieldDef
//
FieldDef *new_member(const char *name, Type *type, int offset)
{
    FieldDef *field = xalloc(sizeof(FieldDef), __func__, __FILE__, __LINE__);
    field->name     = xstrdup(name);
    field->type     = type;
    field->offset   = offset;
    return field;
}

//
// Deallocate FieldDef.
//
void free_member(FieldDef *def)
{
    while (def) {
        FieldDef *next = def->next;

        free_type(def->type);
        xfree(def->name);
        xfree(def);
        def = next;
    }
}

//
// Deallocate StructDef.
//
void free_struct(StructDef *def)
{
    if (def) {
        free_member(def->members);
        xfree(def->tag);
        xfree(def);
    }
}

static void typetab_destroy_callback(intptr_t ptr)
{
    free_struct((StructDef *)ptr);
}

//
// Initialize the type table (create an empty table)
// Postcondition: StructDef table is empty and ready for use.
//
void typetab_init()
{
    // Empty.
}

//
// Destroy the type table (free all memory)
// Postcondition: All StructDef and FieldDef memory is freed, table is empty.
//
void typetab_destroy()
{
    map_destroy_free(&typetab, typetab_destroy_callback);
}

//
// Add a struct definition
// Precondition: tag is a non-null string, members is a valid list of elements or NULL.
// Postcondition: A StructDef with tag, alignment, size, and copied members is added/replaced in
// typetab.
//
void typetab_add_struct(const char *tag, int alignment, int size, FieldDef *members, int level)
{
    if (translator_debug) {
        printf("--- %s() %s\n", __func__, tag);
    }
    // Build new definition.
    StructDef *def = xalloc(sizeof(StructDef), __func__, __FILE__, __LINE__);
    def->tag       = xstrdup(tag);
    def->alignment = alignment;
    def->size      = size;
    def->members   = members;

    map_insert_free(&typetab, tag, (intptr_t)def, level, typetab_destroy_callback);
}

//
// Check if a struct tag exists
// Precondition: tag is a non-null string.
// Postcondition: Returns true if tag exists in typetab, else false.
//
bool typetab_exists(const char *tag)
{
    intptr_t value = 0;
    if (!map_get(&typetab, tag, &value)) {
        return false;
    }
    return true;
}

//
// Get a struct definition by tag (fails if not found)
// Precondition: tag is a non-null string.
// Postcondition: Returns non-null StructDef* if found, else terminates with error.
//
StructDef *typetab_find(const char *tag)
{
    intptr_t value = 0;
    if (!map_get(&typetab, tag, &value)) {
        fatal_error("Struct or union '%s' not found", tag);
    }
    return (StructDef *)value;
}

//
// Remove names from the tree, which exceed given level.
//
void typetab_purge(int level)
{
    map_remove_level(&typetab, level);
}
