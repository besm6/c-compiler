#include <gtest/gtest.h>

extern "C" {
#include "optimize.h"
#include "cfg.h"
#include "xalloc.h"

// Exposed for direct unit-testing of the folding pass.
Tac_Instruction *constant_fold(Tac_Instruction *body);
void eliminate_unreachable(OptCfg *cfg);
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

static Tac_Val *make_const_float(double v)
{
    Tac_Const *c   = tac_new_const(TAC_CONST_FLOAT);
    c->u.float_val = v;
    Tac_Val *val   = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant = c;
    return val;
}

static Tac_Val *make_const_double(double v)
{
    Tac_Const *c    = tac_new_const(TAC_CONST_DOUBLE);
    c->u.double_val = v;
    Tac_Val *val    = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant = c;
    return val;
}

static Tac_Val *make_const_long_double(long double v)
{
    Tac_Const *c         = tac_new_const(TAC_CONST_LONG_DOUBLE);
    c->u.long_double_val = v;
    Tac_Val *val         = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant      = c;
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

static Tac_Val *make_const_char(int v)
{
    Tac_Const *c   = tac_new_const(TAC_CONST_CHAR);
    c->u.char_val  = (int)(int8_t)v;
    Tac_Val *val   = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant = c;
    return val;
}

static Tac_Val *make_const_uchar(unsigned char v)
{
    Tac_Const *c    = tac_new_const(TAC_CONST_UCHAR);
    c->u.uchar_val  = v;
    Tac_Val *val    = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant = c;
    return val;
}

static Tac_Val *make_const_long(long v)
{
    Tac_Const *c   = tac_new_const(TAC_CONST_LONG);
    c->u.long_val  = v;
    Tac_Val *val   = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant = c;
    return val;
}

static Tac_Val *make_const_long_long(long long v)
{
    Tac_Const *c        = tac_new_const(TAC_CONST_LONG_LONG);
    c->u.long_long_val  = v;
    Tac_Val *val        = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant     = c;
    return val;
}

static Tac_Val *make_const_uint(unsigned v)
{
    Tac_Const *c   = tac_new_const(TAC_CONST_UINT);
    c->u.uint_val  = v;
    Tac_Val *val   = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant = c;
    return val;
}

static Tac_Val *make_const_ulong(unsigned long v)
{
    Tac_Const *c   = tac_new_const(TAC_CONST_ULONG);
    c->u.ulong_val = v;
    Tac_Val *val   = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant = c;
    return val;
}

static Tac_Val *make_const_ulong_long(unsigned long long v)
{
    Tac_Const *c         = tac_new_const(TAC_CONST_ULONG_LONG);
    c->u.ulong_long_val  = v;
    Tac_Val *val         = tac_new_val(TAC_VAL_CONSTANT);
    val->u.constant      = c;
    return val;
}

// All 21 conversion instructions share the same {src, dst} layout; sign_extend is the proxy.
static Tac_Instruction *make_conversion(Tac_InstructionKind kind,
                                        Tac_Val *src, Tac_Val *dst)
{
    Tac_Instruction *i   = tac_new_instruction(kind);
    i->u.sign_extend.src = src;
    i->u.sign_extend.dst = dst;
    return i;
}

static Tac_Instruction *make_jump_if_zero(Tac_Val *cond, const char *target)
{
    Tac_Instruction *i          = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
    i->u.jump_if_zero.condition = cond;
    i->u.jump_if_zero.target    = xstrdup(target);
    return i;
}

static Tac_Instruction *make_jump_if_not_zero(Tac_Val *cond, const char *target)
{
    Tac_Instruction *i              = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_NOT_ZERO);
    i->u.jump_if_not_zero.condition = cond;
    i->u.jump_if_not_zero.target    = xstrdup(target);
    return i;
}

static Tac_Instruction *make_label(const char *name)
{
    Tac_Instruction *i = tac_new_instruction(TAC_INSTRUCTION_LABEL);
    i->u.label.name    = xstrdup(name);
    return i;
}

static Tac_Instruction *make_jump(const char *target)
{
    Tac_Instruction *i = tac_new_instruction(TAC_INSTRUCTION_JUMP);
    i->u.jump.target   = xstrdup(target);
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

// 1.5f + 2.5f  →  Copy(ConstFloat(4.0), t)
TEST(OptimizerTest, BinaryFoldFloatAdd)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_ADD,
                                        make_const_float(1.5), make_const_float(2.5), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    ASSERT_NE(body->u.copy.src, nullptr);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_FLOAT);
    EXPECT_DOUBLE_EQ(body->u.copy.src->u.constant->u.float_val, 4.0);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// 5.0 - 3.0  →  Copy(ConstDouble(2.0), t)
TEST(OptimizerTest, BinaryFoldDoubleSubtract)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_SUBTRACT,
                                        make_const_double(5.0), make_const_double(3.0), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_DOUBLE);
    EXPECT_DOUBLE_EQ(body->u.copy.src->u.constant->u.double_val, 2.0);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// 2.0 * 3.0  →  Copy(ConstDouble(6.0), t)
