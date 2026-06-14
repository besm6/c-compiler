#include "optimizer_test_fixture.h"

// ---------------------------------------------------------------------------
// Null / error cases
// ---------------------------------------------------------------------------

TEST_F(OptimizerTest, NullBodyReturnsNull)
{
    EXPECT_EQ(optimize_function(nullptr, opt_flags_default(), nullptr), nullptr);
}

// ---------------------------------------------------------------------------
// Unary constant folding
// ---------------------------------------------------------------------------

// ~0  →  Copy(ConstInt(-1), t)
TEST_F(OptimizerTest, UnaryFoldComplement)
{
    Tac_Instruction *body = make_unary(TAC_UNARY_COMPLEMENT, make_const_int(0), make_var("t"));
    body                  = constant_fold(body);

    AssertFoldedInt(body, -1);
    ASSERT_NE(body->u.copy.dst, nullptr);
    EXPECT_EQ(body->u.copy.dst->kind, TAC_VAL_VAR);
    EXPECT_STREQ(body->u.copy.dst->u.var_name, "t");
}

// !0  →  Copy(ConstInt(1), t)
TEST_F(OptimizerTest, UnaryFoldNot0)
{
    Tac_Instruction *body = make_unary(TAC_UNARY_NOT, make_const_int(0), make_var("t"));
    body                  = constant_fold(body);

    AssertFoldedInt(body, 1);
}

// !5  →  Copy(ConstInt(0), t)
TEST_F(OptimizerTest, UnaryFoldNotNonzero)
{
    Tac_Instruction *body = make_unary(TAC_UNARY_NOT, make_const_int(5), make_var("t"));
    body                  = constant_fold(body);

    AssertFoldedInt(body, 0);
}

// -(3)  →  Copy(ConstInt(-3), t)
TEST_F(OptimizerTest, UnaryFoldNegate)
{
    Tac_Instruction *body = make_unary(TAC_UNARY_NEGATE, make_const_int(3), make_var("t"));
    body                  = constant_fold(body);

    AssertFoldedInt(body, -3);
}

// Unary with Var src is not folded — instruction unchanged.
TEST_F(OptimizerTest, UnaryFoldVarUnchanged)
{
    Tac_Instruction *body = make_unary(TAC_UNARY_NOT, make_var("x"), make_var("t"));
    Tac_Instruction *orig = body;
    body                  = constant_fold(body);

    EXPECT_EQ(body, orig);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_UNARY);
}

// ---------------------------------------------------------------------------
// Binary constant folding
// ---------------------------------------------------------------------------

// 3 + 4  →  Copy(ConstInt(7), t)
TEST_F(OptimizerTest, BinaryFoldAdd)
{
    Tac_Instruction *body =
        make_binary(TAC_BINARY_ADD, make_const_int(3), make_const_int(4), make_var("t"));
    body = constant_fold(body);

    AssertFoldedInt(body, 7);
}

// 6 / 2  →  Copy(ConstInt(3), t)
TEST_F(OptimizerTest, BinaryFoldDivide)
{
    Tac_Instruction *body =
        make_binary(TAC_BINARY_DIVIDE, make_const_int(6), make_const_int(2), make_var("t"));
    body = constant_fold(body);

    AssertFoldedInt(body, 3);
}

// 10 % 3  →  Copy(ConstInt(1), t)
TEST_F(OptimizerTest, BinaryFoldRemainder)
{
    Tac_Instruction *body =
        make_binary(TAC_BINARY_REMAINDER, make_const_int(10), make_const_int(3), make_var("t"));
    body = constant_fold(body);

    AssertFoldedInt(body, 1);
}

// 6 / 0  →  instruction unchanged (division by zero)
TEST_F(OptimizerTest, BinaryFoldDivideByZero)
{
    Tac_Instruction *body =
        make_binary(TAC_BINARY_DIVIDE, make_const_int(6), make_const_int(0), make_var("t"));
    Tac_Instruction *orig = body;
    body                  = constant_fold(body);

    EXPECT_EQ(body, orig);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_BINARY);
}

// 5 == 5  →  Copy(ConstInt(1), t)  — comparison always yields ConstInt
TEST_F(OptimizerTest, BinaryFoldEqual)
{
    Tac_Instruction *body =
        make_binary(TAC_BINARY_EQUAL, make_const_int(5), make_const_int(5), make_var("t"));
    body = constant_fold(body);

    AssertFoldedInt(body, 1);
}

// 3 < 5  →  Copy(ConstInt(1), t)
TEST_F(OptimizerTest, BinaryFoldLessThan)
{
    Tac_Instruction *body =
        make_binary(TAC_BINARY_LESS_THAN, make_const_int(3), make_const_int(5), make_var("t"));
    body = constant_fold(body);

    AssertFoldedInt(body, 1);
}

// 5 & 3  →  Copy(ConstInt(1), t)
TEST_F(OptimizerTest, BinaryFoldBitwiseAnd)
{
    Tac_Instruction *body =
        make_binary(TAC_BINARY_BITWISE_AND, make_const_int(5), make_const_int(3), make_var("t"));
    body = constant_fold(body);

    AssertFoldedInt(body, 1);
}

// 1 << 3  →  Copy(ConstInt(8), t)
TEST_F(OptimizerTest, BinaryFoldLeftShift)
{
    Tac_Instruction *body =
        make_binary(TAC_BINARY_LEFT_SHIFT, make_const_int(1), make_const_int(3), make_var("t"));
    body = constant_fold(body);

    AssertFoldedInt(body, 8);
}

