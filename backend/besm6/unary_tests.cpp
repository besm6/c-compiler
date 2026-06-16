#include "codegen_test.h"

// Unary negate (task #6).  Operands are volatile so the optimizer cannot fold the
// negation at compile time; each negate reaches the backend and lowers to the
// representation-specific sequence: signed int → x-a 0, unsigned → b/uneg helper,
// double → ntr 0 / x-a 0 / ntr 7.

// Signed int negate: -5 → -5, and negating a negative restores the magnitude.
TEST_F(CodegenTest, NegateSignedInt)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = 5;
            volatile int b = -7;
            printf("%d %d\n", -a, -b);
        }
    )");
    EXPECT_EQ("-5 7\n", result);
}

// Madlen shape of the signed-int negate: load, x-a 0 (0 - A), store.
TEST_F(CodegenTest, NegateSignedIntMadlen)
{
    std::string output = CompileToMadlen("extern int g; void foo(int a) { g = -a; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
             ,x-a,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// Unsigned negate: -5u is the 48-bit modular complement, printed in octal.
TEST_F(CodegenTest, NegateUnsigned)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile unsigned a = 5;
            printf("%o\n", -a);
        }
    )");
    EXPECT_EQ("7777777777777773\n", result);
}

// Unsigned negate lowers to the b/uneg runtime helper.
TEST_F(CodegenTest, NegateUnsignedMadlen)
{
    std::string output = CompileToMadlen("extern unsigned g; void foo(unsigned a) { g = -a; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
             ,call, b/uneg
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// Double negate: flips the FP sign bit.  No %f runtime yet, so the result word is
// printed in octal.  The operand is a global initialized through static data (a ,real,
// word) rather than an inline FP literal, which the assembler does not yet accept.
TEST_F(CodegenTest, NegateDouble)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        double a = 2.0;
        void program() {
            printf("%o %o\n", a, -a);
        }
    )");
    EXPECT_EQ("4110000000000000 4060000000000000\n", result);
}

// Double negate brackets x-a 0 with ntr 0 / ntr 7 to enable normalization+rounding.
TEST_F(CodegenTest, NegateDoubleMadlen)
{
    std::string output = CompileToMadlen("extern double g; void foo(double a) { g = -a; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
             ,ntr, 0
             ,x-a,
             ,ntr, 7
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// Unary complement (~), part of task #6.  The sequence is uniform for int and
// unsigned: load, aex =7777777777777777 (flip all 48 bits), store.

// Unsigned complement: ~5u is the exact 48-bit complement, printed in octal.
TEST_F(CodegenTest, ComplementUnsigned)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile unsigned a = 5;
            printf("%o\n", ~a);
        }
    )");
    EXPECT_EQ("7777777777777772\n", result);
}

// Signed-int complement: flipping all 48 bits also sets the exponent field, so the
// result word is non-canonical (accepted UB).  Print the raw word in octal.
TEST_F(CodegenTest, ComplementSignedInt)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = 5;
            printf("%o\n", ~a);
        }
    )");
    EXPECT_EQ("7777777777777772\n", result);
}

// Madlen shape of complement: load, aex against the all-ones literal, store.  The
// path is type-independent, so this also covers int.
TEST_F(CodegenTest, ComplementMadlen)
{
    std::string output = CompileToMadlen("extern unsigned g; void foo(unsigned a) { g = ~a; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
             ,aex, =7777777777777777
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// Unary logical not (!), part of task #6.  Lowers to the b/not runtime helper, which
// returns 1 if the operand is zero and 0 otherwise, for any operand type.

// !0 is 1; !5 is 0.  The helper result reaches A and is printed in decimal.
TEST_F(CodegenTest, LogicalNot)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = 0;
            volatile int b = 5;
            printf("%d %d\n", !a, !b);
        }
    )");
    EXPECT_EQ("1 0\n", result);
}

// Madlen shape of logical not: load, call b/not, store.  The path is type-independent.
TEST_F(CodegenTest, LogicalNotMadlen)
{
    std::string output = CompileToMadlen("extern unsigned g; void foo(unsigned a) { g = !a; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
             ,call, b/not
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}
