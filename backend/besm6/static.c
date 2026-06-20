#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "abi.h"
#include "besm.h"
#include "internal.h"
#include "tac.h"
#include "utf8_to_koi7.h"
#include "xalloc.h"

static void insert_before_end(Besm_Block *block, Besm_Instr *chain);

// True for an array whose innermost element is a character type — its bytes are
// packed 6-per-word (see codegen_sizeof), so its static init list must be packed
// the same way rather than emitted one word per element.
static bool is_char_array(const Tac_Type *t)
{
    if (t->kind != TAC_TYPE_ARRAY)
        return false;
    const Tac_Type *e = t->u.array.elem_type;
    while (e->kind == TAC_TYPE_ARRAY)
        e = e->u.array.elem_type;
    return e->kind == TAC_TYPE_CHAR || e->kind == TAC_TYPE_SCHAR || e->kind == TAC_TYPE_UCHAR;
}

// Byte count an init item contributes to a packed character array.  A string's
// KOI-7 length can be shorter than its nominal (UTF-8) source length, so the byte
// count is taken from the converted data rather than from the array's type size.
static size_t char_init_item_bytes(const Tac_StaticInit *init)
{
    switch (init->kind) {
    case TAC_STATIC_INIT_I8:
    case TAC_STATIC_INIT_U8:
        return 1;
    case TAC_STATIC_INIT_ZERO:
        return (size_t)init->u.zero_bytes;
    case TAC_STATIC_INIT_STRING: {
        char *koi7 = xalloc(strlen(init->u.string.val) + 1, __func__, __FILE__, __LINE__);
        utf8_to_koi7(init->u.string.val, koi7);
        size_t nb = strlen(koi7) + (init->u.string.null_terminated ? 1 : 0);
        xfree(koi7);
        return nb;
    }
    default:
        fatal_error("unexpected static init kind %d in char array", (int)init->kind);
        return 0;
    }
}

// Flatten a character array's static init list (byte values, zero byte-runs, and
// embedded string literals) into one byte buffer packed 6 bytes per word.  Data
// words are emitted as BESM_DATA_LOG; complete all-zero words at the tail are
// coalesced into a single BESM_DATA_BSS (matching the convention for zero padding).
static Besm_Instr *char_array_log_items(const Tac_StaticInit *init)
{
    size_t total = 0;
    for (const Tac_StaticInit *it = init; it; it = it->next)
        total += char_init_item_bytes(it);
    if (total == 0)
        total = 1;

    unsigned char *buf = xalloc(total, __func__, __FILE__, __LINE__);
    memset(buf, 0, total);
    // `data_end` is the byte offset where the trailing run of explicit ZERO padding
    // begins.  Bytes from string/value items (even zero ones, like a string's null
    // terminator) are emitted as data words; only whole words past `data_end` become
    // a single BSS reservation.
    size_t pos = 0, data_end = 0;
    for (const Tac_StaticInit *it = init; it; it = it->next) {
        if (it->kind == TAC_STATIC_INIT_STRING) {
            char *koi7 = xalloc(strlen(it->u.string.val) + 1, __func__, __FILE__, __LINE__);
            utf8_to_koi7(it->u.string.val, koi7);
            size_t nb = strlen(koi7) + (it->u.string.null_terminated ? 1 : 0);
            for (size_t i = 0; i < nb && pos + i < total; i++)
                buf[pos + i] = (unsigned char)koi7[i];
            pos += nb;
            data_end = pos;
            xfree(koi7);
        } else if (it->kind == TAC_STATIC_INIT_I8) {
            if (pos < total)
                buf[pos] = (uint8_t)it->u.char_val;
            pos++;
            data_end = pos;
        } else if (it->kind == TAC_STATIC_INIT_U8) {
            if (pos < total)
                buf[pos] = it->u.uchar_val;
            pos++;
            data_end = pos;
        } else { // TAC_STATIC_INIT_ZERO
            pos += (size_t)it->u.zero_bytes; // buffer already zeroed; not counted as data
        }
    }

    size_t n_words   = (total + 5) / 6;
    size_t log_words = (data_end + 5) / 6; // words covering the string/value data

    Besm_Instr *head = NULL, **tail = &head;
    for (size_t w = 0; w < log_words; w++) {
        unsigned long long word = 0;
        for (int b = 0; b < 6; b++)
            word = (word << 8) | buf[w * 6 + b];
        Besm_Instr *si = besm_new_instr(BESM_DATA_LOG);
        si->log_val    = word;
        *tail          = si;
        tail           = &si->next;
    }
    long zero_words = (long)n_words - (long)log_words;
    if (zero_words > 0) {
        Besm_Instr *bss = besm_new_instr(BESM_DATA_BSS);
        bss->addr       = (int)zero_words;
        *tail           = bss;
    }
    xfree(buf);
    return head;
}

