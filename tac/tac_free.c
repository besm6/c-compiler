#include <stdlib.h>

#include "tac.h"
#include "xalloc.h"

void free_tac_program(Tac_Program *program)
{
    if (program == NULL)
        return;
    free_tac_toplevel(program->decls);
    xfree(program);
}

// Free a Tac_Const and its contents
void free_tac_const(Tac_Const *constant)
{
    if (!constant)
        return;
    xfree(constant);
}

// Free a Tac_Val and its contents recursively
void free_tac_val(Tac_Val *val)
{
    if (!val)
        return;
    if (val->kind == TAC_VAL_CONSTANT && val->u.constant) {
        free_tac_const(val->u.constant);
    }
    if (val->u.var_name) {
        xfree(val->u.var_name);
    }
    free_tac_val(val->next);
    xfree(val);
}

// Free a Tac_Instruction and its contents recursively
void free_tac_instruction(Tac_Instruction *instr)
{
    if (!instr)
        return;
    switch (instr->kind) {
    case TAC_INSTRUCTION_RETURN:
        free_tac_val(instr->u.return_.src);
        break;
    case TAC_INSTRUCTION_SIGN_EXTEND:
    case TAC_INSTRUCTION_TRUNCATE:
    case TAC_INSTRUCTION_ZERO_EXTEND:
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
        free_tac_val(instr->u.sign_extend.src);
        free_tac_val(instr->u.sign_extend.dst);
        break;
    case TAC_INSTRUCTION_UNARY:
        free_tac_val(instr->u.unary.src);
        free_tac_val(instr->u.unary.dst);
        break;
    case TAC_INSTRUCTION_BINARY:
        free_tac_val(instr->u.binary.src1);
        free_tac_val(instr->u.binary.src2);
        free_tac_val(instr->u.binary.dst);
        break;
    case TAC_INSTRUCTION_COPY:
    case TAC_INSTRUCTION_GET_ADDRESS:
        free_tac_val(instr->u.copy.src);
        free_tac_val(instr->u.copy.dst);
        break;
    case TAC_INSTRUCTION_LOAD:
        free_tac_val(instr->u.load.src_ptr);
        free_tac_val(instr->u.load.dst);
        break;
    case TAC_INSTRUCTION_STORE:
        free_tac_val(instr->u.store.src);
        free_tac_val(instr->u.store.dst_ptr);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        free_tac_val(instr->u.add_ptr.ptr);
        free_tac_val(instr->u.add_ptr.index);
        free_tac_val(instr->u.add_ptr.dst);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
        free_tac_val(instr->u.copy_to_offset.src);
        if (instr->u.copy_to_offset.dst) {
            xfree(instr->u.copy_to_offset.dst);
        }
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
        if (instr->u.copy_from_offset.src) {
            xfree(instr->u.copy_from_offset.src);
        }
        free_tac_val(instr->u.copy_from_offset.dst);
        break;
    case TAC_INSTRUCTION_JUMP:
        if (instr->u.jump.target) {
            xfree(instr->u.jump.target);
        }
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        free_tac_val(instr->u.jump_if_zero.condition);
        if (instr->u.jump_if_zero.target) {
            xfree(instr->u.jump_if_zero.target);
        }
        break;
    case TAC_INSTRUCTION_LABEL:
        if (instr->u.label.name) {
            xfree(instr->u.label.name);
        }
        break;
    case TAC_INSTRUCTION_FUN_CALL:
        if (instr->u.fun_call.fun_name) {
            xfree(instr->u.fun_call.fun_name);
        }
        free_tac_val(instr->u.fun_call.args);
        free_tac_val(instr->u.fun_call.dst);
        break;
    }
    free_tac_instruction(instr->next);
    xfree(instr);
}

// Free a Tac_Type and its contents recursively
void free_tac_type(Tac_Type *type)
{
    if (!type)
        return;
    switch (type->kind) {
    case TAC_TYPE_FUN_TYPE:
        free_tac_type(type->u.fun_type.param_types);
        free_tac_type(type->u.fun_type.ret_type);
        break;
    case TAC_TYPE_POINTER:
        free_tac_type(type->u.pointer.target_type);
        break;
    case TAC_TYPE_ARRAY:
        free_tac_type(type->u.array.elem_type);
        break;
    case TAC_TYPE_STRUCTURE:
        if (type->u.structure.tag) {
            xfree(type->u.structure.tag);
        }
        break;
    default:
        break;
    }
    free_tac_type(type->next);
    xfree(type);
}

// Free a Tac_Param and its contents recursively
void free_tac_param(Tac_Param *param)
{
    if (!param)
        return;
    if (param->name) {
        xfree(param->name);
    }
    free_tac_param(param->next);
    xfree(param);
}

// Free a Tac_StaticInit and its contents recursively
void free_tac_static_init(Tac_StaticInit *init)
{
    if (!init)
        return;
    if (init->kind == TAC_STATIC_INIT_STRING && init->u.string.val) {
        xfree(init->u.string.val);
    }
    if (init->kind == TAC_STATIC_INIT_POINTER && init->u.pointer_name) {
        xfree(init->u.pointer_name);
    }
    free_tac_static_init(init->next);
    xfree(init);
}

// Free a Tac_TopLevel and its contents recursively
void free_tac_toplevel(Tac_TopLevel *toplevel)
{
    if (!toplevel)
        return;
    switch (toplevel->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        if (toplevel->u.function.name) {
            xfree(toplevel->u.function.name);
        }
        free_tac_param(toplevel->u.function.params);
        free_tac_instruction(toplevel->u.function.body);
        break;
    case TAC_TOPLEVEL_STATIC_VARIABLE:
        if (toplevel->u.static_variable.name) {
            xfree(toplevel->u.static_variable.name);
        }
        free_tac_type(toplevel->u.static_variable.type);
        free_tac_static_init(toplevel->u.static_variable.init_list);
        break;
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        if (toplevel->u.static_constant.name) {
            xfree(toplevel->u.static_constant.name);
        }
        free_tac_type(toplevel->u.static_constant.type);
        free_tac_static_init(toplevel->u.static_constant.init);
        break;
    }
    free_tac_toplevel(toplevel->next);
    xfree(toplevel);
}
