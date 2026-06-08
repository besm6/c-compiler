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
)", output);
}

TEST_F(CodegenTest, VarIntPtrTentative)
{
    std::string output = CompileToMadlen("int *foo;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,bss, 1
             ,end,
)", output);
}

TEST_F(CodegenTest, VarCharPtrTentative)
{
    std::string output = CompileToMadlen("char *foo;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,bss, 1
             ,end,
)", output);
}

TEST_F(CodegenTest, VarVoidPtrTentative)
{
    std::string output = CompileToMadlen("void *foo;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,bss, 1
             ,end,
)", output);
}

TEST_F(CodegenTest, VarIntArrayTentative)
{
    std::string output = CompileToMadlen("int arr[5];");
    EXPECT_EQ(R"(c
      arr:   ,name,
             ,bss, 5
             ,end,
)", output);
}

TEST_F(CodegenTest, VarCharArrayTentative)
{
    std::string output = CompileToMadlen("char arr[10];");
    EXPECT_EQ(R"(c
      arr:   ,name,
             ,bss, 2
             ,end,
)", output);
}

TEST_F(CodegenTest, VarPtrArrayTentative)
{
    std::string output = CompileToMadlen("int *arr[4];");
    EXPECT_EQ(R"(c
      arr:   ,name,
             ,bss, 4
             ,end,
)", output);
}

TEST_F(CodegenTest, VarDoubleArrayTentative)
{
    std::string output = CompileToMadlen("double arr[3];");
    EXPECT_EQ(R"(c
      arr:   ,name,
             ,bss, 3
             ,end,
)", output);
}

TEST_F(CodegenTest, Var2DArrayTentative)
{
    std::string output = CompileToMadlen("int arr[2][3];");
    EXPECT_EQ(R"(c
      arr:   ,name,
             ,bss, 6
             ,end,
)", output);
}

TEST_F(CodegenTest, VarStruct1FieldTentative)
{
    std::string output = CompileToMadlen("struct { int x; } s;");
    EXPECT_EQ(R"(c
        s:   ,name,
             ,bss, 1
             ,end,
)", output);
}

TEST_F(CodegenTest, VarStruct2FieldTentative)
{
    std::string output = CompileToMadlen("struct { int x; int y; } s;");
    EXPECT_EQ(R"(c
        s:   ,name,
             ,bss, 2
             ,end,
)", output);
}

TEST_F(CodegenTest, VarStruct3FieldTentative)
{
    std::string output = CompileToMadlen("struct { int x; int y; int z; } s;");
    EXPECT_EQ(R"(c
        s:   ,name,
             ,bss, 3
             ,end,
)", output);
}

TEST_F(CodegenTest, VarStructMixedTentative)
{
    std::string output = CompileToMadlen("struct { char c; int n; double d; } s;");
    EXPECT_EQ(R"(c
        s:   ,name,
             ,bss, 3
             ,end,
)", output);
}

TEST_F(CodegenTest, VarNamedStructTentative)
{
    std::string output = CompileToMadlen("struct pt { int x; int y; }; struct pt s;");
    EXPECT_EQ(R"(c
        s:   ,name,
             ,bss, 2
             ,end,
)", output);
}

TEST_F(CodegenTest, VarIntInit)
{
    std::string output = CompileToMadlen("int foo = 42;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 52
             ,end,
)", output);
}

TEST_F(CodegenTest, VarLongInit)
{
    std::string output = CompileToMadlen("long foo = 4321;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 10341
             ,end,
)", output);
}

TEST_F(CodegenTest, VarShortInit)
{
    std::string output = CompileToMadlen("short foo = 123;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 173
             ,end,
)", output);
}

TEST_F(CodegenTest, VarCharInit)
{
    std::string output = CompileToMadlen("char foo = '+';");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 53
             ,end,
)", output);
}

TEST_F(CodegenTest, VarUnsignedInit)
{
    std::string output = CompileToMadlen("unsigned foo = 01234567076543210;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 1234567076543210
             ,end,
)", output);
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
)", output);
}

TEST_F(CodegenTest, VarIntPtrInitLiteral)
{
    std::string output = CompileToMadlen("int *bar = (int*) 42;");
    EXPECT_EQ(R"(c
      bar:   ,name,
             ,log, 52
             ,end,
)", output);
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
)", output);
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
)", output);
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
)", output);
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
)", output);
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
)", output);
}
