#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abi.h"
#include "besm.h"
#include "codegen.h"
#include "frame.h"
#include "string_map.h"
#include "utf8_to_koi7.h"
#include "xalloc.h"

// Forward declarations.
static void codegen_function(const Tac_TopLevel *program, const Tac_TopLevel *tl,
                             FILE *out);
static void codegen_instr(const Tac_Instruction *instr, const Frame *f,
                          Besm_Block *block, Besm_Instr **tail);

_Noreturn static void fatal_error(const char *fmt, ...)
{
    fprintf(stderr, "codegen error: ");
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

// Append a new instruction to a block, maintaining *tail.
static Besm_Instr *emit(Besm_Block *block, Besm_Instr **tail, Besm_InstrKind kind)
{
    Besm_Instr *i = besm_new_instr(kind);
    if (!block->body)
        block->body = i;
    else
        (*tail)->next = i;
    *tail = i;
    return i;
}

// Emit XTA: A = mem[reg + off].
static void emit_xta(Besm_Block *b, Besm_Instr **t, int reg, int off)
{
    Besm_Instr *i = emit(b, t, BESM_MEM_XTA);
    i->reg        = reg;
    i->addr       = off;
}

// Emit ATX: mem[reg + off] = A.
static void emit_atx(Besm_Block *b, Besm_Instr **t, int reg, int off)
{
    Besm_Instr *i = emit(b, t, BESM_MEM_ATX);
    i->reg        = reg;
    i->addr       = off;
}

// Emit an arithmetic instruction: A op= mem[reg + off].
static void emit_arith(Besm_Block *b, Besm_Instr **t, Besm_InstrKind kind, int reg, int off)
{
    Besm_Instr *i = emit(b, t, kind);
    i->reg        = reg;
    i->addr       = off;
}

// Frame lookup with fatal_error on miss.
static void lookup(const Frame *f, const char *name, int *reg, int *off)
{
    if (!frame_lookup(f, name, reg, off))
        fatal_error("variable '%s' not in frame", name);
}

// Build a heap-allocated Madlen literal-address string for a TAC constant.
// Signed int/char/long: masked to 41 bits (raw two's-complement, exponent=0).
// Unsigned uint/ulong/uchar: masked to 48 bits.
// Float/double: "=r<value>" in %.13g format.
static char *const_lit_name(const Tac_Const *c)
{
    char buf[64];
    switch (c->kind) {
    case TAC_CONST_INT:
        snprintf(buf, sizeof(buf), "=%llo",
                 (unsigned long long)((long long)c->u.int_val & (long long)0x1FFFFFFFFFF));
        break;
    case TAC_CONST_LONG:
        snprintf(buf, sizeof(buf), "=%llo",
                 (unsigned long long)(c->u.long_val & (long long)0x1FFFFFFFFFF));
        break;
    case TAC_CONST_LONG_LONG:
        snprintf(buf, sizeof(buf), "=%llo",
                 (unsigned long long)(c->u.long_long_val & (long long)0x1FFFFFFFFFF));
        break;
    case TAC_CONST_CHAR:
        snprintf(buf, sizeof(buf), "=%llo",
                 (unsigned long long)((long long)c->u.char_val & (long long)0x1FFFFFFFFFF));
        break;
    case TAC_CONST_UINT:
        snprintf(buf, sizeof(buf), "=%llo",
                 (unsigned long long)(c->u.uint_val & 0xFFFFFFFFFFFFULL));
        break;
    case TAC_CONST_ULONG:
        snprintf(buf, sizeof(buf), "=%llo",
                 (unsigned long long)(c->u.ulong_val & 0xFFFFFFFFFFFFULL));
        break;
    case TAC_CONST_ULONG_LONG:
        snprintf(buf, sizeof(buf), "=%llo",
                 (unsigned long long)(c->u.ulong_long_val & 0xFFFFFFFFFFFFULL));
        break;
    case TAC_CONST_UCHAR:
        snprintf(buf, sizeof(buf), "=%llo", (unsigned long long)c->u.uchar_val);
        break;
    case TAC_CONST_FLOAT:
    case TAC_CONST_DOUBLE: {
        double val = (c->kind == TAC_CONST_FLOAT) ? (double)c->u.float_val : c->u.double_val;
        char   num[48];
        mad_format_real(num, sizeof(num), val);
        snprintf(buf, sizeof(buf), "=r%s", num);
        break;
    }
    default:
        fatal_error("unsupported constant kind %d", (int)c->kind);
    }
    return xstrdup(buf);
}

// Emit XTA for a TAC value: variable from frame, UTC+XTA for a global, or =N/=rX for a constant.
static void emit_xta_val(Besm_Block *b, Besm_Instr **t, const Frame *f, const Tac_Val *v)
{
    if (v->kind == TAC_VAL_VAR) {
        int reg, off;
        if (frame_lookup(f, v->u.var_name, &reg, &off)) {
            emit_xta(b, t, reg, off);
        } else {
            Besm_Instr *utc = emit(b, t, BESM_MOD_UTC);
            utc->name       = xstrdup(v->u.var_name);
            emit_xta(b, t, 0, 0);
        }
    } else {
        Besm_Instr *i = emit(b, t, BESM_MEM_XTA);
        i->name       = const_lit_name(v->u.constant);
    }
}

// Emit XTS for a TAC value (push A to stack, load v into A) — used for args 1..N-1.
// For a global variable: UTC sets C to the global's address, then XTS loads mem[C+0].
static void emit_xts_val(Besm_Block *b, Besm_Instr **t, const Frame *f, const Tac_Val *v)
{
    if (v->kind == TAC_VAL_VAR) {
        int reg, off;
        if (frame_lookup(f, v->u.var_name, &reg, &off)) {
            Besm_Instr *i = emit(b, t, BESM_MEM_XTS);
            i->reg        = reg;
            i->addr       = off;
        } else {
            Besm_Instr *utc = emit(b, t, BESM_MOD_UTC);
            utc->name       = xstrdup(v->u.var_name);
            Besm_Instr *i   = emit(b, t, BESM_MEM_XTS);
            (void)i;   // reg=0, addr=0 → XTS mem[C+0]
        }
    } else {
        Besm_Instr *i = emit(b, t, BESM_MEM_XTS);
        i->name       = const_lit_name(v->u.constant);
    }
}

// Emit an arithmetic instruction for a TAC value: local, global (via UTC), or constant literal.
static void emit_arith_val(Besm_Block *b, Besm_Instr **t, Besm_InstrKind kind,
                           const Frame *f, const Tac_Val *v)
{
    if (v->kind == TAC_VAL_VAR) {
        int reg, off;
        if (frame_lookup(f, v->u.var_name, &reg, &off)) {
            emit_arith(b, t, kind, reg, off);
        } else {
            Besm_Instr *utc = emit(b, t, BESM_MOD_UTC);
            utc->name       = xstrdup(v->u.var_name);
            emit(b, t, kind);   // reg=0, addr=0 → op mem[C+0]
        }
    } else {
        Besm_Instr *i = emit(b, t, kind);
        i->name       = const_lit_name(v->u.constant);
    }
}

// Emit a binary op that lowers to a runtime helper:  dst = helper(src1, src2).
//
// Used by the integer comparisons (b/eq, b/ne, b/lt, b/le, b/gt, b/ge and the unsigned
// orderings) and by unsigned add (b/uadd).  These follow the lightweight helper
// convention (NOT the C ABI):
//   - first operand `a` sits on the stack top, second operand `b` is in A;
//   - the helper reads `a` through the stack register (M17 pre-decrement), which both
//     consumes `a` and pops it (r15 -= 1), so the caller emits no stack adjustment;
//   - the result is left in A; the helper returns via 13 ,uj,.
//   - ,call, self-declares the external (like printf), so no ,subp, is needed.
//
// Sequence (a = src1, b = src2):
//   XTA src1        — A = a
//   XTS src2        — push a (r15 += 1); A = b
//   ,call, helper   — helper pops a (r15 -= 1); result in A
//   reg ,ATX, off   — store result into dst's frame slot
//
static void emit_binop_helper(Besm_Block *b, Besm_Instr **t, const Frame *f,
                              const Tac_Val *src1, const Tac_Val *src2,
                              const char *helper, int dr, int doff)
{
    emit_xta_val(b, t, f, src1);
    emit_xts_val(b, t, f, src2);
    Besm_Instr *call = emit(b, t, BESM_BRANCH_CALL);
    call->name       = xstrdup(helper);
    emit_atx(b, t, dr, doff);
}

// Extract the integer value of an integer constant (used for constant shift counts).
static long tac_const_int(const Tac_Const *c)
{
    switch (c->kind) {
    case TAC_CONST_INT:        return c->u.int_val;
    case TAC_CONST_LONG:       return (long)c->u.long_val;
    case TAC_CONST_LONG_LONG:  return (long)c->u.long_long_val;
    case TAC_CONST_CHAR:       return c->u.char_val;
    case TAC_CONST_UINT:       return (long)c->u.uint_val;
    case TAC_CONST_ULONG:      return (long)c->u.ulong_val;
    case TAC_CONST_ULONG_LONG: return (long)c->u.ulong_long_val;
    case TAC_CONST_UCHAR:      return c->u.uchar_val;
    default:
        fatal_error("non-integer constant kind %d as shift count", (int)c->kind);
    }
}

// Emit a shift:  dst = src1 << src2  (left) or  dst = src1 >> src2  (right).
//
// Shifts are logical for both int and unsigned, and right-shift does no sign extension.
// BESM-6 ASN/ASX shift by (exponent_field - 64): a field > 64 shifts right, < 64 shifts
// left.  So left by k uses field 64-k, right by k uses field 64+k.
//
//   - Constant count k: load the value, then a single ASN with the computed field.
//   - Variable count: runtime-helper convention (value on stack top, count in A) — call
//     b/lsh / b/rsh, which leave the result in A and pop the value.
static void emit_shift(Besm_Block *b, Besm_Instr **t, const Frame *f,
                       const Tac_Val *src1, const Tac_Val *src2,
                       bool left, int dr, int doff)
{
    emit_xta_val(b, t, f, src1);
    if (src2->kind == TAC_VAL_CONSTANT) {
        long k = tac_const_int(src2->u.constant);
        Besm_Instr *asn = emit(b, t, BESM_EXP_SHIFTN);
        asn->addr       = (int)(left ? 64 - k : 64 + k);
    } else {
        emit_xts_val(b, t, f, src2);
        Besm_Instr *call = emit(b, t, BESM_BRANCH_CALL);
        call->name       = xstrdup(left ? "b/lsh" : "b/rsh");
    }
    emit_atx(b, t, dr, doff);
}

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

static void codegen_static_variable(const Tac_TopLevel *tl, FILE *out)
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

static void codegen_static_constant(const Tac_TopLevel *tl, FILE *out)
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

void codegen_program(const Tac_TopLevel *program, const Tac_TopLevel *tl, FILE *out)
{
    switch (tl->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        codegen_function(program, tl, out);
        break;
    case TAC_TOPLEVEL_STATIC_VARIABLE:
        codegen_static_variable(tl, out);
        break;
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        codegen_static_constant(tl, out);
        break;
    }
}

// Declare `v` as an external (SUBP) if it is a module-level name that has no
// frame slot and has not been declared yet.  SUBP allocates no memory; it just
// tells the single-pass assembler the name is external, and must precede the
// first UTC that references it.
static void declare_global_operand(Besm_Block *block, Besm_Instr **tail,
                                   const Frame *f, StringMap *declared,
                                   const Tac_Val *v)
{
    if (!v || v->kind != TAC_VAL_VAR)
        return;
    int sr, so;
    intptr_t dummy;
    if (frame_lookup(f, v->u.var_name, &sr, &so))
        return;                       // local / param
    if (map_get(declared, v->u.var_name, &dummy))
        return;                       // already declared
    Besm_Instr *ssubp = emit(block, tail, BESM_STMT_SUBP);
    ssubp->name       = xstrdup(v->u.var_name);
    map_insert(declared, v->u.var_name, 1, 0);
}

static void codegen_function(const Tac_TopLevel *program, const Tac_TopLevel *tl,
                             FILE *out)
{
    const char *name = tl->u.function.name;

    int num_params = 0;
    for (const Tac_Param *p = tl->u.function.params; p; p = p->next)
        num_params++;
    bool needs_param_setup = (num_params >= 2) || tl->u.function.variadic;
    bool is_empty          = (tl->u.function.body == NULL);

    Besm_Module *module = besm_new_module(name);
    Besm_Func   *func   = besm_new_func(name, BESM_CC_BESM6_C);
    module->funcs       = func;

    Besm_Block  *block  = besm_new_block();
    func->blocks        = block;

    Besm_Instr *tail = NULL;

    Besm_Instr *iname = emit(block, &tail, BESM_STMT_NAME);
    iname->name       = xstrdup(name);

    if (is_empty) {
        // Optimized prologue for empty functions: no b/save or b/ret.
        if (strcmp(name, "main") == 0) {
            Besm_Instr *entry_prog = emit(block, &tail, BESM_STMT_ENTRY);
            entry_prog->name       = xstrdup("program");
        }
        if (needs_param_setup) {
            // 14 ,utc, 1
            Besm_Instr *utc14 = emit(block, &tail, BESM_MOD_UTC);
            utc14->reg        = REG_CNT;
            utc14->addr       = 1;
            // 15 ,utm,
            Besm_Instr *utm15 = emit(block, &tail, BESM_REG_UTM);
            utm15->reg        = REG_SP;
        }
        // 13 ,uj,
        Besm_Instr *uj13 = emit(block, &tail, BESM_BRANCH_UJ);
        uj13->reg        = REG_RET;
        emit(block, &tail, BESM_STMT_END);
    } else {
        // Full prologue: push last argument + capture return address, call b/save,
        // then extend the stack by the number of auto-variable slots.
        //
        // Build the frame early so we can declare SUBP references for static
        // constants before the first instruction that uses them (single-pass assembler).
        Frame *f      = frame_build(tl, program);
        int num_autos = frame_num_autos(f);

        Besm_Instr *subp_cret = emit(block, &tail, BESM_STMT_SUBP);
        subp_cret->name       = xstrdup("b/ret");

        if (strcmp(name, "main") == 0) {
            Besm_Instr *entry_prog = emit(block, &tail, BESM_STMT_ENTRY);
            entry_prog->name       = xstrdup("program");
        }

        // Declare each module-level name a function references as a SUBP word.  SUBP
        // allocates no memory; it just tells the assembler the name is external.  It
        // must appear before the first instruction that uses the name (single-pass
        // assembler), so scan every operand of every instruction up front.  A name with
        // a frame slot is a local/param and is skipped; a map avoids duplicates.
        StringMap declared;
        map_init(&declared);
        for (const Tac_Instruction *instr = tl->u.function.body; instr; instr = instr->next) {
            switch (instr->kind) {
            case TAC_INSTRUCTION_RETURN:
                declare_global_operand(block, &tail, f, &declared, instr->u.return_.src);
                break;
            case TAC_INSTRUCTION_COPY:
                declare_global_operand(block, &tail, f, &declared, instr->u.copy.src);
                declare_global_operand(block, &tail, f, &declared, instr->u.copy.dst);
                break;
            case TAC_INSTRUCTION_GET_ADDRESS:
                declare_global_operand(block, &tail, f, &declared, instr->u.get_address.src);
                break;
            case TAC_INSTRUCTION_UNARY:
                declare_global_operand(block, &tail, f, &declared, instr->u.unary.src);
                declare_global_operand(block, &tail, f, &declared, instr->u.unary.dst);
                break;
            case TAC_INSTRUCTION_BINARY:
                declare_global_operand(block, &tail, f, &declared, instr->u.binary.src1);
                declare_global_operand(block, &tail, f, &declared, instr->u.binary.src2);
                declare_global_operand(block, &tail, f, &declared, instr->u.binary.dst);
                break;
            case TAC_INSTRUCTION_LOAD:
                declare_global_operand(block, &tail, f, &declared, instr->u.load.src_ptr);
                declare_global_operand(block, &tail, f, &declared, instr->u.load.dst);
                break;
            case TAC_INSTRUCTION_STORE:
                declare_global_operand(block, &tail, f, &declared, instr->u.store.src);
                declare_global_operand(block, &tail, f, &declared, instr->u.store.dst_ptr);
                break;
            case TAC_INSTRUCTION_ADD_PTR:
                declare_global_operand(block, &tail, f, &declared, instr->u.add_ptr.ptr);
                declare_global_operand(block, &tail, f, &declared, instr->u.add_ptr.index);
                declare_global_operand(block, &tail, f, &declared, instr->u.add_ptr.dst);
                break;
            case TAC_INSTRUCTION_JUMP_IF_ZERO:
                declare_global_operand(block, &tail, f, &declared, instr->u.jump_if_zero.condition);
                break;
            case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
                declare_global_operand(block, &tail, f, &declared, instr->u.jump_if_not_zero.condition);
                break;
            case TAC_INSTRUCTION_FUN_CALL:
                // After copy propagation, a global may appear directly as a FUN_CALL
                // argument (UTC/XTA sequence).
                for (const Tac_Val *a = instr->u.fun_call.args; a; a = a->next)
                    declare_global_operand(block, &tail, f, &declared, a);
                declare_global_operand(block, &tail, f, &declared, instr->u.fun_call.dst);
                break;
            default:
                break;
            }
        }
        map_destroy(&declared);

        Besm_Instr *its13 = emit(block, &tail, BESM_MEM_ITS);
        its13->addr       = REG_RET;

        Besm_Instr *call_csave = emit(block, &tail, BESM_BRANCH_CALL);
        call_csave->name       = xstrdup(num_params == 0 ? "b/save0" : "b/save");

        if (num_autos > 0) {
            Besm_Instr *utm_sp = emit(block, &tail, BESM_REG_UTM);
            utm_sp->reg        = REG_SP;
            utm_sp->addr       = num_autos;
        }

        for (const Tac_Instruction *instr = tl->u.function.body; instr; instr = instr->next)
            codegen_instr(instr, f, block, &tail);

        Besm_Instr *uj_cret = emit(block, &tail, BESM_BRANCH_UJ);
        uj_cret->name       = xstrdup("b/ret");
        emit(block, &tail, BESM_STMT_END);

        frame_free(f);
    }

    emit_madlen_module(out, module);
    besm_free_module(module);
}

static void codegen_instr(const Tac_Instruction *instr, const Frame *f,
                          Besm_Block *block, Besm_Instr **tail)
{
    switch (instr->kind) {
    // COPY  dst = src
    //
    // In C:  b = a;
    // TAC:   copy src → dst
    //
    // Four sub-cases depending on whether each operand is a local (frame slot) or a
    // module-level global (addressed via UTC/XTA or UTC/ATX through C):
    //
    //   local  → local : reg_src ,XTA, off_src  /  reg_dst ,ATX, off_dst
    //   global → local : ,UTC, src_name  /  ,XTA,  /  reg_dst ,ATX, off_dst
    //   local  → global: reg_src ,XTA, off_src  /  ,UTC, dst_name  /  ,ATX,
    //   global → global: ,UTC, src_name  /  ,XTA,  /  ,UTC, dst_name  /  ,ATX,
    //
    // Bare ,XTA, / ,ATX, (reg=0, addr=0) load/store the word whose address is in C.
    // UTC sets C = address of the named global (using mem[0]=0 architecturally as base).
    // UTC does not disturb A, so the local→global order (XTA before UTC) is safe.
    //
    case TAC_INSTRUCTION_COPY: {
        const Tac_Val *src = instr->u.copy.src;
        const Tac_Val *dst = instr->u.copy.dst;
        emit_xta_val(block, tail, f, src);
        int dr, doff;
        if (frame_lookup(f, dst->u.var_name, &dr, &doff)) {
            emit_atx(block, tail, dr, doff);
        } else {
            Besm_Instr *utc = emit(block, tail, BESM_MOD_UTC);
            utc->name       = xstrdup(dst->u.var_name);
            emit(block, tail, BESM_MEM_ATX);
        }
        break;
    }
    // GET_ADDRESS  dst = &src
    //
    // In C:  p = &a;
    // TAC:   get_address src → dst   (src is the variable being addressed;
    //                                 dst receives its runtime word address)
    //
    // BESM-6 sequence (r14 is a scratch index register):
    //   reg_src ,UTC, off_src — C = M[reg_src] + off_src: copy
    //                           the word address of src into C
    //   14 ,VTM,              — M[14] = C, so r14 now holds the word address of src
    //   ,ITA, 14              — A = M[14]: load that address into the accumulator
    //   reg_dst ,ATX, off_dst — store A (the address) into dst's frame slot
    //
    case TAC_INSTRUCTION_GET_ADDRESS: {
        int dr, doff;
        lookup(f, instr->u.get_address.dst->u.var_name, &dr, &doff);
        const char *src_name = instr->u.get_address.src->u.var_name;
        int sr, so;
        if (frame_lookup(f, src_name, &sr, &so)) {
            // Local variable: compute address from its frame slot.
            if (so == 0) {
                Besm_Instr *ita = emit(block, tail, BESM_MEM_ITA);
                ita->addr       = sr;
            } else {
                Besm_Instr *utc = emit(block, tail, BESM_MOD_UTC);
                utc->reg        = sr;
                utc->addr       = so;
                Besm_Instr *vtm = emit(block, tail, BESM_REG_VTM);
                vtm->reg        = 14;
                Besm_Instr *ita = emit(block, tail, BESM_MEM_ITA);
                ita->addr       = 14;
            }
        } else {
            // Module-level static: M[0]=0 architecturally, so UTC 0,name gives C = label addr.
            // The SUBP declaration for src_name is emitted before the prologue (single-pass).
            Besm_Instr *utc = emit(block, tail, BESM_MOD_UTC);
            utc->name       = xstrdup(src_name);
            Besm_Instr *vtm = emit(block, tail, BESM_REG_VTM);
            vtm->reg        = 14;
            Besm_Instr *ita = emit(block, tail, BESM_MEM_ITA);
            ita->addr       = 14;
        }
        emit_atx(block, tail, dr, doff);
        break;
    }
    // LOAD  dst = *src_ptr
    //
    // In C:  b = *p;
    // TAC:   load *src_ptr → dst   (src_ptr is a pointer variable in the frame;
    //                               dst receives the dereferenced value)
    //
    // BESM-6 sequence (r1 is used as a pointer index register):
    //   reg_ptr ,XTA, off_ptr   — load the pointer value (a word address) into A
    //   ,ATI, 1                 — M[1] = A: store the pointer into index register r1
    //   1 ,XTA, 0               — A = mem[M[1]+0]: dereference — load the word that
    //                              r1 points to into A
    //   reg_dst ,ATX, off_dst   — store the loaded value into dst's frame slot
    //
    // All BESM-6 pointers are word addresses; the offset in the final XTA is always
    // 0 because TAC LOAD always reads the base of the pointed-to object.
    case TAC_INSTRUCTION_LOAD: {
        int pr, po, dr, doff;
        lookup(f, instr->u.load.src_ptr->u.var_name, &pr, &po);
        lookup(f, instr->u.load.dst->u.var_name, &dr, &doff);
        emit_xta(block, tail, pr, po);
        Besm_Instr *ati = emit(block, tail, BESM_MEM_ATI);
        ati->addr       = 1;
        emit_xta(block, tail, 1, 0);
        emit_atx(block, tail, dr, doff);
        break;
    }
    // STORE  *dst_ptr = src
    //
    // In C:  *p = a;
    // TAC:   store src → *dst_ptr   (dst_ptr is a pointer variable in the frame;
    //                                src is the value to write through it)
    //
    // BESM-6 sequence (r1 is used as a pointer index register):
    //   reg_ptr ,XTA, off_ptr   — load the pointer value (a word address) into A
    //   ,ATI, 1                 — M[1] = A: store the pointer into index register r1
    //   reg_src ,XTA, off_src   — load the source value into A
    //   1 ,ATX, 0               — mem[M[1]+0] = A: write A through the pointer
    //
    // The pointer must be loaded before the source because ATI consumes A.
    // The write offset is always 0 for the same reason as in LOAD above.
    case TAC_INSTRUCTION_STORE: {
        int pr, po, sr, so;
        lookup(f, instr->u.store.dst_ptr->u.var_name, &pr, &po);
        lookup(f, instr->u.store.src->u.var_name, &sr, &so);
        emit_xta(block, tail, pr, po);
        Besm_Instr *ati = emit(block, tail, BESM_MEM_ATI);
        ati->addr       = 1;
        emit_xta(block, tail, sr, so);
        emit_atx(block, tail, 1, 0);
        break;
    }
    // UNARY  dst = op src
    //
    // Negate, complement, and logical not are implemented (task #6).
    //
    // Logical not (!) lowers to the b/not runtime helper for every operand type: the
    // helper tests A against zero, leaving 1 if the operand is zero and 0 otherwise.  It
    // is a unary helper like b/uneg: operand in A, result in A, returns via 13 ,uj,.
    //
    // Negate needs three representation-specific sequences:
    //   - signed int:  X-A 0   (0 - A; mem[0]=0 architecturally).  R=7 after b/save
    //                  suppresses normalization/rounding, so this works directly.
    //   - unsigned:    the b/uneg runtime helper (48-bit modular negate).  It is a
    //                  unary helper: operand in A, result in A, returns via 13 ,uj,.
    //   - double:      enable normalization+rounding (NTR 0) around X-A 0, then
    //                  restore (NTR 7).  The surrounding NTRs are the temporary form;
    //                  a later pass will trim the ones that prove unnecessary.
    //
    // Complement is uniform for int and unsigned: AEX against an all-ones word flips
    // all 48 bits.  For unsigned this is the exact 48-bit complement; for signed int
    // it also flips the exponent field, yielding a non-canonical word (accepted UB).
    case TAC_INSTRUCTION_UNARY: {
        const Tac_Val *src = instr->u.unary.src;
        const Tac_Val *dst = instr->u.unary.dst;
        int rd, od;
        lookup(f, dst->u.var_name, &rd, &od);

        switch (instr->u.unary.op) {
        case TAC_UNARY_NEGATE:
            emit_xta_val(block, tail, f, src);
            emit(block, tail, BESM_ARITH_RSUB);
            emit_atx(block, tail, rd, od);
            break;
        case TAC_UNARY_COMPLEMENT: {
            // Same sequence for int and unsigned: flip all 48 bits.
            emit_xta_val(block, tail, f, src);
            Besm_Instr *aex = emit(block, tail, BESM_LOG_AEX);
            aex->name       = xstrdup("=7777777777777777"); // 48 one-bits, octal
            emit_atx(block, tail, rd, od);
            break;
        }
        case TAC_UNARY_NEGATE_UNSIGNED: {
            emit_xta_val(block, tail, f, src);
            Besm_Instr *call = emit(block, tail, BESM_BRANCH_CALL);
            call->name       = xstrdup("b/uneg");
            emit_atx(block, tail, rd, od);
            break;
        }
        case TAC_UNARY_NOT: {
            // Logical NOT for any operand type: b/not returns 1 if A == 0, else 0.
            emit_xta_val(block, tail, f, src);
            Besm_Instr *call = emit(block, tail, BESM_BRANCH_CALL);
            call->name       = xstrdup("b/not");
            emit_atx(block, tail, rd, od);
            break;
        }
        case TAC_UNARY_NEGATE_DOUBLE: {
            emit_xta_val(block, tail, f, src);
            Besm_Instr *ntr_on = emit(block, tail, BESM_EXP_SETR);
            ntr_on->addr       = 0;
            emit(block, tail, BESM_ARITH_RSUB);
            Besm_Instr *ntr_off = emit(block, tail, BESM_EXP_SETR);
            ntr_off->addr       = 7;
            emit_atx(block, tail, rd, od);
            break;
        }
        default:
            fatal_error("TODO: unary op %d (Phase H)", (int)instr->u.unary.op);
        }
        break;
    }
    // BINARY  dst = src1 op src2
    //
    // For integer add/subtract and the bitwise ops (and/or/xor), R=7 after b/save
    // suppresses normalization and rounding, so the arithmetic and logical
    // instructions work directly on raw words.  No NTR is needed.
    //
    // BESM-6 sequence:
    //   reg_src1 ,XTA, off_src1   — load src1 from its frame slot into A
    //   reg_src2 ,A+X, off_src2   — A = A op src2  (A-X / AAX / AOX / AEX)
    //   reg_dst  ,ATX, off_dst    — store A into dst's frame slot
    //
    case TAC_INSTRUCTION_BINARY: {
        const Tac_Val *src1 = instr->u.binary.src1;
        const Tac_Val *src2 = instr->u.binary.src2;
        const Tac_Val *dst  = instr->u.binary.dst;
        int rd, od;
        lookup(f, dst->u.var_name, &rd, &od);

        // Comparisons lower to a runtime relational helper.  The unsigned ordering ops
        // temporarily reuse the signed helpers (correct within the 41-bit signed range)
        // until the b/ult/b/ule/b/ugt/b/uge library lands (TODO: task #20).  b/eq/b/ne
        // are signedness-independent.
        const char *cmp_helper = NULL;
        switch (instr->u.binary.op) {
        case TAC_BINARY_EQUAL:                     cmp_helper = "b/eq"; break;
        case TAC_BINARY_NOT_EQUAL:                 cmp_helper = "b/ne"; break;
        case TAC_BINARY_LESS_THAN:                 cmp_helper = "b/lt"; break;
        case TAC_BINARY_LESS_OR_EQUAL:             cmp_helper = "b/le"; break;
        case TAC_BINARY_GREATER_THAN:              cmp_helper = "b/gt"; break;
        case TAC_BINARY_GREATER_OR_EQUAL:          cmp_helper = "b/ge"; break;
        case TAC_BINARY_LESS_THAN_UNSIGNED:        cmp_helper = "b/ult"; break;
        case TAC_BINARY_LESS_OR_EQUAL_UNSIGNED:    cmp_helper = "b/ule"; break;
        case TAC_BINARY_GREATER_THAN_UNSIGNED:     cmp_helper = "b/ugt"; break;
        case TAC_BINARY_GREATER_OR_EQUAL_UNSIGNED: cmp_helper = "b/uge"; break;
        default: break;
        }
        if (cmp_helper) {
            emit_binop_helper(block, tail, f, src1, src2, cmp_helper, rd, od);
            break;
        }

        // Unsigned add cannot use the inline additive unit: full 48-bit unsigned values
        // carry data in the exponent field (bits 48-42), which A+X misreads.  The b/uadd
        // helper does true 48-bit modular add via 24-bit half-words with explicit carry.
        // Signed ADD stays inline as A+X below.
        if (instr->u.binary.op == TAC_BINARY_ADD_UNSIGNED) {
            emit_binop_helper(block, tail, f, src1, src2, "b/uadd", rd, od);
            break;
        }

        // Unsigned subtract has the same exponent-field hazard as unsigned add: A-X
        // misreads the data in bits 48-42.  The b/usub helper does true 48-bit modular
        // subtract.  Signed SUBTRACT stays inline as A-X below.
        if (instr->u.binary.op == TAC_BINARY_SUBTRACT_UNSIGNED) {
            emit_binop_helper(block, tail, f, src1, src2, "b/usub", rd, od);
            break;
        }

        // Shifts are logical for int and unsigned alike (right-shift does no sign
        // extension), so all three shift ops reduce to "left" or "right".  Constant
        // counts inline an ASN; variable counts call b/lsh / b/rsh.
        if (instr->u.binary.op == TAC_BINARY_LEFT_SHIFT) {
            emit_shift(block, tail, f, src1, src2, /*left=*/true, rd, od);
            break;
        }
        if (instr->u.binary.op == TAC_BINARY_RIGHT_SHIFT ||
            instr->u.binary.op == TAC_BINARY_RIGHT_SHIFT_LOGICAL) {
            emit_shift(block, tail, f, src1, src2, /*left=*/false, rd, od);
            break;
        }

        // Bitwise and/or/xor map directly to the logical instructions AAX/AOX/AEX,
        // which act on raw 48-bit words with no normalization (same shape as ADD/SUB).
        // The result is correct for both signed (exponent field = 0) and unsigned
        // (full 48-bit) operands, so no signedness distinction is needed.
        Besm_InstrKind op_kind;
        switch (instr->u.binary.op) {
        case TAC_BINARY_ADD:         op_kind = BESM_ARITH_ADD; break;
        case TAC_BINARY_SUBTRACT:    op_kind = BESM_ARITH_SUB; break;
        case TAC_BINARY_BITWISE_AND: op_kind = BESM_LOG_AAX;   break;
        case TAC_BINARY_BITWISE_OR:  op_kind = BESM_LOG_AOX;   break;
        case TAC_BINARY_BITWISE_XOR: op_kind = BESM_LOG_AEX;   break;
        default:
            fatal_error("TODO: binary op %d (Phase B)", (int)instr->u.binary.op);
        }
        emit_xta_val(block, tail, f, src1);
        emit_arith_val(block, tail, op_kind, f, src2);
        emit_atx(block, tail, rd, od);
        break;
    }
    // FUN_CALL  [dst =] fun(args...)
    //
    // BESM-6 sequence (N args):
    //   ,XTA, arg0         — load first arg into A (=N for integer constants)
    //   ,XTS, arg1 ... argN-1  — push each subsequent arg (XTS: stack←A, A←argI)
    //   14 ,VTM, -N        — set r14 = -N (negative arg count); omitted if N=0
    //   ,CALL, fun_name    — call; r13 ← return address
    //   reg ,ATX, off      — store result (A) into dst frame slot, if dst present
    case TAC_INSTRUCTION_FUN_CALL: {
        const char    *fun_name = instr->u.fun_call.fun_name;
        const Tac_Val *args     = instr->u.fun_call.args;
        const Tac_Val *dst      = instr->u.fun_call.dst;

        int nargs = 0;
        for (const Tac_Val *a = args; a; a = a->next)
            nargs++;

        if (nargs > 0) {
            emit_xta_val(block, tail, f, args);
            for (const Tac_Val *a = args->next; a; a = a->next)
                emit_xts_val(block, tail, f, a);
            Besm_Instr *vtm = emit(block, tail, BESM_REG_VTM);
            vtm->reg        = REG_CNT;
            vtm->addr       = -nargs;
        }

        Besm_Instr *call = emit(block, tail, BESM_BRANCH_CALL);
        call->name       = xstrdup(fun_name);

        if (dst && dst->kind == TAC_VAL_VAR) {
            int dr, doff;
            lookup(f, dst->u.var_name, &dr, &doff);
            emit_atx(block, tail, dr, doff);
        }
        break;
    }
    // RETURN  [src]
    //
    // Load return value into A (if any), then jump to the return label.
    // Void functions do not emit this instruction; non-void functions emit it
    // before the unconditional epilogue jump (the duplicate UJ is dead code).
    case TAC_INSTRUCTION_RETURN: {
        const Tac_Val *src = instr->u.return_.src;
        if (src)
            emit_xta_val(block, tail, f, src);
        Besm_Instr *uj = emit(block, tail, BESM_BRANCH_UJ);
        uj->name       = xstrdup("b/ret");
        break;
    }
    // LABEL  name:
    //
    // Emit an inline label definition point.  No storage is allocated.
    case TAC_INSTRUCTION_LABEL: {
        Besm_Instr *lbl = emit(block, tail, BESM_STMT_LABEL);
        lbl->name       = xstrdup(instr->u.label.name);
        break;
    }
    // JUMP  target
    //
    // Unconditional branch: ,UJ, target.
    case TAC_INSTRUCTION_JUMP: {
        Besm_Instr *uj = emit(block, tail, BESM_BRANCH_UJ);
        uj->name       = xstrdup(instr->u.jump.target);
        break;
    }
    // JUMP_IF_ZERO  condition → target
    //
    // XTA sets logical ω from the loaded value (ω=0 iff A=0).
    // UZA branches when ω=0 (condition is zero).  No NTR needed.
    case TAC_INSTRUCTION_JUMP_IF_ZERO: {
        emit_xta_val(block, tail, f, instr->u.jump_if_zero.condition);
        Besm_Instr *uza = emit(block, tail, BESM_BRANCH_UZA);
        uza->name       = xstrdup(instr->u.jump_if_zero.target);
        break;
    }
    // JUMP_IF_NOT_ZERO  condition → target
    //
    // Same as above but U1A branches when ω≠0 (condition is non-zero).
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO: {
        emit_xta_val(block, tail, f, instr->u.jump_if_not_zero.condition);
        Besm_Instr *u1a = emit(block, tail, BESM_BRANCH_U1A);
        u1a->name       = xstrdup(instr->u.jump_if_not_zero.target);
        break;
    }
    default:
        fatal_error("TODO: codegen for TAC instruction kind %d (Phase B)", (int)instr->kind);
    }
}
