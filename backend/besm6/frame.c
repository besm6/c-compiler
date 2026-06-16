#include "frame.h"

#include <limits.h>
#include <stdint.h>

#include "abi.h"
#include "string_map.h"
#include "xalloc.h"

// Encode (reg, offset, temp) into a single intptr_t so we can store it in the map.
// reg occupies bits 16-19; a "compiler temporary" marker is packed at bit 20 so the
// peephole pass can recover temp-ness from an auto slot (map_iterate exposes only the
// value, not the key, so the flag must travel inside the value).
#define SLOT_ENCODE(reg, off, temp) \
    (((intptr_t)((temp) ? 1 : 0) << 20) | ((intptr_t)(reg) << 16) | (intptr_t)(off))
#define SLOT_REG(v)  ((((int)(v)) >> 16) & 0xf)
#define SLOT_OFF(v)  (((int)(v)) & 0xffff)
#define SLOT_TEMP(v) ((((int)(v)) >> 20) & 1)

struct Frame {
    StringMap slots;     // name -> SLOT_ENCODE(reg, offset, temp)
    int num_autos;
    bool *auto_is_temp;  // size num_autos; true if that auto slot holds a '%'+digit temporary
};

// A TAC name denotes a compiler temporary (new_temp) when it is '%' followed by a digit;
// parameters and named locals are '%' followed by a letter or '_'.
static bool name_is_temp(const char *name)
{
    return name[0] == '%' && name[1] >= '0' && name[1] <= '9';
}

// Per-temporary live range, recorded during the body walk and consumed by the
// linear-scan slot assignment below.  Positions are linear instruction indices;
// `block` is the basic-block index of the first occurrence.  A temporary seen in
// more than one block (`multiblock`), or whose address is taken, is never reused
// — it gets a permanent dedicated slot.
typedef struct {
    const char *name; // borrowed from the TAC node (valid for the frame's lifetime)
    int first_pos;
    int last_pos;
    int block;
    bool multiblock;
} TempInfo;

// Working context threaded through the collection walk.  Non-temporary frame
// names are assigned auto slots immediately (preserving the historical layout);
// '%'+digit temporaries are only recorded here, then packed by linear scan.
typedef struct {
    Frame *f;
    int *auto_count;      // non-temp auto slot counter (== &f->num_autos)
    int pos;              // current instruction index
    int blk;              // current basic-block index
    StringMap temp_index; // temp name -> index into temps[]
    TempInfo *temps;
    int num_temps;
    int cap_temps;
} Collect;

// Grow the temps[] array if full.
static void temps_reserve(Collect *c)
{
    if (c->num_temps < c->cap_temps)
        return;
    int newcap     = c->cap_temps ? c->cap_temps * 2 : 8;
    TempInfo *grown = (TempInfo *)xalloc(newcap * sizeof(TempInfo), __func__, __FILE__, __LINE__);
    for (int i = 0; i < c->num_temps; i++)
        grown[i] = c->temps[i];
    if (c->temps)
        xfree(c->temps);
    c->temps     = grown;
    c->cap_temps = newcap;
}

// Record one occurrence of a '%'+digit temporary at the current position/block.
// `addressed` forces the temporary to stay in a dedicated, never-reused slot
// (its address is taken, so name-based liveness would be unsound).
static void record_temp(Collect *c, const char *name, bool addressed)
{
    intptr_t idx;
    if (map_get(&c->temp_index, name, &idx)) {
        TempInfo *t = &c->temps[idx];
        t->last_pos = c->pos;
        if (c->blk != t->block)
            t->multiblock = true;
        if (addressed)
            t->multiblock = true;
        return;
    }
    temps_reserve(c);
    TempInfo *t   = &c->temps[c->num_temps];
    t->name       = name;
    t->first_pos  = c->pos;
    t->last_pos   = c->pos;
    t->block      = c->blk;
    t->multiblock = addressed;
    map_insert(&c->temp_index, name, (intptr_t)c->num_temps, 0);
    c->num_temps++;
}

