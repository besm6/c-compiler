#include <gtest/gtest.h>

extern "C" {
#include "optimize.h"
#include "xalloc.h"

// Exposed for direct unit-testing of the folding pass.
Tac_Instruction *constant_fold(Tac_Instruction *body);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Tac_Val *make_const_int(int v)
{
    Tac_Const *c  = tac_new_const(TAC_CONST_INT);
    c->u.int_val  = v;
    Tac_Val *val  = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant = c;
    return val;
}

static Tac_Val *make_var(const char *name)
{
    Tac_Val *val    = tac_new_val(TAC_VAL_VAR);
    val->u.var_name = xstrdup(name);
    return val;
}

static Tac_Instruction *make_unary(Tac_UnaryOperator op,
                                   Tac_Val *src, Tac_Val *dst)
{
    Tac_Instruction *i = tac_new_instruction(TAC_INSTRUCTION_UNARY);
    i->u.unary.op  = op;
    i->u.unary.src = src;
    i->u.unary.dst = dst;
    return i;
}

static Tac_Instruction *make_binary(Tac_BinaryOperator op,
                                    Tac_Val *src1, Tac_Val *src2, Tac_Val *dst)
{
    Tac_Instruction *i   = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    i->u.binary.op   = op;
    i->u.binary.src1 = src1;
    i->u.binary.src2 = src2;
    i->u.binary.dst  = dst;
    return i;
}

static Tac_Instruction *make_return(Tac_Val *src)
{
    Tac_Instruction *i = tac_new_instruction(TAC_INSTRUCTION_RETURN);
    i->u.return_.src   = src;
    return i;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(OptimizerTest, NullBodyReturnsNull) {
    EXPECT_EQ(optimize_function(nullptr, opt_flags_default()), nullptr);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// ~0  →  Copy(ConstInt(-1), t)
TEST(OptimizerTest, UnaryFoldComplement)
{
    Tac_Instruction *body = make_unary(TAC_UNARY_COMPLEMENT,
                                       make_const_int(0), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    ASSERT_NE(body->u.copy.src, nullptr);
    EXPECT_EQ(body->u.copy.src->kind, TAC_VAL_CONSTANT);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, -1);
    ASSERT_NE(body->u.copy.dst, nullptr);
    EXPECT_EQ(body->u.copy.dst->kind, TAC_VAL_VAR);
    EXPECT_STREQ(body->u.copy.dst->u.var_name, "t");

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// !0  →  Copy(ConstInt(1), t)
TEST(OptimizerTest, UnaryFoldNot0)
{
    Tac_Instruction *body = make_unary(TAC_UNARY_NOT,
                                       make_const_int(0), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 1);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// !5  →  Copy(ConstInt(0), t)
TEST(OptimizerTest, UnaryFoldNotNonzero)
{
    Tac_Instruction *body = make_unary(TAC_UNARY_NOT,
                                       make_const_int(5), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 0);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// -(3)  →  Copy(ConstInt(-3), t)
TEST(OptimizerTest, UnaryFoldNegate)
{
    Tac_Instruction *body = make_unary(TAC_UNARY_NEGATE,
                                       make_const_int(3), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, -3);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// Unary with Var src is not folded — instruction unchanged.
TEST(OptimizerTest, UnaryFoldVarUnchanged)
{
    Tac_Instruction *body = make_unary(TAC_UNARY_NOT,
                                       make_var("x"), make_var("t"));
    Tac_Instruction *orig = body;
    body = constant_fold(body);

    EXPECT_EQ(body, orig);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_UNARY);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// 3 + 4  →  Copy(ConstInt(7), t)
TEST(OptimizerTest, BinaryFoldAdd)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_ADD,
                                        make_const_int(3), make_const_int(4), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    ASSERT_NE(body->u.copy.src, nullptr);
    EXPECT_EQ(body->u.copy.src->kind, TAC_VAL_CONSTANT);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 7);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// 6 / 2  →  Copy(ConstInt(3), t)
TEST(OptimizerTest, BinaryFoldDivide)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_DIVIDE,
                                        make_const_int(6), make_const_int(2), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 3);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// 10 % 3  →  Copy(ConstInt(1), t)
TEST(OptimizerTest, BinaryFoldRemainder)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_REMAINDER,
                                        make_const_int(10), make_const_int(3), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 1);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// 6 / 0  →  instruction unchanged (division by zero)
TEST(OptimizerTest, BinaryFoldDivideByZero)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_DIVIDE,
                                        make_const_int(6), make_const_int(0), make_var("t"));
    Tac_Instruction *orig = body;
    body = constant_fold(body);

    EXPECT_EQ(body, orig);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_BINARY);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// 5 == 5  →  Copy(ConstInt(1), t)  — comparison always yields ConstInt
TEST(OptimizerTest, BinaryFoldEqual)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_EQUAL,
                                        make_const_int(5), make_const_int(5), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 1);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// 3 < 5  →  Copy(ConstInt(1), t)
TEST(OptimizerTest, BinaryFoldLessThan)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_LESS_THAN,
                                        make_const_int(3), make_const_int(5), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 1);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// 5 & 3  →  Copy(ConstInt(1), t)
TEST(OptimizerTest, BinaryFoldBitwiseAnd)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_BITWISE_AND,
                                        make_const_int(5), make_const_int(3), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 1);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// 1 << 3  →  Copy(ConstInt(8), t)
TEST(OptimizerTest, BinaryFoldLeftShift)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_LEFT_SHIFT,
                                        make_const_int(1), make_const_int(3), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 8);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// Var src — not folded, instruction unchanged.
TEST(OptimizerTest, BinaryFoldVarUnchanged)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_ADD,
                                        make_var("x"), make_const_int(1), make_var("t"));
    Tac_Instruction *orig = body;
    body = constant_fold(body);

    EXPECT_EQ(body, orig);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_BINARY);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// optimize_function on a constant binary folds it and reaches fixed point.
TEST(OptimizerTest, BinaryFixedPoint)
{
    // Body: Binary(ADD, ConstInt(3), ConstInt(4), Var("t")) → Return(Var("t"))
    Tac_Instruction *ret  = make_return(make_var("t"));
    Tac_Instruction *head = make_binary(TAC_BINARY_ADD,
                                        make_const_int(3), make_const_int(4), make_var("t"));
    head->next = ret;

    Tac_Instruction *result = optimize_function(head, opt_flags_default());

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(result->u.copy.src->u.constant->u.int_val, 7);

    tac_free_instruction(result);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// optimize_function on a constant unary folds it and reaches fixed point.
TEST(OptimizerTest, UnaryFixedPoint)
{
    // Body: Unary(NOT, ConstInt(0), Var("t")) → Return(Var("t"))
    Tac_Instruction *ret  = make_return(make_var("t"));
    Tac_Instruction *head = make_unary(TAC_UNARY_NOT,
                                       make_const_int(0), make_var("t"));
    head->next = ret;

    Tac_Instruction *result = optimize_function(head, opt_flags_default());

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(result->u.copy.src->u.constant->u.int_val, 1);

    tac_free_instruction(result);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}