TEST(OptimizerTest, BinaryFoldDoubleMultiply)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_MULTIPLY,
                                        make_const_double(2.0), make_const_double(3.0), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_DOUBLE);
    EXPECT_DOUBLE_EQ(body->u.copy.src->u.constant->u.double_val, 6.0);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// 1.0 / 0.0  →  Copy(ConstDouble(inf), t)  — IEEE division by zero is defined, never skipped.
TEST(OptimizerTest, BinaryFoldDoubleDivideByZero)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_DIVIDE,
                                        make_const_double(1.0), make_const_double(0.0), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_DOUBLE);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// 3.0 == 3.0  →  Copy(ConstInt(1), t)  — comparison always yields ConstInt
TEST(OptimizerTest, BinaryFoldDoubleEqual)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_EQUAL,
                                        make_const_double(3.0), make_const_double(3.0), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 1);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// 1.5L + 2.5L  →  Copy(ConstLongDouble(4.0L), t)
TEST(OptimizerTest, BinaryFoldLongDoubleAdd)
{
    Tac_Instruction *body = make_binary(TAC_BINARY_ADD,
                                        make_const_long_double(1.5L), make_const_long_double(2.5L),
                                        make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_LONG_DOUBLE);
    EXPECT_DOUBLE_EQ((double)body->u.copy.src->u.constant->u.long_double_val, 4.0);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// ---------------------------------------------------------------------------
// Type conversion folding tests (task 9)
// ---------------------------------------------------------------------------

// --- SIGN_EXTEND ---

// SignExtend(ConstInt(3))  →  Copy(ConstLong(3), t)
TEST(OptimizerTest, ConvSignExtendIntToLong)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_SIGN_EXTEND,
                                            make_const_int(3), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    ASSERT_NE(body->u.copy.src, nullptr);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_LONG);
    EXPECT_EQ(body->u.copy.src->u.constant->u.long_val, 3L);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// SignExtend(ConstInt(-5))  →  Copy(ConstLong(-5), t)  — negative preserves sign
TEST(OptimizerTest, ConvSignExtendIntNegToLong)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_SIGN_EXTEND,
                                            make_const_int(-5), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_LONG);
    EXPECT_EQ(body->u.copy.src->u.constant->u.long_val, -5L);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// SignExtend(ConstChar(127))  →  Copy(ConstInt(127), t)
TEST(OptimizerTest, ConvSignExtendCharToInt)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_SIGN_EXTEND,
                                            make_const_char(127), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 127);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// SignExtend(ConstChar(-128))  →  Copy(ConstInt(-128), t)  — min negative char
TEST(OptimizerTest, ConvSignExtendCharNegToInt)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_SIGN_EXTEND,
                                            make_const_char(-128), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, -128);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// SignExtend(ConstLong(42))  →  Copy(ConstLongLong(42), t)
TEST(OptimizerTest, ConvSignExtendLongToLongLong)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_SIGN_EXTEND,
                                            make_const_long(42L), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_LONG_LONG);
    EXPECT_EQ(body->u.copy.src->u.constant->u.long_long_val, 42LL);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// SignExtend(Var)  →  instruction unchanged
TEST(OptimizerTest, ConvSignExtendVarUnchanged)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_SIGN_EXTEND,
                                            make_var("x"), make_var("t"));
    Tac_Instruction *orig = body;
    body = constant_fold(body);

    EXPECT_EQ(body, orig);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_SIGN_EXTEND);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// SignExtend(ConstFloat)  →  instruction unchanged (wrong src type)
TEST(OptimizerTest, ConvSignExtendWrongTypeUnchanged)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_SIGN_EXTEND,
                                            make_const_float(1.0), make_var("t"));
    Tac_Instruction *orig = body;
    body = constant_fold(body);

    EXPECT_EQ(body, orig);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_SIGN_EXTEND);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// --- ZERO_EXTEND ---

