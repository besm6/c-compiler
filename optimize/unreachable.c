#include "cfg.h"
#include "string_map.h"
#include "xalloc.h"
#include <string.h>

static void remove_useless_jumps(OptCfg *cfg)
{
    for (int i = 0; i < cfg->nblocks; i++) {
        OptBlock *b = cfg->blocks[i];
        if (!b->reachable || !b->last)
            continue;
        if (b->last->kind != TAC_INSTRUCTION_JUMP)
            continue;
        const char *target = b->last->u.jump.target;

        int next = i + 1;
        while (next < cfg->nblocks && !cfg->blocks[next]->reachable)
            next++;
        if (next >= cfg->nblocks || !cfg->blocks[next]->first)
            continue;
        const OptBlock *nb = cfg->blocks[next];
        if (nb->first->kind != TAC_INSTRUCTION_LABEL)
            continue;
        if (strcmp(nb->first->u.label.name, target) != 0)
            continue;

        Tac_Instruction *jmp = b->last;
        if (b->first == b->last) {
            b->first = b->last = NULL;
        } else {
            Tac_Instruction *prev = b->first;
            while (prev->next != jmp)
                prev = prev->next;
            prev->next = NULL;
            b->last = prev;
        }
        jmp->next = NULL;
        tac_free_instruction(jmp);
    }
}

static void remove_unused_labels(OptCfg *cfg)
{
    StringMap targets;
    map_init(&targets);
    for (int i = 0; i < cfg->nblocks; i++) {
        OptBlock *b = cfg->blocks[i];
        if (!b->reachable)
            continue;
        for (Tac_Instruction *ins = b->first; ins; ins = ins->next) {
            const char *t = NULL;
            if (ins->kind == TAC_INSTRUCTION_JUMP)
                t = ins->u.jump.target;
            else if (ins->kind == TAC_INSTRUCTION_JUMP_IF_ZERO)
                t = ins->u.jump_if_zero.target;
            else if (ins->kind == TAC_INSTRUCTION_JUMP_IF_NOT_ZERO)
                t = ins->u.jump_if_not_zero.target;
            if (t) {
                intptr_t dummy;
                if (!map_get(&targets, t, &dummy))
                    map_insert(&targets, t, 1, 0);
            }
        }
    }

    const Tac_Instruction *entry = cfg->blocks[0]->first;

    for (int i = 0; i < cfg->nblocks; i++) {
        OptBlock *b = cfg->blocks[i];
        if (!b->reachable || !b->first)
            continue;
        if (b->first->kind != TAC_INSTRUCTION_LABEL)
            continue;
        Tac_Instruction *lbl = b->first;
        if (lbl == entry)
            continue;
        intptr_t dummy;
        if (map_get(&targets, lbl->u.label.name, &dummy))
            continue;

        Tac_Instruction *new_first = lbl->next;
        lbl->next = NULL;
        tac_free_instruction(lbl);
        b->first = new_first;
        if (new_first == NULL)
            b->last = NULL;
    }

    map_destroy(&targets);
}

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

    remove_useless_jumps(cfg);
    remove_unused_labels(cfg);
}
