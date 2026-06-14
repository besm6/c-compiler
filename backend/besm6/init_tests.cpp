#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "codegen_test.h"

TEST_F(CodegenTest, VarIntTentative)
{
    std::string output = CompileToMadlen("int foo;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,bss, 1
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarIntPtrTentative)
{
    std::string output = CompileToMadlen("int *foo;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,bss, 1
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarCharPtrTentative)
{
    std::string output = CompileToMadlen("char *foo;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,bss, 1
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarVoidPtrTentative)
{
    std::string output = CompileToMadlen("void *foo;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,bss, 1
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarIntArrayTentative)
{
    std::string output = CompileToMadlen("int arr[5];");
    EXPECT_EQ(R"(c
      arr:   ,name,
             ,bss, 5
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarCharArrayTentative)
{
    std::string output = CompileToMadlen("char arr[10];");
    EXPECT_EQ(R"(c
      arr:   ,name,
             ,bss, 2
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarPtrArrayTentative)
{
    std::string output = CompileToMadlen("int *arr[4];");
    EXPECT_EQ(R"(c
      arr:   ,name,
             ,bss, 4
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarDoubleArrayTentative)
{
    std::string output = CompileToMadlen("double arr[3];");
    EXPECT_EQ(R"(c
      arr:   ,name,
             ,bss, 3
             ,end,
)",
              output);
}

TEST_F(CodegenTest, Var2DArrayTentative)
{
    std::string output = CompileToMadlen("int arr[2][3];");
    EXPECT_EQ(R"(c
      arr:   ,name,
             ,bss, 6
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarStruct1FieldTentative)
{
    std::string output = CompileToMadlen("struct { int x; } s;");
    EXPECT_EQ(R"(c
        s:   ,name,
             ,bss, 1
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarStruct2FieldTentative)
{
    std::string output = CompileToMadlen("struct { int x; int y; } s;");
    EXPECT_EQ(R"(c
        s:   ,name,
             ,bss, 2
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarStruct3FieldTentative)
{
    std::string output = CompileToMadlen("struct { int x; int y; int z; } s;");
    EXPECT_EQ(R"(c
        s:   ,name,
             ,bss, 3
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarStructMixedTentative)
{
    std::string output = CompileToMadlen("struct { char c; int n; double d; } s;");
    EXPECT_EQ(R"(c
        s:   ,name,
             ,bss, 3
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarNamedStructTentative)
{
    std::string output = CompileToMadlen(R"(
        struct pt {
            int x;
            int y;
        };
        struct pt s;
)");
    EXPECT_EQ(R"(c
        s:   ,name,
             ,bss, 2
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarIntInit)
{
    std::string output = CompileToMadlen("int foo = 42;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 52
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarLongInit)
{
    std::string output = CompileToMadlen("long foo = 4321;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 10341
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarShortInit)
{
    std::string output = CompileToMadlen("short foo = 123;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 173
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarCharInit)
{
    std::string output = CompileToMadlen("char foo = '+';");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 53
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarUnsignedInit)
{
    std::string output = CompileToMadlen("unsigned foo = 01234567076543210;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 1234567076543210
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarIntPtrInitName)
{
    std::string output = CompileToMadlen("extern int foo; int *bar = &foo;");
    EXPECT_EQ(R"(c
      bar:   ,name,
      foo:   ,subp,
             ,z00,
             ,z00, foo
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarIntPtrInitLiteral)
{
    std::string output = CompileToMadlen("int *bar = (int*) 42;");
    EXPECT_EQ(R"(c
      bar:   ,name,
             ,log, 52
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarCharPtrInit)
{
    std::string output = CompileToMadlen("extern char foo; char *bar = &foo;");
    EXPECT_EQ(R"(c
      bar:   ,name,
      foo:   ,subp,
           8 ,z00,
             ,z00, foo
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarVoidPtrInitChar)
{
    std::string output = CompileToMadlen("extern char foo; void *bar = &foo;");
    EXPECT_EQ(R"(c
      bar:   ,name,
      foo:   ,subp,
           8 ,z00,
             ,z00, foo
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarVoidPtrInitInt)
{
    std::string output = CompileToMadlen("extern int foo; void *bar = &foo;");
    EXPECT_EQ(R"(c
      bar:   ,name,
      foo:   ,subp,
          13 ,z00,
             ,z00, foo
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarIntPtrInitNameOffset)
{
    std::string output = CompileToMadlen("extern int foo[]; int *bar = &foo[5];");
    EXPECT_EQ(R"(c
      bar:   ,name,
      foo:   ,subp,
             ,z00,
             ,z00, foo+5
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarCharPtrInitNameOffset)
{
    std::string output = CompileToMadlen("extern char foo[]; char *bar = &foo[15];");
    EXPECT_EQ(R"(c
      bar:   ,name,
      foo:   ,subp,
          10 ,z00,
             ,z00, foo+2
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarFloatInit)
{
    std::string output = CompileToMadlen("float foo = 3.1415;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,real, 3.1415
             ,end,
)",
              output);
}

TEST_F(CodegenTest, VarDoubleInit)
{
    std::string output = CompileToMadlen("double foo = 2.71828e-25;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,real, 2.71828e-25
             ,end,
)",
              output);
}

TEST_F(CodegenTest, ArrayIntInit)
{
    std::string output = CompileToMadlen("int foo[] = { 12, 34, 56 };");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 14
             ,log, 42
             ,log, 70
             ,end,
)",
              output);
}

TEST_F(CodegenTest, StructIntInit)
{
    std::string output = CompileToMadlen("struct { int foo, bar; } quz = { 12, 34 };");
    EXPECT_EQ(R"(c
      quz:   ,name,
             ,log, 14
             ,log, 42
             ,end,
)",
              output);
}

TEST_F(CodegenTest, ArrayDoubleInit)
{
    std::string output = CompileToMadlen("double foo[] = { 1.5, 2.5, 3.5 };");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,real, 1.5
             ,real, 2.5
             ,real, 3.5
             ,end,
)",
              output);
}

TEST_F(CodegenTest, Array2DIntInit)
{
    std::string output = CompileToMadlen("int foo[2][3] = { {1, 2, 3}, {4, 5, 6} };");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 1
             ,log, 2
             ,log, 3
             ,log, 4
             ,log, 5
             ,log, 6
             ,end,
)",
              output);
}

TEST_F(CodegenTest, ArrayStructInit)
{
    std::string output = CompileToMadlen(R"(
        struct pt {
            int x, y;
        };
        struct pt foo[] = { {1, 2}, {3, 4} };
)");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 1
             ,log, 2
             ,log, 3
             ,log, 4
             ,end,
)",
              output);
}

TEST_F(CodegenTest, StructArrayInit)
{
    std::string output = CompileToMadlen(R"(
        struct {
            int arr[3];
        } foo = { {10, 20, 30} };
)");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 12
             ,log, 24
             ,log, 36
             ,end,
)",
              output);
}

TEST_F(CodegenTest, StructMixedInit)
{
    std::string output = CompileToMadlen(R"(
        struct {
            int n;
            double d;
        } foo = { 7, 2.5 };
)");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 7
             ,real, 2.5
             ,end,
)",
              output);
}

TEST_F(CodegenTest, StructNestedInit)
{
    std::string output = CompileToMadlen(R"(
        struct pt {
            int x, y;
        };
        struct {
            struct pt p;
            int z;
        } foo = { {1, 2}, 3 };
)");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 1
             ,log, 2
             ,log, 3
             ,end,
)",
              output);
}

TEST_F(CodegenTest, ArrayPartialInit)
{
    std::string output = CompileToMadlen("int foo[5] = { 1, 2 };");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 1
             ,log, 2
             ,bss, 3
             ,end,
)",
              output);
}

TEST_F(CodegenTest, StructPartialInit)
{
    std::string output = CompileToMadlen(R"(
        struct {
            int a;
            int b;
            int c;
        } foo = { 5 };
)");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 5
             ,bss, 2
             ,end,
)",
              output);
}

TEST_F(CodegenTest, StrEmptyInit)
{
    std::string output = CompileToMadlen("char foo[] = \"\";");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 0
             ,end,
)",
              output);
}

