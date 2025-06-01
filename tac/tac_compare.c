#include <stdbool.h>
#include <string.h>

#include "tac.h"

// Compare two Tac_Const structures
bool compare_tac_const(const Tac_Const *a, const Tac_Const *b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    switch (a->kind) {
    case TAC_CONST_INT:
        return a->u.int_val == b->u.int_val;
    case TAC_CONST_LONG:
        return a->u.long_val == b->u.long_val;
    case TAC_CONST_LONG_LONG:
        return a->u.long_long_val == b->u.long_long_val;
    case TAC_CONST_UINT:
        return a->u.uint_val == b->u.uint_val;
    case TAC_CONST_ULONG:
        return a->u.ulong_val == b->u.ulong_val;
    case TAC_CONST_ULONG_LONG:
        return a->u.ulong_long_val == b->u.ulong_long_val;
    case TAC_CONST_DOUBLE:
        return a->u.double_val == b->u.double_val;
    case TAC_CONST_CHAR:
        return a->u.char_val == b->u.char_val;
    case TAC_CONST_UCHAR:
        return a->u.uchar_val == b->u.uchar_val;
    }
    return false;
}

// Compare two Tac_Val structures recursively
bool compare_tac_val(const Tac_Val *a, const Tac_Val *b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    if (a->kind == TAC_VAL_CONSTANT) {
        if (!compare_tac_const(a->u.constant, b->u.constant))
            return false;
    } else {
        if ((a->u.var_name == NULL) != (b->u.var_name == NULL))
            return false;
        if (a->u.var_name && strcmp(a->u.var_name, b->u.var_name) != 0)
            return false;
    }
    return compare_tac_val(a->next, b->next);
}

// Compare two Tac_Type structures recursively
bool compare_tac_type(const Tac_Type *a, const Tac_Type *b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    switch (a->kind) {
    case TAC_TYPE_FUN_TYPE:
        if (!compare_tac_type(a->u.fun_type.param_types, b->u.fun_type.param_types))
            return false;
        if (!compare_tac_type(a->u.fun_type.ret_type, b->u.fun_type.ret_type))
            return false;
        break;
    case TAC_TYPE_POINTER:
        if (!compare_tac_type(a->u.pointer.target_type, b->u.pointer.target_type))
            return false;
        break;
    case TAC_TYPE_ARRAY:
        if (!compare_tac_type(a->u.array.elem_type, b->u.array.elem_type))
            return false;
        if (a->u.array.size != b->u.array.size)
            return false;
        break;
    case TAC_TYPE_STRUCTURE:
        if ((a->u.structure.tag == NULL) != (b->u.structure.tag == NULL))
            return false;
        if (a->u.structure.tag && strcmp(a->u.structure.tag, b->u.structure.tag) != 0)
            return false;
        break;
    default:
        break;
    }
    return compare_tac_type(a->next, b->next);
}

// Compare two Tac_Param structures recursively
bool compare_tac_param(const Tac_Param *a, const Tac_Param *b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    if ((a->name == NULL) != (b->name == NULL))
        return false;
    if (a->name && strcmp(a->name, b->name) != 0)
        return false;
    return compare_tac_param(a->next, b->next);
}