//
// Note one frame-resident name occurrence.  Frame-resident names start with '%';
// any other name is a parameter (seeded directly in frame_build) or a module-level
// global and is skipped.  '%'+digit temporaries are recorded for later linear-scan
// reuse; every other '%' name is assigned a dedicated auto slot immediately (unless
// already present, e.g. a parameter or a repeat occurrence).
//
static void note_name(Collect *c, const char *name, bool addressed)
{
    if (name[0] != '%')
        return; // parameter or module-level global — not an auto slot
    if (name_is_temp(name)) {
        record_temp(c, name, addressed);
        return;
    }
    intptr_t dummy;
    if (map_get(&c->f->slots, name, &dummy))
        return; // already assigned (e.g. a parameter)
    map_insert(&c->f->slots, name, SLOT_ENCODE(REG_AUTO, *c->auto_count, false), 0);
    (*c->auto_count)++;
}

//
// Visit all Tac_Val * values in a chain and note their VAR names.
//
static void collect_vals(Collect *c, const Tac_Val *v)
{
    for (; v; v = v->next) {
        if (v->kind == TAC_VAL_VAR)
            note_name(c, v->u.var_name, false);
    }
}

//
// Visit all values referenced by a single instruction.
//
static void collect_instr(Collect *c, const Tac_Instruction *instr)
{
    switch (instr->kind) {
    case TAC_INSTRUCTION_RETURN:
        collect_vals(c, instr->u.return_.src);
        break;
    // All type-conversion instructions follow the {src, dst} pattern.
    case TAC_INSTRUCTION_SIGN_EXTEND:
        collect_vals(c, instr->u.sign_extend.src);
        collect_vals(c, instr->u.sign_extend.dst);
        break;
    case TAC_INSTRUCTION_TRUNCATE:
        collect_vals(c, instr->u.truncate.src);
        collect_vals(c, instr->u.truncate.dst);
        break;
    case TAC_INSTRUCTION_ZERO_EXTEND:
        collect_vals(c, instr->u.zero_extend.src);
        collect_vals(c, instr->u.zero_extend.dst);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
        collect_vals(c, instr->u.double_to_int.src);
        collect_vals(c, instr->u.double_to_int.dst);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
        collect_vals(c, instr->u.double_to_uint.src);
        collect_vals(c, instr->u.double_to_uint.dst);
        break;
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
        collect_vals(c, instr->u.int_to_double.src);
        collect_vals(c, instr->u.int_to_double.dst);
        break;
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
        collect_vals(c, instr->u.uint_to_double.src);
        collect_vals(c, instr->u.uint_to_double.dst);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
        collect_vals(c, instr->u.float_to_double.src);
        collect_vals(c, instr->u.float_to_double.dst);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
        collect_vals(c, instr->u.double_to_float.src);
        collect_vals(c, instr->u.double_to_float.dst);
        break;
    case TAC_INSTRUCTION_INT_TO_FLOAT:
        collect_vals(c, instr->u.int_to_float.src);
        collect_vals(c, instr->u.int_to_float.dst);
        break;
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
        collect_vals(c, instr->u.uint_to_float.src);
        collect_vals(c, instr->u.uint_to_float.dst);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_INT:
        collect_vals(c, instr->u.float_to_int.src);
        collect_vals(c, instr->u.float_to_int.dst);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
        collect_vals(c, instr->u.float_to_uint.src);
        collect_vals(c, instr->u.float_to_uint.dst);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
        collect_vals(c, instr->u.long_double_to_int.src);
        collect_vals(c, instr->u.long_double_to_int.dst);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
        collect_vals(c, instr->u.long_double_to_uint.src);
        collect_vals(c, instr->u.long_double_to_uint.dst);
        break;
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
        collect_vals(c, instr->u.int_to_long_double.src);
        collect_vals(c, instr->u.int_to_long_double.dst);
        break;
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
        collect_vals(c, instr->u.uint_to_long_double.src);
        collect_vals(c, instr->u.uint_to_long_double.dst);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
        collect_vals(c, instr->u.long_double_to_double.src);
        collect_vals(c, instr->u.long_double_to_double.dst);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
        collect_vals(c, instr->u.double_to_long_double.src);
        collect_vals(c, instr->u.double_to_long_double.dst);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
        collect_vals(c, instr->u.long_double_to_float.src);
        collect_vals(c, instr->u.long_double_to_float.dst);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
        collect_vals(c, instr->u.float_to_long_double.src);
        collect_vals(c, instr->u.float_to_long_double.dst);
        break;
    case TAC_INSTRUCTION_PTR_TO_CHAR_PTR:
        collect_vals(c, instr->u.ptr_to_char_ptr.src);
        collect_vals(c, instr->u.ptr_to_char_ptr.dst);
        break;
    case TAC_INSTRUCTION_CHAR_PTR_TO_PTR:
        collect_vals(c, instr->u.char_ptr_to_ptr.src);
        collect_vals(c, instr->u.char_ptr_to_ptr.dst);
        break;
    case TAC_INSTRUCTION_UNARY:
        collect_vals(c, instr->u.unary.src);
        collect_vals(c, instr->u.unary.dst);
        break;
    case TAC_INSTRUCTION_BINARY:
        collect_vals(c, instr->u.binary.src1);
        collect_vals(c, instr->u.binary.src2);
        collect_vals(c, instr->u.binary.dst);
        break;
    case TAC_INSTRUCTION_COPY:
        collect_vals(c, instr->u.copy.src);
        collect_vals(c, instr->u.copy.dst);
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        // src is a local variable (gets a slot) or a global (skipped). Its address is
        // taken, so a temporary src must keep a dedicated slot (addressed = true).
        if (instr->u.get_address.src->kind == TAC_VAL_VAR)
            note_name(c, instr->u.get_address.src->u.var_name, true);
        collect_vals(c, instr->u.get_address.dst);
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        collect_vals(c, instr->u.load.src_ptr);
        collect_vals(c, instr->u.load.dst);
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        collect_vals(c, instr->u.store.src);
        collect_vals(c, instr->u.store.dst_ptr);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        collect_vals(c, instr->u.add_ptr.ptr);
        collect_vals(c, instr->u.add_ptr.index);
        collect_vals(c, instr->u.add_ptr.dst);
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        collect_vals(c, instr->u.ptr_diff.ptr_a);
        collect_vals(c, instr->u.ptr_diff.ptr_b);
        collect_vals(c, instr->u.ptr_diff.dst);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        collect_vals(c, instr->u.copy_to_offset.src);
        note_name(c, instr->u.copy_to_offset.dst, false);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        note_name(c, instr->u.copy_from_offset.src, false);
        collect_vals(c, instr->u.copy_from_offset.dst);
        break;
    case TAC_INSTRUCTION_JUMP:
    case TAC_INSTRUCTION_LABEL:
        break; // no values
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
        collect_vals(c, instr->u.jump_if_zero.condition);
        break;
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        collect_vals(c, instr->u.jump_if_not_zero.condition);
        break;
    case TAC_INSTRUCTION_FUN_CALL:
        collect_vals(c, instr->u.fun_call.args);
        collect_vals(c, instr->u.fun_call.dst);
        break;
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        break; // multi-word slot reserved in a dedicated first pass (frame_build)
    }
}

