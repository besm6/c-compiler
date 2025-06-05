//
// Internal types for translator.
//
#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tac.h"
#include "ast.h"

// Enable debug output
extern int translator_debug;
extern int import_debug;
extern int export_debug;
extern int wio_debug;
extern int xalloc_debug;

// Resolve identifiers.
void resolve(ExternalDecl *ast);

// Typecheck definitions and uses of functions adn variables.
void typecheck(ExternalDecl *ast);

// Annotate loops and break/continue statements.
void label_loops(ExternalDecl *ast);

// Convert the AST to TAC.
Tac_TopLevel *translate(ExternalDecl *ast);

// Error handling.
void _Noreturn fatal_error(const char *message, ...) __attribute__((noreturn));

#ifdef GTEST_API_
//TODO
#endif

#ifdef __cplusplus
}
#endif

#endif /* TRANSLATOR_H */
