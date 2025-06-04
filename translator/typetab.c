#include "typetab.h"

#include <stdint.h>
//#include <string.h>

#include "ast.h"
#include "string_map.h"
#include "translator.h"
#include "xalloc.h"

static StringMap typetab;

//
// Deallocate FieldDef.
//
void free_field_definition(FieldDef *def)
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
void free_struct_definition(StructDef *def)
{
    if (def) {
        free_field_definition(def->members);
        xfree(def->tag);
        xfree(def);
    }
}

static void typetab_destroy_callback(intptr_t ptr)
{
    free_struct_definition((StructDef *)ptr);
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
