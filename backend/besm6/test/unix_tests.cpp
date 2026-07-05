//
// Golden-file tests for the Unix (b6as) assembler emitter (emit_unix.c).
//
// These assert the exact `.s` text produced for a representative set (scalar, call,
// global, float const, string, static array, pointer relocation) — the U1 acceptance
// criteria.  The `$` in helper names (b$save, b$ret) passes through the Unix sanitizer
// unchanged, whereas the Madlen emitter lowers it to `/`.
//
// Listing format: lines indent with 4 spaces; an instruction's leading modreg occupies a
// ` NN ` field (space + register right-aligned to width 2 + space) — exactly 4 columns, so
// mnemonics align at column 4 with or without a register.
//
#include "codegen_test.h"

// A scalar-returning function: prologue (its/b$save), param load, `#`-pool constant, epilogue.
TEST_F(CodegenTest, UnixScalarReturn)
{
    std::string out = CompileToUnix("int f(int x) { return x + 1; }");
    EXPECT_EQ(R"(    .text
    .globl f
f:
    its 13
 13 vjm b$save
  6 xta
    a+x #01
    uj b$ret
)",
              out);
}

// A direct call: ` 13 vjm g` (link register r13); b6as auto-externs the undefined callee.
TEST_F(CodegenTest, UnixCall)
{
    std::string out = CompileToUnix("int g(int); int f(int x) { return g(x) + 1; }");
    EXPECT_EQ(R"(    .text
    .globl f
f:
    its 13
 13 vjm b$save
  6 xta
 14 vtm -1
 13 vjm g
    a+x #01
    uj b$ret
)",
              out);
}

// A module-level global: an all-zero tentative definition lands in .bss; the accessor
// reaches it via `utc counter` + bare `xta`.
TEST_F(CodegenTest, UnixGlobalAccess)
{
    std::string out = CompileToUnix("int counter; int f(void) { return counter; }");
    EXPECT_EQ(R"(    .bss
    .globl counter
counter:
    . = . + 1
    .text
    .globl f
f:
    its 13
 13 vjm b$save0
    utc counter
    xta
    uj b$ret
)",
              out);
}

// A floating constant flows through the `#`-pool.  b6as has no float literal syntax, so
// the real is rendered as its native BESM-6 48-bit FP bit pattern in octal (3.14).
TEST_F(CodegenTest, UnixFloatConst)
{
    std::string out = CompileToUnix("double f(void) { return 3.14; }");
    EXPECT_EQ(R"(    .text
    .globl f
f:
    its 13
 13 vjm b$save0
    xta #04114436560507534
    uj b$ret
)",
              out);
}

// A static array: one octal `.word` per element in .data.
TEST_F(CodegenTest, UnixStaticArray)
{
    std::string out = CompileToUnix("int a[3] = {10, 20, 30};");
    EXPECT_EQ(R"(    .data
    .globl a
a:
    .word 012
    .word 024
    .word 036
)",
              out);
}

// A string-valued pointer: a fat pointer (byte-offset marker 13 on the leading modreg)
// emitted as a raw `@00` pair, followed by the folded KOI-7 string constant.
TEST_F(CodegenTest, UnixStringGlobal)
{
    std::string out = CompileToUnix("char *msg = \"HI\";");
    EXPECT_EQ(R"(    .data
    .globl msg
msg:
 13 @00
    @00 _str0
_str0:
    .word 02204440000000000
)",
              out);
}

// A plain pointer to a global: the Z00 pair coalesces into one relocatable `.word g`.
TEST_F(CodegenTest, UnixPointerToGlobal)
{
    std::string out = CompileToUnix("int g; int *p = &g;");
    EXPECT_EQ(R"(    .bss
    .globl g
g:
    . = . + 1
    .data
    .globl p
p:
    .word g
)",
              out);
}

// A double global: its native-FP value renders as a `.word` holding the BESM-6 48-bit FP
// bit pattern in octal (2.5).
TEST_F(CodegenTest, UnixDoubleGlobal)
{
    std::string out = CompileToUnix("double d = 2.5;");
    EXPECT_EQ(R"(    .data
    .globl d
d:
    .word 04112000000000000
)",
              out);
}

// A Madlen literal-address expression (`=:64`, the INT-format exponent word) becomes a
// b6as `#`-pool constant; b6as has no `=` literal syntax.
TEST_F(CodegenTest, UnixIntFormatLiteral)
{
    std::string out = CompileToUnix("double g(int n) { return n; }");
    EXPECT_EQ(R"(    .text
    .globl g
g:
    its 13
 13 vjm b$save
  6 xta
    aox #06400000000000000
    ntr 0
    a+x
    ntr 7
    uj b$ret
)",
              out);
}

// A numeric compiler label (`%0`) sanitizes to `..0`, not `.0` — a bare `.N` is a b6as
// bit-mask literal, so a digit-leading body gets a second leading dot to stay a name.
TEST_F(CodegenTest, UnixNumericLabel)
{
    std::string out = CompileToUnix("int f(int x) { if (x) return 1; return 0; }");
    EXPECT_EQ(R"(    .text
    .globl f
f:
    its 13
 13 vjm b$save
  6 xta
    uza ..0
    xta #01
    uj b$ret
..0:
    xta #00
    uj b$ret
)",
              out);
}