// Does `instr` end a basic block (a terminator: jump, branch, or return)?  The
// next instruction begins a new block.  A LABEL begins a block too, handled at the
// head of the walk.  Conservative splitting keeps the single-block classification
// sound: any temporary spanning two segments is flagged multiblock and not reused.
static bool ends_block(const Tac_Instruction *instr)
{
    switch (instr->kind) {
    case TAC_INSTRUCTION_JUMP:
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
    case TAC_INSTRUCTION_RETURN:
        return true;
    default:
        return false;
    }
}

//
// Linear-scan assignment of auto slots to the collected '%'+digit temporaries.
// Temporaries are already in increasing-first_pos order (first-seen order), so a
// single pass suffices.  Non-overlapping single-block temporaries share a slot;
// multi-block (and address-taken) temporaries keep a permanent dedicated slot.
// Temp slots are packed above the non-temp autos (offsets >= base_temp), so the
// auto_is_temp classification stays unambiguous.
//
static void assign_temps(Collect *c)
{
    if (c->num_temps == 0)
        return;

    int base_temp = c->f->num_autos;
    int n         = c->num_temps;

    int *freestk   = (int *)xalloc(n * sizeof(int), __func__, __FILE__, __LINE__);
    int *act_last  = (int *)xalloc(n * sizeof(int), __func__, __FILE__, __LINE__);
    int *act_rel   = (int *)xalloc(n * sizeof(int), __func__, __FILE__, __LINE__);
    int free_top   = 0;
    int act_n      = 0;
    int high_water = 0;

    for (int i = 0; i < n; i++) {
        const TempInfo *t = &c->temps[i];
        int first         = t->first_pos;

        // Expire actives whose live range ends strictly before this one begins,
        // returning their relative offsets to the free stack.
        int w = 0;
        for (int a = 0; a < act_n; a++) {
            if (act_last[a] < first) {
                freestk[free_top++] = act_rel[a];
            } else {
                act_last[w] = act_last[a];
                act_rel[w]  = act_rel[a];
                w++;
            }
        }
        act_n = w;

        int rel = (free_top > 0) ? freestk[--free_top] : high_water++;
        // Multi-block / address-taken temporaries never expire: a dedicated slot.
        int eff_last   = t->multiblock ? INT_MAX : t->last_pos;
        act_last[act_n] = eff_last;
        act_rel[act_n]  = rel;
        act_n++;

        map_insert(&c->f->slots, t->name, SLOT_ENCODE(REG_AUTO, base_temp + rel, true), 0);
    }

    c->f->num_autos = base_temp + high_water;

    xfree(freestk);
    xfree(act_last);
    xfree(act_rel);
}

