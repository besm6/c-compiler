#include "fixture.h"

// Test labeled statement: label: x;
TEST_F(ParserTest, ParseLabeledStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { label: x; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_LABELED, stmt->kind);
    EXPECT_STREQ("label", stmt->u.labeled.label);
    EXPECT_EQ(STMT_EXPR, stmt->u.labeled.stmt->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.labeled.stmt->u.expr->kind);
    EXPECT_STREQ("x", stmt->u.labeled.stmt->u.expr->u.var);
}

// Test case statement: case 42: x;
TEST_F(ParserTest, ParseCaseStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { case 42: x; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_CASE, stmt->kind);
    EXPECT_EQ(EXPR_LITERAL, stmt->u.case_stmt.expr->kind);
    EXPECT_EQ(LITERAL_INT, stmt->u.case_stmt.expr->u.literal->kind);
    EXPECT_EQ(42, stmt->u.case_stmt.expr->u.literal->u.int_val);
    EXPECT_EQ(STMT_EXPR, stmt->u.case_stmt.stmt->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.case_stmt.stmt->u.expr->kind);
    EXPECT_STREQ("x", stmt->u.case_stmt.stmt->u.expr->u.var);
}

// Test default statement: default: x;
TEST_F(ParserTest, ParseDefaultStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { default: x; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_DEFAULT, stmt->kind);
    EXPECT_EQ(STMT_EXPR, stmt->u.default_stmt->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.default_stmt->u.expr->kind);
    EXPECT_STREQ("x", stmt->u.default_stmt->u.expr->u.var);
}

// Test compound statement: { x; y; }
TEST_F(ParserTest, ParseCompoundStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { { x; y; } }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_COMPOUND, stmt->kind);
    DeclOrStmt *items = stmt->u.compound;
    EXPECT_NE(nullptr, items);
    EXPECT_EQ(DECL_OR_STMT_STMT, items->kind);
    EXPECT_EQ(STMT_EXPR, items->u.stmt->kind);
    EXPECT_EQ(EXPR_VAR, items->u.stmt->u.expr->kind);
    EXPECT_STREQ("x", items->u.stmt->u.expr->u.var);
    EXPECT_NE(nullptr, items->next);
    EXPECT_EQ(DECL_OR_STMT_STMT, items->next->kind);
    EXPECT_EQ(STMT_EXPR, items->next->u.stmt->kind);
    EXPECT_EQ(EXPR_VAR, items->next->u.stmt->u.expr->kind);
    EXPECT_STREQ("y", items->next->u.stmt->u.expr->u.var);
    EXPECT_EQ(nullptr, items->next->next);
}

// Test empty expression statement: ;
TEST_F(ParserTest, ParseEmptyExpressionStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { ; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(nullptr, stmt->u.expr);
}

// Test expression statement: x;
TEST_F(ParserTest, ParseExpressionStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { x; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_EXPR, stmt->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.var);
}

// Test if statement: if (x) y;
TEST_F(ParserTest, ParseIfStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { if (x) y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_IF, stmt->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.if_stmt.condition->kind);
    EXPECT_STREQ("x", stmt->u.if_stmt.condition->u.var);
    EXPECT_EQ(STMT_EXPR, stmt->u.if_stmt.then_stmt->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.if_stmt.then_stmt->u.expr->kind);
    EXPECT_STREQ("y", stmt->u.if_stmt.then_stmt->u.expr->u.var);
    EXPECT_EQ(nullptr, stmt->u.if_stmt.else_stmt);
}

// Test if-else statement: if (x) y; else z;
TEST_F(ParserTest, ParseIfElseStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { if (x) y; else z; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_IF, stmt->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.if_stmt.condition->kind);
    EXPECT_STREQ("x", stmt->u.if_stmt.condition->u.var);
    EXPECT_EQ(STMT_EXPR, stmt->u.if_stmt.then_stmt->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.if_stmt.then_stmt->u.expr->kind);
    EXPECT_STREQ("y", stmt->u.if_stmt.then_stmt->u.expr->u.var);
    EXPECT_NE(nullptr, stmt->u.if_stmt.else_stmt);
    EXPECT_EQ(STMT_EXPR, stmt->u.if_stmt.else_stmt->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.if_stmt.else_stmt->u.expr->kind);
    EXPECT_STREQ("z", stmt->u.if_stmt.else_stmt->u.expr->u.var);
}

// Test switch statement: switch (x) { case 1: y; }
TEST_F(ParserTest, ParseSwitchStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { switch (x) { case 1: y; } }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_SWITCH, stmt->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.switch_stmt.expr->kind);
    EXPECT_STREQ("x", stmt->u.switch_stmt.expr->u.var);
    EXPECT_EQ(STMT_COMPOUND, stmt->u.switch_stmt.body->kind);
    EXPECT_NE(nullptr, stmt->u.switch_stmt.body->u.compound);
    EXPECT_EQ(DECL_OR_STMT_STMT, stmt->u.switch_stmt.body->u.compound->kind);
    EXPECT_EQ(STMT_CASE, stmt->u.switch_stmt.body->u.compound->u.stmt->kind);
    EXPECT_EQ(EXPR_LITERAL, stmt->u.switch_stmt.body->u.compound->u.stmt->u.case_stmt.expr->kind);
    EXPECT_EQ(1,
              stmt->u.switch_stmt.body->u.compound->u.stmt->u.case_stmt.expr->u.literal->u.int_val);
    EXPECT_EQ(STMT_EXPR, stmt->u.switch_stmt.body->u.compound->u.stmt->u.case_stmt.stmt->kind);
    EXPECT_STREQ("y",
                 stmt->u.switch_stmt.body->u.compound->u.stmt->u.case_stmt.stmt->u.expr->u.var);
}

