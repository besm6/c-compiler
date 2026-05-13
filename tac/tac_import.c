#include "tac.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wio.h"
#include "xalloc.h"

static void check_input(const WFILE *in, const char *ctx)
{
    if (weof(in)) {
        fprintf(stderr, "Error: premature EOF reading %s\n", ctx);
        exit(1);
    }
    if (werror(in)) {
        fprintf(stderr, "Error: I/O error reading %s\n", ctx);
        exit(1);
    }
}

static Tac_Const *import_const(WFILE *in)
{
    size_t present = wgetw(in);
    check_input(in, "const present");
    if (!present)
        return NULL;
    size_t kind = wgetw(in);
    check_input(in, "const kind");
    Tac_Const *c = tac_new_const((Tac_ConstKind)kind);
    switch (c->kind) {
    case TAC_CONST_INT:
        c->u.int_val = (int)wgetw(in);
        check_input(in, "const int");
        break;
    case TAC_CONST_LONG:
        c->u.long_val = (long)wgetw(in);
        check_input(in, "const long");
        break;
    case TAC_CONST_LONG_LONG:
        c->u.long_long_val = (long long)wgetw(in);
        check_input(in, "const long long");
        break;
    case TAC_CONST_UINT:
        c->u.uint_val = (unsigned int)wgetw(in);
        check_input(in, "const uint");
        break;
    case TAC_CONST_ULONG:
        c->u.ulong_val = (unsigned long)wgetw(in);
        check_input(in, "const ulong");
        break;
    case TAC_CONST_ULONG_LONG:
        c->u.ulong_long_val = (unsigned long long)wgetw(in);
        check_input(in, "const ulong_long");
        break;
    case TAC_CONST_DOUBLE:
        c->u.double_val = wgetd(in);
        check_input(in, "const double");
        break;
    case TAC_CONST_CHAR:
        c->u.char_val = (int)wgetw(in);
        check_input(in, "const char");
        break;
    case TAC_CONST_UCHAR:
        c->u.uchar_val = (unsigned char)wgetw(in);
        check_input(in, "const uchar");
        break;
    default:
        break;
    }
    return c;
}

static Tac_Val *import_val(WFILE *in)
{
    size_t present = wgetw(in);
    check_input(in, "val present");
    if (!present)
        return NULL;
    size_t kind = wgetw(in);
    check_input(in, "val kind");
    Tac_Val *v = tac_new_val((Tac_ValKind)kind);
    if (v->kind == TAC_VAL_CONSTANT) {
        v->u.constant = import_const(in);
    } else {
        v->u.var_name = wgetstr(in);
        check_input(in, "val var_name");
    }
    v->next = import_val(in);
    return v;
}

static Tac_Type *import_type(WFILE *in);

static Tac_Type *import_type(WFILE *in)
{
    size_t present = wgetw(in);
    check_input(in, "type present");
    if (!present)
        return NULL;
    size_t kind = wgetw(in);
    check_input(in, "type kind");
    Tac_Type *t = tac_new_type((Tac_TypeKind)kind);
    switch (t->kind) {
    case TAC_TYPE_CHAR:
    case TAC_TYPE_SCHAR:
    case TAC_TYPE_UCHAR:
    case TAC_TYPE_SHORT:
    case TAC_TYPE_INT:
    case TAC_TYPE_LONG:
    case TAC_TYPE_LONG_LONG:
    case TAC_TYPE_USHORT:
    case TAC_TYPE_UINT:
    case TAC_TYPE_ULONG:
    case TAC_TYPE_ULONG_LONG:
    case TAC_TYPE_FLOAT:
    case TAC_TYPE_DOUBLE:
    case TAC_TYPE_VOID:
        break;
    case TAC_TYPE_FUN_TYPE:
        t->u.fun_type.param_types = import_type(in);
        t->u.fun_type.ret_type    = import_type(in);
        break;
    case TAC_TYPE_POINTER:
        t->u.pointer.target_type = import_type(in);
        break;
    case TAC_TYPE_ARRAY:
        t->u.array.elem_type = import_type(in);
        t->u.array.size      = (int)wgetw(in);
        check_input(in, "array size");
        break;
    case TAC_TYPE_STRUCTURE:
        t->u.structure.tag = wgetstr(in);
        check_input(in, "structure tag");
        break;
    default:
        break;
    }
    t->next = import_type(in);
    return t;
}

