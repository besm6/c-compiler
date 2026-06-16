#include <stdbool.h>
#include <stddef.h>
#include <string.h>

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
// elimination), rule #28 (dead temp-store elimination) and rule #29 (NTR mode
// coalescing).  Rules #30–#32 append entries to the rule table below.
//

//
// Tracked implicit machine state, valid only along straight-line code.
//
// A value in A is described by the frame slot it mirrors: register (r6/r7) plus
// offset.  Most rewrites are licensed by knowing "A currently holds slot (reg,off)".
// The mode register R is also tracked (for NTR mode coalescing, rule #29); the
// logical flag ω is machine state the compare→branch rule (#30) will add when it lands.
//
typedef struct {
    bool a_known; // true: A mirrors the frame slot below
    unsigned a_reg;
    int a_off;
    bool r_known; // true: r_val is the current mode register R
    int r_val;
} PeepState;

// Reset all tracked state — used at every basic-block boundary.
static void state_reset(PeepState *st)
{
    st->a_known = false;
    st->r_known = false;
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
    // Mode register R: `ntr` (SETR) sets it to its operand.  Nothing else changes R
    // in straight-line code — a CALL is a block boundary, handled by the sweep.
    if (i->kind == BESM_EXP_SETR) {
        st->r_known = true;
        st->r_val   = i->addr;
    }
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
// Rule #29(a) — redundant NTR elimination.  See docs/Peephole_Rewrites.md §5.3.
//
// `cur` is an `ntr n` (SETR) that re-establishes a mode-register value R already
// holds (the tracked state says R == n), so it has no effect; report a match so the
// caller deletes it.  This drops the leading `ntr 0` of an FP op when R is already 0,
// and a trailing `ntr 7` when R is already 7 (e.g. straight after `b/save`).
//
static bool rule_redundant_ntr(const Besm_Instr *cur, const PeepState *st)
{
    return cur->kind == BESM_EXP_SETR && st->r_known && cur->addr == st->r_val;
}

//
// Rule table.  Each rule inspects the cursor instruction and the tracked state and
// returns true when the cursor should be deleted.  Rules #30–#32 append here.
//
// Rule #28 (dead temp-store elimination) is *not* a table entry: deciding whether an
// `atx` is dead needs forward look-ahead within the block plus the frame's temp-slot
// classification, neither of which the `(cur, st)` predicate signature carries.  It is
// handled by `dead_temp_store` directly in the sweep below.
//
typedef bool (*PeepRule)(const Besm_Instr *cur, const PeepState *st);

static const PeepRule rule_table[] = {
    rule_redundant_reload,
    rule_redundant_ntr,
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

//
// Rule #29(b) — dead NTR elimination.  See docs/Peephole_Rewrites.md §5.3.
//
// Two FP ops in a row emit `ntr 7` (restore) immediately chased by `ntr 0` (re-enter
// FP mode), with only R-independent moves between them.  The first `ntr` is dead: its
// R value is overwritten before anything reads it.  Together with rule #29(a) (which
// then drops the redundant re-set), R stays 0 across the run and is restored once at
// the end.
//

// Is `i`'s result independent of the mode register R?  These are the data-movement and
// index-register ops whose behavior does not depend on R, so they may sit between a
// dead `ntr` and the `ntr` that overwrites it.  Everything else — additive/
// multiplicative arithmetic (A±X / A*X / A/X), exponent and shift ops, etc. — may
// depend on R and therefore ends the scan, keeping the `ntr` live.
static bool instr_is_r_independent(const Besm_Instr *i)
{
    switch (i->kind) {
    case BESM_MEM_XTA:
    case BESM_MEM_ATX:
    case BESM_MEM_STX:
    case BESM_MEM_XTS:
    case BESM_MEM_ITA:
    case BESM_MEM_ATI:
    case BESM_MEM_ITS:
    case BESM_MEM_STI:
    case BESM_MEM_MTJ:
    case BESM_REG_VTM:
    case BESM_REG_UTM:
    case BESM_REG_JADDM:
    case BESM_MOD_UTC:
    case BESM_MOD_WTC:
        return true;
    default:
        return false;
    }
}

//
// `cur` is an `ntr` (SETR).  Scan forward within the basic block: if a later `ntr`
// overwrites R before any R-dependent instruction reads it, `cur` is dead.  If an
// R-dependent instruction or the block boundary comes first, `cur` is live (the
// boundary case is the "restore R once at the end" that must survive).
//
static bool dead_ntr_set(const Besm_Instr *cur)
{
    if (cur->kind != BESM_EXP_SETR)
        return false;
    for (const Besm_Instr *j = cur->next; j && !is_block_boundary(j); j = j->next) {
        if (j->kind == BESM_EXP_SETR)
            return true; // R overwritten before any use: store is dead
        if (!instr_is_r_independent(j))
            return false; // R is used: store is live
    }
    return false; // block ends with no overwrite: keep (restore-at-end)
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
        // Rule #29(b): dead NTR elimination (needs forward look-ahead).
        if (!deleted && (dead_temp_store(cur, frame, multiblock) || dead_ntr_set(cur))) {
            Besm_Instr *next = cur->next;
            delete_instr(block, prev, cur);
            cur     = next;
            changed = true;
            deleted = true;
        }
        if (deleted)
            continue; // prev and tracked state stay valid; re-test the new cur

        if (is_block_boundary(cur)) {
            state_reset(&st);
            // `b/save`/`b/save0` leave R = 7; seed it so a redundant `ntr 7` just after
            // the prologue (or anywhere R is already 7) is recognised.  Every other CALL
            // may change R (the arithmetic helpers borrow the FP unit), so R stays
            // unknown there.
            if (cur->kind == BESM_BRANCH_CALL && cur->name != NULL &&
                (strcmp(cur->name, "b/save") == 0 || strcmp(cur->name, "b/save0") == 0)) {
                st.r_known = true;
                st.r_val   = 7;
            }
        } else {
            state_step(&st, cur);
        }

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