// ZeroExtend(ConstUChar(200))  →  Copy(ConstUInt(200), t)  — >127 shows zero-extension
TEST(OptimizerTest, ConvZeroExtendUCharToUInt)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_ZERO_EXTEND,
                                            make_const_uchar(200), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_UINT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.uint_val, 200u);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// ZeroExtend(ConstUInt(100000))  →  Copy(ConstULong(100000), t)
TEST(OptimizerTest, ConvZeroExtendUIntToULong)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_ZERO_EXTEND,
                                            make_const_uint(100000u), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_ULONG);
    EXPECT_EQ(body->u.copy.src->u.constant->u.ulong_val, 100000UL);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// ZeroExtend(ConstULong(1234567890))  →  Copy(ConstULongLong(1234567890), t)
TEST(OptimizerTest, ConvZeroExtendULongToULongLong)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_ZERO_EXTEND,
                                            make_const_ulong(1234567890UL), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_ULONG_LONG);
    EXPECT_EQ(body->u.copy.src->u.constant->u.ulong_long_val, 1234567890ULL);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// ZeroExtend(Var)  →  instruction unchanged
TEST(OptimizerTest, ConvZeroExtendVarUnchanged)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_ZERO_EXTEND,
                                            make_var("x"), make_var("t"));
    Tac_Instruction *orig = body;
    body = constant_fold(body);

    EXPECT_EQ(body, orig);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_ZERO_EXTEND);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// --- TRUNCATE ---

// Truncate(ConstLong(263))  →  Copy(ConstInt(263), t)  — value fits in 32 bits
TEST(OptimizerTest, ConvTruncateLongToInt)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_TRUNCATE,
                                            make_const_long(263L), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 263);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// Truncate(ConstLong(0x100000007))  →  Copy(ConstInt(7), t)  — high bits cut off
TEST(OptimizerTest, ConvTruncateLongHighBitsToInt)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_TRUNCATE,
                                            make_const_long(0x100000007L), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 7);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// Truncate(ConstLongLong(0x100000009))  →  Copy(ConstInt(9), t)
TEST(OptimizerTest, ConvTruncateLongLongToInt)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_TRUNCATE,
                                            make_const_long_long(0x100000009LL), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 9);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// Truncate(ConstInt(263))  →  Copy(ConstChar(7), t)  — (int8_t)(263 & 0xFF) = 7
TEST(OptimizerTest, ConvTruncateIntToChar)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_TRUNCATE,
                                            make_const_int(263), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_CHAR);
    EXPECT_EQ(body->u.copy.src->u.constant->u.char_val, 7);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// Truncate(ConstInt(-1))  →  Copy(ConstChar(-1), t)  — negative preserved
TEST(OptimizerTest, ConvTruncateIntNegToChar)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_TRUNCATE,
                                            make_const_int(-1), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_CHAR);
    EXPECT_EQ(body->u.copy.src->u.constant->u.char_val, -1);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// Truncate(ConstULongLong(0x100000003))  →  Copy(ConstUInt(3), t)
TEST(OptimizerTest, ConvTruncateULongLongToUInt)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_TRUNCATE,
                                            make_const_ulong_long(0x100000003ULL), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_UINT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.uint_val, 3u);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// Truncate(ConstULong(0x100000005))  →  Copy(ConstUInt(5), t)
TEST(OptimizerTest, ConvTruncateULongToUInt)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_TRUNCATE,
                                            make_const_ulong(0x100000005UL), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_UINT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.uint_val, 5u);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// Truncate(ConstUInt(300))  →  Copy(ConstUChar(44), t)  — 300 & 0xFF = 44
TEST(OptimizerTest, ConvTruncateUIntToUChar)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_TRUNCATE,
                                            make_const_uint(300u), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_UCHAR);
    EXPECT_EQ(body->u.copy.src->u.constant->u.uchar_val, (unsigned char)44);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// Truncate(Var)  →  instruction unchanged
TEST(OptimizerTest, ConvTruncateVarUnchanged)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_TRUNCATE,
                                            make_var("x"), make_var("t"));
    Tac_Instruction *orig = body;
    body = constant_fold(body);

    EXPECT_EQ(body, orig);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_TRUNCATE);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// --- Integer → floating-point ---

