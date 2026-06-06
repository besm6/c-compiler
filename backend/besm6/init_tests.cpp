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

TEST_F(CodegenTest, DISABLED_VarIntInit)
{
    std::string output = CompileToMadlen("int foo = 42;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 52
             ,end,
)", output);
}

TEST_F(CodegenTest, DISABLED_VarLongInit)
{
    std::string output = CompileToMadlen("long foo = 4321;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 10341
             ,end,
)", output);
}

TEST_F(CodegenTest, DISABLED_VarShortInit)
{
    std::string output = CompileToMadlen("short foo = 123;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 173
             ,end,
)", output);
}

TEST_F(CodegenTest, DISABLED_VarCharInit)
{
    std::string output = CompileToMadlen("char foo = '+';");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 53
             ,end,
)", output);
}

TEST_F(CodegenTest, DISABLED_VarUnsignedInit)
{
    std::string output = CompileToMadlen("unsigned foo = 01234567076543210;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,log, 1234567076543210
             ,end,
)", output);
}

TEST_F(CodegenTest, DISABLED_VarIntPtrInit)
{
    std::string output = CompileToMadlen("int foo; int *bar = &foo;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,bss, 1
             ,end,
c
      bar:   ,name,
      foo:   ,subp,
             ,z00,
             ,z00, foo
             ,end,
)", output);
}

TEST_F(CodegenTest, DISABLED_VarCharPtrInit)
{
    std::string output = CompileToMadlen("char foo; char *bar = &fooh;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,bss, 1
             ,end,
c
      bar:   ,name,
      foo:   ,subp,
           8 ,z00,
             ,z00, foo
             ,end,
)", output);
}

TEST_F(CodegenTest, DISABLED_VarVoidPtrInit)
{
    std::string output = CompileToMadlen("char foo; void *bar = &foo;");
    EXPECT_EQ(R"(c
      foo:   ,name,
             ,bss, 1
             ,end,
c
      bar:   ,name,
      foo:   ,subp,
           8 ,z00,
             ,z00, foo
             ,end,
)", output);
}
