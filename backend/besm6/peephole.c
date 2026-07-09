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
// elimination), rule #28 (dead temp-store elimination), rule #29 (NTR mode coalescing)
// and rule #31 (branch / label cleanup).  Rule #30 (compare → branch fusion) needs no
// dedicated code: it is the emergent product of #27 (which drops the boolean reload) and
// #28 (which drops the now-dead boolean store), made correct by the runtime helpers'
// logical-ω exit contract (see docs/Besm6_Runtime_Library.md, "ω mode and the AU mode
// register R").  Rule #31's three control-flow rewrites — jump-to-next-label, unreachable
// tail (which also collapses the duplicate `uj b/ret`), and conditional-over-jump
// inversion — are not `rule_table` entries either: two need list look-ahead and one
// mutates the list, neither of which the `(cur, st)` predicate signature carries, so they
// are handled directly in the sweep.  Rule #32 would append entries to the rule table.
//
// Cutting across all of them is the *C group* (see `is_c_setter` below): a UTC or WTC and
// the instruction that follows it are one indivisible unit, because the C address-modifier
// register survives exactly one instruction.  The sweep therefore analyses and rewrites a
// group as a whole — a consumer's `(reg,addr)` fields do not name a frame slot, and deleting
// one while leaving its setter would re-bind C to whatever fell into the gap.  Rule #27 in
// consequence matches on a `Loc`, the location a group or a plain `xta`/`atx` addresses,
// rather than on a raw `(reg,off)` pair.  See docs/Peephole_Rewrites.md §5.9.
//

//
// The memory location a value lives in.
//
// Instruction selection reaches a location three ways, and the peephole must be able to
// tell them apart before it can say "A already holds this".  A plain `xta/atx reg,off`
// names a frame slot.  A `utc g` + bare `xta/atx` names a module-level global.  Anything
// else — a dereference, an address computation, a nonzero literal — is LOC_NONE: a real
// value, but at an address this pass cannot name, so it licenses no rewrite.
//
// A *zero* literal is the one constant that does name a location.  Instruction selection
// gives it no operand (see emit.c's attach_const), so it addresses mem[M[0] + 0] = mem[0],
// which the machine guarantees reads as zero.  It therefore classifies as LOC_FRAME(0, 0),
// a slot no `frame_lookup` can ever hand out — those carry r6 (REG_PAR) or r7 (REG_AUTO) —
// and one nothing ever stores to.  So `a_loc == LOC_FRAME(0,0)` holds exactly when A == 0,
// and rule #27 collapsing a second zero load onto the first is sound.
//
typedef enum {
    LOC_NONE,   // unnameable: no rewrite may match it
    LOC_FRAME,  // mem[M[reg] + off]           — a frame slot
    LOC_GLOBAL, // mem[&name + off]            — a module-level global
    LOC_DEREF,  // mem[mem[ptr]]               — through the pointer named below
} LocKind;

typedef struct {
    LocKind kind;
    int reg;          // LOC_FRAME: the index register (r6/r7);  LOC_DEREF: ditto, of the ptr
    int off;          // LOC_FRAME: slot number;  LOC_GLOBAL: word offset from `name`;
                      // LOC_DEREF: the pointer's slot number
    const char *name; // LOC_GLOBAL: the global's symbol;  LOC_DEREF: the global *pointer*'s
                      // symbol, or NULL when the pointer is the frame slot (reg, off).
                      // Borrowed from the group's UTC ->name; see the lifetime note below.
} Loc;

static Loc loc_none(void)
{
    Loc l = { LOC_NONE, 0, 0, NULL };
    return l;
}

// Do two locations denote the same word?  LOC_NONE never matches, not even itself.
//
// Two LOC_DEREFs through the same pointer denote the same word only if the pointer still
// holds the same value.  Nothing here checks that, and nothing needs to: the pointer lives
// in a frame slot or a global, so any write to it is an `atx`/`stx` that settles `a_loc`
// on the pointer itself (or on LOC_NONE), discarding the LOC_DEREF before it can be matched.
// See the state-invariant note on PeepState.
static bool loc_eq(Loc a, Loc b)
{
    if (a.kind != b.kind)
        return false;
    switch (a.kind) {
    case LOC_FRAME:
        return a.reg == b.reg && a.off == b.off;
    case LOC_GLOBAL:
        return a.off == b.off && strcmp(a.name, b.name) == 0;
    case LOC_DEREF:
        if ((a.name == NULL) != (b.name == NULL))
            return false;
        if (a.name != NULL)
            return strcmp(a.name, b.name) == 0;
        return a.reg == b.reg && a.off == b.off;
    default:
        return false;
    }
}