TEST_F(CodegenTest, StrSingleCharInit)
{
    std::string output = CompileToMadlen("char foo[] = \"A\";");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 2020000000000000
             ,end,
)",
              output);
}

TEST_F(CodegenTest, StrThreeCharsInit)
{
    std::string output = CompileToMadlen("char foo[] = \"ABC\";");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 2024110300000000
             ,end,
)",
              output);
}

TEST_F(CodegenTest, StrSixCharsNoNullInit)
{
    std::string output = CompileToMadlen("char foo[6] = \"ABCDEF\";");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 2024110321042506
             ,end,
)",
              output);
}

TEST_F(CodegenTest, StrSixCharsWithNullInit)
{
    std::string output = CompileToMadlen("char foo[] = \"ABCDEF\";");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 2024110321042506
             ,log, 0
             ,end,
)",
              output);
}

TEST_F(CodegenTest, StrSevenCharsInit)
{
    std::string output = CompileToMadlen("char foo[] = \"ABCDEFG\";");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 2024110321042506
             ,log, 2160000000000000
             ,end,
)",
              output);
}

TEST_F(CodegenTest, StrWithZeroPaddingInit)
{
    std::string output = CompileToMadlen("char foo[8] = \"ABC\";");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 2024110300000000
             ,bss, 1
             ,end,
)",
              output);
}

