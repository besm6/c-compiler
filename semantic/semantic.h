//
// Internal types for translator.
//
#ifndef SEMANTIC_H
#define SEMANTIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ast.h"
#include "tac.h"
#include <stdnoreturn.h>

// Level of scope for nested compound operators.
extern int scope_level;

// Enable debug output
extern int semantic_debug;
extern int xalloc_debug;

// Semantic analysis entry points: type-check and label loops.
void typecheck_decl(ExternalDecl *d);
void typecheck_program(Program *p);

// Annotate loops and break/continue statements.
void label_loops(ExternalDecl *ast);

// Error handling.
_Noreturn void fatal_error(const char *message, ...);

// Convert literal to given arithmetic type and return as Tac_StaticInit.
Tac_StaticInit *new_static_init_from_literal(const Type *type, const Literal *lit);

//
// Helpers for Type.
//
size_t get_size(const Type *t);
size_t get_alignment(const Type *t);
bool is_complete(const Type *t);
bool is_scalar(const Type *t);
bool is_arithmetic(const Type *t);
bool is_integer(const Type *t);
bool is_character(const Type *t);
bool is_pointer(const Type *t);
bool is_complete_pointer(const Type *t);
bool is_signed(const Type *t);
int round_away_from_zero(int alignment, int size);
Type *resolve_typedef_names(Type *t);

#ifdef __cplusplus
}
#endif

#endif /* SEMANTIC_H */
