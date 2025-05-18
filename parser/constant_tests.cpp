//
// Constant expressions
//
#include "fixture.h"

TEST_F(ParserTest, EnumConstant_negative)
{
    // Expected constant expression in enum constant.
    EXPECT_DEATH(program = parse(CreateTempFile("enum { a = x; };")),
        "");
}

TEST_F(ParserTest, StructBitfieldConstant_negative)
{
    // Expected constant expression in field bitwidth.
    EXPECT_DEATH(program = parse(CreateTempFile("struct { int a : x; };")),
        "");
}

TEST_F(ParserTest, AlignAsConstant_negative)
{
    // Expected constant expression in alignas() argument.
    EXPECT_DEATH(program = parse(CreateTempFile("int x = _Alignas(y);")),
        "");
}

TEST_F(ParserTest, StaticAssertConstant_negative)
{
    // Expected constant expression in static assert.
    EXPECT_DEATH(program = parse(CreateTempFile("_Static_assert(x, \"msg\");")),
        "");
}

TEST_F(ParserTest, CaseConstant_negative)
{
    // Expected constant expression in case label.
    EXPECT_DEATH(program = parse(CreateTempFile("int main() { switch (x) { case y: return; } }")),
        "");
}
