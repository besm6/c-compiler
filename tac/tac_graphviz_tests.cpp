#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "tac.h"
#include "xalloc.h"

extern "C" int xalloc_debug;

// Declared in tac.h but defined in tac_graphviz.c
extern "C" void tac_export_dot(FILE *fd, const Tac_TopLevel *toplevel);

class TacDotTest : public ::testing::Test {
protected:
    void SetUp() override { xalloc_debug = 1; }

    void TearDown() override
    {
        xreport_lost_memory();
        EXPECT_EQ(xtotal_allocated_size(), 0);
        xfree_all();
    }

    static std::string capture(const Tac_TopLevel *tl)
    {
        FILE *f = tmpfile();
        EXPECT_NE(f, nullptr);
        tac_export_dot(f, tl);
        long len = ftell(f);
        rewind(f);
        std::string result(static_cast<size_t>(len), '\0');
        fread(&result[0], 1, static_cast<size_t>(len), f);
        fclose(f);
        return result;
    }

    static std::string capture_null()
    {
        FILE *f = tmpfile();
        EXPECT_NE(f, nullptr);
        tac_export_dot(f, nullptr);
        long len = ftell(f);
        rewind(f);
        std::string result(static_cast<size_t>(len), '\0');
        fread(&result[0], 1, static_cast<size_t>(len), f);
        fclose(f);
        return result;
    }

    static Tac_Val *make_const_int(int v)
    {
        Tac_Const *c    = tac_new_const(TAC_CONST_INT);
        c->u.int_val    = v;
        Tac_Val *val    = tac_new_val(TAC_VAL_CONSTANT);
        val->u.constant = c;
        return val;
    }

    static Tac_Val *make_var(const char *name)
    {
        Tac_Val *val    = tac_new_val(TAC_VAL_VAR);
        val->u.var_name = xstrdup(name);
        return val;
    }

    static Tac_TopLevel *make_empty_function(const char *name, bool global)
    {
        Tac_TopLevel *tl      = tac_new_toplevel(TAC_TOPLEVEL_FUNCTION);
        tl->u.function.name   = xstrdup(name);
        tl->u.function.global = global;
        return tl;
    }
};

// ---------------------------------------------------------------------------
// Toplevel: function
// ---------------------------------------------------------------------------

TEST_F(TacDotTest, FunctionEmpty)
{
    Tac_TopLevel *tl = make_empty_function("foo", true);
    std::string out  = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("digraph TAC"), std::string::npos);
    EXPECT_NE(out.find("Function: foo"), std::string::npos);
    EXPECT_NE(out.find("(global)"), std::string::npos);
    EXPECT_EQ(out.find("param"), std::string::npos);
    EXPECT_EQ(out.find("instr"), std::string::npos);
}

TEST_F(TacDotTest, GlobalFalse)
{
    Tac_TopLevel *tl = make_empty_function("bar", false);
    std::string out  = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Function: bar"), std::string::npos);
    EXPECT_EQ(out.find("(global)"), std::string::npos);
}

TEST_F(TacDotTest, NullInput)
{
    std::string out = capture_null();
    EXPECT_NE(out.find("digraph TAC"), std::string::npos);
    EXPECT_NE(out.find("null"), std::string::npos);
}

TEST_F(TacDotTest, FunctionWithParams)
{
    Tac_TopLevel *tl  = make_empty_function("add", true);
    Tac_Param *p1     = tac_new_param();
    p1->name          = xstrdup("x");
    Tac_Param *p2     = tac_new_param();
    p2->name          = xstrdup("y");
    p1->next          = p2;
    tl->u.function.params = p1;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Param: x"), std::string::npos);
    EXPECT_NE(out.find("Param: y"), std::string::npos);
    EXPECT_NE(out.find("label=\"param\""), std::string::npos);
}

// ---------------------------------------------------------------------------
// Toplevel: static variable and constant
// ---------------------------------------------------------------------------

TEST_F(TacDotTest, StaticVariable)
{
    Tac_TopLevel *tl              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
    tl->u.static_variable.name   = xstrdup("g_count");
    tl->u.static_variable.global = true;
    tl->u.static_variable.type   = tac_new_type(TAC_TYPE_INT);
    Tac_StaticInit *init          = tac_new_static_init(TAC_STATIC_INIT_I32);
    init->u.int_val               = 0;
    tl->u.static_variable.init_list = init;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("StaticVariable: g_count"), std::string::npos);
    EXPECT_NE(out.find("(global)"), std::string::npos);
    EXPECT_NE(out.find("Type: int"), std::string::npos);
    EXPECT_NE(out.find("StaticInit: i32"), std::string::npos);
    EXPECT_NE(out.find("label=\"type\""), std::string::npos);
    EXPECT_NE(out.find("label=\"init\""), std::string::npos);
}

