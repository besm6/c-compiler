#include "symtab.h"

#include <stdint.h>
#include <string.h>

#include "ast.h"
#include "internal.h"
#include "string_map.h"
#include "translator.h"
#include "xalloc.h"

StringMap symtab;

//
// Build new symbol.
//
Symbol *new_symbol(const char *name, Type *t, SymbolKind kind)
{
    if (translator_debug) {
        printf("--- %s() %s\n", __func__, name);
        print_type(stdout, t, 4);
    }
    Symbol *sym = xalloc(sizeof(Symbol), __func__, __FILE__, __LINE__);
    sym->name   = xstrdup(name);
    sym->type   = t;
    sym->kind   = kind;
    return sym;
}

//
// Deallocate symbol.
//
void free_symbol(Symbol *sym)
{
    if (!sym)
        return;
    switch (sym->kind) {
    case SYM_STATIC:
        free_tac_static_init(sym->u.static_var.init_list);
        break;
    case SYM_CONST:
        free_tac_static_init(sym->u.const_init);
        break;
    default:
        break;
    }
    free_type(sym->type);
    xfree(sym->name);
    xfree(sym);
}

static void symtab_destroy_callback(intptr_t ptr)
{
    free_symbol((Symbol *)ptr);
}

//
// Initialize the symbol table (create an empty table)
// Postcondition: Symbol table is empty and ready for use.
//
void symtab_init()
{
    // Empty.
}

//
// Destroy the symbol table (free all memory)
// Postcondition: All Symbol and Tac_StaticInit memory is freed, table is empty.
//
void symtab_destroy()
{
    map_destroy_free(&symtab, symtab_destroy_callback);
}

//
// Add an automatic (local) variable
// Precondition: name is a non-null string, t is a valid Type*.
// Postcondition: A Symbol with SYM_LOCAL, name, and linkage is added/replaced in symtab.
//
void symtab_add_automatic_var_linkage(const char *name, bool has_linkage, int level)
{
    Symbol *sym = new_symbol(name, NULL, SYM_LOCAL);

    sym->has_linkage = has_linkage;
    map_insert_free(&symtab, name, (intptr_t)sym, level, symtab_destroy_callback);
}

//
// Add an automatic (local) variable
// Precondition: name is a non-null string, t is a valid Type*.
// Postcondition: A Symbol with SYM_LOCAL, name, and t is added/replaced in symtab.
//
void symtab_add_automatic_var_type(const char *name, const Type *t, int level)
{
    Symbol *sym = new_symbol(name, clone_type(t, __func__, __FILE__, __LINE__), SYM_LOCAL);

    map_insert_free(&symtab, name, (intptr_t)sym, level, symtab_destroy_callback);
}

//
// Add a static variable
// Precondition: name is a non-null string, t is a valid Type*, init_list is valid if init_kind ==
// INIT_INITIALIZED, else NULL. Postcondition: A Symbol with SYM_STATIC, name, t, global, and init
// state is added/replaced in symtab.
//
void symtab_add_static_var(const char *name, const Type *t, bool global, InitKind init_kind,
                           Tac_StaticInit *init_list)
{
    Symbol *sym = new_symbol(name, clone_type(t, __func__, __FILE__, __LINE__), SYM_STATIC);
    sym->u.static_var.global    = global;
    sym->u.static_var.init_kind = init_kind;
    sym->u.static_var.init_list = init_list;

    map_insert_free(&symtab, name, (intptr_t)sym, 0, symtab_destroy_callback);
}

//
// Add a function
// Precondition: name is a non-null string, t is a valid Type* (function type).
// Postcondition: A Symbol with SYM_FUNC, name, t, global, and defined is added/replaced in symtab.
//
void symtab_add_fun(const char *name, const Type *t, bool global, bool defined)
{
    Symbol *sym         = new_symbol(name, clone_type(t, __func__, __FILE__, __LINE__), SYM_FUNC);
    sym->u.func.global  = global;
    sym->u.func.defined = defined;

    map_insert_free(&symtab, name, (intptr_t)sym, 0, symtab_destroy_callback);
}

//
// Add a string literal
// Precondition: s is a non-null string.
// Postcondition: A Symbol with SYM_CONST, a unique name, type Array(Char, len(s)+1), and string
// initializer is added. Returns: The unique name (owned by caller) for the string literal.
//
char *symtab_add_string(const char *s)
{
    if (!s) {
        fatal_error("symtab_add_string: NULL string input");
        return NULL; // cannot happen
    }

    // Generate unique identifier (e.g., _str0, _str1)
    // TODO: move to a separate file unique.c
    static int str_id = 0;
    char name[32];
    snprintf(name, sizeof(name), "_str%d", str_id++);

    // Create array type: char[strlen(s) + 1]
    Type *t            = new_type(TYPE_ARRAY, __func__, __FILE__, __LINE__);
    t->u.array.element = new_type(TYPE_CHAR, __func__, __FILE__, __LINE__);
    t->u.array.size    = new_expression(EXPR_LITERAL);

    // Set array size
    t->u.array.size->u.literal            = new_literal(LITERAL_INT);
    t->u.array.size->u.literal->u.int_val = strlen(s) + 1;

    // Create Tac_StaticInit
    Tac_StaticInit *init           = new_tac_static_init(TAC_STATIC_INIT_STRING);
    init->u.string.val             = xstrdup(s);
    init->u.string.null_terminated = true;

    // Add to symbol table
    Symbol *sym       = new_symbol(name, t, SYM_CONST);
    sym->u.const_init = init;
    map_insert_free(&symtab, name, (intptr_t)sym, 0, symtab_destroy_callback);

    // Return the unique name (owned by caller)
    return xstrdup(name);
}

//
// Get a symbol by name (fails if not found)
// Precondition: name is a non-null string.
// Postcondition: Returns non-null Symbol* if found, else terminates with error.
//
Symbol *symtab_get(const char *name)
{
    Symbol *sym = symtab_get_opt(name);
    if (!sym) {
        fatal_error("Symbol '%s' not found", name);
    }
    return sym;
}

//
// Get a symbol by name (returns NULL if not found)
// Precondition: name is a non-null string.
// Postcondition: Returns Symbol* if found, else NULL.
//
Symbol *symtab_get_opt(const char *name)
{
    intptr_t value = 0;
    if (!map_get(&symtab, name, &value)) {
        return NULL;
    }
    return (Symbol *)value;
}

//
// Check if a symbol has global linkage
// Precondition: name is a non-null string, exists in symtab.
// Postcondition: Returns true if the symbol is global (SYM_FUNC or SYM_STATIC with global=true),
// else false.
//
bool symtab_is_global(const char *name)
{
    Symbol *sym = symtab_get_opt(name);
    if (!sym) {
        return false;
    }
    switch (sym->kind) {
    case SYM_FUNC:
        return sym->u.func.global;
    case SYM_STATIC:
        return sym->u.static_var.global;
    default:
        return false;
    }
}

//
// Remove names from the tree, which exceed given level.
//
void symtab_purge(int level)
{
    map_remove_level(&symtab, level);
}
