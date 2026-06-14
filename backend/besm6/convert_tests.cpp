#include "codegen_test.h"

// Integer width conversions (task #17).  Under the BESM-6 target short/int/long/pointer
// are all one 48-bit word, so the only width that differs from a full word is char (8
// bits).  TRUNCATE and ZERO_EXTEND therefore both reduce to masking the low 8 bits
// (aax =377); SIGN_EXTEND additionally reinterprets bit 7 as the sign via the branchless
// (x ^ 0x80) - 0x80 trick (aax =377 / aex =200 / a-x =200).  Operands are volatile in the
// run tests so the optimizer cannot fold the conversion away.

// --- Madlen shape -----------------------------------------------------------------

// int → char: keep the low 8 bits.
TEST_F(CodegenTest, TruncateToCharMadlen)
{
    std::string output = CompileToMadlen("extern char g; void foo(int a) { g = a; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
             ,aax, =377
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// unsigned char → unsigned: clear all but the low 8 bits (same sequence as truncate).
TEST_F(CodegenTest, ZeroExtendFromUCharMadlen)
{
    std::string output =
        CompileToMadlen("extern unsigned g; void foo(unsigned char a) { g = a; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
             ,aax, =377
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// signed char → int: mask, flip the sign bit, subtract 0x80.
TEST_F(CodegenTest, SignExtendFromSCharMadlen)
{
    std::string output =
        CompileToMadlen("extern int g; void foo(signed char a) { g = a; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
             ,aax, =377
             ,aex, =200
             ,a-x, =200
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// --- Runtime behaviour ------------------------------------------------------------

// Truncation keeps only the low 8 bits: 300 & 0xFF = 44 (bit 7 clear → positive).
TEST_F(CodegenTest, TruncateToChar)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = 300;
            char c = a;
            printf("%d\n", c);
        }
    )");
    EXPECT_EQ("44\n", result);
}

// Sign extension of a byte with bit 7 set: (signed char)200 = -56.
TEST_F(CodegenTest, SignExtendNegativeByte)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = 200;
            signed char c = a;
            int x = c;
            printf("%d\n", x);
        }
    )");
    EXPECT_EQ("-56\n", result);
}

// Sign extension round-trips a small negative value.
TEST_F(CodegenTest, SignExtendNegativeOne)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile signed char c = -5;
            int x = c;
            printf("%d\n", x);
        }
    )");
    EXPECT_EQ("-5\n", result);
}

// Zero extension never sets the high bits, even for a byte with bit 7 set.
TEST_F(CodegenTest, ZeroExtendHighByte)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile unsigned char c = 200;
            unsigned int x = c;
            printf("%o\n", x);
        }
    )");
    EXPECT_EQ("310\n", result); // 200 decimal in octal
}

// Truncate then zero-extend: 300 → 44, kept positive through the widening.
TEST_F(CodegenTest, TruncateThenZeroExtend)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = 300;
            unsigned char c = a;
            unsigned int x = c;
            printf("%o\n", x);
        }
    )");
    EXPECT_EQ("54\n", result); // 44 decimal in octal
}
