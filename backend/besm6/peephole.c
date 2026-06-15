#include <stdbool.h>
#include <stddef.h>

#include "abi.h"
#include "besm.h"
#include "frame.h"
#include "xalloc.h"

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
// Currently implemented: the framework itself (this file), rule #27 (redundant reload
// elimination) and rule #28 (dead temp-store elimination).  Rules #29–#32 append entries
// to the rule table below.
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
// returns true when the cursor should be deleted.  Rules #29–#32 append here.
//
// Rule #28 (dead temp-store elimination) is *not* a table entry: deciding whether an
// `atx` is dead needs forward look-ahead within the block plus the frame's temp-slot
// classification, neither of which the `(cur, st)` predicate signature carries.  It is
// handled by `dead_temp_store` directly in the sweep below.
//
typedef bool (*PeepRule)(const Besm_Instr *cur, const PeepState *st);

static const PeepRule rule_table[] = {
    rule_redundant_reload,
};
#define NUM_RULES (sizeof(rule_table) / sizeof(rule_table[0]))

//
// Rule #28 — dead temp-store elimination.  See docs/Peephole_Rewrites.md §5.2.
//
// An `atx reg,off` that stores a compiler temporary's frame slot is dead when nothing
// reads that slot before it is overwritten or the basic block ends.  Restricting to
// '%'-temporaries is essential: temporaries are never address-taken, so every read of
// the slot appears as a direct `(reg,off)` operand a forward scan can see, whereas a
// named local could be read through a pointer (an aliased access invisible to the scan).
//

// Does `i` reference auto slot `off` (r7 + off) as a direct memory *read* operand?  The
// `reg`/`addr` fields are overloaded (index-register ops put an index register in `reg`
// or an immediate in `addr`), so only the kinds that take a frame-slot memory operand
// and load from it count.
static bool instr_reads_auto_slot(const Besm_Instr *i, int off)
{
    if (i->name != NULL || (int)i->reg != REG_AUTO || i->addr != off)
        return false;
    switch (i->kind) {
    case BESM_MEM_XTA:
    case BESM_MEM_XTS:
    case BESM_ARITH_ADD:
    case BESM_ARITH_SUB:
    case BESM_ARITH_RSUB:
    case BESM_ARITH_ABSSUB:
    case BESM_ARITH_MUL:
    case BESM_ARITH_DIV:
    case BESM_ARITH_CNEG:
    case BESM_LOG_AAX:
    case BESM_LOG_AOX:
    case BESM_LOG_AEX:
    case BESM_LOG_ARX:
    case BESM_LOG_APX:
    case BESM_LOG_AUX:
    case BESM_LOG_ACX:
    case BESM_LOG_ANX:
    case BESM_EXP_EADDX:
    case BESM_EXP_ESUBX:
    case BESM_EXP_SHIFTX:
    case BESM_EXP_SETRMEM:
        return true;
    default:
        return false;
    }
}

// Does `i` *write* auto slot `off` (r7 + off)?  `atx` stores A; `stx` stores A and pops.
static bool instr_writes_auto_slot(const Besm_Instr *i, int off)
{
    if (i->name != NULL || (int)i->reg != REG_AUTO || i->addr != off)
        return false;
    return i->kind == BESM_MEM_ATX || i->kind == BESM_MEM_STX;
}

//
// `cur` is an `atx` to a temporary's auto slot.  Scan forward within the current basic
// block: if the slot is read before being overwritten, the store is live; if overwritten
// first, the store is dead; if neither happens before the block boundary, the store is
// dead only when the temporary lives in a single basic block (so it cannot be live-out).
//
static bool dead_temp_store(const Besm_Instr *cur, const Frame *frame, const bool *multiblock)
{
    if (cur->kind != BESM_MEM_ATX || cur->name != NULL || (int)cur->reg != REG_AUTO)
        return false;
    if (!frame || !frame_slot_is_temp(frame, REG_AUTO, cur->addr))
        return false;

    int off = cur->addr;
    for (const Besm_Instr *j = cur->next; j && !is_block_boundary(j); j = j->next) {
        if (instr_reads_auto_slot(j, off))
            return false; // value is used: store is live
        if (instr_writes_auto_slot(j, off))
            return true; // overwritten before any read: store is dead
    }
    // Block ends with no further read.  Dead iff the temporary is confined to one block.
    return multiblock == NULL || !multiblock[off];
}

// If `i` directly reads or writes an auto slot, report its offset.  Used to attribute
// each slot reference to the basic block it occurs in (multi-block analysis).
static bool instr_auto_slot_ref(const Besm_Instr *i, int *off)
{
    if (i->name != NULL || (int)i->reg != REG_AUTO)
        return false;
    if (instr_reads_auto_slot(i, i->addr) || instr_writes_auto_slot(i, i->addr)) {
        *off = i->addr;
        return true;
    }
    return false;
}

//
// Compute, for each auto slot, whether it is referenced in more than one basic block.
// A temporary confined to a single block is never live across an edge, which licenses
// the block-end case of rule #28.  Returns a freshly allocated array of size `num_autos`
// (NULL when num_autos == 0); the caller frees it.
//
static bool *compute_multiblock(const Besm_Block *block, int num_autos)
{
    if (num_autos <= 0)
        return NULL;
    bool *multiblock = (bool *)xalloc(num_autos * sizeof(bool), __func__, __FILE__, __LINE__);
    int *firstblk    = (int *)xalloc(num_autos * sizeof(int), __func__, __FILE__, __LINE__);
    for (int i = 0; i < num_autos; i++) {
        multiblock[i] = false;
        firstblk[i]   = -1;
    }

    int blockidx = 0;
    for (const Besm_Instr *i = block->body; i; i = i->next) {
        int off;
        if (instr_auto_slot_ref(i, &off) && off >= 0 && off < num_autos) {
            if (firstblk[off] < 0)
                firstblk[off] = blockidx;
            else if (firstblk[off] != blockidx)
                multiblock[off] = true;
        }
        if (is_block_boundary(i))
            blockidx++;
    }

    xfree(firstblk);
    return multiblock;
}

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
static bool peephole_sweep(Besm_Block *block, const Frame *frame, const bool *multiblock)
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
        // Rule #28: dead temp-store elimination (needs look-ahead + the frame).
        if (!deleted && dead_temp_store(cur, frame, multiblock)) {
            Besm_Instr *next = cur->next;
            delete_instr(block, prev, cur);
            cur     = next;
            changed = true;
            deleted = true;
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

void besm_peephole(Besm_Func *func, const Frame *frame)
{
    if (!func)
        return;
    int num_autos = frame ? frame_num_autos(frame) : 0;
    for (Besm_Func *fn = func; fn; fn = fn->next) {
        for (Besm_Block *block = fn->blocks; block; block = block->next) {
            // The multi-block classification is stable across the rewrites this pass
            // performs (neither #27 nor #28 ever drops a temporary's only reference in a
            // block), so compute it once before the fixpoint loop.
            bool *multiblock = compute_multiblock(block, num_autos);
            // Iterate to a fixpoint: one rewrite can expose another.
            while (peephole_sweep(block, frame, multiblock))
                ;
            if (multiblock)
                xfree(multiblock);
        }
    }
}
