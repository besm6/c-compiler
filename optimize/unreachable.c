#include "cfg.h"
#include "xalloc.h"

void eliminate_unreachable(OptCfg *cfg)
{
    OptBlock **queue = xalloc(cfg->nblocks * sizeof(OptBlock *),
                              __func__, __FILE__, __LINE__);
    int head = 0, tail = 0;

    cfg->blocks[0]->reachable = true;
    queue[tail++] = cfg->blocks[0];

    while (head < tail) {
        OptBlock *b = queue[head++];
        for (int i = 0; i < b->nsucc; i++) {
            OptBlock *succ = b->succs[i];
            if (!succ->reachable) {
                succ->reachable = true;
                queue[tail++] = succ;
            }
        }
    }
    xfree(queue);

    for (int i = 0; i < cfg->nblocks; i++) {
        OptBlock *b = cfg->blocks[i];
        if (!b->reachable) {
            tac_free_instruction(b->first);
            b->first = NULL;
            b->last  = NULL;
        }
    }
}
