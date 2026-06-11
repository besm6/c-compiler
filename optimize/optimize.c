// ============================================================================
// optimize.c — the machine-independent TAC optimization pipeline.
//
// No single pass is sufficient on its own; the four passes form a virtuous
// cycle and amplify one another:
//
//   - Constant folding produces constants that copy propagation can substitute
//     into expressions, which constant folding can then evaluate again.
//   - Constant folding turns conditional jumps into unconditional ones, creating
//     unreachable blocks that unreachable-code elimination can remove.
//   - Copy propagation eliminates the variable in a copy's destination, turning
//     the copy into a dead store that dead-store elimination can remove.
//   - Dead-store elimination removes instructions, which may make previously
//     reachable blocks empty, which unreachable-code elimination can clean up.
//
// Because the passes feed each other, the optimizer runs them in a loop until
// the instruction list stops changing (a fixed point). Within one iteration the
// pass order is fixed: constant folding runs first — it is the only pass that
// works on the flat instruction list and needs no CFG — and the remaining three
// run on the CFG in the order unreachable → copy-prop → dead-store, so each can
// exploit what the previous one produced in the same iteration.
//
// See docs/TAC_Optimization.md §"The optimization pipeline".
// ============================================================================

#include "optimize.h"
#include "cfg.h"

// Pass entry points, implemented in the sibling translation units.
Tac_Instruction *constant_fold(Tac_Instruction *body);
void eliminate_unreachable(OptCfg *cfg);
void propagate_copies(OptCfg *cfg, const Tac_TopLevel *toplevel);
void eliminate_dead_stores(const OptCfg *cfg, const Tac_TopLevel *toplevel);

// Default flags: every CLI-toggleable pass is enabled. Constant folding has no
// flag — it always runs, to keep the downstream code generators simpler.
OptFlags opt_flags_default(void) {
    return (OptFlags){ .unreachable_elim = true,
                       .copy_propagation = true,
                       .dead_store_elim  = true };
}

// Run the pipeline on one function body to a fixed point and return the
// optimized list. The body is transformed in place across iterations; the
// caller owns the returned list. Each TAC_TOPLEVEL_FUNCTION is optimized
// independently (intraprocedural); `toplevel` is the program's top-level list,
// needed only so the CFG passes can identify static-duration variables.
Tac_Instruction *optimize_function(Tac_Instruction *body, OptFlags flags,
                                   const Tac_TopLevel *toplevel) {
    if (!body) return NULL;
    for (;;) {
        // Constant folding first, on the flat list (no CFG required).
        body = constant_fold(body);

        // Split into basic blocks for the three CFG-based passes.
        OptCfg *cfg = cfg_build(body);

        if (flags.unreachable_elim)
            eliminate_unreachable(cfg);
        if (flags.copy_propagation)
            propagate_copies(cfg, toplevel);
        if (flags.dead_store_elim)
            eliminate_dead_stores(cfg, toplevel);

        // A pass (e.g. dead_store_elim) may remove the entry block's first
        // instruction, freeing it and leaving `body` dangling. When that
        // happens the entry block's `first` no longer equals `body`. We must
        // detect this *before* cfg_flatten, because afterward we may neither
        // dereference `body` (use-after-free in tac_compare_instruction) nor
        // free it again (double free).
        bool body_freed = (cfg->blocks[0]->first != body);

        // Rejoin the (possibly modified) blocks into a flat list.
        Tac_Instruction *new_body = cfg_flatten(cfg);
        cfg_free(cfg);

        // An empty result is also a terminal condition: nothing left to iterate.
        if (!new_body) return new_body;
        // Fixed point: when this iteration produced a structurally identical
        // list, the loop has converged and we return. Otherwise free the now-
        // superseded old body and iterate again on the new one. (When the entry
        // instruction was freed by a pass, `body` is already gone, so we skip
        // both the comparison and the free.)
        if (!body_freed && tac_compare_instruction(new_body, body))
            return new_body;
        if (!body_freed)
            tac_free_instruction(body);
        body = new_body;
    }
}
