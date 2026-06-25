//
// Chapter 1 — A Minimal Compiler: invalid parser input.
// Imported from "Writing a C Compiler" (tests/chapter_1/invalid_parse).
// Each program is lexically valid but cannot be parsed; the parser reports a
// fatal error.  Tests assert on the diagnostic text.
//
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "fixture.h"

// fatal_error() is defined once for the whole parser-tests binary in
// simple_tests.cpp; the compiler libraries call it.

// return with no expression, then end of file.
TEST_F(ParserTest, Chapter1_EndBeforeExpr_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile("int main(void) {\n    return\n")),
                 "expected ';', got end of file");
}

// A bare identifier 'foo' is not a valid top-level construct.
TEST_F(ParserTest, Chapter1_ExtraJunk_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile("int main(void)\n{\n    return 2;\n}\nfoo\n")),
                 "Empty type specifier list");
}

// A function name must be an identifier, not a constant.
TEST_F(ParserTest, Chapter1_InvalidFunctionName_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile("int 3 (void) {\n    return 0;\n}\n")),
                 "Expected identifier or '\\('");
}

// 'RETURN' is not the keyword 'return'; it parses as an identifier.
TEST_F(ParserTest, Chapter1_KeywordWrongCase_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile("int main(void) {\n    RETURN 0;\n}\n")),
                 "expected ';', got integer constant");
}

// No return type: 'main(void)' has no type specifier.
TEST_F(ParserTest, Chapter1_MissingType_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile("main(void) {\n    return 0;\n}\n")),
                 "Empty type specifier list");
}

// 'returns' is a misspelled keyword, parsed as an identifier.
TEST_F(ParserTest, Chapter1_MisspelledKeyword_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile("int main(void) {\n    returns 0;\n}\n")),
                 "expected ';', got integer constant");
}

// Missing semicolon after the return value.
TEST_F(ParserTest, Chapter1_NoSemicolon_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile("int main (void) {\n    return 0\n}\n")),
                 "expected ';', got '\\}'");
}

// 'int' is a keyword, not an expression.
TEST_F(ParserTest, Chapter1_NotExpression_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile("int main(void) {\n    return int;\n}\n")),
                 "Expected primary expression");
}

// 'retur n' — a space splits the keyword into two identifiers.
TEST_F(ParserTest, Chapter1_SpaceInKeyword_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile("int main(void){\n    retur n 0;\n}\n")),
                 "expected ';', got identifier");
}

// Parentheses in the wrong order: 'int main )( {'.
TEST_F(ParserTest, Chapter1_SwitchedParens_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile("int main )( {\n    return 0;\n}\n")),
                 "Empty type specifier list");
}

// Function body's closing brace is missing.
TEST_F(ParserTest, Chapter1_UnclosedBrace_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile("int main(void) {\n    return 0;\n\n")),
                 "expected '\\}', got end of file");
}

// Parameter list's closing parenthesis is missing.
TEST_F(ParserTest, Chapter1_UnclosedParen_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile("int main( {\n    return 0;\n}\n")),
                 "Empty type specifier list");
}
