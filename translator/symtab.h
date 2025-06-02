#include <stdbool.h>

#include "ast.h"

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

// Forward declaration for StaticInitializer (list of static initializers)
typedef struct StaticInitializer StaticInitializer;

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
            bool global;                  // True if variable has global linkage
            InitKind init_kind;           // Initialization state
            StaticInitializer *init_list; // For INIT_INITIALIZED
            // No data needed for INIT_TENTATIVE or INIT_NONE
        } static_var; // For SYM_STATIC

        StaticInitializer *const_init; // For SYM_CONST (string literals)
        // No data needed for SYM_LOCAL
    } u;
} Symbol;

typedef enum {
    INIT_CHAR,
    INIT_INT,
    INIT_LONG,
    INIT_UCHAR,
    INIT_UINT,
    INIT_ULONG,
    INIT_DOUBLE,
    INIT_STRING,
    INIT_ZERO,
    INIT_POINTER
} StaticInitKind;

// Structure for a (list of) static initializer(s)
typedef struct {
    struct StaticInitializer *next; // Next initializer in list
    StaticInitKind kind;
    union {
        int8_t char_val;    // INIT_CHAR
        int32_t int_val;    // INIT_INT
        int64_t long_val;   // INIT_LONG
        uint8_t uchar_val;  // INIT_UCHAR
        uint32_t uint_val;  // INIT_UINT
        uint64_t ulong_val; // INIT_ULONG
        double double_val;  // INIT_DOUBLE
        struct {
            char *str;
            bool null_terminated;
        } string_val;   // INIT_STRING
        int zero_bytes; // INIT_ZERO
        char *ptr_id;   // INIT_POINTER (string ID)
    } u;
} StaticInitializer;

// Initialize the symbol table (create an empty table)
void symtab_init(void);
// Postcondition: Symbol table is empty and ready for use.

// Destroy the symbol table (free all memory)
void symtab_destroy(void);
// Postcondition: All Symbol and StaticInitializer memory is freed, table is invalid.

// Add an automatic (local) variable
void symtab_add_automatic_var(char *name, Type *t);
// Precondition: name is a non-null string, t is a valid Type*.
// Postcondition: A Symbol with SYM_LOCAL, name, and t is added/replaced in symtab.

// Add a static variable
void symtab_add_static_var(char *name, Type *t, bool global, InitKind init_kind,
                           StaticInitializer *init_list);
// Precondition: name is a non-null string, t is a valid Type*, init_list is valid if init_kind ==
// INIT_INITIALIZED, else NULL. Postcondition: A Symbol with SYM_STATIC, name, t, global, and init
// state is added/replaced in symtab.

// Add a function
void symtab_add_fun(char *name, Type *t, bool global, bool defined);
// Precondition: name is a non-null string, t is a valid Type* (function type).
// Postcondition: A Symbol with SYM_FUNC, name, t, global, and defined is added/replaced in symtab.

// Add a string literal
char *symtab_add_string(char *s);
// Precondition: s is a non-null string.
// Postcondition: A Symbol with SYM_CONST, a unique name, type Array(Char, len(s)+1), and string
// initializer is added. Returns: The unique name (owned by symtab) for the string literal.

// Get a symbol by name (fails if not found)
Symbol *symtab_get(char *name);
// Precondition: name is a non-null string.
// Postcondition: Returns non-null Symbol* if found, else terminates with error.

// Get a symbol by name (returns NULL if not found)
Symbol *symtab_get_opt(char *name);
// Precondition: name is a non-null string.
// Postcondition: Returns Symbol* if found, else NULL.

// Check if a symbol has global linkage
bool symtab_is_global(char *name);
// Precondition: name is a non-null string, exists in symtab.
// Postcondition: Returns true if the symbol is global (SYM_FUNC or SYM_STATIC with global=true),
// else false.

// Iterator callback type for traversing symbols
typedef void (*SymtabIterator)(char *name, Symbol *symbol, void *user_data);
// Callback function to process a symbol during iteration.

// Iterate over all symbols
void symtab_iter(SymtabIterator callback, void *user_data);
// Precondition: callback is a valid function pointer.
// Postcondition: callback is called for each (name, Symbol*) pair in symtab.

// Get the number of symbols (for testing or debugging)
size_t symtab_size(void);
// Postcondition: Returns the number of symbols in the table.
