#pragma once
#include <stdbool.h>
#include "tac.h"

typedef struct OptBlock {
    int id;
    Tac_Instruction *first;
    Tac_Instruction *last;
    struct OptBlock **succs;
    int nsucc;
    bool reachable;
} OptBlock;

typedef struct OptCfg {
    OptBlock **blocks;
    int nblocks;
} OptCfg;

OptCfg *cfg_build(Tac_Instruction *body);
Tac_Instruction *cfg_flatten(OptCfg *cfg);
void cfg_free(OptCfg *cfg);
