//
// Types for symbol table.
//
#ifndef SYMTAB_H
#define SYMTAB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "ast.h"
#include "tac.h"

// Enum for symbol kinds (corresponding to identifier_attrs)
typedef enum {
    SYM_FUNC,   // FunAttr
    SYM_STATIC, // StaticAttr
    SYM_CONST,  // ConstAttr
    SYM_LOCAL   // LocalAttr
} SymbolKind;

// Enum for initialization state (corresponding to initial_value)
typedef enum {
    INIT_TENTATIVE,   // Tentative
    INIT_INITIALIZED, // Initial
    INIT_NONE         // NoInitializer
} InitKind;

// Structure for a symbol entry
typedef struct Symbol {
    char *name;      // Symbol name (Ident, owned copy)
    Type *type;      // Symbol type (Type* from ast.h)
    SymbolKind kind; // Kind of symbol (func, static, const, local)
    union {
        struct {
            bool defined; // True if function body is defined
            bool global;  // True if function has global linkage
        } func;           // For SYM_FUNC

        struct {
            bool global;               // True if variable has global linkage
            InitKind init_kind;        // Initialization state
            Tac_StaticInit *init_list; // For INIT_INITIALIZED
            // No data needed for INIT_TENTATIVE or INIT_NONE
        } static_var; // For SYM_STATIC

        Tac_StaticInit *const_init; // For SYM_CONST (string literals)
        // No data needed for SYM_LOCAL
    } u;
} Symbol;

// Initialize the symbol table (create an empty table)
void symtab_init(void);
// Postcondition: Symbol table is empty and ready for use.

// Destroy the symbol table (free all memory)
void symtab_destroy(void);
// Postcondition: All Symbol and Tac_StaticInit memory is freed, table is empty.

// Add an automatic (local) variable
void symtab_add_automatic_var(const char *name, const Type *t);
// Precondition: name is a non-null string, t is a valid Type*.
// Postcondition: A Symbol with SYM_LOCAL, name, and t is added/replaced in symtab.

// Add a static variable
void symtab_add_static_var(const char *name, const Type *t, bool global, InitKind init_kind,
                           Tac_StaticInit *init_list);
// Precondition: name is a non-null string, t is a valid Type*, init_list is valid if init_kind ==
// INIT_INITIALIZED, else NULL. Postcondition: A Symbol with SYM_STATIC, name, t, global, and init
// state is added/replaced in symtab.

// Add a function
void symtab_add_fun(const char *name, const Type *t, bool global, bool defined);
// Precondition: name is a non-null string, t is a valid Type* (function type).
// Postcondition: A Symbol with SYM_FUNC, name, t, global, and defined is added/replaced in symtab.

// Add a string literal
char *symtab_add_string(const char *s);
// Precondition: s is a non-null string.
// Postcondition: A Symbol with SYM_CONST, a unique name, type Array(Char, len(s)+1), and string
// initializer is added. Returns: The unique name (owned by caller) for the string literal.

// Get a symbol by name (fails if not found)
Symbol *symtab_get(const char *name);
// Precondition: name is a non-null string.
// Postcondition: Returns non-null Symbol* if found, else terminates with error.

// Get a symbol by name (returns NULL if not found)
Symbol *symtab_get_opt(const char *name);
// Precondition: name is a non-null string.
// Postcondition: Returns Symbol* if found, else NULL.

// Check if a symbol has global linkage
bool symtab_is_global(const char *name);
// Precondition: name is a non-null string, exists in symtab.
// Postcondition: Returns true if the symbol is global (SYM_FUNC or SYM_STATIC with global=true),
// else false.

// Print all symbols.
void symtab_print(void);

//
// Allocation, deallocation.
//
Symbol *new_symbol(const char *name, Type *t, SymbolKind kind);
void free_symbol(Symbol *sym);

// Convert literal to given arithmetic type and return as Tac_StaticInit.
Tac_StaticInit *new_static_init_from_literal(const Type *type, const Literal *lit);

#ifdef __cplusplus
}
#endif

#endif /* SYMTAB_H */