// Var src — not folded, instruction unchanged.
TEST_F(OptimizerTest, BinaryFoldVarUnchanged)
{
    Tac_Instruction *body =
        make_binary(TAC_BINARY_ADD, make_var("x"), make_const_int(1), make_var("t"));
    Tac_Instruction *orig = body;
    body                  = constant_fold(body);

    EXPECT_EQ(body, orig);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_BINARY);
}

// optimize_function on a constant binary folds it and reaches fixed point.
// const_fold: Binary(ADD,3,4,t) → Copy(7,t); copy_prop: Return(7); dead_store: Copy removed.
TEST_F(OptimizerTest, BinaryFixedPoint)
{
    // Body: Binary(ADD, ConstInt(3), ConstInt(4), Var("t")) → Return(Var("t"))
    Tac_Instruction *ret = make_return(make_var("t"));
    Tac_Instruction *head =
        make_binary(TAC_BINARY_ADD, make_const_int(3), make_const_int(4), make_var("t"));
    head->next = ret;

    Tac_Instruction *result = optimize_function(head, opt_flags_default(), nullptr);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->kind, TAC_INSTRUCTION_RETURN);
    EXPECT_EQ(result->u.return_.src->u.constant->u.int_val, 7);
}

// optimize_function on a constant unary folds it and reaches fixed point.
// const_fold: Unary(NOT,0,t) → Copy(1,t); copy_prop: Return(1); dead_store: Copy removed.
TEST_F(OptimizerTest, UnaryFixedPoint)
{
    // Body: Unary(NOT, ConstInt(0), Var("t")) → Return(Var("t"))
    Tac_Instruction *ret  = make_return(make_var("t"));
    Tac_Instruction *head = make_unary(TAC_UNARY_NOT, make_const_int(0), make_var("t"));
    head->next            = ret;

    Tac_Instruction *result = optimize_function(head, opt_flags_default(), nullptr);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->kind, TAC_INSTRUCTION_RETURN);
    EXPECT_EQ(result->u.return_.src->u.constant->u.int_val, 1);
}

// ---------------------------------------------------------------------------
// Floating-point constant folding
// ---------------------------------------------------------------------------

// 1.5f + 2.5f  →  Copy(ConstFloat(4.0), t)
TEST_F(OptimizerTest, BinaryFoldFloatAdd)
{
    Tac_Instruction *body =
        make_binary(TAC_BINARY_ADD, make_const_float(1.5), make_const_float(2.5), make_var("t"));
    body = constant_fold(body);

    AssertFoldedFloat(body, 4.0);
}

// 5.0 - 3.0  →  Copy(ConstDouble(2.0), t)
TEST_F(OptimizerTest, BinaryFoldDoubleSubtract)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_SUBTRACT, make_const_double(5.0),
                                        make_const_double(3.0), make_var("t"));
    body                  = constant_fold(body);

    AssertFoldedDouble(body, 2.0);
}

// 2.0 * 3.0  →  Copy(ConstDouble(6.0), t)
TEST_F(OptimizerTest, BinaryFoldDoubleMultiply)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_MULTIPLY, make_const_double(2.0),
                                        make_const_double(3.0), make_var("t"));
    body                  = constant_fold(body);

    AssertFoldedDouble(body, 6.0);
}

// 1.0 / 0.0  →  Copy(ConstDouble(inf), t)  — IEEE division by zero is defined, never skipped.
TEST_F(OptimizerTest, BinaryFoldDoubleDivideByZero)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_DIVIDE, make_const_double(1.0),
                                        make_const_double(0.0), make_var("t"));
    body                  = constant_fold(body);

    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_DOUBLE);
}

// 3.0 == 3.0  →  Copy(ConstInt(1), t)  — comparison always yields ConstInt
TEST_F(OptimizerTest, BinaryFoldDoubleEqual)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_EQUAL, make_const_double(3.0),
                                        make_const_double(3.0), make_var("t"));
    body                  = constant_fold(body);

    AssertFoldedInt(body, 1);
}

// 1.5L + 2.5L  →  Copy(ConstLongDouble(4.0L), t)
TEST_F(OptimizerTest, BinaryFoldLongDoubleAdd)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_ADD, make_const_long_double(1.5L),
                                        make_const_long_double(2.5L), make_var("t"));
    body                  = constant_fold(body);

    AssertFoldedLongDouble(body, 4.0);
}

// The FP-specific binary ops the translator now emits for double/float operands fold
// exactly like their plain counterparts.  1.5 * 2.5 → Copy(ConstDouble(3.75), t).
TEST_F(OptimizerTest, BinaryFoldDoubleOpVariants)
{
    Tac_Instruction *add = constant_fold(make_binary(
        TAC_BINARY_ADD_DOUBLE, make_const_double(1.5), make_const_double(2.5), make_var("t")));
    AssertFoldedDouble(add, 4.0);

    Tac_Instruction *sub = constant_fold(make_binary(
        TAC_BINARY_SUBTRACT_DOUBLE, make_const_double(5.0), make_const_double(3.0), make_var("t")));
    AssertFoldedDouble(sub, 2.0);

    Tac_Instruction *mul = constant_fold(make_binary(
        TAC_BINARY_MULTIPLY_DOUBLE, make_const_double(1.5), make_const_double(2.5), make_var("t")));
    AssertFoldedDouble(mul, 3.75);

    Tac_Instruction *div = constant_fold(make_binary(
        TAC_BINARY_DIVIDE_DOUBLE, make_const_double(7.0), make_const_double(2.0), make_var("t")));
    AssertFoldedDouble(div, 3.5);
}
