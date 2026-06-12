#include "codegen_test.h"

TEST_F(CodegenTest, AddTwoParams)
{
    // binary ADD src1=a(6,0) src2=b(6,1) dst=t.0; copy t.0 → global g
    // frame: a@(6,0), b@(6,1), t.0@(7,0); num_autos=1
    std::string output = CompileToMadlen("int g; void foo(int a, int b) { g = a + b; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
           6 ,a+x, 1
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, SubTwoParams)
{
    // binary SUBTRACT src1=a(6,0) src2=b(6,1) dst=t.0; copy t.0 → global g
    // frame: a@(6,0), b@(6,1), t.0@(7,0); num_autos=1
    std::string output = CompileToMadlen("int g; void foo(int a, int b) { g = a - b; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
           6 ,a-x, 1
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, AddAutoAndParam)
{
    // binary ADD src1=b(param) src2=c(auto) dst=t.0; copy t.0 → global g
    // frame: b@(6,0), c@(7,0), t.0@(7,1); num_autos=2
    std::string output = CompileToMadlen("int g; void foo(int b) { int c; g = b + c; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 2
           6 ,xta,
           7 ,a+x,
           7 ,atx, 1
           7 ,xta, 1
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, AddTwoAutos)
{
    // binary ADD src1=a(auto) src2=b(auto) dst=t.0; copy t.0 → global g
    // frame: a@(7,0), b@(7,1), t.0@(7,2); num_autos=3
    std::string output = CompileToMadlen("int g; void foo(void) { int a; int b; g = a + b; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save0
          15 ,utm, 3
           7 ,xta,
           7 ,a+x, 1
           7 ,atx, 2
           7 ,xta, 2
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// BINARY with a constant right operand: g = a + 5.
TEST_F(CodegenTest, BinaryConstSrc2)
{
    std::string output = CompileToMadlen("int g; void foo(int a) { g = a + 5; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
             ,a+x, =5
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// BINARY with a constant left operand: g = 5 + a.
TEST_F(CodegenTest, BinaryConstSrc1)
{
    std::string output = CompileToMadlen("int g; void foo(int a) { g = 5 + a; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
             ,xta, =5
           6 ,a+x,
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// Integer comparisons (task #4).  Operands are volatile so the optimizer cannot fold
// the comparisons at compile time; each comparison lowers to a runtime relational
// helper call (b/lt, b/le, b/gt, b/ge, b/eq, b/ne) that leaves a raw 0/1 in A.

// All six operators with a > b (5 vs 3): <, <=, >, >=, ==, != → 0 0 1 1 0 1.
TEST_F(CodegenTest, CompareSignedGreater)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = 5, b = 3;
            printf("%d%d%d%d%d%d\n", a < b, a <= b, a > b, a >= b, a == b, a != b);
        }
    )");
    EXPECT_EQ("001101\n", result);
}

// All six operators with a < b (3 vs 5): <, <=, >, >=, ==, != → 1 1 0 0 0 1.
TEST_F(CodegenTest, CompareSignedLess)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = 3, b = 5;
            printf("%d%d%d%d%d%d\n", a < b, a <= b, a > b, a >= b, a == b, a != b);
        }
    )");
    EXPECT_EQ("110001\n", result);
}

// All six operators with a == b (4 vs 4): <, <=, >, >=, ==, != → 0 1 0 1 1 0.
TEST_F(CodegenTest, CompareSignedEqual)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = 4, b = 4;
            printf("%d%d%d%d%d%d\n", a < b, a <= b, a > b, a >= b, a == b, a != b);
        }
    )");
    EXPECT_EQ("010110\n", result);
}

// Negative operands exercise the 41-bit signed sign test (-7 < 2).
TEST_F(CodegenTest, CompareSignedNegative)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = -7, b = 2;
            printf("%d%d%d%d%d%d\n", a < b, a <= b, a > b, a >= b, a == b, a != b);
        }
    )");
    EXPECT_EQ("110001\n", result);
}

// The four unsigned ordering ops on small values (5 vs 3): <, <=, >, >= → 0 0 1 1.
TEST_F(CodegenTest, CompareUnsignedGreater)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile unsigned x = 5, y = 3;
            printf("%d%d%d%d\n", x < y, x <= y, x > y, x >= y);
        }
    )");
    EXPECT_EQ("0011\n", result);
}

// The four unsigned ordering ops on small values (3 vs 5): <, <=, >, >= → 1 1 0 0.
TEST_F(CodegenTest, CompareUnsignedLess)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile unsigned x = 3, y = 5;
            printf("%d%d%d%d\n", x < y, x <= y, x > y, x >= y);
        }
    )");
    EXPECT_EQ("1100\n", result);
}

// The four unsigned ordering ops on small values (4 vs 4): <, <=, >, >= → 0 1 0 1.
TEST_F(CodegenTest, CompareUnsignedEqual)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile unsigned x = 4, y = 4;
            printf("%d%d%d%d\n", x < y, x <= y, x > y, x >= y);
        }
    )");
    EXPECT_EQ("0101\n", result);
}

// A comparison feeding an if: the 0/1 result drives the already-working
// JUMP_IF_ZERO control flow.
TEST_F(CodegenTest, CompareInBranch)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = 7, b = 4;
            if (a > b)
                printf("yes\n");
            else
                printf("no\n");
        }
    )");
    EXPECT_EQ("YES\n", result);
}

// Madlen-level shape check: a comparison emits xta / xts / ,call, b/lt / atx, with no
// caller-side stack pop (the helper pops its own operand).
TEST_F(CodegenTest, CompareMadlenShape)
{
    std::string out = CompileToMadlen(R"(
        int cmp(int a, int b) {
            return a < b;
        }
    )");
    EXPECT_NE(out.find(",xts,"), std::string::npos);
    EXPECT_NE(out.find(",call, b/lt"), std::string::npos);
    // No UTM stack adjustment is emitted around the helper call.
    EXPECT_EQ(out.find("15 ,utm, -1"), std::string::npos);
}
