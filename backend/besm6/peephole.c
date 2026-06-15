#include <stdbool.h>
#include <stddef.h>

#include "besm.h"

//
// Peephole optimization on the BESM-6 backend instruction stream.
//
// The pass works on the `Besm_Instr` linked list a function's single `Besm_Block`
// holds, *after* instruction selection and *before* Madlen emission.  It is the
// backend's final polish: it removes the store/reload, mode-register, and
// compare/branch residue that one-TAC-node-at-a-time selection necessarily leaves
// behind.  See docs/Peephole_Rewrites.md for the theory and the worked before/after
// sequences, and backend/besm6/TODO.md (Phase M) for the rule catalogue.
//
// Currently implemented: the framework itself (this file) plus rule #27, redundant
// reload elimination.  Rules #28–#32 append entries to the rule table below.
//

//
// Tracked implicit machine state, valid only along straight-line code.
//
// A value in A is described by the frame slot it mirrors: register (r6/r7) plus
// offset.  Most rewrites are licensed by knowing "A currently holds slot (reg,off)".
// The mode register R and the logical flag ω are also machine state a peephole
// rule may track; the mode-coalescing (#29) and compare→branch (#30) rules will add
// fields here when they land.
//
typedef struct {
    bool a_known; // true: A mirrors the frame slot below
    unsigned a_reg;
    int a_off;
} PeepState;

// Reset all tracked state — used at every basic-block boundary.
static void state_reset(PeepState *st)
{
    st->a_known = false;
}

//
// Does this instruction end a basic block?  A label can be re-entered from
// elsewhere and a branch transfers control (a CALL also clobbers A), so tracked
// state cannot be assumed to survive past it.  The non-dataflow assembler
// directives (NAME/SUBP/BASE/ENTRY/END and the data pseudo-ops) likewise carry no
// straight-line machine state, so they reset too.
//
static bool is_block_boundary(const Besm_Instr *i)
{
    switch (i->kind) {
    case BESM_BRANCH_UZA:
    case BESM_BRANCH_U1A:
    case BESM_BRANCH_UJ:
    case BESM_BRANCH_VJM:
    case BESM_BRANCH_VZM:
    case BESM_BRANCH_V1M:
    case BESM_BRANCH_VLM:
    case BESM_BRANCH_CALL:
    case BESM_BRANCH_STOP:
    case BESM_STMT_LABEL:
    case BESM_STMT_NAME:
    case BESM_STMT_BASE:
    case BESM_STMT_SUBP:
    case BESM_STMT_ENTRY:
    case BESM_STMT_END:
        return true;
    default:
        return false;
    }
}

//
// Update the tracked state to reflect executing `i` in straight-line code.  This
// runs only for non-boundary instructions (boundaries reset the state instead).
//
// XTA and ATX of a frame slot (no symbolic name) both leave A mirroring that slot.
// Every other instruction may clobber A, so conservatively mark A unknown; later
// rules refine this.
//
static void state_step(PeepState *st, const Besm_Instr *i)
{
    switch (i->kind) {
    case BESM_MEM_XTA:
    case BESM_MEM_ATX:
        if (i->name == NULL) {
            st->a_known = true;
            st->a_reg   = i->reg;
            st->a_off   = i->addr;
            return;
        }
        st->a_known = false;
        return;
    default:
        st->a_known = false;
        return;
    }
}

//
// Rule #27 — redundant reload elimination.
//
// `cur` is an `xta reg,off` reload of a frame slot whose value the tracked state
// says A already holds (the immediately preceding `atx` to that slot stored it and
// did not disturb A).  The reload is pure waste; report a match so the caller
// splices it out.  Section 5.1 of docs/Peephole_Rewrites.md.
//
static bool rule_redundant_reload(const Besm_Instr *cur, const PeepState *st)
{
    return cur->kind == BESM_MEM_XTA && cur->name == NULL && st->a_known &&
           cur->reg == st->a_reg && cur->addr == st->a_off;
}

//
// Rule table.  Each rule inspects the cursor instruction and the tracked state and
// returns true when the cursor should be deleted.  Rules #28–#32 append here.
//
typedef bool (*PeepRule)(const Besm_Instr *cur, const PeepState *st);

static const PeepRule rule_table[] = {
    rule_redundant_reload,
};
#define NUM_RULES (sizeof(rule_table) / sizeof(rule_table[0]))

// Splice a single node out of a block's list and free it.  `besm_free_instr`
// recurses on ->next, so unlink first (set cur->next = NULL) to free only `cur`.
static void delete_instr(Besm_Block *block, Besm_Instr *prev, Besm_Instr *cur)
{
    if (prev)
        prev->next = cur->next;
    else
        block->body = cur->next;
    cur->next = NULL;
    besm_free_instr(cur);
}

// One forward sweep over a block.  Returns true if any node was deleted.
static bool peephole_sweep(Besm_Block *block)
{
    PeepState st;
    state_reset(&st);

    bool changed       = false;
    Besm_Instr *prev   = NULL;
    Besm_Instr *cur    = block->body;

    while (cur) {
        bool deleted = false;
        for (size_t r = 0; r < NUM_RULES; r++) {
            if (rule_table[r](cur, &st)) {
                Besm_Instr *next = cur->next;
                delete_instr(block, prev, cur);
                cur     = next;
                changed = true;
                deleted = true;
                break;
            }
        }
        if (deleted)
            continue; // prev and tracked state stay valid; re-test the new cur

        if (is_block_boundary(cur))
            state_reset(&st);
        else
            state_step(&st, cur);

        prev = cur;
        cur  = cur->next;
    }
    return changed;
}

void besm_peephole(Besm_Func *func)
{
    if (!func)
        return;
    for (Besm_Func *fn = func; fn; fn = fn->next) {
        for (Besm_Block *block = fn->blocks; block; block = block->next) {
            // Iterate to a fixpoint: one rewrite can expose another.
            while (peephole_sweep(block))
                ;
        }
    }
}
