#include "tac_export.h"

#include <stdlib.h>

#include "wio.h"

int export_tac_debug;

static void export_const(WFILE *out, const Tac_Const *c)
{
    if (!c) {
        wputw(0, out);
        return;
    }
    wputw(1, out);
    wputw((size_t)c->kind, out);
    switch (c->kind) {
    case TAC_CONST_INT:
        wputw((unsigned)c->u.int_val, out);
        break;
    case TAC_CONST_LONG:
        wputw((size_t)c->u.long_val, out);
        break;
    case TAC_CONST_LONG_LONG:
        wputw((size_t)(unsigned long long)c->u.long_long_val, out);
        break;
    case TAC_CONST_UINT:
        wputw((size_t)c->u.uint_val, out);
        break;
    case TAC_CONST_ULONG:
        wputw((size_t)c->u.ulong_val, out);
        break;
    case TAC_CONST_ULONG_LONG:
        wputw((size_t)c->u.ulong_long_val, out);
        break;
    case TAC_CONST_DOUBLE:
        wputd(c->u.double_val, out);
        break;
    case TAC_CONST_CHAR:
        wputw((unsigned)c->u.char_val, out);
        break;
    case TAC_CONST_UCHAR:
        wputw((unsigned)c->u.uchar_val, out);
        break;
    default:
        break;
    }
}

static void export_val(WFILE *out, const Tac_Val *v)
{
    if (!v) {
        wputw(0, out);
        return;
    }
    wputw(1, out);
    wputw((size_t)v->kind, out);
    if (v->kind == TAC_VAL_CONSTANT) {
        export_const(out, v->u.constant);
    } else {
        wputstr(v->u.var_name ? v->u.var_name : "", out);
    }
    export_val(out, v->next);
}

static void export_instr(WFILE *out, const Tac_Instruction *instr)
{
    if (!instr) {
        wputw(0, out);
        return;
    }
    wputw(1, out);
    wputw((size_t)instr->kind, out);
    switch (instr->kind) {
    case TAC_INSTRUCTION_RETURN:
        export_val(out, instr->u.return_.src);
        break;
    case TAC_INSTRUCTION_SIGN_EXTEND:
    case TAC_INSTRUCTION_TRUNCATE:
    case TAC_INSTRUCTION_ZERO_EXTEND:
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
        export_val(out, instr->u.sign_extend.src);
        export_val(out, instr->u.sign_extend.dst);
        break;
    case TAC_INSTRUCTION_UNARY:
        wputw((size_t)instr->u.unary.op, out);
        export_val(out, instr->u.unary.src);
        export_val(out, instr->u.unary.dst);
        break;
    case TAC_INSTRUCTION_BINARY:
        wputw((size_t)instr->u.binary.op, out);
        export_val(out, instr->u.binary.src1);
        export_val(out, instr->u.binary.src2);
        export_val(out, instr->u.binary.dst);
        break;
    case TAC_INSTRUCTION_COPY:
    case TAC_INSTRUCTION_GET_ADDRESS:
        export_val(out, instr->u.copy.src);
        export_val(out, instr->u.copy.dst);
        break;
    case TAC_INSTRUCTION_LOAD:
        export_val(out, instr->u.load.src_ptr);
        export_val(out, instr->u.load.dst);
        break;
    case TAC_INSTRUCTION_STORE:
        export_val(out, instr->u.store.src);
        export_val(out, instr->u.store.dst_ptr);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        export_val(out, instr->u.add_ptr.ptr);
        export_val(out, instr->u.add_ptr.index);
        wputw((size_t)instr->u.add_ptr.scale, out);
        export_val(out, instr->u.add_ptr.dst);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
        export_val(out, instr->u.copy_to_offset.src);
        wputstr(instr->u.copy_to_offset.dst ? instr->u.copy_to_offset.dst : "", out);
        wputw((size_t)instr->u.copy_to_offset.offset, out);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
        wputstr(instr->u.copy_from_offset.src ? instr->u.copy_from_offset.src : "", out);
        wputw((size_t)instr->u.copy_from_offset.offset, out);
        export_val(out, instr->u.copy_from_offset.dst);
        break;
    case TAC_INSTRUCTION_JUMP:
        wputstr(instr->u.jump.target ? instr->u.jump.target : "", out);
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        export_val(out, instr->u.jump_if_zero.condition);
        wputstr(instr->u.jump_if_zero.target ? instr->u.jump_if_zero.target : "", out);
        break;
    case TAC_INSTRUCTION_LABEL:
        wputstr(instr->u.label.name ? instr->u.label.name : "", out);
        break;
    case TAC_INSTRUCTION_FUN_CALL:
        wputstr(instr->u.fun_call.fun_name ? instr->u.fun_call.fun_name : "", out);
        export_val(out, instr->u.fun_call.args);
        export_val(out, instr->u.fun_call.dst);
        break;
    default:
        break;
    }
    export_instr(out, instr->next);
}

static void export_param(WFILE *out, const Tac_Param *p)
{
    if (!p) {
        wputw(0, out);
        return;
    }
    wputw(1, out);
    wputstr(p->name ? p->name : "", out);
    export_param(out, p->next);
}

void tac_export_begin_stream(WFILE *out)
{
    wputw(TAC_TAG_STREAM, out);
}

void tac_export_toplevel(WFILE *out, const Tac_TopLevel *tl)
{
    if (!tl) {
        wputw(0, out);
        return;
    }
    wputw(1, out);
    wputw((size_t)tl->kind, out);
    switch (tl->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        wputstr(tl->u.function.name ? tl->u.function.name : "", out);
        wputw(tl->u.function.global ? 1 : 0, out);
        export_param(out, tl->u.function.params);
        export_instr(out, tl->u.function.body);
        break;
    case TAC_TOPLEVEL_STATIC_VARIABLE:
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        /* Not produced by current translator */
        wputw(0, out);
        break;
    default:
        break;
    }
}

void tac_export_end_stream(WFILE *out)
{
    wputw(0, out);
    wflush(out);
}