//
// Tracked implicit machine state, valid only along straight-line code.
//
// A value in A is described by the location it mirrors.  Most rewrites are licensed by
// knowing "A currently holds location L".  The mode register R is also tracked (for NTR
// mode coalescing, rule #29).  The logical flag ω is not tracked: compare → branch fusion
// (#30) falls out of #27 + #28 and relies on the helpers' logical-ω exit contract rather
// than on ω state carried by this pass.
//
// `a_loc` needs no memory-clobber analysis, which is worth spelling out because it looks
// like it should.  On this machine memory is only ever written *from A* (`atx`, `stx`), so a
// store can never falsify "A mirrors L": either it writes somewhere other than L, or it
// writes L with the value A already holds.  A's mirror can therefore only go stale when A
// itself changes, or when the frame base M[6]/M[7] moves — and `state_step` settles `a_loc`
// at every instruction that does either (conservatively, to LOC_NONE unless it is an
// `xta`/`atx` naming a location).  Only UTC and WTC leave `a_loc` untouched, and they touch
// neither A nor memory.
//
// A LOC_DEREF additionally depends on the pointer's value.  That falls out of the same
// invariant: the pointer is itself in memory, so it can only be written from A, by an
// `atx`/`stx` that settles `a_loc` on the pointer's own location or on LOC_NONE — either
// way discarding the LOC_DEREF.  A CALL, which may write anything, is a block boundary.
//
// `Loc.name` borrows the `->name` of the group's UTC.  Deletion always removes a whole group
// at the cursor, and `a_loc` is only ever read to match a *later* group, so the instruction
// a tracked name points into always outlives the state that names it.
//
typedef struct {
    Loc a_loc;    // the location A currently mirrors (LOC_NONE: unknown)
    bool r_known; // true: r_val is the current mode register R
    int r_val;
    bool in_unreachable; // true: we are past an unconditional transfer (uj/stop),
                         // before the next label or structural directive (rule #31)
} PeepState;

// Reset all tracked state — used at every basic-block boundary.
static void state_reset(PeepState *st)
{
    st->a_loc          = loc_none();
    st->r_known        = false;
    st->in_unreachable = false;
}

// True when `i` carries a symbolic or constant operand (a name — global/label/literal —
// or a structural constant), as opposed to a plain frame-slot memory operand `(reg,off)`.
// The state machine must not mistake a constant or global load for a frame slot, so every
// operand-classification test below uses this rather than a bare `name == NULL` check.
static bool has_operand_symbol(const Besm_Instr *i)
{
    return i->name != NULL || i->konst != NULL;
}

//
// C groups.
//
// The C address-modifier register is reset to zero after *every* instruction except UTC
// (022) and WTC (023).  A C-setter and the single instruction that follows it therefore
// form an atomic pair: the follower's effective address is `addr + M[reg] + C`, and nothing
// may be inserted between them or deleted from between them.  The backend emits three
// shapes (see emit.c): `utc name` + consumer, `wtc reg,off` + consumer, and — for a
// dereference through a global pointer — `utc name` + `wtc 0,0` + consumer, where the middle
// WTC is at once the first setter's consumer and the second setter.
//
// Two consequences for this pass.  A bare `xta`/`atx` (reg 0, addr 0) after a setter is *not*
// a frame-slot access: it reads or writes mem[C], an address the tracked state has no name
// for.  And no rewrite may delete a consumer while leaving its setter, which would silently
// re-bind C to whatever instruction fell into the gap.
//
static bool is_c_setter(const Besm_Instr *i)
{
    return i->kind == BESM_MOD_UTC || i->kind == BESM_MOD_WTC;
}

// Walk the setter chain starting at `first` and return the single instruction that consumes
// C, or NULL if the block ends on a setter.  `*count` gets the number of nodes in the whole
// group (setters plus consumer), which is what a deletion must splice out.
static Besm_Instr *c_group_consumer(Besm_Instr *first, int *count)
{
    int n            = 0;
    Besm_Instr *i    = first;
    while (i != NULL && is_c_setter(i)) {
        n++;
        i = i->next;
    }
    *count = (i != NULL) ? n + 1 : n;
    return i;
}

