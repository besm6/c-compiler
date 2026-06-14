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
    case TAC_STATIC_INIT_I8:  return (unsigned long long)(uint8_t)init->u.char_val;
    case TAC_STATIC_INIT_U8:  return (unsigned long long)init->u.uchar_val;
    case TAC_STATIC_INIT_I16: return (unsigned long long)(int64_t)init->u.short_val & 0x1FFFFFFFFFF;
    case TAC_STATIC_INIT_I32: return (unsigned long long)(int64_t)init->u.int_val   & 0x1FFFFFFFFFF;
    case TAC_STATIC_INIT_I64: return (unsigned long long)init->u.long_val           & 0x1FFFFFFFFFF;
    case TAC_STATIC_INIT_U16: return (unsigned long long)init->u.ushort_val;
    case TAC_STATIC_INIT_U32: return (unsigned long long)init->u.uint_val;
    case TAC_STATIC_INIT_U64: return init->u.ulong_val & 0xFFFFFFFFFFFF;
    default: fatal_error("non-integer static init in log_val");
    }
}

void codegen_static_variable(const Tac_TopLevel *tl, FILE *out)
{
    const char             *name = tl->u.static_variable.name;
    const Tac_StaticInit *init   = tl->u.static_variable.init_list;

    Besm_Module      *module  = besm_new_module(name);
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
            case TAC_STATIC_INIT_I8:  case TAC_STATIC_INIT_I16:
            case TAC_STATIC_INIT_I32: case TAC_STATIC_INIT_I64:
            case TAC_STATIC_INIT_U8:  case TAC_STATIC_INIT_U16:
            case TAC_STATIC_INIT_U32: case TAC_STATIC_INIT_U64:
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
                subp->name = xstrdup(init->u.pointer.name);
                *tail = subp; tail = &subp->next;
                Besm_Instr *z00a = besm_new_instr(BESM_DATA_Z00);
                *tail = z00a; tail = &z00a->next;
                Besm_Instr *z00b = besm_new_instr(BESM_DATA_Z00);
                z00b->name = xstrdup(init->u.pointer.name);
                z00b->addr = byte_offset / 6;
                *tail = z00b; tail = &z00b->next;
                continue;
            }
            case TAC_STATIC_INIT_FAT_POINTER: {
                int byte_off = init->u.pointer.byte_offset;
                Besm_Instr *subp = besm_new_instr(BESM_STMT_SUBP);
                subp->name = xstrdup(init->u.pointer.name);
                *tail = subp; tail = &subp->next;
                Besm_Instr *z00a = besm_new_instr(BESM_DATA_Z00);
                z00a->reg = 8 + (unsigned)(5 - byte_off % 6);
                *tail = z00a; tail = &z00a->next;
                Besm_Instr *z00b = besm_new_instr(BESM_DATA_Z00);
                z00b->name = xstrdup(init->u.pointer.name);
                z00b->addr = byte_off / 6;
                *tail = z00b; tail = &z00b->next;
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
            case TAC_STATIC_INIT_STRING: {
                const char *raw = init->u.string.val;
                char *koi7      = xalloc(strlen(raw) + 1, __func__, __FILE__, __LINE__);
                utf8_to_koi7(raw, koi7);
                const char *s = koi7;
                size_t len    = strlen(s);
                size_t nbytes = len + (init->u.string.null_terminated ? 1 : 0);
                if (nbytes == 0) nbytes = 1;
                for (size_t w = 0; w * 6 < nbytes; w++) {
                    unsigned long long word = 0;
                    for (int b = 0; b < 6; b++) {
                        size_t pos      = w * 6 + b;
                        unsigned char c = (pos < len) ? (unsigned char)s[pos] : 0;
                        word            = (word << 8) | c;
                    }
                    Besm_Instr *si = besm_new_instr(BESM_DATA_LOG);
                    si->log_val    = word;
                    *tail = si;
                    tail  = &si->next;
                }
                xfree(koi7);
                continue;
            }
            default:
                fatal_error("TODO: non-float static init (Phase C)");
            }
            *tail = item;
            tail  = &item->next;
        }
    }

    emit_madlen_module(out, module);
    besm_free_module(module);
}

void codegen_static_constant(const Tac_TopLevel *tl, FILE *out)
{
    const char           *name = tl->u.static_constant.name;
    const Tac_StaticInit *init = tl->u.static_constant.init;

    Besm_Module      *module  = besm_new_module(name);
    module->comment           = xstrdup("const");
    Besm_DataSection *section = besm_new_data_section(BESM_SK_DATA);
    section->name             = xstrdup(name);
    module->sections          = section;

    Besm_Instr **tail = &section->items;
    for (; init; init = init->next) {
        Besm_Instr *item;
        switch (init->kind) {
        case TAC_STATIC_INIT_I8:  case TAC_STATIC_INIT_I16:
        case TAC_STATIC_INIT_I32: case TAC_STATIC_INIT_I64:
        case TAC_STATIC_INIT_U8:  case TAC_STATIC_INIT_U16:
        case TAC_STATIC_INIT_U32: case TAC_STATIC_INIT_U64:
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
            subp->name = xstrdup(init->u.pointer.name);
            *tail = subp; tail = &subp->next;
            Besm_Instr *z00a = besm_new_instr(BESM_DATA_Z00);
            *tail = z00a; tail = &z00a->next;
            Besm_Instr *z00b = besm_new_instr(BESM_DATA_Z00);
            z00b->name = xstrdup(init->u.pointer.name);
            z00b->addr = byte_offset / 6;
            *tail = z00b; tail = &z00b->next;
            continue;
        }
        case TAC_STATIC_INIT_FAT_POINTER: {
            int byte_off = init->u.pointer.byte_offset;
            Besm_Instr *subp = besm_new_instr(BESM_STMT_SUBP);
            subp->name = xstrdup(init->u.pointer.name);
            *tail = subp; tail = &subp->next;
            Besm_Instr *z00a = besm_new_instr(BESM_DATA_Z00);
            z00a->reg = 8 + (unsigned)(5 - byte_off % 6);
            *tail = z00a; tail = &z00a->next;
            Besm_Instr *z00b = besm_new_instr(BESM_DATA_Z00);
            z00b->name = xstrdup(init->u.pointer.name);
            z00b->addr = byte_off / 6;
            *tail = z00b; tail = &z00b->next;
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
        case TAC_STATIC_INIT_STRING: {
            const char *raw = init->u.string.val;
            char *koi7      = xalloc(strlen(raw) + 1, __func__, __FILE__, __LINE__);
            utf8_to_koi7(raw, koi7);
            const char *s = koi7;
            size_t len    = strlen(s);
            size_t nbytes = len + (init->u.string.null_terminated ? 1 : 0);
            if (nbytes == 0) nbytes = 1;
            for (size_t w = 0; w * 6 < nbytes; w++) {
                unsigned long long word = 0;
                for (int b = 0; b < 6; b++) {
                    size_t pos      = w * 6 + b;
                    unsigned char c = (pos < len) ? (unsigned char)s[pos] : 0;
                    word            = (word << 8) | c;
                }
                Besm_Instr *si = besm_new_instr(BESM_DATA_LOG);
                si->log_val    = word;
                *tail = si;
                tail  = &si->next;
            }
            xfree(koi7);
            continue;
        }
        default:
            fatal_error("unsupported static constant init kind %d", (int)init->kind);
        }
        *tail = item;
        tail  = &item->next;
    }

    emit_madlen_module(out, module);
    besm_free_module(module);
}
