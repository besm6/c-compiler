#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "abi.h"
#include "besm.h"
#include "codegen.h"
#include "frame.h"
#include "xalloc.h"

// Forward declarations.
static void codegen_function(const Tac_TopLevel *tl, FILE *out);
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

// Extract the integer value from a TAC constant (integer and char kinds only).
static long long get_const_int_val(const Tac_Const *c)
{
    switch (c->kind) {
    case TAC_CONST_INT:        return c->u.int_val;
    case TAC_CONST_LONG:       return c->u.long_val;
    case TAC_CONST_LONG_LONG:  return c->u.long_long_val;
    case TAC_CONST_UINT:       return (long long)c->u.uint_val;
    case TAC_CONST_ULONG:      return (long long)c->u.ulong_val;
    case TAC_CONST_ULONG_LONG: return (long long)c->u.ulong_long_val;
    case TAC_CONST_CHAR:       return c->u.char_val;
    case TAC_CONST_UCHAR:      return c->u.uchar_val;
    default:
        fatal_error("TODO: float constant in function-call argument");
    }
}

// Emit XTA for a TAC value: variable from frame, or octal literal =N for a constant.
static void emit_xta_val(Besm_Block *b, Besm_Instr **t, const Frame *f, const Tac_Val *v)
{
    if (v->kind == TAC_VAL_VAR) {
        int reg, off;
        lookup(f, v->u.var_name, &reg, &off);
        emit_xta(b, t, reg, off);
    } else {
        long long ival = get_const_int_val(v->u.constant);
        char name[32];
        snprintf(name, sizeof(name), "=%llo", (unsigned long long)ival);
        Besm_Instr *i = emit(b, t, BESM_MEM_XTA);
        i->name       = xstrdup(name);
    }
}

// Emit XTS for a TAC value (push A to stack, load v into A) — used for args 1..N-1.
static void emit_xts_val(Besm_Block *b, Besm_Instr **t, const Frame *f, const Tac_Val *v)
{
    if (v->kind == TAC_VAL_VAR) {
        int reg, off;
        lookup(f, v->u.var_name, &reg, &off);
        Besm_Instr *i = emit(b, t, BESM_MEM_XTS);
        i->reg        = reg;
        i->addr       = off;
    } else {
        long long ival = get_const_int_val(v->u.constant);
        char name[32];
        snprintf(name, sizeof(name), "=%llo", (unsigned long long)ival);
        Besm_Instr *i = emit(b, t, BESM_MEM_XTS);
        i->name       = xstrdup(name);
    }
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
            default:
                fatal_error("TODO: non-integer static init (Phase C)");
            }
            *tail = item;
            tail  = &item->next;
        }
    }

    emit_madlen_module(out, module);
    besm_free_module(module);
}

void codegen_program(const Tac_TopLevel *tl, FILE *out)
{
    switch (tl->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        codegen_function(tl, out);
        break;
    case TAC_TOPLEVEL_STATIC_VARIABLE:
        codegen_static_variable(tl, out);
        break;
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        fatal_error("TODO: static constant (Phase C)");
    }
}

static void codegen_function(const Tac_TopLevel *tl, FILE *out)
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
        Besm_Instr *subp_cret = emit(block, &tail, BESM_STMT_SUBP);
        subp_cret->name       = xstrdup("b/ret");

        Besm_Instr *its13 = emit(block, &tail, BESM_MEM_ITS);
        its13->addr       = REG_RET;

        Besm_Instr *call_csave = emit(block, &tail, BESM_BRANCH_CALL);
        call_csave->name       = xstrdup(num_params == 0 ? "b/save0" : "b/save");

        Frame *f      = frame_build(tl);
        int num_autos = frame_num_autos(f);
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
    // TAC:   copy src → dst   (both src and dst are TAC_VAL_VAR frame slots)
    //
    // BESM-6 sequence:
    //   reg_src ,XTA, off_src   — load src from its frame slot into A
    //   reg_dst ,ATX, off_dst   — store A into dst's frame slot
    //
    // frame_lookup resolves each name to (reg, offset) where reg is REG_PAR (r6)
    // for function parameters or REG_AUTO (r7) for local variables.
    // The XTA/ATX pair works uniformly regardless of which base register is used.
    //
    // COPY from a constant (e.g. b = 0) requires INT-format encoding and is
    // deferred to task #21.
    case TAC_INSTRUCTION_COPY: {
        const Tac_Val *src = instr->u.copy.src;
        const Tac_Val *dst = instr->u.copy.dst;
        if (src->kind != TAC_VAL_VAR)
            fatal_error("TODO: COPY from constant (Phase B task 21)");
        int sr, so, dr, doff;
        lookup(f, src->u.var_name, &sr, &so);
        lookup(f, dst->u.var_name, &dr, &doff);
        emit_xta(block, tail, sr, so);
        emit_atx(block, tail, dr, doff);
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
        int sr, so, dr, doff;
        lookup(f, instr->u.get_address.src->u.var_name, &sr, &so);
        lookup(f, instr->u.get_address.dst->u.var_name, &dr, &doff);
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
    // BINARY  dst = src1 op src2
    //
    // For integer add/subtract, R=7 after b/save suppresses normalization and
    // rounding, so the arithmetic instructions work directly on INT-format values.
    // No NTR is needed.
    //
    // BESM-6 sequence:
    //   reg_src1 ,XTA, off_src1   — load src1 from its frame slot into A
    //   reg_src2 ,A+X, off_src2   — A = A + src2  (A-X for subtract)
    //   reg_dst  ,ATX, off_dst    — store A into dst's frame slot
    //
    // Constants as operands need INT-format encoding; deferred to task #21.
    case TAC_INSTRUCTION_BINARY: {
        const Tac_Val *src1 = instr->u.binary.src1;
        const Tac_Val *src2 = instr->u.binary.src2;
        const Tac_Val *dst  = instr->u.binary.dst;
        if (src1->kind != TAC_VAL_VAR || src2->kind != TAC_VAL_VAR)
            fatal_error("TODO: BINARY from constant (Phase B task 21)");
        int r1, o1, r2, o2, rd, od;
        lookup(f, src1->u.var_name, &r1, &o1);
        lookup(f, src2->u.var_name, &r2, &o2);
        lookup(f, dst->u.var_name,  &rd, &od);
        Besm_InstrKind op_kind;
        switch (instr->u.binary.op) {
        case TAC_BINARY_ADD:      op_kind = BESM_ARITH_ADD; break;
        case TAC_BINARY_SUBTRACT: op_kind = BESM_ARITH_SUB; break;
        default:
            fatal_error("TODO: binary op %d (Phase B)", (int)instr->u.binary.op);
        }
        emit_xta(block, tail, r1, o1);
        emit_arith(block, tail, op_kind, r2, o2);
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
    default:
        fatal_error("TODO: codegen for TAC instruction kind %d (Phase B)", (int)instr->kind);
    }
}