// The memory location a C group addresses.  Three shapes name one (all emitted by emit.c):
//
//   `utc name` + `xta/atx 0,woff`      LOC_GLOBAL(name, addr + woff)   emit_xta_val etc.
//   `wtc reg,off` + `xta/atx`          LOC_DEREF via frame pointer     emit_wtc_ptr, local
//   `utc name` + `wtc` + `xta/atx`     LOC_DEREF via global pointer    emit_wtc_ptr, global
//
// Everything else is LOC_NONE:
//
//   `utc reg,off`      — address arithmetic (emit_member_fatptr, GET_ADDRESS_DECAY, the
//                        prologue's `utc 14,1`); C holds an address, not a location's name
//   any other consumer — `xts`, `asx`, arithmetic, `vjm`, `vtm`: not a plain word access
//
// A `utc` whose name is a Madlen constant literal (`,utc, =i1`) also classifies as
// LOC_GLOBAL, which is sound: a constant-pool entry has a fixed address like any global.
static Loc c_group_loc(const Besm_Instr *first)
{
    Loc l               = loc_none();
    const Besm_Instr *c = first->next; // provisional consumer

    if (first->kind == BESM_MOD_UTC) {
        // A `utc` names a location only when it carries a symbol and no index register:
        // C = &name + addr.  `utc reg,off` computes an address instead.
        if (first->name == NULL || first->konst != NULL || first->reg != 0)
            return loc_none();
        if (c != NULL && c->kind == BESM_MOD_WTC && !has_operand_symbol(c) && c->reg == 0 &&
            c->addr == 0 && first->addr == 0) {
            l.kind = LOC_DEREF; // C = mem[&name]: through the global pointer `name`
            l.name = first->name;
            c      = c->next;
        } else if (c != NULL && !is_c_setter(c)) {
            l.kind = LOC_GLOBAL;
            l.name = first->name;
            l.off  = first->addr; // the consumer's own offset is added below
        } else {
            return loc_none();
        }
    } else { // BESM_MOD_WTC: C = mem[M[reg] + off], a frame-resident pointer
        if (has_operand_symbol(first))
            return loc_none();
        l.kind = LOC_DEREF;
        l.reg  = (int)first->reg;
        l.off  = first->addr;
    }

    // The consumer must be a bare word access through C: `xta`/`atx` with EA = C + addr.
    if (c == NULL || (c->kind != BESM_MEM_XTA && c->kind != BESM_MEM_ATX) || c->reg != 0 ||
        has_operand_symbol(c))
        return loc_none();
    if (l.kind == LOC_GLOBAL)
        l.off += c->addr;
    else if (c->addr != 0)
        return loc_none(); // an offset off a dereferenced pointer: not a shape we model
    return l;
}

// The location a non-group `xta`/`atx` addresses: its frame slot, unless it carries a
// symbolic or constant operand (a global load emits its own UTC; a nonzero literal names no
// slot).  An operandless `xta` reaching here is a zero load and yields LOC_FRAME(0, 0) —
// mem[0] — never a real slot; a bare `xta`/`atx` that reads mem[C] belongs to a C group and
// is stepped by the sweep, so it never reaches this function.
static Loc plain_loc(const Besm_Instr *i)
{
    if ((i->kind != BESM_MEM_XTA && i->kind != BESM_MEM_ATX) || has_operand_symbol(i))
        return loc_none();
    Loc l = { LOC_FRAME, (int)i->reg, i->addr, NULL };
    return l;
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
// Update the tracked state to reflect executing a single non-group instruction `i` in
// straight-line code.  This runs only for non-boundary instructions (boundaries reset the
// state instead); C groups are stepped by the sweep, which knows their effective location.
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
        st->a_loc = plain_loc(i);
        return;
    default:
        st->a_loc = loc_none();
        return;
    }
}

