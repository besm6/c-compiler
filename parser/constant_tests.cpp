//
// Constant expressions
//
#include "fixture.h"

TEST_F(ParserTest, EnumConstant_negative)
{
    // Expected constant expression in enum constant.
    EXPECT_DEATH(program = parse(CreateTempFile("enum { a = x; };")), "");
}

TEST_F(ParserTest, StructBitfieldConstant_negative)
{
    // Expected constant expression in field bitwidth.
    EXPECT_DEATH(program = parse(CreateTempFile("struct { int a : x; };")), "");
}

TEST_F(ParserTest, AlignAsConstant_negative)
{
    // Expected constant expression in alignas() argument.
    EXPECT_DEATH(program = parse(CreateTempFile("int x = _Alignas(y);")), "");
}

TEST_F(ParserTest, StaticAssertConstant_negative)
{
    // Expected constant expression in static assert.
    EXPECT_DEATH(program = parse(CreateTempFile("_Static_assert(x, \"msg\");")), "");
}

TEST_F(ParserTest, CaseConstant_negative)
{
    // Expected constant expression in case label.
    EXPECT_DEATH(program = parse(CreateTempFile("int main() { switch (x) { case y: return; } }")),
                 "");
}

// A plain decimal constant is a signed int.
TEST_F(ParserTest, IntegerConstant_plain)
{
    Declaration *decl = GetDeclaration("int x = 5;");
    Literal *lit      = decl->u.var.declarators->init->u.expr->u.literal;
    EXPECT_EQ(LITERAL_INT, lit->kind);
    EXPECT_EQ(5, lit->u.int_val);
}

// A `U` suffix (no `L`) makes a small constant `unsigned int`, not signed.
TEST_F(ParserTest, IntegerConstant_uSuffix)
{
    Declaration *decl = GetDeclaration("unsigned x = 5U;");
    Literal *lit      = decl->u.var.declarators->init->u.expr->u.literal;
    EXPECT_EQ(LITERAL_UINT, lit->kind);
    EXPECT_EQ(5u, lit->u.uint_val);
}

// A `U` suffix alone is enough to keep all 48 bits of a wide unsigned constant:
// the value exceeds the host's unsigned int, so it widens to unsigned long, but
// no explicit `L` is required and the full value survives.
TEST_F(ParserTest, IntegerConstant_uSuffixWide)
{
    Declaration *decl = GetDeclaration("unsigned long x = 0xFFFFFFFFFFFFU;");
    Literal *lit      = decl->u.var.declarators->init->u.expr->u.literal;
    EXPECT_EQ(LITERAL_ULONG, lit->kind);
    EXPECT_EQ(0xFFFFFFFFFFFFUL, lit->u.ulong_val);
}

// The `UL` suffix still selects unsigned long.
TEST_F(ParserTest, IntegerConstant_ulSuffix)
{
    Declaration *decl = GetDeclaration("unsigned long x = 5UL;");
    Literal *lit      = decl->u.var.declarators->init->u.expr->u.literal;
    EXPECT_EQ(LITERAL_ULONG, lit->kind);
    EXPECT_EQ(5UL, lit->u.ulong_val);
}