static unsigned long long static_init_log_val(const Tac_StaticInit *init)
{
    switch (init->kind) {
    case TAC_STATIC_INIT_I8:
        return (unsigned long long)(uint8_t)init->u.char_val;
    case TAC_STATIC_INIT_U8:
        return (unsigned long long)init->u.uchar_val;
    case TAC_STATIC_INIT_I16:
        return (unsigned long long)(int64_t)init->u.short_val & 0x1FFFFFFFFFF;
    case TAC_STATIC_INIT_I32:
        return (unsigned long long)(int64_t)init->u.int_val & 0x1FFFFFFFFFF;
    case TAC_STATIC_INIT_I64:
        return (unsigned long long)init->u.long_val & 0x1FFFFFFFFFF;
    case TAC_STATIC_INIT_U16:
        return (unsigned long long)init->u.ushort_val;
    case TAC_STATIC_INIT_U32:
        return (unsigned long long)init->u.uint_val;
    case TAC_STATIC_INIT_U64:
        return init->u.ulong_val & 0xFFFFFFFFFFFF;
    default:
        fatal_error("non-integer static init in log_val");
    }
}

// Build the chain of data directives for a static object of the given type and init list.
// `init == NULL` reserves zeroed storage (a `,bss,`).  The returned items carry no label;
// callers attach one as needed (a data section's `,name,`, or the first item for a static
// local emitted inside a function module).
static Besm_Instr *static_data_items(const Tac_Type *type, const Tac_StaticInit *init)
{
    if (init == NULL) {
        Besm_Instr *item = besm_new_instr(BESM_DATA_BSS);
        item->addr       = codegen_sizeof(type);
        return item;
    }

    // A character array packs 6 bytes per word, so its whole init list (byte values,
    // zero runs, embedded strings) is flattened into a packed byte stream rather than
    // emitted one word per element.
    if (is_char_array(type))
        return char_array_log_items(init);

    Besm_Instr *head  = NULL;
    Besm_Instr **tail = &head;
    {
        for (; init; init = init->next) {
            Besm_Instr *item;
            switch (init->kind) {
            case TAC_STATIC_INIT_I8:
            case TAC_STATIC_INIT_I16:
            case TAC_STATIC_INIT_I32:
            case TAC_STATIC_INIT_I64:
            case TAC_STATIC_INIT_U8:
            case TAC_STATIC_INIT_U16:
            case TAC_STATIC_INIT_U32:
            case TAC_STATIC_INIT_U64:
                item          = besm_new_instr(BESM_DATA_LOG);
                item->log_val = static_init_log_val(init);
                break;
            case TAC_STATIC_INIT_ZERO:
                item       = besm_new_instr(BESM_DATA_BSS);
                item->addr = (init->u.zero_bytes + 5) / 6;
                break;
            case TAC_STATIC_INIT_POINTER: {
                int byte_offset = init->u.pointer.byte_offset;
                if (byte_offset % 6 != 0)
                    fatal_error("Pointer byte offset is not a multiple of word size");
                Besm_Instr *subp = besm_new_instr(BESM_STMT_SUBP);
                subp->name       = xstrdup(init->u.pointer.name);
                *tail            = subp;
                tail             = &subp->next;
                Besm_Instr *z00a = besm_new_instr(BESM_DATA_Z00);
                *tail            = z00a;
                tail             = &z00a->next;
                Besm_Instr *z00b = besm_new_instr(BESM_DATA_Z00);
                z00b->name       = xstrdup(init->u.pointer.name);
                z00b->addr       = byte_offset / 6;
                *tail            = z00b;
                tail             = &z00b->next;
                continue;
            }
            case TAC_STATIC_INIT_FAT_POINTER: {
                int byte_off     = init->u.pointer.byte_offset;
                Besm_Instr *subp = besm_new_instr(BESM_STMT_SUBP);
                subp->name       = xstrdup(init->u.pointer.name);
                *tail            = subp;
                tail             = &subp->next;
                Besm_Instr *z00a = besm_new_instr(BESM_DATA_Z00);
                z00a->reg        = 8 + (unsigned)(5 - byte_off % 6);
                *tail            = z00a;
                tail             = &z00a->next;
                Besm_Instr *z00b = besm_new_instr(BESM_DATA_Z00);
                z00b->name       = xstrdup(init->u.pointer.name);
                z00b->addr       = byte_off / 6;
                *tail            = z00b;
                tail             = &z00b->next;
                continue;
            }
            case TAC_STATIC_INIT_FLOAT:
                item           = besm_new_instr(BESM_DATA_REAL);
                item->real_val = init->u.float_val;
                break;
            case TAC_STATIC_INIT_DOUBLE:
                item           = besm_new_instr(BESM_DATA_REAL);
                item->real_val = init->u.double_val;
                break;
            case TAC_STATIC_INIT_LONG_DOUBLE:
                // long double ≡ double on BESM-6 (one 48-bit native-FP word).
                item           = besm_new_instr(BESM_DATA_REAL);
                item->real_val = (double)init->u.long_double_val;
                break;
            case TAC_STATIC_INIT_STRING:
                // Char-array init (e.g. `char arr[] = "ABC"`); the section NAME
                // already labels the first word, so no per-item label here.
                *tail = besm_string_log_items(init, NULL);
                while (*tail)
                    tail = &(*tail)->next;
                continue;
            default:
                fatal_error("TODO: non-float static init (Phase C)");
            }
            *tail = item;
            tail  = &item->next;
        }
    }
    return head;
}

