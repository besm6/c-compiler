//
// Internal types for translator.
//
#ifndef TRANSLATE_H
#define TRANSLATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ast.h"
#include "tac.h"

// Enable debug output
extern int translator_debug;
extern int import_debug;
extern int export_debug;
extern int wio_debug;
extern int xalloc_debug;

// Convert the AST to TAC.
Tac_TopLevel *translate(ExternalDecl *ast);

#ifdef __cplusplus
}
#endif

#endif /* TRANSLATE_H */
