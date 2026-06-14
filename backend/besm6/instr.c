#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "abi.h"
#include "besm.h"
#include "frame.h"
#include "internal.h"
#include "tac.h"
#include "xalloc.h"

// Convert a struct-member byte offset to a word offset.  BESM-6 word-sized members
// (int/long/pointer/float/double) are 6 bytes, so a word-aligned member's byte offset
// is a multiple of 6.  A sub-word offset means a packed char member (task #21).
static int member_word_offset(int byte_offset)
{
    if (byte_offset % BESM6_WORD_BYTES != 0)
        fatal_error("CopyTo/FromOffset: sub-word offset %d (char member — task #21)",
                    byte_offset);
    return byte_offset / BESM6_WORD_BYTES;
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

// True if `name` is a module-level array object.  An array variable decays to the
// address of its label, whereas a pointer variable holds an address in its storage —
// the TAC carries the bare name in both cases, so ADD_PTR must distinguish them here.
static bool global_is_array(const Tac_TopLevel *program, const char *name)
{
    for (const Tac_TopLevel *tl = program; tl; tl = tl->next)
        if (tl->kind == TAC_TOPLEVEL_STATIC_VARIABLE &&
            strcmp(tl->u.static_variable.name, name) == 0)
            return tl->u.static_variable.type->kind == TAC_TYPE_ARRAY;
    return false;
}

void codegen_instr(const Tac_TopLevel *program, const Tac_Instruction *instr,
                   const Frame *f, Besm_Block *block, Besm_Instr **tail)
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
    // The source may be a frame var, a global, or a constant (e.g. arr[i] = 5 after
    // the optimizer folds the value), so it is loaded via emit_xta_val.
    case TAC_INSTRUCTION_STORE: {
        int pr, po;
        lookup(f, instr->u.store.dst_ptr->u.var_name, &pr, &po);
        emit_xta(block, tail, pr, po);
        Besm_Instr *ati = emit(block, tail, BESM_MEM_ATI);
        ati->addr       = 1;
        emit_xta_val(block, tail, f, instr->u.store.src);
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

        // Multiply uses the b/mul runtime helper (the inline A*X needs FP normalization and
        // INT-format bridging, which the helper encapsulates).  Correct for signed operands.
        if (instr->u.binary.op == TAC_BINARY_MULTIPLY) {
            emit_binop_helper(block, tail, f, src1, src2, "b/mul", rd, od);
            break;
        }

        // Unsigned multiply uses b/umul, which forms the full 48-bit (low) product over the
        // entire unsigned range via operand splitting, without the signed 41-bit truncation
        // b/mul applies.
        if (instr->u.binary.op == TAC_BINARY_MULTIPLY_UNSIGNED) {
            emit_binop_helper(block, tail, f, src1, src2, "b/umul", rd, od);
            break;
        }

        // Signed divide and remainder use the b/div / b/mod runtime helpers, which bridge
        // raw operands to INT-format, FP-divide the absolute values, correct the exponent
        // and reapply the sign (b/mod = a - (a/b)*b).  Correct for signed operands and for
        // unsigned within the 41-bit range; full 48-bit unsigned divide/remainder use
        // b/udiv / b/umod below.
        if (instr->u.binary.op == TAC_BINARY_DIVIDE) {
            emit_binop_helper(block, tail, f, src1, src2, "b/div", rd, od);
            break;
        }
        if (instr->u.binary.op == TAC_BINARY_REMAINDER) {
            emit_binop_helper(block, tail, f, src1, src2, "b/mod", rd, od);
            break;
        }

        // Unsigned divide uses b/udiv.  The signed b/div borrows the hardware FP unit
        // (40-bit mantissa, bit 48 read as a sign), so it is wrong for unsigned operands
        // >= 2^40 or with bit 48 set.  b/udiv instead does an integer long division over
        // the full 48-bit word (divisor-shift / subtract loop).
        if (instr->u.binary.op == TAC_BINARY_DIVIDE_UNSIGNED) {
            emit_binop_helper(block, tail, f, src1, src2, "b/udiv", rd, od);
            break;
        }

        // Unsigned remainder uses b/umod.  The signed b/mod shares b/div's FP bridge and so
        // is wrong for the same out-of-range unsigned operands.  b/umod computes the full
        // 48-bit residue as a - (a/b)*b, reusing b/udiv / b/umul / b/usub.
        if (instr->u.binary.op == TAC_BINARY_REMAINDER_UNSIGNED) {
            emit_binop_helper(block, tail, f, src1, src2, "b/umod", rd, od);
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
    // Integer width conversions (task #17).
    //
    // Under the BESM-6 target (semantic/target.c) short/int/long/pointer are all one
    // 48-bit word, so emit_cast only emits TRUNCATE/ZERO_EXTEND/SIGN_EXTEND when one
    // side is char (1 byte = 8 bits); same-size conversions become a COPY.  Therefore the
    // narrow side is always char and the mask is always 8-bit.  (The only other size
    // mismatch is the 2-word long long / long double types — deferred to task #24, which
    // still fall through to default below.)
    //
    // TRUNCATE  dst = (narrow)src   — keep the low 8 bits.
    // ZERO_EXTEND dst = (wider unsigned)src — same: clear all but the low 8 bits.
    case TAC_INSTRUCTION_TRUNCATE:
    case TAC_INSTRUCTION_ZERO_EXTEND: {
        const Tac_Val *src = (instr->kind == TAC_INSTRUCTION_TRUNCATE)
                                 ? instr->u.truncate.src
                                 : instr->u.zero_extend.src;
        const Tac_Val *dst = (instr->kind == TAC_INSTRUCTION_TRUNCATE)
                                 ? instr->u.truncate.dst
                                 : instr->u.zero_extend.dst;
        int rd, od;
        lookup(f, dst->u.var_name, &rd, &od);
        emit_xta_val(block, tail, f, src);
        Besm_Instr *aax = emit(block, tail, BESM_LOG_AAX);
        aax->name       = xstrdup("=377"); // mask low 8 bits
        emit_atx(block, tail, rd, od);
        break;
    }
    // SIGN_EXTEND  dst = (wider signed)src   — signed char → wider.
    //
    // Branchless 8-bit sign extension via the (x ^ 0x80) - 0x80 trick: after masking to 8
    // bits, flipping bit 7 and subtracting 0x80 reinterprets the value as signed 8-bit.
    // Integer AEX/A-X act on raw words under R=7, so no NTR is needed.  The leading AAX
    // discards any high garbage bits before the sign is reconstructed from bit 7.
    case TAC_INSTRUCTION_SIGN_EXTEND: {
        const Tac_Val *src = instr->u.sign_extend.src;
        const Tac_Val *dst = instr->u.sign_extend.dst;
        int rd, od;
        lookup(f, dst->u.var_name, &rd, &od);
        emit_xta_val(block, tail, f, src);
        Besm_Instr *aax = emit(block, tail, BESM_LOG_AAX);
        aax->name       = xstrdup("=377"); // mask low 8 bits
        Besm_Instr *aex = emit(block, tail, BESM_LOG_AEX);
        aex->name       = xstrdup("=200"); // flip sign bit (0x80)
        Besm_Instr *sub = emit(block, tail, BESM_ARITH_SUB);
        sub->name       = xstrdup("=200"); // subtract 0x80
        emit_atx(block, tail, rd, od);
        break;
    }
    // ADD_PTR  dst = ptr + index * scale
    //
    // `scale` is the element size in BYTES (e.g. int → 6 = one word).  BESM-6 is
    // word-addressed, so convert to a word scale (scale / 6) and scale the index by it:
    //   word_scale == 1   : plain add
    //   word_scale == 2^k : ASN by k bits on the index (logical left shift)
    //   otherwise         : b/mul the index by word_scale
    //
    // The base addition differs by operand kind.  An array variable's value IS the address
    // of its label, while a pointer variable holds an address in its storage; the TAC
    // carries the bare name in both cases (no explicit decay), so global_is_array decides:
    //   global array : materialize the label address and fold in the scaled index via the
    //                  index register on UTC:  ,ATI, 1 / 1 ,UTC, arr / 14 ,VTM, / ,ITA, 14
    //   pointer      : the pointer's stored value is the base address — A+X ptr
    //
    // Byte-offset addressing (scale not a whole number of words: struct members, char
    // arrays) is deferred to tasks #20/#21.
    case TAC_INSTRUCTION_ADD_PTR: {
        const Tac_Val *ptr   = instr->u.add_ptr.ptr;
        const Tac_Val *index = instr->u.add_ptr.index;
        const Tac_Val *dst   = instr->u.add_ptr.dst;
        int scale            = instr->u.add_ptr.scale;
        if (scale % 6 != 0)
            fatal_error("ADD_PTR: sub-word scale %d (struct member / char* — tasks #20/#21)",
                        scale);
        int word_scale = scale / 6;
        int rd, od;
        lookup(f, dst->u.var_name, &rd, &od);

        // (a) scaled index in A.
        emit_xta_val(block, tail, f, index);
        if (word_scale != 1) {
            if ((word_scale & (word_scale - 1)) == 0) {
                int k = 0;
                while ((1 << k) < word_scale)
                    k++;
                Besm_Instr *asn = emit(block, tail, BESM_EXP_SHIFTN);
                asn->addr       = 64 - k; // logical left shift by k bits
            } else {
                // Runtime-helper multiply: push the index, load =word_scale, call b/mul.
                Besm_Instr *xts = emit(block, tail, BESM_MEM_XTS);
                char buf[32];
                snprintf(buf, sizeof(buf), "=%o", word_scale);
                xts->name        = xstrdup(buf);
                Besm_Instr *call = emit(block, tail, BESM_BRANCH_CALL);
                call->name       = xstrdup("b/mul");
            }
        }

        // (b) add the base.
        bool array_base = false;
        if (ptr->kind == TAC_VAL_VAR) {
            int pr, po;
            if (!frame_lookup(f, ptr->u.var_name, &pr, &po) &&
                global_is_array(program, ptr->u.var_name))
                array_base = true;
        }
        if (array_base) {
            Besm_Instr *ati = emit(block, tail, BESM_MEM_ATI);
            ati->addr       = 1; // M[1] = scaled index
            Besm_Instr *utc = emit(block, tail, BESM_MOD_UTC);
            utc->reg        = 1;
            utc->name       = xstrdup(ptr->u.var_name); // C = M[1] + addr(arr)
            Besm_Instr *vtm = emit(block, tail, BESM_REG_VTM);
            vtm->reg        = 14; // M[14] = C
            Besm_Instr *ita = emit(block, tail, BESM_MEM_ITA);
            ita->addr       = 14; // A = M[14] = element word address
        } else {
            emit_arith_val(block, tail, BESM_ARITH_ADD, f, ptr);
        }
        emit_atx(block, tail, rd, od);
        break;
    }
    // COPY_FROM_OFFSET  dst = base[offset]
    //
    // In C:  x = s.field;   (s is a named struct variable, not a pointer)
    // TAC:   copy_from_offset src=base, offset → dst
    //
    // `offset` is a byte offset; convert to a word offset.  The base is a local frame
    // slot (REG_AUTO/REG_PAR + slot offset) or a module-level global:
    //   local  : reg ,XTA, slot_off + woff      — load the member word
    //   global : ,UTC, base  /  ,XTA, woff      — C = addr(base), then mem[C+woff]
    // Then store A into the dst Val (local frame slot or global), as in COPY.
    //
    case TAC_INSTRUCTION_COPY_FROM_OFFSET: {
        const char    *base = instr->u.copy_from_offset.src;
        const Tac_Val *dst  = instr->u.copy_from_offset.dst;
        int            woff = member_word_offset(instr->u.copy_from_offset.offset);
        int br, bo;
        if (frame_lookup(f, base, &br, &bo)) {
            emit_xta(block, tail, br, bo + woff);
        } else {
            Besm_Instr *utc = emit(block, tail, BESM_MOD_UTC);
            utc->name       = xstrdup(base);
            emit_xta(block, tail, 0, woff);
        }
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
    // COPY_TO_OFFSET  base[offset] = src
    //
    // In C:  s.field = x;   (s is a named struct variable, not a pointer)
    // TAC:   copy_to_offset src → dst=base, offset
    //
    // Load src into A (local/global/constant), then store at the word offset into base:
    //   local  : reg ,ATX, slot_off + woff
    //   global : ,UTC, base  /  ,ATX, woff
    //
    case TAC_INSTRUCTION_COPY_TO_OFFSET: {
        const Tac_Val *src  = instr->u.copy_to_offset.src;
        const char    *base = instr->u.copy_to_offset.dst;
        int            woff = member_word_offset(instr->u.copy_to_offset.offset);
        emit_xta_val(block, tail, f, src);
        int br, bo;
        if (frame_lookup(f, base, &br, &bo)) {
            emit_atx(block, tail, br, bo + woff);
        } else {
            Besm_Instr *utc = emit(block, tail, BESM_MOD_UTC);
            utc->name       = xstrdup(base);
            emit_atx(block, tail, 0, woff);
        }
        break;
    }
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        // Frame-slot reservation only; the slot is sized in frame_build and the
        // prologue's stack extension covers it. No runtime instruction is emitted.
        break;
    default:
        fatal_error("TODO: codegen for TAC instruction kind %d (Phase B)", (int)instr->kind);
    }
}