void codegen_static_variable(const Tac_TopLevel *program, const Tac_TopLevel *tl, FILE *out)
{
    const char *name           = tl->u.static_variable.name;
    const Tac_StaticInit *init = tl->u.static_variable.init_list;

    Besm_Module *module      = besm_new_module(name);
    Besm_DataSection *section = besm_new_data_section(init == NULL ? BESM_SK_BSS : BESM_SK_DATA);
    section->name             = xstrdup(name);
    section->items            = static_data_items(tl->u.static_variable.type, init);
    module->sections          = section;

    besm_fold_string_constants(module, program);
    emit_madlen_module(out, module);
    besm_free_module(module);
}

// True for a data directive that stores a word and can therefore carry a Madlen label in
// its `name` field (unlike SUBP, which declares an external, or Z00, which uses `name` as
// its operand).
static bool data_item_labelable(const Besm_Instr *item)
{
    switch (item->kind) {
    case BESM_DATA_LOG:
    case BESM_DATA_BSS:
    case BESM_DATA_REAL:
    case BESM_DATA_INT:
        return true;
    default:
        return false;
    }
}

// Emit each block-scope static local of `fn` as a module-local labeled datum, spliced into
// the function module just before its `,end,` (after the code).  String constants referenced
// by a static-local initializer are folded in by the caller's besm_fold_string_constants.
void besm_emit_static_locals(Besm_Module *module, const Tac_TopLevel *fn)
{
    if (!module->funcs)
        return;
    Besm_Block *last = module->funcs->blocks;
    while (last->next)
        last = last->next;

    for (const Tac_StaticLocal *sl = fn->u.function.static_locals; sl; sl = sl->next) {
        Besm_Instr *items = static_data_items(sl->type, sl->init_list);
        if (items && data_item_labelable(items))
            items->name = xstrdup(sl->name);
        else
            fatal_error("static local %s: unsupported initializer layout", sl->name);
        insert_before_end(last, items);
    }
}

// Pack a string static-init into a chain of BESM_DATA_LOG words (6 KOI-7 bytes per
// word, big-endian).  When `label` is non-NULL it is set as the Madlen label of the
// first word.  Used both for char-array data and for string constants folded into a
// referencing module.
Besm_Instr *besm_string_log_items(const Tac_StaticInit *init, const char *label)
{
    if (init->kind != TAC_STATIC_INIT_STRING)
        fatal_error("string constant init is not a string");

    const char *raw = init->u.string.val;
    char *koi7      = xalloc(strlen(raw) + 1, __func__, __FILE__, __LINE__);
    utf8_to_koi7(raw, koi7);
    const char *s = koi7;
    size_t len    = strlen(s);
    size_t nbytes = len + (init->u.string.null_terminated ? 1 : 0);
    if (nbytes == 0)
        nbytes = 1;

    Besm_Instr *head = NULL, **tail = &head;
    for (size_t w = 0; w * 6 < nbytes; w++) {
        unsigned long long word = 0;
        for (int b = 0; b < 6; b++) {
            size_t pos      = w * 6 + b;
            unsigned char c = (pos < len) ? (unsigned char)s[pos] : 0;
            word            = (word << 8) | c;
        }
        Besm_Instr *si = besm_new_instr(BESM_DATA_LOG);
        si->log_val    = word;
        if (w == 0 && label)
            si->name = xstrdup(label);
        *tail = si;
        tail  = &si->next;
    }
    xfree(koi7);
    return head;
}

