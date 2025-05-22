#include "fixture.h"

TEST_F(ParserTest, ExportEmptyProgram)
{
    program = new_program();

    int fd = CreateAstFile();
    export_ast(fd, program);

    Program *deserialized = import_ast(fd);
    EXPECT_TRUE(compare_program(program, deserialized));
    close(fd);
    free_program(deserialized);
}

TEST_F(ParserTest, ExportSimpleFunction)
{
    program = parse(CreateTempFile("int main() { return 0; }"));
    ASSERT_NE(nullptr, program);
    print_program(stdout, program);

    int fd = CreateAstFile();
    export_ast(fd, program);

    Program *deserialized = import_ast(fd);
    print_program(stdout, deserialized);
    EXPECT_TRUE(compare_program(program, deserialized));
    close(fd);
    free_program(deserialized);
}

#if 0
TEST_F(ParserTest, ExportComplexType)
{
    program = parse(CreateTempFile("struct Person { char name[50]; int age; }"));
    ...
}

TEST_F(ParserTest, ExportExpressionStmt)
{
    program = parse(CreateTempFile("int calc() { 5 + 3; }"));
    ...
}

TEST_F(ParserTest, ImportPrematureEOF)
{
    int fd = CreateAstFile();
    EXPECT_DEATH(import_ast(fd), "Premature EOF");
}

TEST_F(ParserTest, ImportInvalidTag)
{
    int fd = CreateAstFile();
    size_t tag = -1;
    write(fd, &tag, sizeof(tag)); // Invalid tag
    EXPECT_DEATH(import_ast(fd), "Expected TAG_PROGRAM");
    close(fd);
}

TEST_F(ParserTest, ImportInputError)
{
    program = new_program();

    int fd = CreateAstFile();
    export_ast(fd, program);

    mock_error = true; // TODO
    EXPECT_DEATH(import_ast(fd), "Input error");
}
#endif
