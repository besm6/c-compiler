#include <string.h>

#include "tac.h"

#define INDENT_STEP 2

// Helper function to print indentation to a file
static void print_indent(FILE *fd, int depth)
{
    for (int i = 0; i < depth * INDENT_STEP; i++) {
        fputc(' ', fd);
    }
}

// Print a Tac_Const to a file
void print_tac_const(FILE *fd, const Tac_Const *constant, int depth)
{
    if (!constant) {
        print_indent(fd, depth);
        fprintf(fd, "Const: NULL\n");
        return;
    }
    print_indent(fd, depth);
    fprintf(fd, "Const: ");
    switch (constant->kind) {
    case TAC_CONST_INT:
        fprintf(fd, "int %d\n", constant->u.int_val);
        break;
    case TAC_CONST_LONG:
        fprintf(fd, "long %ld\n", constant->u.long_val);
        break;
    case TAC_CONST_LONG_LONG:
        fprintf(fd, "long long %lld\n", constant->u.long_long_val);
        break;
    case TAC_CONST_UINT:
        fprintf(fd, "uint %u\n", constant->u.uint_val);
        break;
    case TAC_CONST_ULONG:
        fprintf(fd, "ulong %lu\n", constant->u.ulong_val);
        break;
    case TAC_CONST_ULONG_LONG:
        fprintf(fd, "ulong long %llu\n", constant->u.ulong_long_val);
        break;
    case TAC_CONST_DOUBLE:
        fprintf(fd, "double %f\n", constant->u.double_val);
        break;
    case TAC_CONST_CHAR:
        fprintf(fd, "char %d\n", constant->u.char_val);
        break;
    case TAC_CONST_UCHAR:
        fprintf(fd, "uchar %u\n", constant->u.uchar_val);
        break;
    }
}

// Print a Tac_Val recursively to a file
void print_tac_val(FILE *fd, const Tac_Val *val, int depth)
{
    if (!val) {
        print_indent(fd, depth);
        fprintf(fd, "Val: NULL\n");
        return;
    }
    print_indent(fd, depth);
    fprintf(fd, "Val: %s\n", val->kind == TAC_VAL_CONSTANT ? "CONSTANT" : "VAR");
    print_indent(fd, depth + 1);
    if (val->kind == TAC_VAL_CONSTANT) {
        fprintf(fd, "Constant:\n");
        print_tac_const(fd, val->u.constant, depth + 2);
    } else {
        fprintf(fd, "Var: %s\n", val->u.var_name ? val->u.var_name : "(null)");
    }
    if (val->next) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Next:\n");
        print_tac_val(fd, val->next, depth + 2);
    }
}

// Print a Tac_Type recursively to a file
void print_tac_type(FILE *fd, const Tac_Type *type, int depth)
{
    if (!type) {
        print_indent(fd, depth);
        fprintf(fd, "Type: NULL\n");
        return;
    }
    print_indent(fd, depth);
    fprintf(fd, "Type: ");
    switch (type->kind) {
    case TAC_TYPE_CHAR:
        fprintf(fd, "char\n");
        break;
    case TAC_TYPE_SCHAR:
        fprintf(fd, "schar\n");
        break;
    case TAC_TYPE_UCHAR:
        fprintf(fd, "uchar\n");
        break;
    case TAC_TYPE_SHORT:
        fprintf(fd, "short\n");
        break;
    case TAC_TYPE_INT:
        fprintf(fd, "int\n");
        break;
    case TAC_TYPE_LONG:
        fprintf(fd, "long\n");
        break;
    case TAC_TYPE_LONG_LONG:
        fprintf(fd, "long long\n");
        break;
    case TAC_TYPE_USHORT:
        fprintf(fd, "ushort\n");
        break;
    case TAC_TYPE_UINT:
        fprintf(fd, "uint\n");
        break;
    case TAC_TYPE_ULONG:
        fprintf(fd, "ulong\n");
        break;
    case TAC_TYPE_ULONG_LONG:
        fprintf(fd, "ulong long\n");
        break;
    case TAC_TYPE_FLOAT:
        fprintf(fd, "float\n");
        break;
    case TAC_TYPE_DOUBLE:
        fprintf(fd, "double\n");
        break;
    case TAC_TYPE_VOID:
        fprintf(fd, "void\n");
        break;
    case TAC_TYPE_FUN_TYPE:
        fprintf(fd, "fun_type\n");
        break;
    case TAC_TYPE_POINTER:
        fprintf(fd, "pointer\n");
        break;
    case TAC_TYPE_ARRAY:
        fprintf(fd, "array\n");
        break;
    case TAC_TYPE_STRUCTURE:
        fprintf(fd, "structure\n");
        break;
    }
    if (type->kind == TAC_TYPE_FUN_TYPE) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Params:\n");
        print_tac_type(fd, type->u.fun_type.param_types, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Return:\n");
        print_tac_type(fd, type->u.fun_type.ret_type, depth + 2);
    } else if (type->kind == TAC_TYPE_POINTER) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Referenced:\n");
        print_tac_type(fd, type->u.pointer.target_type, depth + 2);
    } else if (type->kind == TAC_TYPE_ARRAY) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Element:\n");
        print_tac_type(fd, type->u.array.elem_type, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Size: %d\n", type->u.array.size);
    } else if (type->kind == TAC_TYPE_STRUCTURE) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Tag: %s\n", type->u.structure.tag ? type->u.structure.tag : "(null)");
    }
    if (type->next) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Next:\n");
        print_tac_type(fd, type->next, depth + 2);
    }
}