// IntToDouble(ConstInt(42))  →  Copy(ConstDouble(42.0), t)
TEST(OptimizerTest, ConvIntToDouble)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_INT_TO_DOUBLE,
                                            make_const_int(42), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_DOUBLE);
    EXPECT_DOUBLE_EQ(body->u.copy.src->u.constant->u.double_val, 42.0);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// IntToDouble(ConstInt(-7))  →  Copy(ConstDouble(-7.0), t)
TEST(OptimizerTest, ConvIntNegToDouble)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_INT_TO_DOUBLE,
                                            make_const_int(-7), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_DOUBLE);
    EXPECT_DOUBLE_EQ(body->u.copy.src->u.constant->u.double_val, -7.0);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// UIntToDouble(ConstUInt(1000000))  →  Copy(ConstDouble(1e6), t)
TEST(OptimizerTest, ConvUIntToDouble)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_UINT_TO_DOUBLE,
                                            make_const_uint(1000000u), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_DOUBLE);
    EXPECT_DOUBLE_EQ(body->u.copy.src->u.constant->u.double_val, 1e6);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// IntToFloat(ConstInt(5))  →  Copy(ConstFloat(5.0), t)
TEST(OptimizerTest, ConvIntToFloat)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_INT_TO_FLOAT,
                                            make_const_int(5), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_FLOAT);
    EXPECT_DOUBLE_EQ(body->u.copy.src->u.constant->u.float_val, 5.0);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// UIntToFloat(ConstUInt(100))  →  Copy(ConstFloat(100.0), t)
TEST(OptimizerTest, ConvUIntToFloat)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_UINT_TO_FLOAT,
                                            make_const_uint(100u), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_FLOAT);
    EXPECT_DOUBLE_EQ(body->u.copy.src->u.constant->u.float_val, 100.0);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// IntToLongDouble(ConstInt(3))  →  Copy(ConstLongDouble(3.0L), t)
TEST(OptimizerTest, ConvIntToLongDouble)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_INT_TO_LONG_DOUBLE,
                                            make_const_int(3), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_LONG_DOUBLE);
    EXPECT_DOUBLE_EQ((double)body->u.copy.src->u.constant->u.long_double_val, 3.0);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// UIntToLongDouble(ConstUInt(7))  →  Copy(ConstLongDouble(7.0L), t)
TEST(OptimizerTest, ConvUIntToLongDouble)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE,
                                            make_const_uint(7u), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_LONG_DOUBLE);
    EXPECT_DOUBLE_EQ((double)body->u.copy.src->u.constant->u.long_double_val, 7.0);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// IntToDouble(ConstFloat)  →  instruction unchanged (wrong src type)
TEST(OptimizerTest, ConvIntToDoubleWrongSrcUnchanged)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_INT_TO_DOUBLE,
                                            make_const_float(1.0), make_var("t"));
    Tac_Instruction *orig = body;
    body = constant_fold(body);

    EXPECT_EQ(body, orig);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_INT_TO_DOUBLE);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// --- Floating-point → integer (truncate toward zero) ---

// DoubleToInt(ConstDouble(3.7))  →  Copy(ConstInt(3), t)
TEST(OptimizerTest, ConvDoubleToInt)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_DOUBLE_TO_INT,
                                            make_const_double(3.7), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 3);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// DoubleToInt(ConstDouble(-3.7))  →  Copy(ConstInt(-3), t)  — truncation toward zero
TEST(OptimizerTest, ConvDoubleNegToInt)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_DOUBLE_TO_INT,
                                            make_const_double(-3.7), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, -3);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// DoubleToInt(ConstFloat(2.9))  →  Copy(ConstInt(2), t)  — float src also accepted
TEST(OptimizerTest, ConvDoubleToIntFromFloat)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_DOUBLE_TO_INT,
                                            make_const_float(2.9), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 2);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// FloatToInt(ConstFloat(2.9))  →  Copy(ConstInt(2), t)
TEST(OptimizerTest, ConvFloatToInt)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_FLOAT_TO_INT,
                                            make_const_float(2.9), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 2);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// FloatToInt(ConstFloat(-1.9))  →  Copy(ConstInt(-1), t)  — truncation toward zero
TEST(OptimizerTest, ConvFloatNegToInt)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_FLOAT_TO_INT,
                                            make_const_float(-1.9), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, -1);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// DoubleToUInt(ConstDouble(3.9))  →  Copy(ConstUInt(3), t)
TEST(OptimizerTest, ConvDoubleToUInt)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_DOUBLE_TO_UINT,
                                            make_const_double(3.9), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_UINT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.uint_val, 3u);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// FloatToUInt(ConstFloat(100.1))  →  Copy(ConstUInt(100), t)
