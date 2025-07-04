//
// Internal types for translator.
//
#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ast.h"
#include "tac.h"

// Level of scope for nested compound operators.
extern int scope_level;

// Enable debug output
extern int translator_debug;
extern int import_debug;
extern int export_debug;
extern int wio_debug;
extern int xalloc_debug;

// Resolve identifiers.
void resolve(ExternalDecl *ast);

// Typecheck definitions and uses of functions adn variables.
void typecheck_program(Program *p);
void typecheck_global_decl(ExternalDecl *d);

// Annotate loops and break/continue statements.
void label_loops(ExternalDecl *ast);

// Convert the AST to TAC.
Tac_TopLevel *translate(ExternalDecl *ast);

// Error handling.
void fatal_error(const char *message, ...) __attribute__((noreturn));

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

#ifdef GTEST_API_
// TODO
#endif

#ifdef __cplusplus
}
#endif

#endif /* TRANSLATOR_H */