TEST_F(TacDotTest, StaticVariableLocalScope)
{
    Tac_TopLevel *tl              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
    tl->u.static_variable.name   = xstrdup("s_val");
    tl->u.static_variable.global = false;
    tl->u.static_variable.type   = tac_new_type(TAC_TYPE_LONG);

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("StaticVariable: s_val"), std::string::npos);
    EXPECT_EQ(out.find("(global)"), std::string::npos);
}

TEST_F(TacDotTest, StaticConstant)
{
    Tac_TopLevel *tl            = tac_new_toplevel(TAC_TOPLEVEL_STATIC_CONSTANT);
    tl->u.static_constant.name  = xstrdup("PI");
    tl->u.static_constant.type  = tac_new_type(TAC_TYPE_DOUBLE);
    Tac_StaticInit *init        = tac_new_static_init(TAC_STATIC_INIT_DOUBLE);
    init->u.double_val          = 3.14;
    tl->u.static_constant.init  = init;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("StaticConstant: PI"), std::string::npos);
    EXPECT_NE(out.find("Type: double"), std::string::npos);
    EXPECT_NE(out.find("StaticInit: double"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Instructions: return
// ---------------------------------------------------------------------------

TEST_F(TacDotTest, InstructionReturn)
{
    Tac_TopLevel *tl       = make_empty_function("f", true);
    Tac_Instruction *instr = tac_new_instruction(TAC_INSTRUCTION_RETURN);
    instr->u.return_.src   = make_const_int(42);
    tl->u.function.body    = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Return"), std::string::npos);
    EXPECT_NE(out.find("label=\"instr\""), std::string::npos);
    EXPECT_NE(out.find("label=\"src\""), std::string::npos);
    EXPECT_NE(out.find("Const: int 42"), std::string::npos);
}

TEST_F(TacDotTest, InstructionReturnVoid)
{
    Tac_TopLevel *tl       = make_empty_function("f", true);
    Tac_Instruction *instr = tac_new_instruction(TAC_INSTRUCTION_RETURN);
    tl->u.function.body    = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Return"), std::string::npos);
    // No src child when return has no value
    EXPECT_EQ(out.find("label=\"src\""), std::string::npos);
}

// ---------------------------------------------------------------------------
// Instructions: type conversions
// ---------------------------------------------------------------------------

TEST_F(TacDotTest, InstructionSignExtend)
{
    Tac_TopLevel *tl          = make_empty_function("f", true);
    Tac_Instruction *instr    = tac_new_instruction(TAC_INSTRUCTION_SIGN_EXTEND);
    instr->u.sign_extend.src  = make_var("x");
    instr->u.sign_extend.dst  = make_var("y");
    tl->u.function.body       = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("SignExtend"), std::string::npos);
    EXPECT_NE(out.find("Val: var x"), std::string::npos);
    EXPECT_NE(out.find("Val: var y"), std::string::npos);
}

TEST_F(TacDotTest, InstructionTruncate)
{
    Tac_TopLevel *tl        = make_empty_function("f", true);
    Tac_Instruction *instr  = tac_new_instruction(TAC_INSTRUCTION_TRUNCATE);
    instr->u.truncate.src   = make_var("a");
    instr->u.truncate.dst   = make_var("b");
    tl->u.function.body     = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Truncate"), std::string::npos);
}

TEST_F(TacDotTest, InstructionZeroExtend)
{
    Tac_TopLevel *tl           = make_empty_function("f", true);
    Tac_Instruction *instr     = tac_new_instruction(TAC_INSTRUCTION_ZERO_EXTEND);
    instr->u.zero_extend.src   = make_var("a");
    instr->u.zero_extend.dst   = make_var("b");
    tl->u.function.body        = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("ZeroExtend"), std::string::npos);
}

TEST_F(TacDotTest, InstructionDoubleToInt)
{
    Tac_TopLevel *tl            = make_empty_function("f", true);
    Tac_Instruction *instr      = tac_new_instruction(TAC_INSTRUCTION_DOUBLE_TO_INT);
    instr->u.double_to_int.src  = make_var("d");
    instr->u.double_to_int.dst  = make_var("i");
    tl->u.function.body         = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("DoubleToInt"), std::string::npos);
}