TEST(OptimizerTest, ConvFloatToUInt)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_FLOAT_TO_UINT,
                                            make_const_float(100.1), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_UINT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.uint_val, 100u);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// LongDoubleToInt(ConstLongDouble(4.5L))  →  Copy(ConstInt(4), t)
TEST(OptimizerTest, ConvLongDoubleToInt)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_LONG_DOUBLE_TO_INT,
                                            make_const_long_double(4.5L), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, 4);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// LongDoubleToInt(ConstLongDouble(-9.9L))  →  Copy(ConstInt(-9), t)
TEST(OptimizerTest, ConvLongDoubleNegToInt)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_LONG_DOUBLE_TO_INT,
                                            make_const_long_double(-9.9L), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_INT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.int_val, -9);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// LongDoubleToUInt(ConstLongDouble(9.9L))  →  Copy(ConstUInt(9), t)
TEST(OptimizerTest, ConvLongDoubleToUInt)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT,
                                            make_const_long_double(9.9L), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_UINT);
    EXPECT_EQ(body->u.copy.src->u.constant->u.uint_val, 9u);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// DoubleToInt(ConstInt)  →  instruction unchanged (wrong src type)
TEST(OptimizerTest, ConvDoubleToIntWrongSrcUnchanged)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_DOUBLE_TO_INT,
                                            make_const_int(3), make_var("t"));
    Tac_Instruction *orig = body;
    body = constant_fold(body);

    EXPECT_EQ(body, orig);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_DOUBLE_TO_INT);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// --- Float ↔ float ---

// FloatToDouble(ConstFloat(1.5))  →  Copy(ConstDouble(1.5), t)
TEST(OptimizerTest, ConvFloatToDouble)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_FLOAT_TO_DOUBLE,
                                            make_const_float(1.5), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_DOUBLE);
    EXPECT_DOUBLE_EQ(body->u.copy.src->u.constant->u.double_val, 1.5);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// FloatToDouble(ConstDouble)  →  instruction unchanged (wrong src type)
TEST(OptimizerTest, ConvFloatToDoubleWrongSrcUnchanged)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_FLOAT_TO_DOUBLE,
                                            make_const_double(1.0), make_var("t"));
    Tac_Instruction *orig = body;
    body = constant_fold(body);

    EXPECT_EQ(body, orig);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_FLOAT_TO_DOUBLE);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// DoubleToFloat(ConstDouble(2.0))  →  Copy(ConstFloat(2.0), t)
TEST(OptimizerTest, ConvDoubleToFloat)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_DOUBLE_TO_FLOAT,
                                            make_const_double(2.0), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_FLOAT);
    EXPECT_DOUBLE_EQ(body->u.copy.src->u.constant->u.float_val, 2.0);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// FloatToLongDouble(ConstFloat(3.14))  →  Copy(ConstLongDouble(≈3.14), t)
TEST(OptimizerTest, ConvFloatToLongDouble)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE,
                                            make_const_float(3.14), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_LONG_DOUBLE);
    EXPECT_NEAR((double)body->u.copy.src->u.constant->u.long_double_val, 3.14, 1e-5);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// LongDoubleToFloat(ConstLongDouble(2.5L))  →  Copy(ConstFloat(2.5), t)
TEST(OptimizerTest, ConvLongDoubleToFloat)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT,
                                            make_const_long_double(2.5L), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_FLOAT);
    EXPECT_DOUBLE_EQ(body->u.copy.src->u.constant->u.float_val, 2.5);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// DoubleToLongDouble(ConstDouble(1.5))  →  Copy(ConstLongDouble(1.5L), t)
TEST(OptimizerTest, ConvDoubleToLongDouble)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE,
                                            make_const_double(1.5), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_LONG_DOUBLE);
    EXPECT_DOUBLE_EQ((double)body->u.copy.src->u.constant->u.long_double_val, 1.5);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// LongDoubleToDouble(ConstLongDouble(1.5L))  →  Copy(ConstDouble(1.5), t)
TEST(OptimizerTest, ConvLongDoubleToDouble)
{
    Tac_Instruction *body = make_conversion(TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE,
                                            make_const_long_double(1.5L), make_var("t"));
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_COPY);
    EXPECT_EQ(body->u.copy.src->u.constant->kind, TAC_CONST_DOUBLE);
    EXPECT_DOUBLE_EQ(body->u.copy.src->u.constant->u.double_val, 1.5);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// JumpIfZero(ConstInt(0), "T")  →  Jump("T")
