#include "cfg.h"
#include "xalloc.h"
#include "string_map.h"

static bool is_terminal(Tac_InstructionKind k)
{
    return k == TAC_INSTRUCTION_JUMP ||
           k == TAC_INSTRUCTION_JUMP_IF_ZERO ||
           k == TAC_INSTRUCTION_JUMP_IF_NOT_ZERO ||
           k == TAC_INSTRUCTION_RETURN;
}

static int count_blocks(const Tac_Instruction *body)
{
    int n = 1;
    bool prev_terminal = false;
    for (const Tac_Instruction *i = body->next; i; i = i->next) {
        if (prev_terminal || i->kind == TAC_INSTRUCTION_LABEL)
            n++;
        prev_terminal = is_terminal(i->kind);
    }
    return n;
}

OptCfg *cfg_build(Tac_Instruction *body)
{
    int nblocks = count_blocks(body);

    OptCfg *cfg = xalloc(sizeof(OptCfg), __func__, __FILE__, __LINE__);
    cfg->nblocks = nblocks;
    cfg->blocks = xalloc(nblocks * sizeof(OptBlock *), __func__, __FILE__, __LINE__);

    for (int i = 0; i < nblocks; i++) {
        OptBlock *b = xalloc(sizeof(OptBlock), __func__, __FILE__, __LINE__);
        b->id = i;
        b->first = NULL;
        b->last = NULL;
        b->succs = NULL;
        b->nsucc = 0;
        b->reachable = false;
        cfg->blocks[i] = b;
    }

    // Pass 1: split instruction list into blocks, build label→block-id map.
    StringMap label_map;
    map_init(&label_map);

    int bid = 0;
    cfg->blocks[0]->first = body;
    if (body->kind == TAC_INSTRUCTION_LABEL)
        map_insert(&label_map, body->u.label.name, (intptr_t)0, 0);

    Tac_Instruction *prev = body;
    for (Tac_Instruction *instr = body->next; instr; instr = instr->next) {
        if (is_terminal(prev->kind) || instr->kind == TAC_INSTRUCTION_LABEL) {
            cfg->blocks[bid]->last = prev;
            prev->next = NULL;
            bid++;
            cfg->blocks[bid]->first = instr;
        }
        if (instr->kind == TAC_INSTRUCTION_LABEL)
            map_insert(&label_map, instr->u.label.name, (intptr_t)bid, 0);
        prev = instr;
    }
    cfg->blocks[bid]->last = prev;

    // Pass 2: wire successor pointers.
    for (int i = 0; i < nblocks; i++) {
        Tac_Instruction *term = cfg->blocks[i]->last;
        if (term->kind == TAC_INSTRUCTION_JUMP) {
            intptr_t target_id;
            map_get(&label_map, term->u.jump.target, &target_id);
            cfg->blocks[i]->succs = xalloc(sizeof(OptBlock *), __func__, __FILE__, __LINE__);
            cfg->blocks[i]->succs[0] = cfg->blocks[target_id];
            cfg->blocks[i]->nsucc = 1;
        } else if (term->kind == TAC_INSTRUCTION_JUMP_IF_ZERO ||
                   term->kind == TAC_INSTRUCTION_JUMP_IF_NOT_ZERO) {
            const char *target = (term->kind == TAC_INSTRUCTION_JUMP_IF_ZERO)
                ? term->u.jump_if_zero.target
                : term->u.jump_if_not_zero.target;
            intptr_t target_id;
            map_get(&label_map, target, &target_id);
            cfg->blocks[i]->succs = xalloc(2 * sizeof(OptBlock *), __func__, __FILE__, __LINE__);
            cfg->blocks[i]->succs[0] = cfg->blocks[target_id];
            cfg->blocks[i]->succs[1] = cfg->blocks[i + 1];
            cfg->blocks[i]->nsucc = 2;
        } else if (term->kind == TAC_INSTRUCTION_RETURN) {
            cfg->blocks[i]->nsucc = 0;
        } else if (i + 1 < nblocks) {
            cfg->blocks[i]->succs = xalloc(sizeof(OptBlock *), __func__, __FILE__, __LINE__);
            cfg->blocks[i]->succs[0] = cfg->blocks[i + 1];
            cfg->blocks[i]->nsucc = 1;
        }
    }

    map_destroy(&label_map);
    return cfg;
}

Tac_Instruction *cfg_flatten(OptCfg *cfg)
{
    Tac_Instruction *head = NULL;
    Tac_Instruction *tail_last = NULL;

    for (int i = 0; i < cfg->nblocks; i++) {
        if (!cfg->blocks[i]->first)
            continue;
        if (!head)
            head = cfg->blocks[i]->first;
        else
            tail_last->next = cfg->blocks[i]->first;
        tail_last = cfg->blocks[i]->last;
    }
    if (tail_last)
        tail_last->next = NULL;
    return head;
}

void cfg_free(OptCfg *cfg)
{
    for (int i = 0; i < cfg->nblocks; i++) {
        xfree(cfg->blocks[i]->succs);
        xfree(cfg->blocks[i]);
    }
    xfree(cfg->blocks);
    xfree(cfg);
}