// Find the init data of the TAC_TOPLEVEL_STATIC_CONSTANT named `name`, or NULL.
static const Tac_StaticInit *find_string_constant(const Tac_TopLevel *program, const char *name)
{
    for (const Tac_TopLevel *tl = program; tl; tl = tl->next)
        if (tl->kind == TAC_TOPLEVEL_STATIC_CONSTANT && tl->u.static_constant.name &&
            strcmp(tl->u.static_constant.name, name) == 0)
            return tl->u.static_constant.init;
    return NULL;
}

// Does any instruction in `list` carry `name` (as an operand or label)?
static bool instr_list_references(const Besm_Instr *list, const char *name)
{
    for (; list; list = list->next)
        if (list->name && strcmp(list->name, name) == 0)
            return true;
    return false;
}

static bool module_references(const Besm_Module *module, const char *name)
{
    for (const Besm_Func *fn = module->funcs; fn; fn = fn->next)
        for (const Besm_Block *b = fn->blocks; b; b = b->next)
            if (instr_list_references(b->body, name))
                return true;
    for (const Besm_DataSection *s = module->sections; s; s = s->next)
        if (instr_list_references(s->items, name))
            return true;
    return false;
}

// Unlink and free every BESM_STMT_SUBP whose name matches `name` from the list at *head.
static void remove_subp(Besm_Instr **head, const char *name)
{
    Besm_Instr *prev = NULL;
    Besm_Instr *i    = *head;
    while (i) {
        if (i->kind == BESM_STMT_SUBP && i->name && strcmp(i->name, name) == 0) {
            Besm_Instr *next = i->next;
            if (prev)
                prev->next = next;
            else
                *head = next;
            i->next = NULL;
            besm_free_instr(i);
            i = next;
        } else {
            prev = i;
            i    = i->next;
        }
    }
}

// Splice `chain` into block->body immediately before the BESM_STMT_END terminator.
static void insert_before_end(Besm_Block *block, Besm_Instr *chain)
{
    if (!chain)
        return;
    Besm_Instr *ctail = chain;
    while (ctail->next)
        ctail = ctail->next;

    Besm_Instr *prev = NULL;
    for (Besm_Instr *i = block->body; i; prev = i, i = i->next) {
        if (i->kind == BESM_STMT_END) {
            ctail->next = i;
            if (prev)
                prev->next = chain;
            else
                block->body = chain;
            return;
        }
    }
    // No END (should not happen for a well-formed function): append at the tail.
    if (prev)
        prev->next = chain;
    else
        block->body = chain;
}

//
// Fold every string constant the module references into the module itself as a local
// label, instead of emitting it as a separate global `,name,` module.  The per-unit
// `_strN` name is module-local here, so it can no longer collide across separately
// assembled objects.  For each referenced constant: drop its external SUBP, then
// append its packed data words (labeled with the constant name) — before the function
// `,end,` for a code module, or at the tail of the data section for a data module.
//
void besm_fold_string_constants(Besm_Module *module, const Tac_TopLevel *program)
{
    for (const Tac_TopLevel *tl = program; tl; tl = tl->next) {
        if (tl->kind != TAC_TOPLEVEL_STATIC_CONSTANT || !tl->u.static_constant.name)
            continue;
        const char *name = tl->u.static_constant.name;
        if (!module_references(module, name))
            continue;

        for (Besm_Func *fn = module->funcs; fn; fn = fn->next)
            for (Besm_Block *b = fn->blocks; b; b = b->next)
                remove_subp(&b->body, name);
        for (Besm_DataSection *s = module->sections; s; s = s->next)
            remove_subp(&s->items, name);

        const Tac_StaticInit *init = find_string_constant(program, name);
        Besm_Instr *chain          = besm_string_log_items(init, name);

        if (module->funcs) {
            Besm_Block *last = module->funcs->blocks;
            while (last->next)
                last = last->next;
            insert_before_end(last, chain);
        } else if (module->sections) {
            Besm_DataSection *last = module->sections;
            while (last->next)
                last = last->next;
            Besm_Instr **t = &last->items;
            while (*t)
                t = &(*t)->next;
            *t = chain;
        }
    }
}
