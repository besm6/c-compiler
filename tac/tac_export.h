#ifndef TAC_EXPORT_H
#define TAC_EXPORT_H

#include <stdio.h>

#include "tac.h"

typedef struct _wfile WFILE;

#define TAC_TAG_STREAM 0x54414331u /* 'TAC1' */

void tac_export_begin_stream(WFILE *out);
void tac_export_toplevel(WFILE *out, const Tac_TopLevel *tl);
void tac_export_end_stream(WFILE *out);

#endif
