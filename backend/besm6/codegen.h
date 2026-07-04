#ifndef BESM6_CODEGEN_H
#define BESM6_CODEGEN_H

#include <stdio.h>

#include "besm.h"
#include "tac.h"

#ifdef __cplusplus
extern "C" {
#endif

// Translate one TAC toplevel declaration to assembly (in the selected dialect)
// written to `out`.  `program` is the head of the full translation-unit toplevel
// chain; it is used to identify module-level names so they are not assigned frame
// slots.
void codegen_program(const Tac_TopLevel *program, const Tac_TopLevel *tl, FILE *out,
                     Besm_Dialect dialect);

#ifdef __cplusplus
}
#endif

#endif // BESM6_CODEGEN_H