// Print a Tac_Param recursively to a file
void print_tac_param(FILE *fd, const Tac_Param *param, int depth)
{
    if (!param) {
        print_indent(fd, depth);
        fprintf(fd, "Param: NULL\n");
        return;
    }
    print_indent(fd, depth);
    fprintf(fd, "Param: %s\n", param->name ? param->name : "(null)");
    if (param->next) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Next:\n");
        print_tac_param(fd, param->next, depth + 2);
    }
}

// Print a Tac_StaticInit recursively to a file
void print_tac_static_init(FILE *fd, const Tac_StaticInit *init, int depth)
{
    if (!init) {
        print_indent(fd, depth);
        fprintf(fd, "StaticInit: NULL\n");
        return;
    }
    print_indent(fd, depth);
    fprintf(fd, "StaticInit: ");
    switch (init->kind) {
    case TAC_STATIC_INIT_INT:
        fprintf(fd, "int %d\n", init->u.int_val);
        break;
    case TAC_STATIC_INIT_LONG:
        fprintf(fd, "long %ld\n", init->u.long_val);
        break;
    case TAC_STATIC_INIT_LONG_LONG:
        fprintf(fd, "long long %lld\n", init->u.long_long_val);
        break;
    case TAC_STATIC_INIT_UINT:
        fprintf(fd, "uint %u\n", init->u.uint_val);
        break;
    case TAC_STATIC_INIT_ULONG:
        fprintf(fd, "ulong %lu\n", init->u.ulong_val);
        break;
    case TAC_STATIC_INIT_ULONG_LONG:
        fprintf(fd, "ulong long %llu\n", init->u.ulong_long_val);
        break;
    case TAC_STATIC_INIT_CHAR:
        fprintf(fd, "char %d\n", init->u.char_val);
        break;
    case TAC_STATIC_INIT_UCHAR:
        fprintf(fd, "uchar %u\n", init->u.uchar_val);
        break;
    case TAC_STATIC_INIT_DOUBLE:
        fprintf(fd, "double %f\n", init->u.double_val);
        break;
    case TAC_STATIC_INIT_ZERO:
        fprintf(fd, "zero %d bytes\n", init->u.zero_bytes);
        break;
    case TAC_STATIC_INIT_STRING:
        fprintf(fd, "string \"%s\" (null-terminated: %d)\n",
                init->u.string.val ? init->u.string.val : "(null)", init->u.string.null_terminated);
        break;
    case TAC_STATIC_INIT_POINTER:
        fprintf(fd, "pointer %s\n", init->u.pointer_name ? init->u.pointer_name : "(null)");
        break;
    }
    if (init->next) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Next:\n");
        print_tac_static_init(fd, init->next, depth + 2);
    }
}

