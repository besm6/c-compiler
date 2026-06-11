#include "optimize.h"
#include "cfg.h"

Tac_Instruction *constant_fold(Tac_Instruction *body);
void eliminate_unreachable(OptCfg *cfg);
void propagate_copies(OptCfg *cfg, const Tac_TopLevel *toplevel);
void eliminate_dead_stores(const OptCfg *cfg, const Tac_TopLevel *toplevel);

OptFlags opt_flags_default(void) {
    return (OptFlags){ .unreachable_elim = true,
                       .copy_propagation = true,
                       .dead_store_elim  = true };
}

Tac_Instruction *optimize_function(Tac_Instruction *body, OptFlags flags,
                                   const Tac_TopLevel *toplevel) {
    if (!body) return NULL;
    for (;;) {
        body = constant_fold(body);

        OptCfg *cfg = cfg_build(body);

        if (flags.unreachable_elim)
            eliminate_unreachable(cfg);
        if (flags.copy_propagation)
            propagate_copies(cfg, toplevel);
        if (flags.dead_store_elim)
            eliminate_dead_stores(cfg, toplevel);

        // A pass (e.g. dead_store_elim) may remove the entry block's first
        // instruction, freeing it and making `body` a dangling pointer.
        // Check before cfg_flatten so we can safely skip the comparison.
        bool body_freed = (cfg->blocks[0]->first != body);

        Tac_Instruction *new_body = cfg_flatten(cfg);
        cfg_free(cfg);

        if (!new_body) return new_body;
        if (!body_freed && tac_compare_instruction(new_body, body))
            return new_body;
        if (!body_freed)
            tac_free_instruction(body);
        body = new_body;
    }
}
