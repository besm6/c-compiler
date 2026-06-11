#include "alias.h"

void collect_alias_sets(const OptCfg *cfg, const Tac_TopLevel *toplevel,
                        StringMap *static_names, StringMap *address_taken)
{
    map_init(static_names);
    for (const Tac_TopLevel *t = toplevel; t; t = t->next) {
        if (t->kind == TAC_TOPLEVEL_STATIC_VARIABLE) {
            const char *n = t->u.static_variable.name;
            map_insert(static_names, n, (intptr_t)n, 0);
        }
    }

    map_init(address_taken);
    for (int i = 0; i < cfg->nblocks; i++) {
        const OptBlock *b = cfg->blocks[i];
        for (const Tac_Instruction *ins = b->first; ins; ins = ins->next) {
            if (ins->kind == TAC_INSTRUCTION_GET_ADDRESS) {
                const Tac_Val *src = ins->u.get_address.src;
                if (src->kind == TAC_VAL_VAR) {
                    const char *n = src->u.var_name;
                    map_insert(address_taken, n, (intptr_t)n, 0);
                }
            }
        }
    }
}