// Print a Tac_Instruction recursively to a file
void print_tac_instruction(FILE *fd, const Tac_Instruction *instr, int depth)
{
    if (!instr) {
        print_indent(fd, depth);
        fprintf(fd, "Instruction: NULL\n");
        return;
    }
    print_indent(fd, depth);
    fprintf(fd, "Instruction: ");
    switch (instr->kind) {
    case TAC_INSTRUCTION_RETURN:
        fprintf(fd, "return\n");
        break;
    case TAC_INSTRUCTION_SIGN_EXTEND:
        fprintf(fd, "sign_extend\n");
        break;
    case TAC_INSTRUCTION_TRUNCATE:
        fprintf(fd, "truncate\n");
        break;
    case TAC_INSTRUCTION_ZERO_EXTEND:
        fprintf(fd, "zero_extend\n");
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
        fprintf(fd, "double_to_int\n");
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
        fprintf(fd, "double_to_uint\n");
        break;
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
        fprintf(fd, "int_to_double\n");
        break;
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
        fprintf(fd, "uint_to_double\n");
        break;
    case TAC_INSTRUCTION_UNARY:
        fprintf(fd, "unary %s\n",
                instr->u.unary.op == TAC_UNARY_COMPLEMENT ? "complement"
                : instr->u.unary.op == TAC_UNARY_NEGATE   ? "negate"
                                                          : "not");
        break;
    case TAC_INSTRUCTION_BINARY:
        fprintf(fd, "binary %s\n",
                instr->u.binary.op == TAC_BINARY_ADD                ? "add"
                : instr->u.binary.op == TAC_BINARY_SUBTRACT         ? "subtract"
                : instr->u.binary.op == TAC_BINARY_MULTIPLY         ? "multiply"
                : instr->u.binary.op == TAC_BINARY_DIVIDE           ? "divide"
                : instr->u.binary.op == TAC_BINARY_REMAINDER        ? "remainder"
                : instr->u.binary.op == TAC_BINARY_EQUAL            ? "equal"
                : instr->u.binary.op == TAC_BINARY_NOT_EQUAL        ? "not_equal"
                : instr->u.binary.op == TAC_BINARY_LESS_THAN        ? "less_than"
                : instr->u.binary.op == TAC_BINARY_LESS_OR_EQUAL    ? "less_or_equal"
                : instr->u.binary.op == TAC_BINARY_GREATER_THAN     ? "greater_than"
                : instr->u.binary.op == TAC_BINARY_GREATER_OR_EQUAL ? "greater_or_equal"
                : instr->u.binary.op == TAC_BINARY_BITWISE_AND      ? "bitwise_and"
                : instr->u.binary.op == TAC_BINARY_BITWISE_OR       ? "bitwise_or"
                : instr->u.binary.op == TAC_BINARY_BITWISE_XOR      ? "bitwise_xor"
                : instr->u.binary.op == TAC_BINARY_LEFT_SHIFT       ? "left_shift"
                                                                    : "right_shift");
        break;
    case TAC_INSTRUCTION_COPY:
        fprintf(fd, "copy\n");
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
        fprintf(fd, "get_address\n");
        break;
    case TAC_INSTRUCTION_LOAD:
        fprintf(fd, "load\n");
        break;
    case TAC_INSTRUCTION_STORE:
        fprintf(fd, "store\n");
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        fprintf(fd, "add_ptr\n");
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
        fprintf(fd, "copy_to_offset\n");
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
        fprintf(fd, "copy_from_offset\n");
        break;
    case TAC_INSTRUCTION_JUMP:
        fprintf(fd, "jump\n");
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
        fprintf(fd, "jump_if_zero\n");
        break;
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        fprintf(fd, "jump_if_not_zero\n");
        break;
    case TAC_INSTRUCTION_LABEL:
        fprintf(fd, "label\n");
        break;
    case TAC_INSTRUCTION_FUN_CALL:
        fprintf(fd, "fun_call\n");
        break;
    }
    switch (instr->kind) {
    case TAC_INSTRUCTION_RETURN:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src:\n");
        print_tac_val(fd, instr->u.return_.src, depth + 2);
        break;
    case TAC_INSTRUCTION_SIGN_EXTEND:
    case TAC_INSTRUCTION_TRUNCATE:
    case TAC_INSTRUCTION_ZERO_EXTEND:
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src:\n");
        print_tac_val(fd, instr->u.sign_extend.src, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        print_tac_val(fd, instr->u.sign_extend.dst, depth + 2);
        break;
    case TAC_INSTRUCTION_UNARY:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src:\n");
        print_tac_val(fd, instr->u.unary.src, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        print_tac_val(fd, instr->u.unary.dst, depth + 2);
        break;
    case TAC_INSTRUCTION_BINARY:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src1:\n");
        print_tac_val(fd, instr->u.binary.src1, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Src2:\n");
        print_tac_val(fd, instr->u.binary.src2, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        print_tac_val(fd, instr->u.binary.dst, depth + 2);
        break;
    case TAC_INSTRUCTION_COPY:
    case TAC_INSTRUCTION_GET_ADDRESS:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src:\n");
        print_tac_val(fd, instr->u.copy.src, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        print_tac_val(fd, instr->u.copy.dst, depth + 2);
        break;
    case TAC_INSTRUCTION_LOAD:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src_ptr:\n");
        print_tac_val(fd, instr->u.load.src_ptr, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        print_tac_val(fd, instr->u.load.dst, depth + 2);
        break;
    case TAC_INSTRUCTION_STORE:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src:\n");
        print_tac_val(fd, instr->u.store.src, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst_ptr:\n");
        print_tac_val(fd, instr->u.store.dst_ptr, depth + 2);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        print_indent(fd, depth + 1);
        fprintf(fd, "Ptr:\n");
        print_tac_val(fd, instr->u.add_ptr.ptr, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Index:\n");
        print_tac_val(fd, instr->u.add_ptr.index, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Scale: %d\n", instr->u.add_ptr.scale);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        print_tac_val(fd, instr->u.add_ptr.dst, depth + 2);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src:\n");
        print_tac_val(fd, instr->u.copy_to_offset.src, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst: %s\n",
                instr->u.copy_to_offset.dst ? instr->u.copy_to_offset.dst : "(null)");
        print_indent(fd, depth + 1);
        fprintf(fd, "Offset: %d\n", instr->u.copy_to_offset.offset);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src: %s\n",
                instr->u.copy_from_offset.src ? instr->u.copy_from_offset.src : "(null)");
        print_indent(fd, depth + 1);
        fprintf(fd, "Offset: %d\n", instr->u.copy_from_offset.offset);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        print_tac_val(fd, instr->u.copy_from_offset.dst, depth + 2);
        break;
    case TAC_INSTRUCTION_JUMP:
        print_indent(fd, depth + 1);
        fprintf(fd, "Target: %s\n", instr->u.jump.target ? instr->u.jump.target : "(null)");
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        print_indent(fd, depth + 1);
        fprintf(fd, "Condition:\n");
        print_tac_val(fd, instr->u.jump_if_zero.condition, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Target: %s\n",
                instr->u.jump_if_zero.target ? instr->u.jump_if_zero.target : "(null)");
        break;
    case TAC_INSTRUCTION_LABEL:
        print_indent(fd, depth + 1);
        fprintf(fd, "Name: %s\n", instr->u.label.name ? instr->u.label.name : "(null)");
        break;
    case TAC_INSTRUCTION_FUN_CALL:
        print_indent(fd, depth + 1);
        fprintf(fd, "Fun_name: %s\n",
                instr->u.fun_call.fun_name ? instr->u.fun_call.fun_name : "(null)");
        print_indent(fd, depth + 1);
        fprintf(fd, "Args:\n");
        print_tac_val(fd, instr->u.fun_call.args, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        print_tac_val(fd, instr->u.fun_call.dst, depth + 2);
        break;
    }
    if (instr->next) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Next:\n");
        print_tac_instruction(fd, instr->next, depth + 2);
    }
}

// Print a Tac_TopLevel recursively to a file
void print_tac_toplevel(FILE *fd, const Tac_TopLevel *toplevel, int depth)
{
    if (!toplevel) {
        print_indent(fd, depth);
        fprintf(fd, "TopLevel: NULL\n");
        return;
    }
    print_indent(fd, depth);
    fprintf(fd, "TopLevel: %s\n",
            toplevel->kind == TAC_TOPLEVEL_FUNCTION          ? "FUNCTION"
            : toplevel->kind == TAC_TOPLEVEL_STATIC_VARIABLE ? "STATIC_VARIABLE"
                                                             : "STATIC_CONSTANT");
    switch (toplevel->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        print_indent(fd, depth + 1);
        fprintf(fd, "Name: %s\n", toplevel->u.function.name ? toplevel->u.function.name : "(null)");
        print_indent(fd, depth + 1);
        fprintf(fd, "Global: %d\n", toplevel->u.function.global);
        print_indent(fd, depth + 1);
        fprintf(fd, "Params:\n");
        print_tac_param(fd, toplevel->u.function.params, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Body:\n");
        print_tac_instruction(fd, toplevel->u.function.body, depth + 2);
        break;
    case TAC_TOPLEVEL_STATIC_VARIABLE:
        print_indent(fd, depth + 1);
        fprintf(fd, "Name: %s\n",
                toplevel->u.static_variable.name ? toplevel->u.static_variable.name : "(null)");
        print_indent(fd, depth + 1);
        fprintf(fd, "Global: %d\n", toplevel->u.static_variable.global);
        print_indent(fd, depth + 1);
        fprintf(fd, "Type:\n");
        print_tac_type(fd, toplevel->u.static_variable.type, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Init_list:\n");
        print_tac_static_init(fd, toplevel->u.static_variable.init_list, depth + 2);
        break;
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        print_indent(fd, depth + 1);
        fprintf(fd, "Name: %s\n",
                toplevel->u.static_constant.name ? toplevel->u.static_constant.name : "(null)");
        print_indent(fd, depth + 1);
        fprintf(fd, "Type:\n");
        print_tac_type(fd, toplevel->u.static_constant.type, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Init:\n");
        print_tac_static_init(fd, toplevel->u.static_constant.init, depth + 2);
        break;
    }
    if (toplevel->next) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Next:\n");
        print_tac_toplevel(fd, toplevel->next, depth + 2);
    }
}

// Main print function
void print_tac_program(FILE *fd, const Tac_Program *program)
{
    if (!program) {
        fprintf(fd, "Program: null\n");
        return;
    }
    fprintf(fd, "Program:\n");
    print_tac_toplevel(fd, program->decls, 2);
}