TEST_F(TacDotTest, InstructionDoubleToUint)
{
    Tac_TopLevel *tl             = make_empty_function("f", true);
    Tac_Instruction *instr       = tac_new_instruction(TAC_INSTRUCTION_DOUBLE_TO_UINT);
    instr->u.double_to_uint.src  = make_var("d");
    instr->u.double_to_uint.dst  = make_var("u");
    tl->u.function.body          = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("DoubleToUInt"), std::string::npos);
}

TEST_F(TacDotTest, InstructionIntToDouble)
{
    Tac_TopLevel *tl            = make_empty_function("f", true);
    Tac_Instruction *instr      = tac_new_instruction(TAC_INSTRUCTION_INT_TO_DOUBLE);
    instr->u.int_to_double.src  = make_var("i");
    instr->u.int_to_double.dst  = make_var("d");
    tl->u.function.body         = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("IntToDouble"), std::string::npos);
}

TEST_F(TacDotTest, InstructionUintToDouble)
{
    Tac_TopLevel *tl             = make_empty_function("f", true);
    Tac_Instruction *instr       = tac_new_instruction(TAC_INSTRUCTION_UINT_TO_DOUBLE);
    instr->u.uint_to_double.src  = make_var("u");
    instr->u.uint_to_double.dst  = make_var("d");
    tl->u.function.body          = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("UIntToDouble"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Instructions: unary
// ---------------------------------------------------------------------------

TEST_F(TacDotTest, InstructionUnaryComplement)
{
    Tac_TopLevel *tl       = make_empty_function("f", true);
    Tac_Instruction *instr = tac_new_instruction(TAC_INSTRUCTION_UNARY);
    instr->u.unary.op      = TAC_UNARY_COMPLEMENT;
    instr->u.unary.src     = make_var("x");
    instr->u.unary.dst     = make_var("r");
    tl->u.function.body    = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Unary: complement"), std::string::npos);
    EXPECT_NE(out.find("Val: var x"), std::string::npos);
    EXPECT_NE(out.find("Val: var r"), std::string::npos);
}

TEST_F(TacDotTest, InstructionUnaryNegate)
{
    Tac_TopLevel *tl       = make_empty_function("f", true);
    Tac_Instruction *instr = tac_new_instruction(TAC_INSTRUCTION_UNARY);
    instr->u.unary.op      = TAC_UNARY_NEGATE;
    instr->u.unary.src     = make_var("x");
    instr->u.unary.dst     = make_var("r");
    tl->u.function.body    = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Unary: negate"), std::string::npos);
}

TEST_F(TacDotTest, InstructionUnaryNot)
{
    Tac_TopLevel *tl       = make_empty_function("f", true);
    Tac_Instruction *instr = tac_new_instruction(TAC_INSTRUCTION_UNARY);
    instr->u.unary.op      = TAC_UNARY_NOT;
    instr->u.unary.src     = make_var("x");
    instr->u.unary.dst     = make_var("r");
    tl->u.function.body    = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Unary: not"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Instructions: binary
// ---------------------------------------------------------------------------

TEST_F(TacDotTest, InstructionBinaryAdd)
{
    Tac_TopLevel *tl        = make_empty_function("f", true);
    Tac_Instruction *instr  = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    instr->u.binary.op      = TAC_BINARY_ADD;
    instr->u.binary.src1    = make_var("a");
    instr->u.binary.src2    = make_var("b");
    instr->u.binary.dst     = make_var("c");
    tl->u.function.body     = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Binary: add"), std::string::npos);
    EXPECT_NE(out.find("label=\"src1\""), std::string::npos);
    EXPECT_NE(out.find("label=\"src2\""), std::string::npos);
    EXPECT_NE(out.find("label=\"dst\""), std::string::npos);
}

TEST_F(TacDotTest, InstructionBinarySubtract)
{
    Tac_TopLevel *tl        = make_empty_function("f", true);
    Tac_Instruction *instr  = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    instr->u.binary.op      = TAC_BINARY_SUBTRACT;
    instr->u.binary.src1    = make_var("a");
    instr->u.binary.src2    = make_var("b");
    instr->u.binary.dst     = make_var("c");
    tl->u.function.body     = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Binary: subtract"), std::string::npos);
}

TEST_F(TacDotTest, InstructionBinaryEqual)
{
    Tac_TopLevel *tl        = make_empty_function("f", true);
    Tac_Instruction *instr  = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    instr->u.binary.op      = TAC_BINARY_EQUAL;
    instr->u.binary.src1    = make_var("a");
    instr->u.binary.src2    = make_var("b");
    instr->u.binary.dst     = make_var("c");
    tl->u.function.body     = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Binary: equal"), std::string::npos);
}

TEST_F(TacDotTest, InstructionBinaryBitwiseOr)
{
    Tac_TopLevel *tl        = make_empty_function("f", true);
    Tac_Instruction *instr  = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    instr->u.binary.op      = TAC_BINARY_BITWISE_OR;
    instr->u.binary.src1    = make_var("a");
    instr->u.binary.src2    = make_var("b");
    instr->u.binary.dst     = make_var("c");
    tl->u.function.body     = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Binary: bitwise_or"), std::string::npos);
}

TEST_F(TacDotTest, InstructionBinaryLeftShift)
{
    Tac_TopLevel *tl        = make_empty_function("f", true);
    Tac_Instruction *instr  = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    instr->u.binary.op      = TAC_BINARY_LEFT_SHIFT;
    instr->u.binary.src1    = make_var("a");
    instr->u.binary.src2    = make_const_int(2);
    instr->u.binary.dst     = make_var("c");
    tl->u.function.body     = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Binary: left_shift"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Instructions: memory operations
// ---------------------------------------------------------------------------

TEST_F(TacDotTest, InstructionCopy)
{
    Tac_TopLevel *tl       = make_empty_function("f", true);
    Tac_Instruction *instr = tac_new_instruction(TAC_INSTRUCTION_COPY);
    instr->u.copy.src      = make_const_int(1);
    instr->u.copy.dst      = make_var("x");
    tl->u.function.body    = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Copy"), std::string::npos);
    EXPECT_NE(out.find("label=\"src\""), std::string::npos);
    EXPECT_NE(out.find("label=\"dst\""), std::string::npos);
}

TEST_F(TacDotTest, InstructionGetAddress)
{
    Tac_TopLevel *tl            = make_empty_function("f", true);
    Tac_Instruction *instr      = tac_new_instruction(TAC_INSTRUCTION_GET_ADDRESS);
    instr->u.get_address.src    = make_var("arr");
    instr->u.get_address.dst    = make_var("ptr");
    tl->u.function.body         = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("GetAddress"), std::string::npos);
}

TEST_F(TacDotTest, InstructionLoad)
{
    Tac_TopLevel *tl        = make_empty_function("f", true);
    Tac_Instruction *instr  = tac_new_instruction(TAC_INSTRUCTION_LOAD);
    instr->u.load.src_ptr   = make_var("ptr");
    instr->u.load.dst       = make_var("val");
    tl->u.function.body     = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Load"), std::string::npos);
    EXPECT_NE(out.find("label=\"src_ptr\""), std::string::npos);
}

TEST_F(TacDotTest, InstructionStore)
{
    Tac_TopLevel *tl          = make_empty_function("f", true);
    Tac_Instruction *instr    = tac_new_instruction(TAC_INSTRUCTION_STORE);
    instr->u.store.src        = make_const_int(99);
    instr->u.store.dst_ptr    = make_var("ptr");
    tl->u.function.body       = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Store"), std::string::npos);
    EXPECT_NE(out.find("label=\"dst_ptr\""), std::string::npos);
}

TEST_F(TacDotTest, InstructionAddPtr)
{
    Tac_TopLevel *tl          = make_empty_function("f", true);
    Tac_Instruction *instr    = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
    instr->u.add_ptr.ptr      = make_var("base");
    instr->u.add_ptr.index    = make_var("idx");
    instr->u.add_ptr.scale    = 4;
    instr->u.add_ptr.dst      = make_var("result");
    tl->u.function.body       = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("AddPtr scale=4"), std::string::npos);
    EXPECT_NE(out.find("label=\"ptr\""), std::string::npos);
    EXPECT_NE(out.find("label=\"index\""), std::string::npos);
}

TEST_F(TacDotTest, InstructionCopyToOffset)
{
    Tac_TopLevel *tl                      = make_empty_function("f", true);
    Tac_Instruction *instr                = tac_new_instruction(TAC_INSTRUCTION_COPY_TO_OFFSET);
    instr->u.copy_to_offset.src           = make_const_int(5);
    instr->u.copy_to_offset.dst           = xstrdup("s");
    instr->u.copy_to_offset.offset        = 8;
    tl->u.function.body                   = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("CopyToOffset"), std::string::npos);
    EXPECT_NE(out.find("offset=8"), std::string::npos);
    EXPECT_NE(out.find("dst=s"), std::string::npos);
    EXPECT_NE(out.find("label=\"src\""), std::string::npos);
}

TEST_F(TacDotTest, InstructionCopyFromOffset)
{
    Tac_TopLevel *tl                        = make_empty_function("f", true);
    Tac_Instruction *instr                  = tac_new_instruction(TAC_INSTRUCTION_COPY_FROM_OFFSET);
    instr->u.copy_from_offset.src           = xstrdup("s");
    instr->u.copy_from_offset.offset        = 4;
    instr->u.copy_from_offset.dst           = make_var("out");
    tl->u.function.body                     = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("CopyFromOffset"), std::string::npos);
    EXPECT_NE(out.find("offset=4"), std::string::npos);
    EXPECT_NE(out.find("src=s"), std::string::npos);
    EXPECT_NE(out.find("label=\"dst\""), std::string::npos);
}

