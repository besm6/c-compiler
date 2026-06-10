#include "optimize.h"
#include "cfg.h"

Tac_Instruction *constant_fold(Tac_Instruction *body);
void eliminate_unreachable(OptCfg *cfg);
void propagate_copies(OptCfg *cfg, const Tac_TopLevel *toplevel);
void eliminate_dead_stores(OptCfg *cfg);

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
            eliminate_dead_stores(cfg);

        Tac_Instruction *new_body = cfg_flatten(cfg);
        cfg_free(cfg);

        if (!new_body || tac_compare_instruction(new_body, body))
            return new_body;

        tac_free_instruction(body);
        body = new_body;
    }
}