static Tac_StaticInit *import_static_init(WFILE *in)
{
    size_t present = wgetw(in);
    check_input(in, "static_init present");
    if (!present)
        return NULL;
    size_t kind = wgetw(in);
    check_input(in, "static_init kind");
    Tac_StaticInit *si = tac_new_static_init((Tac_StaticInitKind)kind);
    switch (si->kind) {
    case TAC_STATIC_INIT_I8:
        si->u.char_val = (int8_t)wgetw(in);
        check_input(in, "static_init i8");
        break;
    case TAC_STATIC_INIT_I32:
        si->u.int_val = (int32_t)wgetw(in);
        check_input(in, "static_init i32");
        break;
    case TAC_STATIC_INIT_I64:
        si->u.long_val = (int64_t)wgetw(in);
        check_input(in, "static_init i64");
        break;
    case TAC_STATIC_INIT_U8:
        si->u.uchar_val = (uint8_t)wgetw(in);
        check_input(in, "static_init u8");
        break;
    case TAC_STATIC_INIT_U32:
        si->u.uint_val = (uint32_t)wgetw(in);
        check_input(in, "static_init u32");
        break;
    case TAC_STATIC_INIT_U64:
        si->u.ulong_val = (uint64_t)wgetw(in);
        check_input(in, "static_init u64");
        break;
    case TAC_STATIC_INIT_DOUBLE:
        si->u.double_val = wgetd(in);
        check_input(in, "static_init double");
        break;
    case TAC_STATIC_INIT_ZERO:
        si->u.zero_bytes = (int)wgetw(in);
        check_input(in, "static_init zero");
        break;
    case TAC_STATIC_INIT_STRING:
        si->u.string.val            = wgetstr(in);
        check_input(in, "static_init string val");
        si->u.string.null_terminated = (bool)wgetw(in);
        check_input(in, "static_init string null_terminated");
        break;
    case TAC_STATIC_INIT_POINTER:
        si->u.pointer_name = wgetstr(in);
        check_input(in, "static_init pointer name");
        break;
    default:
        break;
    }
    si->next = import_static_init(in);
    return si;
}

static Tac_Param *import_param(WFILE *in)
{
    size_t present = wgetw(in);
    check_input(in, "param present");
    if (!present)
        return NULL;
    Tac_Param *p = tac_new_param();
    p->name      = wgetstr(in);
    check_input(in, "param name");
    p->next = import_param(in);
    return p;
}

