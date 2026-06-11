// ============================================================================
// alias.c — alias pre-analysis shared by copy propagation and dead-store
// elimination.
//
// Both passes propagate facts about a variable's value, which is only sound
// while the variable cannot be changed by something we are not tracking. Two
// categories of variables can be, and so must be handled conservatively:
//
//   - Static-duration variables (TAC_TOPLEVEL_STATIC_VARIABLE names, including
//     local `static`s): any function call might read or write them.
//   - Address-taken variables: any variable whose address is taken with
//     GET_ADDRESS may be read or written through the resulting pointer.
//
// This module collects both name sets once per function so the passes can apply
// the conservative kill/liven rules. Temporaries (t.N) never appear in either
// set — their address is never taken — and so are propagated freely.
//
// See docs/TAC_Optimization.md §"Conservatism around aliased variables".
// ============================================================================

#include "alias.h"

// Populate static_names and address_taken (both freshly initialised here). The
// names are borrowed from the TAC nodes — the maps store the same pointers, so
// they stay valid only as long as the underlying instructions/top-levels do.
void collect_alias_sets(const OptCfg *cfg, const Tac_TopLevel *toplevel,
                        StringMap *static_names, StringMap *address_taken)
{
    // Static-duration variables: scan the program's top-level declarations.
    map_init(static_names);
    for (const Tac_TopLevel *t = toplevel; t; t = t->next) {
        if (t->kind == TAC_TOPLEVEL_STATIC_VARIABLE) {
            const char *n = t->u.static_variable.name;
            map_insert(static_names, n, (intptr_t)n, 0);
        }
    }

    // Address-taken variables: every Var that is the source of a GET_ADDRESS.
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
