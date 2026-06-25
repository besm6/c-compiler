//
// Chapter 11 — Long integers: invalid parser input.
// Imported from "Writing a C Compiler" (tests/chapter_11/invalid_parse).  Each
// program is lexically valid but cannot be parsed; the parser reports a fatal
// error.  Tests assert on a substring of the diagnostic.
//
#include "fixture.h"

// --- invalid_parse ----------------------------------------------------------

// "int long int i" — a declaration may not carry two 'int' specifiers.
TEST_F(ParserTest, Chapter11_BadSpecifiers_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    int long int i = 0;
    return i;
}
)")),
                 "multiple int specifiers");
}

// "() 0" — a cast expression must include at least one type specifier.
TEST_F(ParserTest, Chapter11_EmptyCast_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    return () 0;
}
)")),
                 "Expected primary expression");
}

// "int long(void)" — 'long' is a keyword and cannot name a function.
TEST_F(ParserTest, Chapter11_FunNameLong_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int long(void) {
    return 4;
}

int main(void){
    return long();
}
)")),
                 "Expected identifier or");
}

// "(static int) 10" — a cast may only contain type specifiers, not a storage class.
TEST_F(ParserTest, Chapter11_InvalidCast_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    return (static int) 10;
}
)")),
                 "Expected primary expression");
}

// "0 l" — a long suffix may not be separated from its constant by whitespace.
TEST_F(ParserTest, Chapter11_InvalidSuffix_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    return 0 l;
}
)")),
                 "expected ';', got identifier");
}

// "int 10l;" — a long constant cannot stand where an identifier is required.
TEST_F(ParserTest, Chapter11_LongConstantAsVar_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    int 10l;
    return 0;
}
)")),
                 "Expected identifier or");
}

// "return long 0;" — a cast's type specifier must be parenthesized.
TEST_F(ParserTest, Chapter11_MissingCastParentheses_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    return long 0;
}
)")),
                 "Expected primary expression");
}

// "int long = 5" — 'long' is a keyword and cannot name a variable.
TEST_F(ParserTest, Chapter11_VarNameLong_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    int long = 5;
    return long;
}
)")),
                 "Expected identifier or");
}