// ---------------------------------------------------------------------------
// Instructions: control flow
// ---------------------------------------------------------------------------

TEST_F(TacDotTest, InstructionJump)
{
    Tac_TopLevel *tl       = make_empty_function("f", true);
    Tac_Instruction *instr = tac_new_instruction(TAC_INSTRUCTION_JUMP);
    instr->u.jump.target   = xstrdup("loop_end");
    tl->u.function.body    = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Jump: loop_end"), std::string::npos);
    // Jump has no child nodes
    EXPECT_EQ(out.find("label=\"cond\""), std::string::npos);
}

TEST_F(TacDotTest, InstructionJumpIfZero)
{
    Tac_TopLevel *tl                  = make_empty_function("f", true);
    Tac_Instruction *instr            = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
    instr->u.jump_if_zero.condition   = make_var("flag");
    instr->u.jump_if_zero.target      = xstrdup("done");
    tl->u.function.body               = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("JumpIfZero: done"), std::string::npos);
    EXPECT_NE(out.find("label=\"cond\""), std::string::npos);
    EXPECT_NE(out.find("Val: var flag"), std::string::npos);
}

TEST_F(TacDotTest, InstructionJumpIfNotZero)
{
    Tac_TopLevel *tl                      = make_empty_function("f", true);
    Tac_Instruction *instr                = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_NOT_ZERO);
    instr->u.jump_if_not_zero.condition   = make_var("flag");
    instr->u.jump_if_not_zero.target      = xstrdup("loop_top");
    tl->u.function.body                   = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("JumpIfNotZero: loop_top"), std::string::npos);
    EXPECT_NE(out.find("label=\"cond\""), std::string::npos);
}

