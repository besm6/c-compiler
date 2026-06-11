#ifndef BESM6_FRAME_H
#define BESM6_FRAME_H

#include <stdbool.h>

#include "tac.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// Frame: maps TAC variable names to (reg, offset) frame slots.
//
// Params are assigned (REG_PAR, 0), (REG_PAR, 1), ... in declaration order.
// All other named temporaries in the function body are assigned
// (REG_AUTO, 0), (REG_AUTO, 1), ... in first-seen order.
//
// Access pattern:
//   load param  i: REG_PAR ,XTA, i
//   store param i: REG_PAR ,ATX, i
//   load auto   j: REG_AUTO ,XTA, j
//   store auto  j: REG_AUTO ,ATX, j
//
typedef struct Frame Frame;

// Build frame for a function toplevel node.
// `program` is the full list of toplevels in the translation unit; it is used
// to identify module-level names (StaticVariable/StaticConstant) so they are
// not assigned auto slots.
Frame *frame_build(const Tac_TopLevel *fn, const Tac_TopLevel *program);

// Look up a variable name. Returns true and fills *reg and *offset on hit.
bool frame_lookup(const Frame *f, const char *name, int *reg, int *offset);

// Number of auto (REG_AUTO) slots allocated — used to size the frame in the prologue.
int frame_num_autos(const Frame *f);

void frame_free(Frame *f);

#ifdef __cplusplus
}
#endif

#endif // BESM6_FRAME_H
