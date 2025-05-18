#include "fixture.h"
#include "internal.h"

// Test primary expression: identifier
TEST_F(ParserTest, ScanIdentifier)
{
    init_scanner(CreateTempFile("x"));
    advance_token();
    Declarator *decl = parse_declarator();
    ASSERT_NE(decl, nullptr);
    print_declarator(stdout, decl, 0);

    EXPECT_EQ(decl->next, nullptr);
    EXPECT_STREQ(decl->name, "x");
    EXPECT_EQ(decl->pointers, nullptr);
    EXPECT_EQ(decl->suffixes, nullptr);

    free_declarator(decl);
}

// Test primary expression: integer constant
TEST_F(ParserTest, ScanIntegerConstant)
{
    init_scanner(CreateTempFile("42;"));
    advance_token();
    Expr *expr = parse_primary_expression();
    ASSERT_NE(expr, nullptr);
    print_expression(stdout, expr, 0);

    EXPECT_EQ(EXPR_LITERAL, expr->kind);
    EXPECT_EQ(LITERAL_INT, expr->u.literal->kind);
    EXPECT_EQ(42, expr->u.literal->u.int_val);

    free_expression(expr);
}

// Test binary expression: x + y
TEST_F(ParserTest, ScanBinaryExpression)
{
    init_scanner(CreateTempFile("x + y;"));
    advance_token();
    Expr *expr = parse_expression();
    ASSERT_NE(expr, nullptr);
    print_expression(stdout, expr, 0);

    EXPECT_EQ(EXPR_BINARY_OP, expr->kind);
    EXPECT_EQ(BINARY_ADD, expr->u.binary_op.op->kind);
    EXPECT_EQ(EXPR_VAR, expr->u.binary_op.left->kind);
    EXPECT_STREQ("x", expr->u.binary_op.left->u.var);
    EXPECT_EQ(EXPR_VAR, expr->u.binary_op.right->kind);
    EXPECT_STREQ("y", expr->u.binary_op.right->u.var);

    free_expression(expr);
}

// Test function call: f(x, y)
TEST_F(ParserTest, ScanFunctionCall)
{
    init_scanner(CreateTempFile("f(x, y);"));
    advance_token();
    Expr *expr = parse_expression();
    ASSERT_NE(expr, nullptr);
    print_expression(stdout, expr, 0);

    EXPECT_EQ(EXPR_CALL, expr->kind);
    EXPECT_EQ(EXPR_VAR, expr->u.call.func->kind);
    EXPECT_STREQ("f", expr->u.call.func->u.var);
    Expr *args = expr->u.call.args;
    ASSERT_NE(nullptr, args);
    EXPECT_EQ(EXPR_VAR, args->kind);
    EXPECT_STREQ("x", args->u.var);
    ASSERT_NE(nullptr, args->next);
    EXPECT_EQ(EXPR_VAR, args->next->kind);
    EXPECT_STREQ("y", args->next->u.var);
    EXPECT_EQ(nullptr, args->next->next);

    free_expression(expr);
}

// Test if statement: if (x) return y;
TEST_F(ParserTest, ScanIfStatement)
{
    init_scanner(CreateTempFile("if (x) return y;"));
    advance_token();
    Stmt *stmt = parse_statement();
    ASSERT_NE(stmt, nullptr);
    print_statement(stdout, stmt, 0);

    EXPECT_EQ(STMT_IF, stmt->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.if_stmt.condition->kind);
    EXPECT_STREQ("x", stmt->u.if_stmt.condition->u.var);
    EXPECT_EQ(STMT_RETURN, stmt->u.if_stmt.then_stmt->kind);
    EXPECT_EQ(EXPR_VAR, stmt->u.if_stmt.then_stmt->u.expr->kind);
    EXPECT_STREQ("y", stmt->u.if_stmt.then_stmt->u.expr->u.var);
    EXPECT_EQ(nullptr, stmt->u.if_stmt.else_stmt);

    free_statement(stmt);
}