TEST(OptimizerTest, JumpFoldJIZZero)
{
    Tac_Instruction *body = make_jump_if_zero(make_const_int(0), "T");
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_JUMP);
    EXPECT_STREQ(body->u.jump.target, "T");

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// JumpIfZero(ConstInt(1), "T")  →  deleted
TEST(OptimizerTest, JumpFoldJIZNonzero)
{
    Tac_Instruction *ret  = make_return(nullptr);
    Tac_Instruction *body = make_jump_if_zero(make_const_int(1), "T");
    body->next = ret;
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_RETURN);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// JumpIfNotZero(ConstInt(1), "T")  →  Jump("T")
TEST(OptimizerTest, JumpFoldJINZNonzero)
{
    Tac_Instruction *body = make_jump_if_not_zero(make_const_int(1), "T");
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_JUMP);
    EXPECT_STREQ(body->u.jump.target, "T");

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// JumpIfNotZero(ConstInt(0), "T")  →  deleted
TEST(OptimizerTest, JumpFoldJINZZero)
{
    Tac_Instruction *ret  = make_return(nullptr);
    Tac_Instruction *body = make_jump_if_not_zero(make_const_int(0), "T");
    body->next = ret;
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_RETURN);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// JumpIfZero(ConstDouble(0.0), "T")  →  Jump("T")
TEST(OptimizerTest, JumpFoldJIZDoubleZero)
{
    Tac_Instruction *body = make_jump_if_zero(make_const_double(0.0), "T");
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_JUMP);
    EXPECT_STREQ(body->u.jump.target, "T");

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// JumpIfZero(Var("x"), "T")  →  unchanged
TEST(OptimizerTest, JumpFoldJIZVarUnchanged)
{
    Tac_Instruction *body = make_jump_if_zero(make_var("x"), "T");
    body = constant_fold(body);

    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->kind, TAC_INSTRUCTION_JUMP_IF_ZERO);
    EXPECT_EQ(body->u.jump_if_zero.condition->kind, TAC_VAL_VAR);

    tac_free_instruction(body);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// Label("fn") → Return(Var("x")) → Return(NULL)  →  backstop Return(NULL) removed.
TEST(OptimizerTest, UnreachableBackstopReturn)
{
    Tac_Instruction *lbl  = make_label("fn");
    Tac_Instruction *ret1 = make_return(make_var("x"));
    Tac_Instruction *ret2 = make_return(nullptr);
    lbl->next  = ret1;
    ret1->next = ret2;

    OptCfg *cfg = cfg_build(lbl);
    eliminate_unreachable(cfg);
    Tac_Instruction *result = cfg_flatten(cfg);
    cfg_free(cfg);

    // Expect: Label("fn") → Return(Var("x")), nothing after.
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->kind, TAC_INSTRUCTION_LABEL);
    ASSERT_NE(result->next, nullptr);
    EXPECT_EQ(result->next->kind, TAC_INSTRUCTION_RETURN);
    ASSERT_NE(result->next->u.return_.src, nullptr);
    EXPECT_EQ(result->next->u.return_.src->kind, TAC_VAL_VAR);
    EXPECT_EQ(result->next->next, nullptr);

    tac_free_instruction(result);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}

// Label("fn") → Jump("End") → Return(Var("x")) → Label("End") → Return(NULL)
// The block containing Return(Var("x")) is unreachable.
TEST(OptimizerTest, UnreachableDeadBranch)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *jmp   = make_jump("End");
    Tac_Instruction *ret_x = make_return(make_var("x"));
    Tac_Instruction *lbl   = make_label("End");
    Tac_Instruction *ret0  = make_return(nullptr);
    entry->next = jmp;
    jmp->next   = ret_x;
    ret_x->next = lbl;
    lbl->next   = ret0;

    OptCfg *cfg = cfg_build(entry);
    eliminate_unreachable(cfg);
    Tac_Instruction *result = cfg_flatten(cfg);
    cfg_free(cfg);

    // Dead branch (Return(Var("x"))) must be gone.
    for (Tac_Instruction *i = result; i; i = i->next)
        EXPECT_FALSE(i->kind == TAC_INSTRUCTION_RETURN && i->u.return_.src != nullptr);

    tac_free_instruction(result);
    EXPECT_EQ(xtotal_allocated_size(), 0);
}
