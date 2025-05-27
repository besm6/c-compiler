#include "fixture.h"

// Test unary operator: &x
TEST_F(ParserTest, ParseUnaryAddress)
{
    DeclOrStmt *body = GetFunctionBody("void f() { &x; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_UNARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(UNARY_ADDRESS, stmt->u.expr->u.unary_op.op);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.unary_op.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.unary_op.expr->u.var);
}

// Test unary operator: *x
TEST_F(ParserTest, ParseUnaryDeref)
{
    DeclOrStmt *body = GetFunctionBody("void f() { *x; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_UNARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(UNARY_DEREF, stmt->u.expr->u.unary_op.op);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.unary_op.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.unary_op.expr->u.var);
}

// Test unary operator: +x
TEST_F(ParserTest, ParseUnaryPlus)
{
    DeclOrStmt *body = GetFunctionBody("void f() { +x; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_UNARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(UNARY_PLUS, stmt->u.expr->u.unary_op.op);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.unary_op.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.unary_op.expr->u.var);
}

// Test unary operator: -x
TEST_F(ParserTest, ParseUnaryNeg)
{
    DeclOrStmt *body = GetFunctionBody("void f() { -x; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_UNARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(UNARY_NEG, stmt->u.expr->u.unary_op.op);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.unary_op.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.unary_op.expr->u.var);
}

// Test unary operator: ~x
TEST_F(ParserTest, ParseUnaryBitNot)
{
    DeclOrStmt *body = GetFunctionBody("void f() { ~x; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_UNARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(UNARY_BIT_NOT, stmt->u.expr->u.unary_op.op);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.unary_op.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.unary_op.expr->u.var);
}

// Test unary operator: !x
TEST_F(ParserTest, ParseUnaryLogNot)
{
    DeclOrStmt *body = GetFunctionBody("void f() { !x; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_UNARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(UNARY_LOG_NOT, stmt->u.expr->u.unary_op.op);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.unary_op.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.unary_op.expr->u.var);
}

// Test unary operator: ++x
TEST_F(ParserTest, ParseUnaryPreInc)
{
    DeclOrStmt *body = GetFunctionBody("void f() { ++x; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_UNARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(UNARY_PRE_INC, stmt->u.expr->u.unary_op.op);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.unary_op.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.unary_op.expr->u.var);
}

// Test unary operator: --x
TEST_F(ParserTest, ParseUnaryPreDec)
{
    DeclOrStmt *body = GetFunctionBody("void f() { --x; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_UNARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(UNARY_PRE_DEC, stmt->u.expr->u.unary_op.op);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.unary_op.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.unary_op.expr->u.var);
}

// Test unary operator: x++
TEST_F(ParserTest, ParseUnaryPostInc)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x++; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_POST_INC, stmt->u.expr->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.post_inc->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.post_inc->u.var);
}

// Test unary operator: x--
TEST_F(ParserTest, ParseUnaryPostDec)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x--; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_POST_DEC, stmt->u.expr->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.post_dec->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.post_dec->u.var);
}

// Test unary operator: sizeof x
TEST_F(ParserTest, ParseSizeofExpr)
{
    DeclOrStmt *body = GetFunctionBody("void f() { sizeof x; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_SIZEOF_EXPR, stmt->u.expr->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.sizeof_expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.sizeof_expr->u.var);
}

// Test unary operator: sizeof(int)
TEST_F(ParserTest, ParseSizeofType)
{
    DeclOrStmt *body = GetFunctionBody("void f() { sizeof(int); }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_SIZEOF_TYPE, stmt->u.expr->kind);
    EXPECT_EQ(TYPE_INT, stmt->u.expr->u.sizeof_type->kind);
}

// Test unary operator: _Alignof(int)
TEST_F(ParserTest, ParseAlignof)
{
    DeclOrStmt *body = GetFunctionBody("void f() { _Alignof(int); }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_ALIGNOF, stmt->u.expr->kind);
    EXPECT_EQ(TYPE_INT, stmt->u.expr->u.align_of->kind);
}

// Test binary operator: x * y
TEST_F(ParserTest, ParseBinaryMul)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x * y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_MUL, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test binary operator: x / y
TEST_F(ParserTest, ParseBinaryDiv)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x / y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_DIV, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test binary operator: x % y
TEST_F(ParserTest, ParseBinaryMod)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x % y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_MOD, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test binary operator: x + y
TEST_F(ParserTest, ParseBinaryAdd)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x + y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_ADD, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test binary operator: x - y
TEST_F(ParserTest, ParseBinarySub)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x - y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_SUB, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test binary operator: x << y
TEST_F(ParserTest, ParseBinaryLeftShift)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x << y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_LEFT_SHIFT, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test binary operator: x >> y
TEST_F(ParserTest, ParseBinaryRightShift)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x >> y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_RIGHT_SHIFT, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test binary operator: x < y
TEST_F(ParserTest, ParseBinaryLessThan)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x < y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_LT, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test binary operator: x > y
TEST_F(ParserTest, ParseBinaryGreaterThan)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x > y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_GT, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test binary operator: x <= y
TEST_F(ParserTest, ParseBinaryLessEqual)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x <= y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_LE, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test binary operator: x >= y
TEST_F(ParserTest, ParseBinaryGreaterEqual)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x >= y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_GE, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test binary operator: x == y
TEST_F(ParserTest, ParseBinaryEqual)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x == y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_EQ, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test binary operator: x != y
TEST_F(ParserTest, ParseBinaryNotEqual)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x != y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_NE, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test binary operator: x & y
TEST_F(ParserTest, ParseBinaryBitAnd)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x & y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_BIT_AND, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test binary operator: x ^ y
TEST_F(ParserTest, ParseBinaryBitXor)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x ^ y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_BIT_XOR, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test binary operator: x | y
TEST_F(ParserTest, ParseBinaryBitOr)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x | y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_BIT_OR, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test binary operator: x && y
TEST_F(ParserTest, ParseBinaryLogAnd)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x && y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_LOG_AND, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test binary operator: x || y
TEST_F(ParserTest, ParseBinaryLogOr)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x || y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_LOG_OR, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}

// Test type cast
TEST_F(ParserTest, ParseTypeCast)
{
    DeclOrStmt *body = GetFunctionBody(R"(
typedef int foo;
double bar;
enum { qux = 42 };
int f() {
    return ((foo)-1) + ((bar)-2) + ((qux)-3);
}
)");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_RETURN, stmt->kind);

    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_ADD, stmt->u.expr->u.binary_op.op->kind);
    //TODO:
    //EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    //EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    //EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    //EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}