//
// Rule #27 — redundant reload elimination.  Section 5.1 of docs/Peephole_Rewrites.md.
//
// `cur` is an `xta` reload of a location whose value the tracked state says A already holds
// (the preceding `atx` to it stored the value and did not disturb A).  The reload is pure
// waste; report a match so the caller splices it out.  For a plain frame slot the reload is
// the single `xta`; for a global it is the whole `utc name` + `xta` group, which the sweep
// matches through `c_group_loc` instead of this predicate.
//
static bool rule_redundant_reload(const Besm_Instr *cur, const PeepState *st)
{
    return cur->kind == BESM_MEM_XTA && loc_eq(plain_loc(cur), st->a_loc);
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
// returns true when the cursor should be deleted.  (Rule #30, compare → branch fusion,
// needs no entry — it is realized by #27 + #28; see the file header.)  Rules #31–#32
// would append here.
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
// and load from it count.  WTC counts: a word/byte dereference reads the pointer slot with
// `wtc reg,off` (it copies bits 15:1 into the C register), so a store that materialised an
// ADD_PTR address into that slot is live and must not be dropped as a dead temp.
static bool instr_reads_auto_slot(const Besm_Instr *i, int off)
{
    if (has_operand_symbol(i) || (int)i->reg != REG_AUTO || i->addr != off)
        return false;
    switch (i->kind) {
    case BESM_MOD_WTC:
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
    if (has_operand_symbol(i) || (int)i->reg != REG_AUTO || i->addr != off)
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
    if (cur->kind != BESM_MEM_ATX || has_operand_symbol(cur) || (int)cur->reg != REG_AUTO)
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

// Splice a single node out of a block's list and free it (defined below).
static void delete_instr(Besm_Block *block, Besm_Instr *prev, Besm_Instr *cur);

//
// Rule #31 — branch / label cleanup.  See docs/Peephole_Rewrites.md §5.5.
//
// Three rewrites that need only the control-flow shape, not the tracked A/R/ω state.
// None fits the `(cur, st)` `PeepRule` signature: two need list look-ahead and one
// mutates the list, so they are handled directly in the sweep.
//

// Is `i` an instruction the unreachable-tail rule may delete?  A label can be re-entered
// from elsewhere and the structural directives (NAME/SUBP/BASE/ENTRY/END) and data
// pseudo-ops carry no control flow, so they terminate the unreachable run and are never
// deleted; every real instruction after an unconditional transfer is dead and removable.
static bool unreachable_deletable(const Besm_Instr *i)
{
    switch (i->kind) {
    case BESM_STMT_LABEL:
    case BESM_STMT_NAME:
    case BESM_STMT_BASE:
    case BESM_STMT_SUBP:
    case BESM_STMT_ENTRY:
    case BESM_STMT_END:
    case BESM_DATA_INT:
    case BESM_DATA_REAL:
    case BESM_DATA_LOG:
    case BESM_DATA_BSS:
    case BESM_DATA_EQU:
    case BESM_DATA_REF:
    case BESM_DATA_STRING:
    case BESM_DATA_Z00:
        return false;
    default:
        return true;
    }
}

// Rule #31(a) — jump to the next instruction.  A `uj L` immediately followed by the
// definition of label `L` is a no-op fall-through; report a match so the caller deletes
// the `uj`.
static bool jump_to_next_label(const Besm_Instr *cur)
{
    return cur->kind == BESM_BRANCH_UJ && cur->name != NULL && cur->next != NULL &&
           cur->next->kind == BESM_STMT_LABEL && cur->next->name != NULL &&
           strcmp(cur->name, cur->next->name) == 0;
}

// Rule #31(c) — invert a conditional that only skips an unconditional jump.
//
// `uza L` / `uj M` / `L:`  ⇒  `u1a M` / `L:`   (and symmetrically `u1a`⇒`uza`).
// The conditional skips the `uj` exactly when it would NOT branch, so flipping the
// condition and retargeting it to M is equivalent, and the now-dead `uj` is removed.
// The label `L:` is left in place — it may still be a target elsewhere, and an
// unreferenced label is harmless.  Mutates `cur` in place, deletes the `uj` node, and
// returns true on a match.
static bool try_invert_branch_over_jump(Besm_Block *block, Besm_Instr *cur)
{
    if (cur->kind != BESM_BRANCH_UZA && cur->kind != BESM_BRANCH_U1A)
        return false;
    Besm_Instr *uj  = cur->next;
    if (uj == NULL || uj->kind != BESM_BRANCH_UJ || uj->name == NULL)
        return false;
    const Besm_Instr *lbl = uj->next;
    if (lbl == NULL || lbl->kind != BESM_STMT_LABEL || lbl->name == NULL)
        return false;
    if (cur->name == NULL || strcmp(cur->name, lbl->name) != 0)
        return false;

    cur->kind = (cur->kind == BESM_BRANCH_UZA) ? BESM_BRANCH_U1A : BESM_BRANCH_UZA;
    xfree(cur->name);
    cur->name = xstrdup(uj->name);
    delete_instr(block, cur, uj); // unlink `uj` (cur is its predecessor) and free it
    return true;
}

// If `i` directly reads or writes an auto slot, report its offset.  Used to attribute
// each slot reference to the basic block it occurs in (multi-block analysis).
static bool instr_auto_slot_ref(const Besm_Instr *i, int *off)
{
    if (has_operand_symbol(i) || (int)i->reg != REG_AUTO)
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

// Splice a whole C group — `count` consecutive nodes starting at `first` — out of a block's
// list and free them.  Relink around the run before freeing anything, then free node by
// node (`besm_free_instr` recurses on ->next).
static void delete_group(Besm_Block *block, Besm_Instr *prev, Besm_Instr *first, int count)
{
    Besm_Instr *after = first;
    for (int n = 0; n < count; n++)
        after = after->next;
    if (prev)
        prev->next = after;
    else
        block->body = after;

    Besm_Instr *cur = first;
    for (int n = 0; n < count; n++) {
        Besm_Instr *next = cur->next;
        cur->next        = NULL;
        besm_free_instr(cur);
        cur = next;
    }
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
        // Rule #31(b): unreachable tail.  Code after an unconditional transfer and
        // before the next label/directive can never execute; delete it.
        if (st.in_unreachable && unreachable_deletable(cur)) {
            Besm_Instr *next = cur->next;
            delete_instr(block, prev, cur);
            cur     = next;
            changed = true;
            continue; // still unreachable: re-test the new cur (prev/st unchanged)
        }

        // A C group is stepped and rewritten as one unit, so the cursor skips from its
        // setter straight past its consumer.  That is what makes it structurally impossible
        // for any rule below to delete a consumer and leave the setter behind to re-bind C
        // to whatever fell into the gap.  (The unreachable-tail rule above is the one
        // exception, and it is safe: it deletes a run head-first, so a group's setter goes
        // before its consumer is ever the cursor.)
        if (is_c_setter(cur)) {
            int count;
            Besm_Instr *consumer = c_group_consumer(cur, &count);
            if (consumer != NULL) {
                Loc gl = c_group_loc(cur);

                // Rule #27 for a global: the whole `utc name` + `xta` group reloads a
                // location A already holds.  Delete setter and consumer together.
                if (consumer->kind == BESM_MEM_XTA && loc_eq(gl, st.a_loc)) {
                    Besm_Instr *next = consumer->next;
                    delete_group(block, prev, cur, count);
                    cur     = next;
                    changed = true;
                    continue; // prev and tracked state stay valid
                }

                // Step the group.  A word access settles A on the location it touched (a
                // store leaves A mirroring what it wrote); a dereference or an address
                // computation names no location, and every other consumer — `xts`, `asx`,
                // arithmetic, `vtm`, `vjm` — clobbers A.  Both land on LOC_NONE.  No group
                // member is a SETR, so R is unchanged.
                if (consumer->kind == BESM_MEM_XTA || consumer->kind == BESM_MEM_ATX)
                    st.a_loc = gl;
                else
                    st.a_loc = loc_none();
                if (is_block_boundary(consumer)) // `wtc` + `vjm`: the indirect call
                    state_reset(&st);

                prev = consumer;
                cur  = consumer->next;
                continue;
            }
            // Malformed: the block ends on a setter.  Fall through to the single-node path.
        }

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
        // Rule #31(a): jump to the immediately following label (needs look-ahead).
        if (!deleted && (dead_temp_store(cur, frame, multiblock) || dead_ntr_set(cur) ||
                         jump_to_next_label(cur))) {
            Besm_Instr *next = cur->next;
            delete_instr(block, prev, cur);
            cur     = next;
            changed = true;
            deleted = true;
        }
        // Rule #31(c): invert a conditional that only skips an unconditional jump.
        // It mutates `cur` in place and deletes the following `uj`, so re-test `cur`.
        if (!deleted && try_invert_branch_over_jump(block, cur)) {
            changed = true;
            continue; // prev and tracked state stay valid; re-test the rewritten cur
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
                (strcmp(cur->name, "b$save") == 0 || strcmp(cur->name, "b$save0") == 0)) {
                st.r_known = true;
                st.r_val   = 7;
            }
            // An unconditional transfer (uj/stop) makes the following instructions
            // unreachable until the next label or structural directive (rule #31(b)).
            if (cur->kind == BESM_BRANCH_UJ || cur->kind == BESM_BRANCH_STOP)
                st.in_unreachable = true;
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
            // The multi-block classification only ever goes stale in the safe direction.
            // Deleting a `wtc %p` + `xta` reload group drops a read of `%p`, which may
            // have been that block's only reference to the slot; the slot then stays
            // marked multi-block when it is no longer, and rule #28 keeps a store it could
            // have dropped.  Nothing can add a reference, so it is never marked
            // single-block wrongly.  Compute it once before the fixpoint loop.
            bool *multiblock = compute_multiblock(block, num_autos);
            // Iterate to a fixpoint: one rewrite can expose another.
            while (peephole_sweep(block, frame, multiblock))
                ;
            if (multiblock)
                xfree(multiblock);
        }
    }
}