TEST_F(TacDotTest, InstructionLabel)
{
    Tac_TopLevel *tl       = make_empty_function("f", true);
    Tac_Instruction *instr = tac_new_instruction(TAC_INSTRUCTION_LABEL);
    instr->u.label.name    = xstrdup("loop_start");
    tl->u.function.body    = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Label: loop_start"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Instructions: function call
// ---------------------------------------------------------------------------

TEST_F(TacDotTest, InstructionFunCallNoArgsNoReturn)
{
    Tac_TopLevel *tl              = make_empty_function("f", true);
    Tac_Instruction *instr        = tac_new_instruction(TAC_INSTRUCTION_FUN_CALL);
    instr->u.fun_call.fun_name    = xstrdup("init");
    tl->u.function.body           = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("FunCall: init"), std::string::npos);
    EXPECT_EQ(out.find("label=\"arg\""), std::string::npos);
    EXPECT_EQ(out.find("label=\"dst\""), std::string::npos);
}

TEST_F(TacDotTest, InstructionFunCallWithArgs)
{
    Tac_TopLevel *tl              = make_empty_function("f", true);
    Tac_Instruction *instr        = tac_new_instruction(TAC_INSTRUCTION_FUN_CALL);
    instr->u.fun_call.fun_name    = xstrdup("add");
    Tac_Val *a1                   = make_const_int(1);
    Tac_Val *a2                   = make_const_int(2);
    a1->next                      = a2;
    instr->u.fun_call.args        = a1;
    instr->u.fun_call.dst         = make_var("result");
    tl->u.function.body           = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("FunCall: add"), std::string::npos);
    EXPECT_NE(out.find("label=\"arg\""), std::string::npos);
    EXPECT_NE(out.find("label=\"dst\""), std::string::npos);
    EXPECT_NE(out.find("Const: int 1"), std::string::npos);
    EXPECT_NE(out.find("Const: int 2"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Const kinds
// ---------------------------------------------------------------------------

TEST_F(TacDotTest, ConstInt)
{
    Tac_TopLevel *tl       = make_empty_function("f", true);
    Tac_Instruction *instr = tac_new_instruction(TAC_INSTRUCTION_RETURN);
    instr->u.return_.src   = make_const_int(-7);
    tl->u.function.body    = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Const: int -7"), std::string::npos);
}

TEST_F(TacDotTest, ConstLong)
{
    Tac_TopLevel *tl       = make_empty_function("f", true);
    Tac_Const *c           = tac_new_const(TAC_CONST_LONG);
    c->u.long_val          = 100000L;
    Tac_Val *val           = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant        = c;
    Tac_Instruction *instr = tac_new_instruction(TAC_INSTRUCTION_RETURN);
    instr->u.return_.src   = val;
    tl->u.function.body    = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Const: long 100000"), std::string::npos);
}

TEST_F(TacDotTest, ConstLongLong)
{
    Tac_TopLevel *tl       = make_empty_function("f", true);
    Tac_Const *c           = tac_new_const(TAC_CONST_LONG_LONG);
    c->u.long_long_val     = -9000000000LL;
    Tac_Val *val           = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant        = c;
    Tac_Instruction *instr = tac_new_instruction(TAC_INSTRUCTION_RETURN);
    instr->u.return_.src   = val;
    tl->u.function.body    = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Const: long_long -9000000000"), std::string::npos);
}

TEST_F(TacDotTest, ConstUint)
{
    Tac_TopLevel *tl       = make_empty_function("f", true);
    Tac_Const *c           = tac_new_const(TAC_CONST_UINT);
    c->u.uint_val          = 42u;
    Tac_Val *val           = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant        = c;
    Tac_Instruction *instr = tac_new_instruction(TAC_INSTRUCTION_RETURN);
    instr->u.return_.src   = val;
    tl->u.function.body    = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Const: uint 42"), std::string::npos);
}

TEST_F(TacDotTest, ConstUlong)
{
    Tac_TopLevel *tl       = make_empty_function("f", true);
    Tac_Const *c           = tac_new_const(TAC_CONST_ULONG);
    c->u.ulong_val         = 999ul;
    Tac_Val *val           = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant        = c;
    Tac_Instruction *instr = tac_new_instruction(TAC_INSTRUCTION_RETURN);
    instr->u.return_.src   = val;
    tl->u.function.body    = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Const: ulong 999"), std::string::npos);
}

TEST_F(TacDotTest, ConstUlongLong)
{
    Tac_TopLevel *tl       = make_empty_function("f", true);
    Tac_Const *c           = tac_new_const(TAC_CONST_ULONG_LONG);
    c->u.ulong_long_val    = 18000000000ULL;
    Tac_Val *val           = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant        = c;
    Tac_Instruction *instr = tac_new_instruction(TAC_INSTRUCTION_RETURN);
    instr->u.return_.src   = val;
    tl->u.function.body    = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Const: ulong_long 18000000000"), std::string::npos);
}

TEST_F(TacDotTest, ConstDouble)
{
    Tac_TopLevel *tl       = make_empty_function("f", true);
    Tac_Const *c           = tac_new_const(TAC_CONST_DOUBLE);
    c->u.double_val        = 1.5;
    Tac_Val *val           = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant        = c;
    Tac_Instruction *instr = tac_new_instruction(TAC_INSTRUCTION_RETURN);
    instr->u.return_.src   = val;
    tl->u.function.body    = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Const: double"), std::string::npos);
}

TEST_F(TacDotTest, ConstChar)
{
    Tac_TopLevel *tl       = make_empty_function("f", true);
    Tac_Const *c           = tac_new_const(TAC_CONST_CHAR);
    c->u.char_val          = 65; // 'A'
    Tac_Val *val           = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant        = c;
    Tac_Instruction *instr = tac_new_instruction(TAC_INSTRUCTION_RETURN);
    instr->u.return_.src   = val;
    tl->u.function.body    = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Const: char 65"), std::string::npos);
}

TEST_F(TacDotTest, ConstUchar)
{
    Tac_TopLevel *tl       = make_empty_function("f", true);
    Tac_Const *c           = tac_new_const(TAC_CONST_UCHAR);
    c->u.uchar_val         = 200;
    Tac_Val *val           = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant        = c;
    Tac_Instruction *instr = tac_new_instruction(TAC_INSTRUCTION_RETURN);
    instr->u.return_.src   = val;
    tl->u.function.body    = instr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Const: uchar 200"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Type kinds
// ---------------------------------------------------------------------------

TEST_F(TacDotTest, TypePointer)
{
    Tac_TopLevel *tl              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
    tl->u.static_variable.name   = xstrdup("p");
    tl->u.static_variable.global = false;
    Tac_Type *ptr                 = tac_new_type(TAC_TYPE_POINTER);
    Tac_Type *inner               = tac_new_type(TAC_TYPE_INT);
    ptr->u.pointer.target_type   = inner;
    tl->u.static_variable.type   = ptr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Type: pointer"), std::string::npos);
    EXPECT_NE(out.find("Type: int"), std::string::npos);
    EXPECT_NE(out.find("label=\"target\""), std::string::npos);
}

TEST_F(TacDotTest, TypeArray)
{
    Tac_TopLevel *tl              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
    tl->u.static_variable.name   = xstrdup("arr");
    tl->u.static_variable.global = false;
    Tac_Type *arr                 = tac_new_type(TAC_TYPE_ARRAY);
    arr->u.array.size             = 10;
    arr->u.array.elem_type        = tac_new_type(TAC_TYPE_CHAR);
    tl->u.static_variable.type   = arr;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Type: array[10]"), std::string::npos);
    EXPECT_NE(out.find("label=\"elem_type\""), std::string::npos);
    EXPECT_NE(out.find("Type: char"), std::string::npos);
}

TEST_F(TacDotTest, TypeStructure)
{
    Tac_TopLevel *tl              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
    tl->u.static_variable.name   = xstrdup("obj");
    tl->u.static_variable.global = false;
    Tac_Type *s                   = tac_new_type(TAC_TYPE_STRUCTURE);
    s->u.structure.tag            = xstrdup("Point");
    tl->u.static_variable.type   = s;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Type: struct Point"), std::string::npos);
}

TEST_F(TacDotTest, TypeFunType)
{
    Tac_TopLevel *tl              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
    tl->u.static_variable.name   = xstrdup("fp");
    tl->u.static_variable.global = false;
    Tac_Type *ft                  = tac_new_type(TAC_TYPE_FUN_TYPE);
    ft->u.fun_type.ret_type       = tac_new_type(TAC_TYPE_INT);
    Tac_Type *param               = tac_new_type(TAC_TYPE_DOUBLE);
    ft->u.fun_type.param_types    = param;
    tl->u.static_variable.type   = ft;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Type: fun_type"), std::string::npos);
    EXPECT_NE(out.find("label=\"ret_type\""), std::string::npos);
    EXPECT_NE(out.find("label=\"param_type\""), std::string::npos);
}

// ---------------------------------------------------------------------------
// StaticInit kinds
// ---------------------------------------------------------------------------

TEST_F(TacDotTest, StaticInitI8)
{
    Tac_TopLevel *tl              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
    tl->u.static_variable.name   = xstrdup("v");
    tl->u.static_variable.global = false;
    tl->u.static_variable.type   = tac_new_type(TAC_TYPE_CHAR);
    Tac_StaticInit *init          = tac_new_static_init(TAC_STATIC_INIT_I8);
    init->u.char_val              = 10;
    tl->u.static_variable.init_list = init;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("StaticInit: i8 10"), std::string::npos);
}

TEST_F(TacDotTest, StaticInitI32)
{
    Tac_TopLevel *tl              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
    tl->u.static_variable.name   = xstrdup("v");
    tl->u.static_variable.global = false;
    tl->u.static_variable.type   = tac_new_type(TAC_TYPE_INT);
    Tac_StaticInit *init          = tac_new_static_init(TAC_STATIC_INIT_I32);
    init->u.int_val               = -100;
    tl->u.static_variable.init_list = init;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("StaticInit: i32 -100"), std::string::npos);
}

TEST_F(TacDotTest, StaticInitI64)
{
    Tac_TopLevel *tl              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
    tl->u.static_variable.name   = xstrdup("v");
    tl->u.static_variable.global = false;
    tl->u.static_variable.type   = tac_new_type(TAC_TYPE_LONG);
    Tac_StaticInit *init          = tac_new_static_init(TAC_STATIC_INIT_I64);
    init->u.long_val              = 1000000000LL;
    tl->u.static_variable.init_list = init;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("StaticInit: i64 1000000000"), std::string::npos);
}

