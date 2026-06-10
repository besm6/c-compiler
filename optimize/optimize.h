#pragma once
#include <stdbool.h>
#include "tac.h"

typedef struct {
    bool unreachable_elim;   // --no-unreachable disables
    bool copy_propagation;   // --no-copy-prop disables
    bool dead_store_elim;    // --no-dead-store disables
} OptFlags;

OptFlags opt_flags_default(void);

// Returns the optimized body in place (modifies the list).
// Caller owns the result; caller freed the original list.
Tac_Instruction *optimize_function(Tac_Instruction *body, OptFlags flags,
                                   const Tac_TopLevel *toplevel);
