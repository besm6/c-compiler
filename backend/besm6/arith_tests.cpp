#include "codegen_test.h"

TEST_F(CodegenTest, AddTwoParams)
{
    // binary ADD src1=a(6,0) src2=b(6,1) dst=%0; copy %0 → global g
    // frame: a@(6,0), b@(6,1), %0@(7,0); num_autos=1
    std::string output = CompileToMadlen("extern int g; void foo(int a, int b) { g = a + b; }");
    EXPECT_EQ(R"(c
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
    // binary SUBTRACT src1=a(6,0) src2=b(6,1) dst=%0; copy %0 → global g
    // frame: a@(6,0), b@(6,1), %0@(7,0); num_autos=1
    std::string output = CompileToMadlen("extern int g; void foo(int a, int b) { g = a - b; }");
    EXPECT_EQ(R"(c
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
    // binary ADD src1=b(param) src2=c(auto) dst=%0; copy %0 → global g
    // frame: b@(6,0), c@(7,0), %0@(7,1); num_autos=2
    std::string output = CompileToMadlen("extern int g; void foo(int b) { int c; g = b + c; }");
    EXPECT_EQ(R"(c
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
    // binary ADD src1=a(auto) src2=b(auto) dst=%0; copy %0 → global g
    // frame: a@(7,0), b@(7,1), %0@(7,2); num_autos=3
    std::string output = CompileToMadlen("extern int g; void foo(void) { int a; int b; g = a + b; }");
    EXPECT_EQ(R"(c
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
    std::string output = CompileToMadlen("extern int g; void foo(int a) { g = a + 5; }");
    EXPECT_EQ(R"(c
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
    std::string output = CompileToMadlen("extern int g; void foo(int a) { g = 5 + a; }");
    EXPECT_EQ(R"(c
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

// Bitwise AND lowers to a single AAX, same XTA / op / ATX shape as ADD.
TEST_F(CodegenTest, BitwiseAndTwoParams)
{
    // binary BITWISE_AND src1=a(6,0) src2=b(6,1) dst=%0; copy %0 → global g
    // frame: a@(6,0), b@(6,1), %0@(7,0); num_autos=1
    std::string output = CompileToMadlen("extern int g; void foo(int a, int b) { g = a & b; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
           6 ,aax, 1
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// Bitwise OR lowers to a single AOX.
TEST_F(CodegenTest, BitwiseOrTwoParams)
{
    std::string output = CompileToMadlen("extern int g; void foo(int a, int b) { g = a | b; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
           6 ,aox, 1
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// Bitwise XOR lowers to a single AEX.
TEST_F(CodegenTest, BitwiseXorTwoParams)
{
    std::string output = CompileToMadlen("extern int g; void foo(int a, int b) { g = a ^ b; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
           6 ,aex, 1
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// End-to-end: the three bitwise ops compute the expected values at run time.
TEST_F(CodegenTest, BitwiseRun)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = 12, b = 10;
            printf("%d %d %d\n", a & b, a | b, a ^ b);
        }
    )");
    EXPECT_EQ("8 14 6\n", result);
}

// Constant left shift by k inlines a single ASN with field 64-k (here 64-3 = 61).
TEST_F(CodegenTest, LeftShiftConstant)
{
    std::string output = CompileToMadlen("extern int g; void foo(int a) { g = a << 3; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
             ,asn, 61
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// Constant right shift by k inlines a single ASN with field 64+k (here 64+2 = 66).
TEST_F(CodegenTest, RightShiftConstant)
{
    std::string output = CompileToMadlen("extern int g; void foo(int a) { g = a >> 2; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
             ,asn, 66
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// Variable left shift calls the b/lsh runtime helper (value on stack, count in A).
TEST_F(CodegenTest, LeftShiftVariable)
{
    std::string output = CompileToMadlen("extern int g; void foo(int a, int b) { g = a << b; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
           6 ,xts, 1
             ,call, b/lsh
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// Variable right shift calls the b/rsh runtime helper.
TEST_F(CodegenTest, RightShiftVariable)
{
    std::string output = CompileToMadlen("extern int g; void foo(int a, int b) { g = a >> b; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
           6 ,xts, 1
             ,call, b/rsh
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// End-to-end: constant-count shifts compute the expected values at run time.
TEST_F(CodegenTest, ShiftConstantRun)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = 1, b = 40;
            printf("%d %d\n", a << 5, b >> 2);
        }
    )");
    EXPECT_EQ("32 10\n", result);
}

// End-to-end: variable-count shifts (via b/lsh / b/rsh) compute the expected values.
TEST_F(CodegenTest, ShiftVariableRun)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = 3, b = 4, c = 100, d = 3;
            printf("%d %d\n", a << b, c >> d);
        }
    )");
    EXPECT_EQ("48 12\n", result);
}

// End-to-end: unsigned right shift is logical (no sign extension) and matches int.
TEST_F(CodegenTest, ShiftUnsignedRightRun)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile unsigned a = 200, b = 3;
            printf("%d %d\n", a >> b, a << 1);
        }
    )");
    EXPECT_EQ("25 400\n", result);
}

// Madlen-level shape: unsigned ADD lowers to the b/uadd helper (xta / xts / ,call, b/uadd
// / atx) rather than the inline A+X used for signed ADD, and emits no caller-side stack pop.
TEST_F(CodegenTest, AddUnsignedMadlenShape)
{
    std::string out =
        CompileToMadlen("extern unsigned g; void foo(unsigned a, unsigned b) { g = a + b; }");
    EXPECT_NE(out.find(",xts,"), std::string::npos);
    EXPECT_NE(out.find(",call, b/uadd"), std::string::npos);
    // Unsigned add must NOT use the inline additive unit.
    EXPECT_EQ(out.find(",a+x,"), std::string::npos);
    // The helper pops its own operand: no UTM stack adjustment around the call.
    EXPECT_EQ(out.find("15 ,utm, -1"), std::string::npos);
}

// A wide unsigned literal reaches the backend with all 48 bits when written with a plain
// `U` suffix (no `L` required): const_lit_name masks unsigned constants to 48 bits, so
// 0xFFFFFFFFFFFFU emits the full 16-octal-digit literal rather than a 41-bit-masked value.
TEST_F(CodegenTest, WideUnsignedLiteralUSuffix)
{
    std::string out = CompileToMadlen(
        "extern unsigned g; void foo(void) { g = 0xFFFFFFFFFFFFU; }");
    EXPECT_NE(out.find("=7777777777777777"), std::string::npos);
}

// End-to-end: true 48-bit modular unsigned add via b/uadd.  Results are printed in octal
// (%o prints the whole word, leading zeros stripped).
TEST_F(CodegenTest, AddUnsignedRun)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void check(unsigned a, unsigned b) { printf("%o\n", a + b); }
        void program() {
            /* Fixed edge cases. */
            check(0, 0);
            check(0, 1);
            check(1, 0);
            check(1, 1);

            /* Carry from low half into high half. */
            check(077777777U, 1);                           /* 0xFFFFFF + 1 */
            check(077777777U, 1);                           /* 0x0000FFFFFF + 1 */

            /* High half boundary -> mod 2^48. */
            check(07777777700000000U, 0100000000U);         /* 0xFFFFFF000000 + 0x1000000 */

            /* Maximum 48-bit values. */
            check(07777777777777777U, 0);                   /* 0xFFFFFFFFFFFF */
            check(07777777777777777U, 1);                   /* wraps to 0 */
            check(07777777777777777U, 07777777777777777U);  /* wraps */

            /* Halfway / pattern values. */
            check(03777777777777777U, 1);                   /* 0x7FFFFFFFFFFF + 1 */
            check(04000000000000000U, 04000000000000000U);  /* 2^47 + 2^47 = 0 */
            check(05252525252525252U, 02525252525252525U);  /* 0xAAA.. + 0x555.. */
            check(0443212636115274U, 06272460731241441U);   /* 0x123456789ABC + 0xCBA987654321 */

            /* Carry chain across the 24-bit boundary exactly. */
            check(07777777777777777U, 1);
        }
    )");
    EXPECT_EQ(
        "0\n"
        "1\n"
        "1\n"
        "2\n"
        "100000000\n"
        "100000000\n"
        "0\n"
        "7777777777777777\n"
        "0\n"
        "7777777777777776\n"
        "4000000000000000\n"
        "0\n"
        "7777777777777777\n"
        "6735673567356735\n"
        "0\n",
        result);
}