// Compare two Tac_StaticInit structures recursively
bool compare_tac_static_init(const Tac_StaticInit *a, const Tac_StaticInit *b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    switch (a->kind) {
    case TAC_STATIC_INIT_INT:
        return a->u.int_val == b->u.int_val;
    case TAC_STATIC_INIT_LONG:
        return a->u.long_val == b->u.long_val;
    case TAC_STATIC_INIT_LONG_LONG:
        return a->u.long_long_val == b->u.long_long_val;
    case TAC_STATIC_INIT_UINT:
        return a->u.uint_val == b->u.uint_val;
    case TAC_STATIC_INIT_ULONG:
        return a->u.ulong_val == b->u.ulong_val;
    case TAC_STATIC_INIT_ULONG_LONG:
        return a->u.ulong_long_val == b->u.ulong_long_val;
    case TAC_STATIC_INIT_CHAR:
        return a->u.char_val == b->u.char_val;
    case TAC_STATIC_INIT_UCHAR:
        return a->u.uchar_val == b->u.uchar_val;
    case TAC_STATIC_INIT_DOUBLE:
        return a->u.double_val == b->u.double_val;
    case TAC_STATIC_INIT_ZERO:
        return a->u.zero_bytes == b->u.zero_bytes;
    case TAC_STATIC_INIT_STRING:
        if ((a->u.string.val == NULL) != (b->u.string.val == NULL))
            return false;
        if (a->u.string.val && strcmp(a->u.string.val, b->u.string.val) != 0)
            return false;
        return a->u.string.null_terminated == b->u.string.null_terminated;
    case TAC_STATIC_INIT_POINTER:
        if ((a->u.pointer_name == NULL) != (b->u.pointer_name == NULL))
            return false;
        if (a->u.pointer_name && strcmp(a->u.pointer_name, b->u.pointer_name) != 0)
            return false;
        break;
    }
    return compare_tac_static_init(a->next, b->next);
}

// Compare two Tac_Instruction structures recursively
bool compare_tac_instruction(const Tac_Instruction *a, const Tac_Instruction *b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    switch (a->kind) {
    case TAC_INSTRUCTION_RETURN:
        return compare_tac_val(a->u.return_.src, b->u.return_.src);
    case TAC_INSTRUCTION_SIGN_EXTEND:
    case TAC_INSTRUCTION_TRUNCATE:
    case TAC_INSTRUCTION_ZERO_EXTEND:
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
        return compare_tac_val(a->u.sign_extend.src, b->u.sign_extend.src) &&
               compare_tac_val(a->u.sign_extend.dst, b->u.sign_extend.dst);
    case TAC_INSTRUCTION_UNARY:
        return a->u.unary.op == b->u.unary.op && compare_tac_val(a->u.unary.src, b->u.unary.src) &&
               compare_tac_val(a->u.unary.dst, b->u.unary.dst);
    case TAC_INSTRUCTION_BINARY:
        return a->u.binary.op == b->u.binary.op &&
               compare_tac_val(a->u.binary.src1, b->u.binary.src1) &&
               compare_tac_val(a->u.binary.src2, b->u.binary.src2) &&
               compare_tac_val(a->u.binary.dst, b->u.binary.dst);
    case TAC_INSTRUCTION_COPY:
    case TAC_INSTRUCTION_GET_ADDRESS:
        return compare_tac_val(a->u.copy.src, b->u.copy.src) &&
               compare_tac_val(a->u.copy.dst, b->u.copy.dst);
    case TAC_INSTRUCTION_LOAD:
        return compare_tac_val(a->u.load.src_ptr, b->u.load.src_ptr) &&
               compare_tac_val(a->u.load.dst, b->u.load.dst);
    case TAC_INSTRUCTION_STORE:
        return compare_tac_val(a->u.store.src, b->u.store.src) &&
               compare_tac_val(a->u.store.dst_ptr, b->u.store.dst_ptr);
    case TAC_INSTRUCTION_ADD_PTR:
        return compare_tac_val(a->u.add_ptr.ptr, b->u.add_ptr.ptr) &&
               compare_tac_val(a->u.add_ptr.index, b->u.add_ptr.index) &&
               a->u.add_ptr.scale == b->u.add_ptr.scale &&
               compare_tac_val(a->u.add_ptr.dst, b->u.add_ptr.dst);
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
        if ((a->u.copy_to_offset.dst == NULL) != (b->u.copy_to_offset.dst == NULL))
            return false;
        if (a->u.copy_to_offset.dst &&
            strcmp(a->u.copy_to_offset.dst, b->u.copy_to_offset.dst) != 0)
            return false;
        return compare_tac_val(a->u.copy_to_offset.src, b->u.copy_to_offset.src) &&
               a->u.copy_to_offset.offset == b->u.copy_to_offset.offset;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
        if ((a->u.copy_from_offset.src == NULL) != (b->u.copy_from_offset.src == NULL))
            return false;
        if (a->u.copy_from_offset.src &&
            strcmp(a->u.copy_from_offset.src, b->u.copy_from_offset.src) != 0)
            return false;
        return a->u.copy_from_offset.offset == b->u.copy_from_offset.offset &&
               compare_tac_val(a->u.copy_from_offset.dst, b->u.copy_from_offset.dst);
    case TAC_INSTRUCTION_JUMP:
        if ((a->u.jump.target == NULL) != (b->u.jump.target == NULL))
            return false;
        if (a->u.jump.target && strcmp(a->u.jump.target, b->u.jump.target) != 0)
            return false;
        return true;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        if ((a->u.jump_if_zero.target == NULL) != (b->u.jump_if_zero.target == NULL))
            return false;
        if (a->u.jump_if_zero.target &&
            strcmp(a->u.jump_if_zero.target, b->u.jump_if_zero.target) != 0)
            return false;
        return compare_tac_val(a->u.jump_if_zero.condition, b->u.jump_if_zero.condition);
    case TAC_INSTRUCTION_LABEL:
        if ((a->u.label.name == NULL) != (b->u.label.name == NULL))
            return false;
        if (a->u.label.name && strcmp(a->u.label.name, b->u.label.name) != 0)
            return false;
        return true;
    case TAC_INSTRUCTION_FUN_CALL:
        if ((a->u.fun_call.fun_name == NULL) != (b->u.fun_call.fun_name == NULL))
            return false;
        if (a->u.fun_call.fun_name && strcmp(a->u.fun_call.fun_name, b->u.fun_call.fun_name) != 0)
            return false;
        return compare_tac_val(a->u.fun_call.args, b->u.fun_call.args) &&
               compare_tac_val(a->u.fun_call.dst, b->u.fun_call.dst);
    }
    return compare_tac_instruction(a->next, b->next);
}