TEST_F(CodegenTest, StrCyrillicInit)
{
    // "Абракадабра" — 11 Cyrillic chars + NUL = 12 bytes = 2 BESM-6 words.
    // UTF-8 → KOI7: А→41 б→62 р→50 а→41 к→4B а→41 | д→64 а→41 б→62 р→50 а→41 00
    std::string output = CompileToMadlen(R"(
        char foo[] = "Абракадабра";
)");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 2026112020245501
             ,log, 3104054224040400
             ,end,
)",
              output);
}

// ---------------------------------------------------------------------------
// String pointer initialization — TAC_TOPLEVEL_STATIC_CONSTANT tests
// ---------------------------------------------------------------------------
// Each `char *p = "..."` emits two modules:
//   1. The string constant module (_str0) with packed-char log words.
//   2. The pointer variable module (p) with a subp/z00 reference to _str0.

TEST_F(CodegenTest, StrConstantEmptyPtr)
{
    std::string output = CompileToMadlen("char *p = \"\";");
    EXPECT_EQ(R"(c  const
    *str0:   ,name,
             ,log, 0
             ,end,
c
        p:   ,name,
    *str0:   ,subp,
             ,z00,
             ,z00, *str0
             ,end,
)",
              output);
}

TEST_F(CodegenTest, StrConstantSingleCharPtr)
{
    std::string output = CompileToMadlen("char *p = \"A\";");
    EXPECT_EQ(R"(c  const
    *str0:   ,name,
             ,log, 2020000000000000
             ,end,
c
        p:   ,name,
    *str0:   ,subp,
             ,z00,
             ,z00, *str0
             ,end,
)",
              output);
}

TEST_F(CodegenTest, StrConstantThreeCharsPtr)
{
    std::string output = CompileToMadlen("char *p = \"ABC\";");
    EXPECT_EQ(R"(c  const
    *str0:   ,name,
             ,log, 2024110300000000
             ,end,
c
        p:   ,name,
    *str0:   ,subp,
             ,z00,
             ,z00, *str0
             ,end,
)",
              output);
}

// "ABCDE\0" = 6 bytes, exactly one packed word (no second word needed).
TEST_F(CodegenTest, StrConstantFiveCharsPtr)
{
    std::string output = CompileToMadlen("char *p = \"ABCDE\";");
    EXPECT_EQ(R"(c  const
    *str0:   ,name,
             ,log, 2024110321042400
             ,end,
c
        p:   ,name,
    *str0:   ,subp,
             ,z00,
             ,z00, *str0
             ,end,
)",
              output);
}

// "ABCDEF\0" = 7 bytes → two words: full word + null-only word.
TEST_F(CodegenTest, StrConstantSixCharsPtr)
{
    std::string output = CompileToMadlen("char *p = \"ABCDEF\";");
    EXPECT_EQ(R"(c  const
    *str0:   ,name,
             ,log, 2024110321042506
             ,log, 0
             ,end,
c
        p:   ,name,
    *str0:   ,subp,
             ,z00,
             ,z00, *str0
             ,end,
)",
              output);
}

// "ABCDEFG\0" = 8 bytes → two words: ABCDEF + G\0.
TEST_F(CodegenTest, StrConstantSevenCharsPtr)
{
    std::string output = CompileToMadlen("char *p = \"ABCDEFG\";");
    EXPECT_EQ(R"(c  const
    *str0:   ,name,
             ,log, 2024110321042506
             ,log, 2160000000000000
             ,end,
c
        p:   ,name,
    *str0:   ,subp,
             ,z00,
             ,z00, *str0
             ,end,
)",
              output);
}

// Two declarations processed separately: each gets its own _strN constant.
// symtab_add_string assigns unique names regardless of string content.
TEST_F(CodegenTest, StrConstantTwoPtrs)
{
    std::string output = CompileToMadlen("char *p = \"ABC\"; char *q = \"ABC\";");
    EXPECT_EQ(R"(c  const
    *str0:   ,name,
             ,log, 2024110300000000
             ,end,
c
        p:   ,name,
    *str0:   ,subp,
             ,z00,
             ,z00, *str0
             ,end,
c  const
    *str1:   ,name,
             ,log, 2024110300000000
             ,end,
c
        q:   ,name,
    *str1:   ,subp,
             ,z00,
             ,z00, *str1
             ,end,
)",
              output);
}

// A char array init uses TAC_STATIC_INIT_STRING directly (no static constant).
// A char pointer init generates a separate _str0 constant module.
TEST_F(CodegenTest, StrConstantPtrAndArray)
{
    std::string output = CompileToMadlen("char arr[] = \"ABC\"; char *p = \"ABC\";");
    EXPECT_EQ(R"(c
      arr:   ,name,
             ,log, 2024110300000000
             ,end,
c  const
    *str0:   ,name,
             ,log, 2024110300000000
             ,end,
c
        p:   ,name,
    *str0:   ,subp,
             ,z00,
             ,z00, *str0
             ,end,
)",
              output);
}