//
// Reserve a contiguous multi-word frame slot for an aggregate local described by
// an AllocateLocal pseudo-instruction. Run before the scalar scan so the aggregate
// occupies one unbroken block of words; scalars then fill the remaining slots.
//
static void allocate_aggregate(Frame *f, const Tac_Instruction *instr, int *auto_count)
{
    const char *name = instr->u.allocate_local.name;
    if (name[0] != '%')
        return; // defensive: aggregate locals are always '%'-prefixed
    intptr_t dummy;
    if (map_get(&f->slots, name, &dummy))
        return; // already assigned
    // AllocateLocal carries target bytes; convert to whole words (round up).
    int size_words  = (instr->u.allocate_local.size + BESM6_WORD_BYTES - 1) / BESM6_WORD_BYTES;
    int align_words = (instr->u.allocate_local.alignment + BESM6_WORD_BYTES - 1) / BESM6_WORD_BYTES;
    if (align_words < 1)
        align_words = 1;
    if (align_words > 1 && (*auto_count % align_words) != 0)
        *auto_count += align_words - (*auto_count % align_words);
    // Aggregates are named locals ('%'+letter), never temporaries.
    map_insert(&f->slots, name, SLOT_ENCODE(REG_AUTO, *auto_count, name_is_temp(name)), 0);
    *auto_count += size_words;
}

// map_iterate callback: record temp-ness of each auto slot into the bool array.
// map_iterate exposes only the encoded value (not the key), which is why the temp
// marker is packed into the slot encoding.
typedef struct {
    bool *arr;
    int num;
} TempFill;

