#include "ast.h"
#include "string_map.h"
#include "scanner.h" // for debug

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
// Values can be:
//      TOKEN_TYPEDEF_NAME
//      TOKEN_ENUMERATION_CONSTANT
//
bool symtab_define(const char *name, int value, int level)
{
    //printf("--- define %s as %s at level %d\n", name,
    //value == TOKEN_TYPEDEF_NAME ? "TOKEN_TYPEDEF_NAME" :
    //value == TOKEN_ENUMERATION_CONSTANT ? "TOKEN_ENUMERATION_CONSTANT" : "???", level);
    return map_insert(&symtab, name, value, level);
}

//
// Remove one name.
//
void symtab_remove(const char *name)
{
    map_remove_key(&symtab, name);
}

//
// Remove names from the tree, which exceed given level.
//
void symtab_purge(int level)
{
    map_remove_level(&symtab, level);
}

//
// Deallocate the symbol table.
//
void symtab_free()
{
    map_free(&symtab);
}