// Compare two Tac_TopLevel structures recursively
bool compare_tac_toplevel(const Tac_TopLevel *a, const Tac_TopLevel *b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    switch (a->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        if ((a->u.function.name == NULL) != (b->u.function.name == NULL))
            return false;
        if (a->u.function.name && strcmp(a->u.function.name, b->u.function.name) != 0)
            return false;
        if (a->u.function.global != b->u.function.global)
            return false;
        return compare_tac_param(a->u.function.params, b->u.function.params) &&
               compare_tac_instruction(a->u.function.body, b->u.function.body);
    case TAC_TOPLEVEL_STATIC_VARIABLE:
        if ((a->u.static_variable.name == NULL) != (b->u.static_variable.name == NULL))
            return false;
        if (a->u.static_variable.name &&
            strcmp(a->u.static_variable.name, b->u.static_variable.name) != 0)
            return false;
        if (a->u.static_variable.global != b->u.static_variable.global)
            return false;
        return compare_tac_type(a->u.static_variable.type, b->u.static_variable.type) &&
               compare_tac_static_init(a->u.static_variable.init_list,
                                       b->u.static_variable.init_list);
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        if ((a->u.static_constant.name == NULL) != (b->u.static_constant.name == NULL))
            return false;
        if (a->u.static_constant.name &&
            strcmp(a->u.static_constant.name, b->u.static_constant.name) != 0)
            return false;
        return compare_tac_type(a->u.static_constant.type, b->u.static_constant.type) &&
               compare_tac_static_init(a->u.static_constant.init, b->u.static_constant.init);
    }
    return compare_tac_toplevel(a->next, b->next);
}

bool compare_tac_program(const Tac_Program *a, const Tac_Program *b)
{
    if (!a && !b)
        return true;
    if (!a || !b)
        return false;
    return compare_tac_toplevel(a->decls, b->decls);
}