TEST_F(TacDotTest, StaticInitU8)
{
    Tac_TopLevel *tl              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
    tl->u.static_variable.name   = xstrdup("v");
    tl->u.static_variable.global = false;
    tl->u.static_variable.type   = tac_new_type(TAC_TYPE_UCHAR);
    Tac_StaticInit *init          = tac_new_static_init(TAC_STATIC_INIT_U8);
    init->u.uchar_val             = 255;
    tl->u.static_variable.init_list = init;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("StaticInit: u8 255"), std::string::npos);
}

TEST_F(TacDotTest, StaticInitU32)
{
    Tac_TopLevel *tl              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
    tl->u.static_variable.name   = xstrdup("v");
    tl->u.static_variable.global = false;
    tl->u.static_variable.type   = tac_new_type(TAC_TYPE_UINT);
    Tac_StaticInit *init          = tac_new_static_init(TAC_STATIC_INIT_U32);
    init->u.uint_val              = 4000000000u;
    tl->u.static_variable.init_list = init;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("StaticInit: u32 4000000000"), std::string::npos);
}

TEST_F(TacDotTest, StaticInitU64)
{
    Tac_TopLevel *tl              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
    tl->u.static_variable.name   = xstrdup("v");
    tl->u.static_variable.global = false;
    tl->u.static_variable.type   = tac_new_type(TAC_TYPE_ULONG);
    Tac_StaticInit *init          = tac_new_static_init(TAC_STATIC_INIT_U64);
    init->u.ulong_val             = 9000000000ULL;
    tl->u.static_variable.init_list = init;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("StaticInit: u64 9000000000"), std::string::npos);
}

