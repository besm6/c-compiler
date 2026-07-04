//
// Golden-file tests for the Unix (b6as) assembler emitter (emit_unix.c).
//
// These assert the exact `.s` text produced for a representative set (scalar, call,
// global, float const, string, static array, pointer relocation) — the U1 acceptance
// criteria.  The `$` in helper names (b$save, b$ret) passes through the Unix sanitizer
// unchanged, whereas the Madlen emitter lowers it to `/`.
//
#include "codegen_test.h"

// A scalar-returning function: prologue (its/b$save), param load, `#`-pool constant, epilogue.
TEST_F(CodegenTest, UnixScalarReturn)
{
    std::string out = CompileToUnix("int f(int x) { return x + 1; }");
    EXPECT_EQ("\t.text\n"
              "\t.globl f\n"
              "f:\n"
              "\tits 13\n"
              "\t13 vjm b$save\n"
              "\t6 xta\n"
              "\ta+x #01\n"
              "\tuj b$ret\n",
              out);
}

// A direct call: `13 vjm g` (link register r13); b6as auto-externs the undefined callee.
TEST_F(CodegenTest, UnixCall)
{
    std::string out = CompileToUnix("int g(int); int f(int x) { return g(x) + 1; }");
    EXPECT_EQ("\t.text\n"
              "\t.globl f\n"
              "f:\n"
              "\tits 13\n"
              "\t13 vjm b$save\n"
              "\t6 xta\n"
              "\t14 vtm -1\n"
              "\t13 vjm g\n"
              "\ta+x #01\n"
              "\tuj b$ret\n",
              out);
}

// A module-level global: an all-zero tentative definition lands in .bss; the accessor
// reaches it via `utc counter` + bare `xta`.
TEST_F(CodegenTest, UnixGlobalAccess)
{
    std::string out = CompileToUnix("int counter; int f(void) { return counter; }");
    EXPECT_EQ("\t.bss\n"
              "\t.globl counter\n"
              "counter:\n"
              "\t. = . + 1\n"
              "\t.text\n"
              "\t.globl f\n"
              "f:\n"
              "\tits 13\n"
              "\t13 vjm b$save0\n"
              "\tutc counter\n"
              "\txta\n"
              "\tuj b$ret\n",
              out);
}

// A floating constant flows through the `#`-pool as `#<real>` (mandatory decimal point).
TEST_F(CodegenTest, UnixFloatConst)
{
    std::string out = CompileToUnix("double f(void) { return 3.14; }");
    EXPECT_EQ("\t.text\n"
              "\t.globl f\n"
              "f:\n"
              "\tits 13\n"
              "\t13 vjm b$save0\n"
              "\txta #3.14\n"
              "\tuj b$ret\n",
              out);
}

// A static array: one octal `.word` per element in .data.
TEST_F(CodegenTest, UnixStaticArray)
{
    std::string out = CompileToUnix("int a[3] = {10, 20, 30};");
    EXPECT_EQ("\t.data\n"
              "\t.globl a\n"
              "a:\n"
              "\t.word 012\n"
              "\t.word 024\n"
              "\t.word 036\n",
              out);
}

// A string-valued pointer: a fat pointer (byte-offset marker 13 on the leading modreg)
// emitted as a raw `@00` pair, followed by the folded KOI-7 string constant.
TEST_F(CodegenTest, UnixStringGlobal)
{
    std::string out = CompileToUnix("char *msg = \"HI\";");
    EXPECT_EQ("\t.data\n"
              "\t.globl msg\n"
              "msg:\n"
              "\t13 @00\n"
              "\t@00 _str0\n"
              "_str0:\n"
              "\t.word 02204440000000000\n",
              out);
}

// A plain pointer to a global: the Z00 pair coalesces into one relocatable `.word g`.
TEST_F(CodegenTest, UnixPointerToGlobal)
{
    std::string out = CompileToUnix("int g; int *p = &g;");
    EXPECT_EQ("\t.bss\n"
              "\t.globl g\n"
              "g:\n"
              "\t. = . + 1\n"
              "\t.data\n"
              "\t.globl p\n"
              "p:\n"
              "\t.word g\n",
              out);
}

// A double global: its native-FP value renders as a `.word <real>` data word.
TEST_F(CodegenTest, UnixDoubleGlobal)
{
    std::string out = CompileToUnix("double d = 2.5;");
    EXPECT_EQ("\t.data\n"
              "\t.globl d\n"
              "d:\n"
              "\t.word 2.5\n",
              out);
}
