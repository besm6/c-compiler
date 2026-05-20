#ifndef BESM6_CODEGEN_H
#define BESM6_CODEGEN_H

#include <stdio.h>

#include "tac.h"

#ifdef __cplusplus
extern "C" {
#endif

// Translate one TAC toplevel declaration to Madlen assembly written to `out`.
void codegen_program(const Tac_TopLevel *tl, FILE *out);

#ifdef __cplusplus
}
#endif

#endif // BESM6_CODEGEN_H