static Tac_Instruction *import_instr(WFILE *in)
{
    size_t present = wgetw(in);
    check_input(in, "instr present");
    if (!present)
        return NULL;
    size_t kind = wgetw(in);
    check_input(in, "instr kind");
    Tac_Instruction *instr = tac_new_instruction((Tac_InstructionKind)kind);
    switch (instr->kind) {
    case TAC_INSTRUCTION_RETURN:
        instr->u.return_.src = import_val(in);
        break;
    case TAC_INSTRUCTION_SIGN_EXTEND:
    case TAC_INSTRUCTION_TRUNCATE:
    case TAC_INSTRUCTION_ZERO_EXTEND:
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
        instr->u.sign_extend.src = import_val(in);
        instr->u.sign_extend.dst = import_val(in);
        break;
    case TAC_INSTRUCTION_UNARY:
        instr->u.unary.op  = (Tac_UnaryOperator)wgetw(in);
        check_input(in, "unary op");
        instr->u.unary.src = import_val(in);
        instr->u.unary.dst = import_val(in);
        break;
    case TAC_INSTRUCTION_BINARY:
        instr->u.binary.op   = (Tac_BinaryOperator)wgetw(in);
        check_input(in, "binary op");
        instr->u.binary.src1 = import_val(in);
        instr->u.binary.src2 = import_val(in);
        instr->u.binary.dst  = import_val(in);
        break;
    case TAC_INSTRUCTION_COPY:
    case TAC_INSTRUCTION_GET_ADDRESS:
        instr->u.copy.src = import_val(in);
        instr->u.copy.dst = import_val(in);
        break;
    case TAC_INSTRUCTION_LOAD:
        instr->u.load.src_ptr = import_val(in);
        instr->u.load.dst     = import_val(in);
        break;
    case TAC_INSTRUCTION_STORE:
        instr->u.store.src     = import_val(in);
        instr->u.store.dst_ptr = import_val(in);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        instr->u.add_ptr.ptr   = import_val(in);
        instr->u.add_ptr.index = import_val(in);
        instr->u.add_ptr.scale = (int)wgetw(in);
        check_input(in, "add_ptr scale");
        instr->u.add_ptr.dst   = import_val(in);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
        instr->u.copy_to_offset.src    = import_val(in);
        instr->u.copy_to_offset.dst    = wgetstr(in);
        check_input(in, "copy_to_offset dst");
        instr->u.copy_to_offset.offset = (int)wgetw(in);
        check_input(in, "copy_to_offset offset");
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
        instr->u.copy_from_offset.src    = wgetstr(in);
        check_input(in, "copy_from_offset src");
        instr->u.copy_from_offset.offset = (int)wgetw(in);
        check_input(in, "copy_from_offset offset");
        instr->u.copy_from_offset.dst    = import_val(in);
        break;
    case TAC_INSTRUCTION_JUMP:
        instr->u.jump.target = wgetstr(in);
        check_input(in, "jump target");
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        instr->u.jump_if_zero.condition = import_val(in);
        instr->u.jump_if_zero.target    = wgetstr(in);
        check_input(in, "jump_if_zero target");
        break;
    case TAC_INSTRUCTION_LABEL:
        instr->u.label.name = wgetstr(in);
        check_input(in, "label name");
        break;
    case TAC_INSTRUCTION_FUN_CALL:
        instr->u.fun_call.fun_name = wgetstr(in);
        check_input(in, "fun_call fun_name");
        instr->u.fun_call.args = import_val(in);
        instr->u.fun_call.dst  = import_val(in);
        break;
    default:
        break;
    }
    instr->next = import_instr(in);
    return instr;
}

static Tac_TopLevel *import_toplevel(WFILE *in)
{
    size_t present = wgetw(in);
    check_input(in, "toplevel present");
    if (!present)
        return NULL;
    size_t kind = wgetw(in);
    check_input(in, "toplevel kind");
    Tac_TopLevel *tl = tac_new_toplevel((Tac_TopLevelKind)kind);
    switch (tl->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        tl->u.function.name   = wgetstr(in);
        check_input(in, "function name");
        tl->u.function.global = (bool)wgetw(in);
        check_input(in, "function global");
        tl->u.function.params = import_param(in);
        tl->u.function.body   = import_instr(in);
        break;
    case TAC_TOPLEVEL_STATIC_VARIABLE:
        tl->u.static_variable.name      = wgetstr(in);
        check_input(in, "static_variable name");
        tl->u.static_variable.global    = (bool)wgetw(in);
        check_input(in, "static_variable global");
        tl->u.static_variable.type      = import_type(in);
        tl->u.static_variable.init_list = import_static_init(in);
        break;
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        tl->u.static_constant.name = wgetstr(in);
        check_input(in, "static_constant name");
        tl->u.static_constant.type = import_type(in);
        tl->u.static_constant.init = import_static_init(in);
        break;
    default:
        break;
    }
    tl->next = import_toplevel(in);
    return tl;
}

Tac_Program *tac_import_program(WFILE *in)
{
    size_t magic = wgetw(in);
    check_input(in, "stream magic");
    if (magic != TAC_TAG_STREAM) {
        fprintf(stderr, "Error: bad TAC stream magic 0x%zx (expected 0x%x)\n",
                magic, TAC_TAG_STREAM);
        exit(1);
    }
    Tac_Program *prog = tac_new_program();
    prog->decls       = import_toplevel(in);
    return prog;
}
