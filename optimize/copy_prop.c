#include "cfg.h"
#include "string_map.h"
#include "tac.h"

void propagate_copies(OptCfg *cfg, const Tac_TopLevel *toplevel)
{
    StringMap static_names;
    map_init(&static_names);
    for (const Tac_TopLevel *t = toplevel; t; t = t->next) {
        if (t->kind == TAC_TOPLEVEL_STATIC_VARIABLE)
            map_insert(&static_names, t->u.static_variable.name, 1, 0);
    }

    StringMap address_taken;
    map_init(&address_taken);
    for (int i = 0; i < cfg->nblocks; i++) {
        OptBlock *b = cfg->blocks[i];
        for (Tac_Instruction *ins = b->first; ins; ins = ins->next) {
            if (ins->kind == TAC_INSTRUCTION_GET_ADDRESS) {
                const Tac_Val *src = ins->u.get_address.src;
                if (src->kind == TAC_VAL_VAR)
                    map_insert(&address_taken, src->u.var_name, 1, 0);
            }
        }
    }

    map_destroy(&static_names);
    map_destroy(&address_taken);
}