TEST_F(TacDotTest, StaticInitDouble)
{
    Tac_TopLevel *tl              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
    tl->u.static_variable.name   = xstrdup("v");
    tl->u.static_variable.global = false;
    tl->u.static_variable.type   = tac_new_type(TAC_TYPE_DOUBLE);
    Tac_StaticInit *init          = tac_new_static_init(TAC_STATIC_INIT_DOUBLE);
    init->u.double_val            = 2.5;
    tl->u.static_variable.init_list = init;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("StaticInit: double"), std::string::npos);
}

TEST_F(TacDotTest, StaticInitZero)
{
    Tac_TopLevel *tl              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
    tl->u.static_variable.name   = xstrdup("buf");
    tl->u.static_variable.global = false;
    tl->u.static_variable.type   = tac_new_type(TAC_TYPE_ARRAY);
    tl->u.static_variable.type->u.array.elem_type = tac_new_type(TAC_TYPE_CHAR);
    tl->u.static_variable.type->u.array.size      = 16;
    Tac_StaticInit *init          = tac_new_static_init(TAC_STATIC_INIT_ZERO);
    init->u.zero_bytes            = 16;
    tl->u.static_variable.init_list = init;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("StaticInit: zero 16"), std::string::npos);
}

TEST_F(TacDotTest, StaticInitString)
{
    Tac_TopLevel *tl              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_CONSTANT);
    tl->u.static_constant.name   = xstrdup("s");
    tl->u.static_constant.type   = tac_new_type(TAC_TYPE_ARRAY);
    tl->u.static_constant.type->u.array.elem_type = tac_new_type(TAC_TYPE_CHAR);
    tl->u.static_constant.type->u.array.size      = 6;
    Tac_StaticInit *init          = tac_new_static_init(TAC_STATIC_INIT_STRING);
    init->u.string.val            = xstrdup("hello");
    init->u.string.null_terminated = true;
    tl->u.static_constant.init   = init;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("StaticInit: string hello"), std::string::npos);
}

