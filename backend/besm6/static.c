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

void codegen_static_variable(const Tac_TopLevel *program, const Tac_TopLevel *tl, FILE *out)
{
    const char *name           = tl->u.static_variable.name;
    const Tac_StaticInit *init = tl->u.static_variable.init_list;

    Besm_Module *module = besm_new_module(name);
    Besm_DataSection *section;

    if (init == NULL) {
        section          = besm_new_data_section(BESM_SK_BSS);
        section->name    = xstrdup(name);
        module->sections = section;
        Besm_Instr *item = besm_new_instr(BESM_DATA_BSS);
        item->addr       = codegen_sizeof(tl->u.static_variable.type);
        section->items   = item;
    } else {
        section          = besm_new_data_section(BESM_SK_DATA);
        section->name    = xstrdup(name);
        module->sections = section;

        Besm_Instr **tail = &section->items;
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

    besm_fold_string_constants(module, program);
    emit_madlen_module(out, module);
    besm_free_module(module);
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
