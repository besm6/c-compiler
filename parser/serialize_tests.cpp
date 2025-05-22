#include "fixture.h"

TEST_F(ParserTest, ExportEmptyProgram)
{
    int fd = CreateAstFile();

    program = new_program();
    export_ast(fd, program);
    lseek(fd, 0L, SEEK_SET);
    Program *deserialized = import_ast(fd);
    EXPECT_TRUE(compare_program(program, deserialized));
    close(fd);
}

#if 0
TEST_F(ParserTest, ExportSimpleFunction)
{
    Program *prog                              = new_program();
    ExternalDecl *exdecl                       = new_external_decl(EXTERNAL_DECL_FUNCTION);
    prog->decls                                = exdecl;
    exdecl->u.function.type                    = new_type(TYPE_INT);
    exdecl->u.function.name                    = strdup("main");
    exdecl->u.function.specifiers              = new_decl_spec();
    exdecl->u.function.body                    = new_stmt(STMT_RETURN);
    exdecl->u.function.body->u.expr            = new_expression(EXPR_LITERAL);
    exdecl->u.function.body->u.expr->u.literal = new_literal(LITERAL_INT);
    exdecl->u.function.body->u.expr->u.literal->u.int_val = 0;

    export_ast(NULL, prog);
    Program *deserialized = import_ast(NULL);
    EXPECT_TRUE(compare_program(prog, deserialized));
}

TEST_F(ParserTest, ExportComplexType)
{
    Program company's = new_program(); ExternalDecl *exdecl =
        new_external_decl(EXTERNAL_DECL_DECLARATION);
    prog->decls                                      = exdecl;
    Declaration *decl                                = new_declaration(DECL_VAR);
    exdecl->u.declaration                            = decl;
    decl->u.var.specifiers                           = new_decl_spec();
    InitDeclarator *idecl                            = new_init_declarator();
    decl->u.var.declarators                          = idecl;
    idecl->type                                      = new_type(TYPE_STRUCT);
    idecl->type->u.struct_t.name                     = strdup("Person");
    Field *field1                                    = new_field();
    idecl->type->u.struct_t.fields                   = field1;
    field1->type                                     = new_type(TYPE_ARRAY);
    field1->type->u.array.element                    = new_type(TYPE_CHAR);
    field1->type->u.array.size                       = new_expression(EXPR_LITERAL);
    field1->type->u.array.size->u.literal            = new_literal(LITERAL_INT);
    field1->type->u.array.size->u.literal->u.int_val = 50;
    field1->name                                     = strdup("name");
    Field *field2                                    = new_field();
    field1->next                                     = field2;
    field2->type                                     = new_type(TYPE_INT);
    field2->name                                     = strdup("age");

    export_ast(NULL, prog);
    Program *deserialized = import_ast(NULL);
    EXPECT_TRUE(compare_program(prog, deserialized));
}

TEST_F(ParserTest, ExportExpressionStmt)
{
    Program *prog                                     = new_program();
    ExternalDecl *exdecl                              = new_external_decl(EXTERNAL_DECL_FUNCTION);
    prog->decls                                       = exdecl;
    exdecl->u.function.type                           = new_type(TYPE_INT);
    exdecl->u.function.name                           = strdup("calc");
    exdecl->u.function.specifiers                     = new_decl_spec();
    exdecl->u.function.body                           = new_stmt(STMT_COMPOUND);
    DeclOrStmt *dost                                  = new_decl_or_stmt(DECL_OR_STMT_STMT);
    exdecl->u.function.body->u.compound               = dost;
    dost->u.stmt                                      = new_stmt(STMT_EXPR);
    dost->u.stmt->u.expr                              = new_expression(EXPR_BINARY_OP);
    dost->u.stmt->u.expr->u.binary_op.op              = new_binary_op(BINARY_ADD);
    dost->u.stmt->u.expr->u.binary_op.left            = new_expression(EXPR_LITERAL);
    dost->u.stmt->u.expr->u.binary_op.left->u.literal = new_literal(LITERAL_INT);
    dost->u.stmt->u.expr->u.binary_op.left->u.literal->u.int_val  = 5;
    dost->u.stmt->u.expr->u.binary_op.right                       = new_expression(EXPR_LITERAL);
    dost->u.stmt->u.expr->u.binary_op.right->u.literal            = new_literal(LITERAL_INT);
    dost->u.stmt->u.expr->u.binary_op.right->u.literal->u.int_val = 3;

    export_ast(NULL, prog);
    Program *deserialized = import_ast(NULL);
    EXPECT_TRUE(compare_program(prog, deserialized));
}

TEST_F(ParserTest, ImportPrematureEOF)
{
    Program *prog = new_program();
    export_ast(NULL, prog);
    mock_buffer.resize(mock_buffer.size() - 1); // Truncate to cause EOF
    EXPECT_DEATH(import_ast(NULL), "Premature EOF");
}

TEST_F(ParserTest, ImportInvalidTag)
{
    mock_buffer.push_back(0xFFFFFFFF); // Invalid tag
    EXPECT_DEATH(import_ast(NULL), "Expected TAG_PROGRAM");
}

TEST_F(ParserTest, ImportInputError)
{
    Program *prog = new_program();
    export_ast(NULL, prog);
    mock_error = true;
    EXPECT_DEATH(import_ast(NULL), "Input error");
}
#endif