TEST_F(TacDotTest, StaticInitPointer)
{
    Tac_TopLevel *tl              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
    tl->u.static_variable.name   = xstrdup("ptr_init");
    tl->u.static_variable.global = false;
    tl->u.static_variable.type   = tac_new_type(TAC_TYPE_POINTER);
    tl->u.static_variable.type->u.pointer.target_type = tac_new_type(TAC_TYPE_CHAR);
    Tac_StaticInit *init          = tac_new_static_init(TAC_STATIC_INIT_POINTER);
    init->u.pointer_name          = xstrdup("str_const");
    tl->u.static_variable.init_list = init;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("StaticInit: pointer str_const"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Multiple instructions in a body
// ---------------------------------------------------------------------------

TEST_F(TacDotTest, MultipleInstructions)
{
    Tac_TopLevel *tl = make_empty_function("f", true);

    Tac_Instruction *i1 = tac_new_instruction(TAC_INSTRUCTION_COPY);
    i1->u.copy.src      = make_const_int(0);
    i1->u.copy.dst      = make_var("x");

    Tac_Instruction *i2 = tac_new_instruction(TAC_INSTRUCTION_LABEL);
    i2->u.label.name    = xstrdup("top");

    Tac_Instruction *i3 = tac_new_instruction(TAC_INSTRUCTION_RETURN);
    i3->u.return_.src   = make_var("x");

    i1->next = i2;
    i2->next = i3;
    tl->u.function.body = i1;

    std::string out = capture(tl);
    tac_free_toplevel(tl);

    EXPECT_NE(out.find("Copy"), std::string::npos);
    EXPECT_NE(out.find("Label: top"), std::string::npos);
    EXPECT_NE(out.find("Return"), std::string::npos);
}
