#include "ast.h"
#include "string_map.h"

static StringMap symtab;

//
// Find name in the symbol table and return value.
// When not found, return 0.
//
int symtab_find(const char *name)
{
    int value = 0;
    if (map_get(&symtab, name, &value)) {
        return value;
    }
    return 0;
}

//
// Add name to the symbol table, with given value, at given level.
//
bool symtab_define(const char *name, int value, int level)
{
    return map_insert(&symtab, name, value, level);
}

//
// Remove names from the tree, which exceed given level.
//
void symtab_purge(int level)
{
    //TODO: map_remove_level(&symtab, level);
}

//
// Deallocate the symbol table.
//
void symtab_free()
{
    map_free(&symtab);
}