// Test while statement: while (x) y;
TEST_F(ParserTest, ParseWhileStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { while (x) y; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_WHILE, stmt->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.while_stmt.condition->kind);
    EXPECT_STREQ("x", stmt->u.while_stmt.condition->u.var);
    EXPECT_EQ(STMT_EXPR, stmt->u.while_stmt.body->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.while_stmt.body->u.expr->kind);
    EXPECT_STREQ("y", stmt->u.while_stmt.body->u.expr->u.var);
}

// Test do-while statement: do x; while (y);
TEST_F(ParserTest, ParseDoWhileStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { do x; while (y); }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_DO_WHILE, stmt->kind);
    EXPECT_EQ(STMT_EXPR, stmt->u.do_while.body->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.do_while.body->u.expr->kind);
    EXPECT_STREQ("x", stmt->u.do_while.body->u.expr->u.var);
    EXPECT_EQ(EXPR_VAR, stmt->u.do_while.condition->kind);
    EXPECT_STREQ("y", stmt->u.do_while.condition->u.var);
}

// Test for statement: for (i = 0; i < 10; i++) x;
TEST_F(ParserTest, ParseForStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { for (i = 0; i < 10; i++) x; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_FOR, stmt->kind);
    EXPECT_EQ(FOR_INIT_EXPR, stmt->u.for_stmt.init->kind);
    EXPECT_EQ(EXPR_ASSIGN, stmt->u.for_stmt.init->u.expr->kind);
    EXPECT_STREQ("i", stmt->u.for_stmt.init->u.expr->u.assign.target->u.var);
    EXPECT_EQ(EXPR_LITERAL, stmt->u.for_stmt.init->u.expr->u.assign.value->kind);
    EXPECT_EQ(0, stmt->u.for_stmt.init->u.expr->u.assign.value->u.literal->u.int_val);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.for_stmt.condition->kind);
    EXPECT_EQ(BINARY_LT, stmt->u.for_stmt.condition->u.binary_op.op->kind);
    EXPECT_STREQ("i", stmt->u.for_stmt.condition->u.binary_op.left->u.var);
    EXPECT_EQ(10, stmt->u.for_stmt.condition->u.binary_op.right->u.literal->u.int_val);
    EXPECT_EQ(EXPR_POST_INC, stmt->u.for_stmt.update->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.for_stmt.update->u.post_inc->kind);
    EXPECT_STREQ("i", stmt->u.for_stmt.update->u.post_inc->u.var);
    EXPECT_EQ(STMT_EXPR, stmt->u.for_stmt.body->kind);
    EXPECT_STREQ("x", stmt->u.for_stmt.body->u.expr->u.var);
}

// Test for statement with declaration: for (int i = 0; i < 10; i++) x;
TEST_F(ParserTest, ParseForDeclStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { for (int i = 0; i < 10; i++) x; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_FOR, stmt->kind);
    EXPECT_EQ(FOR_INIT_DECL, stmt->u.for_stmt.init->kind);
    EXPECT_EQ(DECL_VAR, stmt->u.for_stmt.init->u.decl->kind);
    EXPECT_EQ(TYPE_INT, stmt->u.for_stmt.init->u.decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("i", stmt->u.for_stmt.init->u.decl->u.var.declarators->declarator->name);
    EXPECT_EQ(EXPR_BINARY_OP, stmt->u.for_stmt.condition->kind);
    EXPECT_EQ(BINARY_LT, stmt->u.for_stmt.condition->u.binary_op.op->kind);
    EXPECT_STREQ("i", stmt->u.for_stmt.condition->u.binary_op.left->u.var);
    EXPECT_EQ(10, stmt->u.for_stmt.condition->u.binary_op.right->u.literal->u.int_val);
    EXPECT_EQ(EXPR_POST_INC, stmt->u.for_stmt.update->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.for_stmt.update->u.post_inc->kind);
    EXPECT_STREQ("i", stmt->u.for_stmt.update->u.post_inc->u.var);
    EXPECT_EQ(STMT_EXPR, stmt->u.for_stmt.body->kind);
    EXPECT_STREQ("x", stmt->u.for_stmt.body->u.expr->u.var);
}

// Test goto statement: goto label;
TEST_F(ParserTest, ParseGotoStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { goto label; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_GOTO, stmt->kind);
    EXPECT_STREQ("label", stmt->u.goto_label);
}

// Test continue statement: continue;
TEST_F(ParserTest, ParseContinueStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { continue; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_CONTINUE, stmt->kind);
}

// Test break statement: break;
TEST_F(ParserTest, ParseBreakStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { break; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_BREAK, stmt->kind);
}

// Test return statement: return x;
TEST_F(ParserTest, ParseReturnStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { return x; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_RETURN, stmt->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.expr->kind);
    EXPECT_STREQ("x", stmt->u.expr->u.var);
}

// Test return statement: return;
TEST_F(ParserTest, ParseEmptyReturnStatement)
{
    DeclOrStmt *body = GetFunctionBody("void f() { return; }");
    EXPECT_EQ(body->kind, DECL_OR_STMT_STMT);
    Stmt *stmt = body->u.stmt;

    EXPECT_EQ(STMT_RETURN, stmt->kind);
    EXPECT_EQ(nullptr, stmt->u.expr);
}
