#include "symtab.h"

#include <stdint.h>

#include "ast.h"
#include "internal.h"
#include "string_map.h"
#include "translator.h"
#include "xalloc.h"

static StringMap symtab;

//
// Build new symbol.
//
Symbol *new_symbol(const char *name, Type *t, SymbolKind kind)
{
    Symbol *sym = xalloc(sizeof(Symbol), __func__, __FILE__, __LINE__);
    sym->name   = xstrdup(name);
    sym->type   = t;
    sym->kind   = kind;
    return sym;
}

//
// Build new StaticInitializer.
//
StaticInitializer *new_static_initializer(StaticInitKind kind)
{
    StaticInitializer *init = xalloc(sizeof(StaticInitializer), __func__, __FILE__, __LINE__);
    init->kind              = kind;
    return init;
}

//
// Deallocate StaticInitializer list.
//
void free_static_initializer(StaticInitializer *list)
{
    while (list != NULL) {
        StaticInitializer *next = list->next;
        if (list->kind == INIT_STRING) {
            xfree(list->u.string_val.str);
        } else if (list->kind == INIT_POINTER) {
            xfree(list->u.ptr_id);
        }
        xfree(list);
        list = next;
    }
}

//
// Deallocate symbol.
//
void free_symbol(Symbol *sym)
{
    switch (sym->kind) {
    case SYM_STATIC:
        free_static_initializer(sym->u.static_var.init_list);
        break;
    case SYM_CONST:
        free_static_initializer(sym->u.const_init);
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
    free_symbol((Symbol*)ptr);
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
// Postcondition: All Symbol and StaticInitializer memory is freed, table is invalid.
//
void symtab_destroy()
{
    map_destroy_free(&symtab, symtab_destroy_callback);
}

//
// Add an automatic (local) variable
// Precondition: name is a non-null string, t is a valid Type*.
// Postcondition: A Symbol with SYM_LOCAL, name, and t is added/replaced in symtab.
//
void symtab_add_automatic_var(const char *name, const Type *t)
{
    Symbol *sym = new_symbol(name, clone_type(t), SYM_LOCAL);

    map_insert(&symtab, name, (intptr_t)sym, 0);
}

//
// Add a static variable
// Precondition: name is a non-null string, t is a valid Type*, init_list is valid if init_kind ==
// INIT_INITIALIZED, else NULL. Postcondition: A Symbol with SYM_STATIC, name, t, global, and init
// state is added/replaced in symtab.
//
void symtab_add_static_var(const char *name, const Type *t, bool global, InitKind init_kind,
                           StaticInitializer *init_list)
{
    Symbol *sym                 = new_symbol(name, clone_type(t), SYM_STATIC);
    sym->u.static_var.global    = global;
    sym->u.static_var.init_kind = init_kind;
    sym->u.static_var.init_list = init_list;

    map_insert(&symtab, name, (intptr_t)sym, 0);
}

//
// Add a function
// Precondition: name is a non-null string, t is a valid Type* (function type).
// Postcondition: A Symbol with SYM_FUNC, name, t, global, and defined is added/replaced in symtab.
//
void symtab_add_fun(const char *name, const Type *t, bool global, bool defined)
{
    Symbol *sym         = new_symbol(name, clone_type(t), SYM_FUNC);
    sym->u.func.global  = global;
    sym->u.func.defined = defined;

    map_insert(&symtab, name, (intptr_t)sym, 0);
}

//
// Add a const array (for string literal)
// Precondition: name is a non-null string, t is type Array(Char, len(s)+1).
// Postcondition: A Symbol with SYM_CONST, name, t, and string initializer is added.
//
void symtab_add_const(const char *name, const Type *t, StaticInitializer *init)
{
    Symbol *sym       = new_symbol(name, clone_type(t), SYM_CONST);
    sym->u.const_init = init;

    map_insert(&symtab, name, (intptr_t)sym, 0);
}

//
// Add a string literal
// Precondition: s is a non-null string.
// Postcondition: A Symbol with SYM_CONST, a unique name, type Array(Char, len(s)+1), and string
// initializer is added. Returns: The unique name (owned by symtab) for the string literal.
//
const char *symtab_add_string(const char *s)
{
    //TODO
    return NULL;
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