static void fill_auto_is_temp(intptr_t value, const void *arg)
{
    const TempFill *tf = (const TempFill *)arg;
    if (SLOT_REG(value) != REG_AUTO)
        return; // params (REG_PAR) are never temporaries
    int off = SLOT_OFF(value);
    if (off >= 0 && off < tf->num)
        tf->arr[off] = SLOT_TEMP(value);
}

Frame *frame_build(const Tac_TopLevel *fn, const Tac_TopLevel *program)
{
    (void)program; // local/global is now encoded in the name (leading '%')

    Frame *f         = (Frame *)xalloc(sizeof(Frame), __func__, __FILE__, __LINE__);
    f->num_autos     = 0;
    f->auto_is_temp  = NULL;
    map_init(&f->slots);

    // Assign params first (REG_PAR, 0..N-1). Param names are '%'-prefixed too, but a
    // parameter is never a compiler temporary.
    int par_count = 0;
    for (const Tac_Param *p = fn->u.function.params; p; p = p->next) {
        map_insert(&f->slots, p->name, SLOT_ENCODE(REG_PAR, par_count, false), 0);
        par_count++;
    }

    // First pass: reserve contiguous slots for aggregate locals (arrays, structs)
    // so their words do not overlap adjacent scalar slots.
    for (const Tac_Instruction *instr = fn->u.function.body; instr; instr = instr->next)
        if (instr->kind == TAC_INSTRUCTION_ALLOCATE_LOCAL)
            allocate_aggregate(f, instr, &f->num_autos);

    // Second pass: scan the body for '%'-prefixed names. Non-temporary names
    // ('%'+letter/'_') get a dedicated one-word auto slot (REG_AUTO) immediately, in
    // first-seen order, just past the aggregates. '%'+digit temporaries are only
    // recorded here (with their live range), then packed by linear scan below so
    // non-overlapping temporaries share a slot. Non-prefixed names are globals
    // (skipped); params are already assigned (REG_PAR).
    Collect c;
    c.f          = f;
    c.auto_count = &f->num_autos;
    c.pos        = 0;
    c.blk        = 0;
    c.temps      = NULL;
    c.num_temps  = 0;
    c.cap_temps  = 0;
    map_init(&c.temp_index);

    for (const Tac_Instruction *instr = fn->u.function.body; instr; instr = instr->next) {
        if (instr->kind == TAC_INSTRUCTION_LABEL)
            c.blk++; // a label begins a new basic block
        collect_instr(&c, instr);
        if (ends_block(instr))
            c.blk++; // a terminator ends the block; the next instruction starts fresh
        c.pos++;
    }

    // Pack the recorded temporaries into reusable auto slots above the non-temp region.
    assign_temps(&c);

    map_destroy(&c.temp_index);
    if (c.temps)
        xfree(c.temps);

    // Build the reverse auto-slot -> temp? lookup the peephole pass consults.
    if (f->num_autos > 0) {
        f->auto_is_temp =
            (bool *)xalloc(f->num_autos * sizeof(bool), __func__, __FILE__, __LINE__);
        for (int i = 0; i < f->num_autos; i++)
            f->auto_is_temp[i] = false;
        TempFill tf = { f->auto_is_temp, f->num_autos };
        map_iterate(&f->slots, fill_auto_is_temp, &tf);
    }

    return f;
}

bool frame_slot_is_temp(const Frame *f, int reg, int off)
{
    return reg == REG_AUTO && off >= 0 && off < f->num_autos && f->auto_is_temp[off];
}

bool frame_lookup(const Frame *f, const char *name, int *reg, int *offset)
{
    intptr_t v;
    if (!map_get(&f->slots, name, &v))
        return false;
    *reg    = SLOT_REG(v);
    *offset = SLOT_OFF(v);
    return true;
}

int frame_num_autos(const Frame *f)
{
    return f->num_autos;
}

void frame_free(Frame *f)
{
    map_destroy(&f->slots);
    if (f->auto_is_temp)
        xfree(f->auto_is_temp);
    xfree(f);
}
