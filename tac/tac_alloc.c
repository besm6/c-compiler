#include <stdlib.h>

#include "tac.h"
#include "xalloc.h"

// Allocate a new Tac_Val with the specified kind
Tac_Val *new_tac_val(Tac_ValKind kind)
{
    Tac_Val *val = (Tac_Val *)xalloc(sizeof(Tac_Val), __func__, __FILE__, __LINE__);
    val->kind    = kind;
    return val;
}

// Allocate a new Tac_Instruction with the specified kind
Tac_Instruction *new_tac_instruction(Tac_InstructionKind kind)
{
    Tac_Instruction *instr =
        (Tac_Instruction *)xalloc(sizeof(Tac_Instruction), __func__, __FILE__, __LINE__);
    instr->kind = kind;
    return instr;
}

// Allocate a new Tac_Type with the specified kind
Tac_Type *new_tac_type(Tac_TypeKind kind)
{
    Tac_Type *type = (Tac_Type *)xalloc(sizeof(Tac_Type), __func__, __FILE__, __LINE__);
    type->kind     = kind;
    return type;
}

// Allocate a new Tac_Const with the specified kind
Tac_Const *new_tac_const(Tac_ConstKind kind)
{
    Tac_Const *constant = (Tac_Const *)xalloc(sizeof(Tac_Const), __func__, __FILE__, __LINE__);
    constant->kind      = kind;
    return constant;
}

// Allocate a new Tac_Param
Tac_Param *new_tac_param(void)
{
    Tac_Param *param = (Tac_Param *)xalloc(sizeof(Tac_Param), __func__, __FILE__, __LINE__);
    return param;
}

// Allocate a new Tac_TopLevel with the specified kind
Tac_TopLevel *new_tac_toplevel(Tac_TopLevelKind kind)
{
    Tac_TopLevel *toplevel =
        (Tac_TopLevel *)xalloc(sizeof(Tac_TopLevel), __func__, __FILE__, __LINE__);
    toplevel->kind = kind;
    return toplevel;
}

// Allocate a new Tac_StaticInit with the specified kind
Tac_StaticInit *new_tac_static_init(Tac_StaticInitKind kind)
{
    Tac_StaticInit *init =
        (Tac_StaticInit *)xalloc(sizeof(Tac_StaticInit), __func__, __FILE__, __LINE__);
    init->kind = kind;
    return init;
}

Tac_Program *new_tac_program()
{
    Tac_Program *p = xalloc(sizeof(Tac_Program), __func__, __FILE__, __LINE__);
    return p;
}
