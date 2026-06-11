#include "alias.h"
#include "cfg.h"
#include "string_map.h"
#include "tac.h"

void eliminate_dead_stores(const OptCfg *cfg, const Tac_TopLevel *toplevel)
{
    if (cfg->nblocks == 0) return;

    // Task 20: alias pre-analysis
    StringMap static_names, address_taken;
    collect_alias_sets(cfg, toplevel, &static_names, &address_taken);

    map_destroy(&static_names);
    map_destroy(&address_taken);
}
