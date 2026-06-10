#include "cfg.h"
#include "xalloc.h"

OptCfg *cfg_build(Tac_Instruction *body) {
    OptBlock *block = xalloc(sizeof(OptBlock), __func__, __FILE__, __LINE__);
    block->id = 0;
    block->first = body;
    block->last = NULL;
    block->succs = NULL;
    block->nsucc = 0;
    block->reachable = false;

    OptBlock **blocks = xalloc(sizeof(OptBlock *), __func__, __FILE__, __LINE__);
    blocks[0] = block;

    OptCfg *cfg = xalloc(sizeof(OptCfg), __func__, __FILE__, __LINE__);
    cfg->blocks = blocks;
    cfg->nblocks = 1;
    return cfg;
}

Tac_Instruction *cfg_flatten(OptCfg *cfg) {
    return cfg->blocks[0]->first;
}

void cfg_free(OptCfg *cfg) {
    for (int i = 0; i < cfg->nblocks; i++) {
        xfree(cfg->blocks[i]->succs);
        xfree(cfg->blocks[i]);
    }
    xfree(cfg->blocks);
    xfree(cfg);
}