// Test variable declaration: int x = 42;
TEST_F(ParserTest, ParseVariableDeclaration)
{
    program = parse(CreateTempFile("int x = 42;"));
    ASSERT_NE(nullptr, program);
    print_program(stdout, program);

    ASSERT_NE(nullptr, program->decls);
    EXPECT_EQ(EXTERNAL_DECL_DECLARATION, program->decls->kind);

    Declaration *decl = program->decls->u.declaration;
    EXPECT_EQ(DECL_VAR, decl->kind);
    EXPECT_EQ(nullptr, decl->u.var.specifiers);

    InitDeclarator *id = decl->u.var.declarators;
    ASSERT_NE(nullptr, id);
    ASSERT_NE(nullptr, id->type);
    EXPECT_EQ(TYPE_INT, id->type->kind);
    EXPECT_STREQ("x", id->name);

    ASSERT_NE(nullptr, id->init);
    EXPECT_EQ(INITIALIZER_SINGLE, id->init->kind);
    EXPECT_EQ(EXPR_LITERAL, id->init->u.expr->kind);
    EXPECT_EQ(LITERAL_INT, id->init->u.expr->u.literal->kind);
    EXPECT_EQ(42, id->init->u.expr->u.literal->u.int_val);
}

// Test function definition: int main() { return 0; }
TEST_F(ParserTest, ParseFunctionDefinition)
{
    program = parse(CreateTempFile("int main() { return 0; }"));
    ASSERT_NE(nullptr, program);
    print_program(stdout, program);

    ASSERT_NE(nullptr, program->decls);
    EXPECT_EQ(EXTERNAL_DECL_FUNCTION, program->decls->kind);
    EXPECT_STREQ("main", program->decls->u.function.name);

    Type *type = program->decls->u.function.type;
    ASSERT_NE(nullptr, type);
    EXPECT_EQ(TYPE_FUNCTION, type->kind);
    ASSERT_NE(nullptr, type->u.function.return_type);
    EXPECT_EQ(TYPE_INT, type->u.function.return_type->kind);

    EXPECT_EQ(nullptr, program->decls->u.function.specifiers);

    Stmt *body = program->decls->u.function.body;
    ASSERT_NE(nullptr, body);
    EXPECT_EQ(STMT_COMPOUND, body->kind);
    ASSERT_NE(nullptr, body->u.compound);
    EXPECT_EQ(DECL_OR_STMT_STMT, body->u.compound->kind);
    EXPECT_EQ(STMT_RETURN, body->u.compound->u.stmt->kind);
    EXPECT_EQ(EXPR_LITERAL, body->u.compound->u.stmt->u.expr->kind);
    EXPECT_EQ(LITERAL_INT, body->u.compound->u.stmt->u.expr->u.literal->kind);
    EXPECT_EQ(0, body->u.compound->u.stmt->u.expr->u.literal->u.int_val);
}

// Test translation unit: int x; void f() {}
TEST_F(ParserTest, ParseTranslationUnit)
{
    program = parse(CreateTempFile("int x; void f() {}"));
    ASSERT_NE(nullptr, program);
    print_program(stdout, program);

    ASSERT_NE(nullptr, program->decls);
    EXPECT_EQ(EXTERNAL_DECL_DECLARATION, program->decls->kind);

    Declaration *decl = program->decls->u.declaration;
    EXPECT_EQ(DECL_VAR, decl->kind);
    EXPECT_EQ(nullptr, decl->u.var.specifiers);

    InitDeclarator *id = decl->u.var.declarators;
    ASSERT_NE(nullptr, id);
    EXPECT_STREQ("x", id->name);
    ASSERT_NE(nullptr, id->type);
    EXPECT_EQ(TYPE_INT, id->type->kind);

    ExternalDecl *func = program->decls->next;
    ASSERT_NE(nullptr, func);
    EXPECT_EQ(EXTERNAL_DECL_FUNCTION, func->kind);
    EXPECT_EQ(nullptr, func->u.function.specifiers);

    EXPECT_STREQ("f", func->u.function.name);
    EXPECT_EQ(STMT_COMPOUND, func->u.function.body->kind);
    EXPECT_EQ(nullptr, func->u.function.body->u.compound);
}
