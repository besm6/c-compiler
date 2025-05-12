#include "fixture.h"

// Helper to get statement from program
Stmt *GetStatement(Program *program)
{
    EXPECT_NE(nullptr, program);
    EXPECT_NE(nullptr, program->decls);
    EXPECT_EQ(EXTERNAL_DECL_DECLARATION, program->decls->kind);
    Declaration *decl = program->decls->u.declaration;
    EXPECT_EQ(DECL_VAR, decl->kind);
    InitDeclarator *id = decl->u.var.declarators;
    EXPECT_NE(nullptr, id);
    Expr *expr = id->init->u.expr;
    EXPECT_EQ(EXPR_CALL, expr->kind); // Simplified AST structure
    return expr->u.call.func;         // Assuming statement is wrapped
}
}
;

// Test unary operator: &x
TEST_F(ParserTest, ParseUnaryAddress)
{
    FILE *f          = CreateTempFile("&x;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_UNARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(UNARY_ADDRESS, stmt->u.expr->u.unary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.unary_op.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.unary_op.expr->u.var);
}

// Test unary operator: *x
TEST_F(ParserTest, ParseUnaryDeref)
{
    FILE *f          = CreateTempFile("*x;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_UNARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(UNARY_DEREF, stmt->u.expr->u.unary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.unary_op.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.unary_op.expr->u.var);
}

// Test unary operator: +x
TEST_F(ParserTest, ParseUnaryPlus)
{
    FILE *f          = CreateTempFile("+x;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_UNARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(UNARY_PLUS, stmt->u.expr->u.unary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.unary_op.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.unary_op.expr->u.var);
}

// Test unary operator: -x
TEST_F(ParserTest, ParseUnaryNeg)
{
    FILE *f          = CreateTempFile("-x;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_UNARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(UNARY_NEG, stmt->u.expr->u.unary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.unary_op.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.unary_op.expr->u.var);
}

// Test unary operator: ~x
TEST_F(ParserTest, ParseUnaryBitNot)
{
    FILE *f          = CreateTempFile("~x;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_UNARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(UNARY_BIT_NOT, stmt->u.expr->u.unary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.unary_op.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.unary_op.expr->u.var);
}

// Test unary operator: !x
TEST_F(ParserTest, ParseUnaryLogNot)
{
    FILE *f          = CreateTempFile("!x;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_UNARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(UNARY_LOG_NOT, stmt->u.expr->u.unary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.unary_op.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.unary_op.expr->u.var);
}

// Test unary operator: ++x
TEST_F(ParserTest, ParseUnaryPreInc)
{
    FILE *f          = CreateTempFile("++x;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_UNARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(UNARY_PRE_INC, stmt->u.expr->u.unary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.unary_op.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.unary_op.expr->u.var);
}

// Test unary operator: --x
TEST_F(ParserTest, ParseUnaryPreDec)
{
    FILE *f          = CreateTempFile("--x;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_UNARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(UNARY_PRE_DEC, stmt->u.expr->u.unary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.unary_op.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.unary_op.expr->u.var);
}

// Test unary operator: x++
TEST_F(ParserTest, ParseUnaryPostInc)
{
    FILE *f          = CreateTempFile("x++;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_POST_INC, stmt->u.expr->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.post_inc->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.post_inc->u.var);
}

// Test unary operator: x--
TEST_F(ParserTest, ParseUnaryPostDec)
{
    FILE *f          = CreateTempFile("x--;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_POST_DEC, stmt->u.expr->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.post_dec->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.post_dec->u.var);
}

// Test unary operator: sizeof x
TEST_F(ParserTest, ParseSizeofExpr)
{
    FILE *f          = CreateTempFile("sizeof x;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_SIZEOF_EXPR, stmt->u.expr->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.sizeof_expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.sizeof_expr->u.var);
}

// Test unary operator: sizeof(int)
TEST_F(ParserTest, ParseSizeofType)
{
    FILE *f          = CreateTempFile("sizeof(int);");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_SIZEOF_TYPE, stmt->u.expr->kind);
    EXPECT_EQ(TYPE_INT, stmt->u.expr->u.sizeof_type->kind);
}

// Test unary operator: _Alignof(int)
TEST_F(ParserTest, ParseAlignof)
{
    FILE *f          = CreateTempFile("_Alignof(int);");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_ALIGNOF, stmt->u.expr->kind);
    EXPECT_EQ(TYPE_INT, stmt->u.expr->u.align_of->kind);
}

// Test binary operator: x * y
TEST_F(ParserTest, ParseBinaryMul)
{
    FILE *f          = CreateTempFile("x * y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
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
    FILE *f          = CreateTempFile("x / y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
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
    FILE *f          = CreateTempFile("x % y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
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
    FILE *f          = CreateTempFile("x + y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
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
    FILE *f          = CreateTempFile("x - y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
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
    FILE *f          = CreateTempFile("x << y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
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
    FILE *f          = CreateTempFile("x >> y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
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
    FILE *f          = CreateTempFile("x < y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
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
    FILE *f          = CreateTempFile("x > y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
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
    FILE *f          = CreateTempFile("x <= y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
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
    FILE *f          = CreateTempFile("x >= y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
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
    FILE *f          = CreateTempFile("x == y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
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
    FILE *f          = CreateTempFile("x != y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
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
    FILE *f          = CreateTempFile("x & y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
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
    FILE *f          = CreateTempFile("x ^ y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
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
    FILE *f          = CreateTempFile("x | y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
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
    FILE *f          = CreateTempFile("x && y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
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
    FILE *f          = CreateTempFile("x || y;");
    Program *program = parse(f);
    fclose(f);

    Stmt *stmt = GetStatement(program);
    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.expr->kind);
    EXPECT_EQ(BINARY_LOG_OR, stmt->u.expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", stmt->u.expr->u.binary_op.right->u.var);
}
