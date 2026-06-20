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
           6 ,xta,
           6 ,a+x, 1
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
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
           6 ,xta,
           6 ,a-x, 1
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
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
          15 ,utm, 1
           6 ,xta,
           7 ,a+x,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

TEST_F(CodegenTest, AddTwoAutos)
{
    // binary ADD src1=a(auto) src2=b(auto) dst=%0; copy %0 → global g
    // frame: a@(7,0), b@(7,1), %0@(7,2); num_autos=3
    std::string output =
        CompileToMadlen("extern int g; void foo(void) { int a; int b; g = a + b; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save0
          15 ,utm, 2
           7 ,xta,
           7 ,a+x, 1
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
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
           6 ,xta,
             ,a+x, =5
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
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
             ,xta, =5
           6 ,a+x,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// Integer comparisons (task #4).  Operands are volatile so the optimizer cannot fold
// the comparisons at compile time; each comparison lowers to a runtime relational
// helper call (b/lt, b/le, b/gt, b/ge, b/eq, b/ne) that leaves a raw 0/1 in A.

// All six operators with a > b (5 vs 3): <, <=, >, >=, ==, != → 0 0 1 1 0 1.
TEST_F(CodegenTest, CompareSignedGreater)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
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
        #include <stdio.h>
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
        #include <stdio.h>
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
        #include <stdio.h>
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
        #include <stdio.h>
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
        #include <stdio.h>
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
        #include <stdio.h>
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
        #include <stdio.h>
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
           6 ,xta,
           6 ,aax, 1
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
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
           6 ,xta,
           6 ,aox, 1
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
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
           6 ,xta,
           6 ,aex, 1
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// End-to-end: the three bitwise ops compute the expected values at run time.
TEST_F(CodegenTest, BitwiseRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
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
           6 ,xta,
             ,asn, 61
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
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
           6 ,xta,
             ,asn, 66
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
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
           6 ,xta,
           6 ,xts, 1
             ,call, b/lsh
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
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
           6 ,xta,
           6 ,xts, 1
             ,call, b/rsh
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// End-to-end: constant-count shifts compute the expected values at run time.
TEST_F(CodegenTest, ShiftConstantRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
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
        #include <stdio.h>
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
        #include <stdio.h>
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
    std::string out = CompileToMadlen("extern unsigned g; void foo(void) { g = 0xFFFFFFFFFFFFU; }");
    EXPECT_NE(out.find("=7777777777777777"), std::string::npos);
}

// End-to-end: true 48-bit modular unsigned add via b/uadd.  Results are printed in octal
// (%o prints the whole word, leading zeros stripped).
TEST_F(CodegenTest, AddUnsignedRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
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

// Madlen-level shape: unsigned SUBTRACT lowers to the b/usub helper (xta / xts / ,call,
// b/usub / atx) rather than the inline A-X used for signed SUBTRACT, with no caller pop.
TEST_F(CodegenTest, SubUnsignedMadlenShape)
{
    std::string out =
        CompileToMadlen("extern unsigned g; void foo(unsigned a, unsigned b) { g = a - b; }");
    EXPECT_NE(out.find(",xts,"), std::string::npos);
    EXPECT_NE(out.find(",call, b/usub"), std::string::npos);
    // Unsigned subtract must NOT use the inline additive unit.
    EXPECT_EQ(out.find(",a-x,"), std::string::npos);
    // The helper pops its own operand: no UTM stack adjustment around the call.
    EXPECT_EQ(out.find("15 ,utm, -1"), std::string::npos);
}

// End-to-end: true 48-bit modular unsigned subtract via b/usub.  Results are printed in
// octal (%o prints the whole word, leading zeros stripped).  Exercises borrow across the
// 24-bit boundary, wrap-around on underflow, and the bit-48-set / bit-48-clear operand
// combinations the b/usub algorithm branches on.
TEST_F(CodegenTest, SubUnsignedRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void check(unsigned a, unsigned b) { printf("%o\n", a - b); }
        void program() {
            /* Fixed edge cases. */
            check(0, 0);
            check(1, 0);
            check(0, 1);                                    /* underflow -> all ones */
            check(1, 1);

            /* Borrow across the 24-bit boundary. */
            check(0100000000U, 1);                          /* 2^24 - 1 */

            /* Maximum 48-bit values. */
            check(07777777777777777U, 07777777777777777U);  /* MAX - MAX = 0 */
            check(0, 07777777777777777U);                   /* -MAX -> 1 */
            check(07777777777777777U, 1);                   /* MAX - 1 */

            /* Bit-48 operand combinations (2^47 has bit 48 set). */
            check(04000000000000000U, 04000000000000000U);  /* both bit48 set */
            check(0, 04000000000000000U);                   /* bit48 in b only -> 2^47 */
            check(04000000000000000U, 1);                   /* bit48 in a only */
            check(04000000000000000U, 07777777777777777U);  /* both bit48 set, underflow */

            /* Pattern values. */
            check(05252525252525252U, 02525252525252525U);  /* 0xAAA.. - 0x555.. */
            check(02525252525252525U, 05252525252525252U);  /* 0x555.. - 0xAAA.. underflow */
            check(06272460731241441U, 0443212636115274U);   /* big - small */
        }
    )");
    EXPECT_EQ(
        "0\n"
        "1\n"
        "7777777777777777\n"
        "0\n"
        "77777777\n"
        "0\n"
        "1\n"
        "7777777777777776\n"
        "0\n"
        "4000000000000000\n"
        "3777777777777777\n"
        "4000000000000001\n"
        "2525252525252525\n"
        "5252525252525253\n"
        "5627246073124145\n",
        result);
}

// Madlen-level shape: signed MULTIPLY lowers to the b/mul helper (xta / xts / ,call, b/mul
// / atx) rather than an inline A*X, and emits no caller-side stack pop.
TEST_F(CodegenTest, MultiplyMadlenShape)
{
    std::string out = CompileToMadlen("extern int g; void foo(int a, int b) { g = a * b; }");
    EXPECT_NE(out.find(",xts,"), std::string::npos);
    EXPECT_NE(out.find(",call, b/mul"), std::string::npos);
    // Multiply must NOT inline the multiplicative unit; the helper bridges INT-format.
    EXPECT_EQ(out.find(",a*x,"), std::string::npos);
    // The helper pops its own operand: no UTM stack adjustment around the call.
    EXPECT_EQ(out.find("15 ,utm, -1"), std::string::npos);
}

// End-to-end: signed multiply via b/mul.  Covers sign combinations, multiply by 0 and 1,
// and large products that stay within the signed 41-bit range (|x| < 2^40).
TEST_F(CodegenTest, MultiplyRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void check(int a, int b) { printf("%d\n", a * b); }
        void program() {
            check(3, 5);              /* 15 */
            check(12345, 0);          /* 0 */
            check(1, -7);             /* -7 */
            check(-6, 7);             /* -42 */
            check(-8, -9);            /* 72 */
            check(1000000, 1000000);  /* 10^12, < 2^40 */
            check(-1000000, 1000000); /* -10^12 */
            check(-12345, -1);        /* 12345 */
        }
    )");
    EXPECT_EQ(
        "15\n"
        "0\n"
        "-7\n"
        "-42\n"
        "72\n"
        "1000000000000\n"
        "-1000000000000\n"
        "12345\n",
        result);
}

// Madlen-level shape: unsigned MULTIPLY lowers to the b/umul helper (xta / xts / ,call,
// b/umul / atx) rather than b/mul or an inline A*X, and emits no caller-side stack pop.
TEST_F(CodegenTest, MultiplyUnsignedMadlenShape)
{
    std::string out =
        CompileToMadlen("extern unsigned g; void foo(unsigned a, unsigned b) { g = a * b; }");
    EXPECT_NE(out.find(",xts,"), std::string::npos);
    EXPECT_NE(out.find(",call, b/umul"), std::string::npos);
    // Unsigned multiply must use b/umul, not the signed b/mul helper or an inline A*X.
    EXPECT_EQ(out.find(",call, b/mul\n"), std::string::npos);
    EXPECT_EQ(out.find(",a*x,"), std::string::npos);
    // The helper pops its own operand: no UTM stack adjustment around the call.
    EXPECT_EQ(out.find("15 ,utm, -1"), std::string::npos);
}

// End-to-end: full 48-bit unsigned multiply via b/umul (operand splitting into 24-bit
// halves).  The low 48 bits of the product are printed in octal (%o prints the whole word,
// leading zeros stripped).  The first block keeps the 24-bit cases (multiply by 0/1, the
// 2^24 boundary, and the maximum 24-bit product 0xFFFFFF^2 = 2^48 - 2^25 + 1); the second
// block exercises full 48-bit inputs and the mod-2^48 wrap of the high half.
TEST_F(CodegenTest, MultiplyUnsignedRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void check(unsigned a, unsigned b) { printf("%o\n", a * b); }
        void program() {
            /* 24-bit inputs. */
            check(0, 0);                  /* 0 */
            check(1, 1);                  /* 1 */
            check(3, 5);                  /* 15 */
            check(010000U, 010000U);      /* 0x1000 * 0x1000 = 2^24 */
            check(12345U, 6789U);         /* mid-range product */
            check(077777777U, 077777777U);/* 0xFFFFFF^2 = 2^48 - 2^25 + 1 */
            check(077777777U, 1);         /* 0xFFFFFF */
            check(052746757U, 04432126U); /* 0xABCDEF * 0x123456 */

            /* Full 48-bit inputs (high half present). */
            check(0x1000000U, 0x1000000U);             /* 2^24 * 2^24 = 2^48 -> 0 */
            check(0xFFFFFFFFFFFFU, 0xFFFFFFFFFFFFU);   /* (-1)^2 mod 2^48 -> 1 */
            check(0x800000000000U, 2U);                /* 2^47 * 2 = 2^48 -> 0 */
            check(0xFFFFFF000000U, 2U);                /* high half doubled */
            check(0xFFFFFFFFFFFFU, 2U);                /* -2 mod 2^48 */
            check(0xABCDEF123456U, 0x1011U);           /* high * small */
            check(0x123456789ABCU, 0xCBA987654321U);   /* both halves, full wrap */
        }
    )");
    EXPECT_EQ(
        "0\n"
        "1\n"
        "17\n"
        "100000000\n"
        "477553635\n"
        "7777777600000001\n"
        "77777777\n"
        "303363226335112\n"
        "0\n"
        "1\n"
        "0\n"
        "7777777600000000\n"
        "7777777777777776\n"
        "2171700336554666\n"
        "7712534615623074\n",
        result);
}

// Madlen-level shape: signed DIVIDE lowers to the b/div helper (xta / xts / ,call, b/div
// / atx) rather than an inline A/X, and emits no caller-side stack pop.
TEST_F(CodegenTest, DivideMadlenShape)
{
    std::string out = CompileToMadlen("extern int g; void foo(int a, int b) { g = a / b; }");
    EXPECT_NE(out.find(",xts,"), std::string::npos);
    EXPECT_NE(out.find(",call, b/div"), std::string::npos);
    // Divide must NOT inline the divide unit; the helper bridges INT-format.
    EXPECT_EQ(out.find(",a/x,"), std::string::npos);
    // The helper pops its own operand: no UTM stack adjustment around the call.
    EXPECT_EQ(out.find("15 ,utm, -1"), std::string::npos);
}

// Madlen-level shape: signed REMAINDER lowers to the b/mod helper.
TEST_F(CodegenTest, RemainderMadlenShape)
{
    std::string out = CompileToMadlen("extern int g; void foo(int a, int b) { g = a % b; }");
    EXPECT_NE(out.find(",xts,"), std::string::npos);
    EXPECT_NE(out.find(",call, b/mod"), std::string::npos);
    EXPECT_EQ(out.find(",a/x,"), std::string::npos);
    EXPECT_EQ(out.find("15 ,utm, -1"), std::string::npos);
}

// End-to-end: signed divide via b/div.  Covers sign combinations (truncation toward zero),
// divide by 1, and a zero dividend.
TEST_F(CodegenTest, DivideRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void check(int a, int b) { printf("%d\n", a / b); }
        void program() {
            check(7, 2);            /* 3 */
            check(-7, 2);           /* -3, truncates toward zero */
            check(7, -2);           /* -3 */
            check(-7, -2);          /* 3 */
            check(6, 3);            /* 2 */
            check(0, 5);            /* 0 */
            check(1000000000, 7);   /* 142857142, multi-digit quotient */
            check(-1000000000, 7);  /* -142857142 */
            check(-42, 1);          /* -42 */
        }
    )");
    EXPECT_EQ(
        "3\n"
        "-3\n"
        "-3\n"
        "3\n"
        "2\n"
        "0\n"
        "142857142\n"
        "-142857142\n"
        "-42\n",
        result);
}

// End-to-end: signed remainder via b/mod.  Result takes the sign of the dividend (C11).
TEST_F(CodegenTest, RemainderRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void check(int a, int b) { printf("%d\n", a % b); }
        void program() {
            check(7, 2);            /* 1 */
            check(-7, 2);           /* -1, sign of dividend */
            check(7, -2);           /* 1 */
            check(-7, -2);          /* -1 */
            check(6, 3);            /* 0 */
            check(0, 5);            /* 0 */
            check(1000000007, 1000);/* 7, multi-digit dividend */
            check(-42, 5);          /* -2 */
        }
    )");
    EXPECT_EQ(
        "1\n"
        "-1\n"
        "1\n"
        "-1\n"
        "0\n"
        "0\n"
        "7\n"
        "-2\n",
        result);
}

// Madlen-level shape: unsigned DIVIDE lowers to the b/udiv helper (xta / xts / ,call,
// b/udiv / atx) rather than the signed b/div or an inline A/X, with no caller-side pop.
TEST_F(CodegenTest, DivideUnsignedMadlenShape)
{
    std::string out =
        CompileToMadlen("extern unsigned g; void foo(unsigned a, unsigned b) { g = a / b; }");
    EXPECT_NE(out.find(",xts,"), std::string::npos);
    EXPECT_NE(out.find(",call, b/udiv"), std::string::npos);
    // Unsigned divide must use b/udiv, not the signed b/div helper or an inline A/X.
    EXPECT_EQ(out.find(",call, b/div"), std::string::npos);
    EXPECT_EQ(out.find(",a/x,"), std::string::npos);
    // The helper pops its own operand: no UTM stack adjustment around the call.
    EXPECT_EQ(out.find("15 ,utm, -1"), std::string::npos);
}

// End-to-end: full 48-bit unsigned divide via b/udiv (quotients printed in octal, %o).
// b/udiv routes on operand size; the blocks below exercise each branch:
//   - fast path  (a,b < 2^40)            -> b/div directly
//   - 2-step     (b < 2^32, a >= 2^40)   -> base-2^8 long division via b/div / b/mod
//   - loop, b in [2^32, 2^40)            -> divisor-shift loop
//   - loop, b >= 2^40                    -> divisor-shift loop (short)
TEST_F(CodegenTest, DivideUnsignedRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void check(unsigned a, unsigned b) { printf("%o\n", a / b); }
        void program() {
            /* fast path: both operands < 2^40 */
            check(7, 2);                    /* 3 */
            check(6, 3);                    /* 2 */
            check(17, 5);                   /* 3 */
            check(100, 10);                 /* 12 octal */
            check(0, 5);                    /* 0 */
            check(42, 1);                   /* 52 octal */
            check(1, 1);                    /* 1 */
            check(1000, 8);                 /* 175 octal, power-of-two divisor */
            check(5, 9);                    /* 0, divisor > dividend */
            check(0xFFFFFFFFFFU, 7);        /* (2^40-1)/7, top of b/div's exact range */

            /* 2-step path: b < 2^32 with a >= 2^40 */
            check(0x10000000000U, 7);       /* 2^40 / 7 */
            check(0xFFFFFFFFFFFFU, 1);      /* -1u / 1 */
            check(0xFFFFFFFFFFFFU, 2);      /* high bit set */
            check(0xFFFFFFFFFFFFU, 3);      /* repeating pattern */
            check(0xFFFFFFFFFFFFU, 7);      /* repeating pattern */
            check(0xABCDEF123456U, 0x1011U);    /* wide / small */
            check(0x123456789ABCU, 0x1000000U); /* divide by 2^24 */

            /* divisor-shift loop: b in [2^32, 2^40) */
            check(0xFFFFFFFFFFFFU, 0x100000000U); /* / 2^32 */
            check(0x800000000000U, 0x40000000U);  /* / 2^30 */

            /* divisor-shift loop: b >= 2^40 (quotient < 2^8) */
            check(0x800000000000U, 2);            /* 2^47 / 2 = 2^46 */
            check(0xFFFFFFFFFFFFU, 0x800000000000U); /* 1 */
            check(0xFFFFFFFFFFFFU, 0xFFFFFFFFFFFFU);  /* 1 */
        }
    )");
    EXPECT_EQ(
        "3\n"
        "2\n"
        "3\n"
        "12\n"
        "0\n"
        "52\n"
        "1\n"
        "175\n"
        "0\n"
        "2222222222222\n"
        "2222222222222\n"
        "7777777777777777\n"
        "3777777777777777\n"
        "2525252525252525\n"
        "1111111111111111\n"
        "526140453247\n"
        "4432126\n"
        "177777\n"
        "400000\n"
        "2000000000000000\n"
        "1\n"
        "1\n",
        result);
}

// Madlen-level shape: unsigned REMAINDER lowers to the b/umod helper (xta / xts / ,call,
// b/umod / atx) rather than the signed b/mod or an inline A/X, with no caller-side pop.
TEST_F(CodegenTest, RemainderUnsignedMadlenShape)
{
    std::string out =
        CompileToMadlen("extern unsigned g; void foo(unsigned a, unsigned b) { g = a % b; }");
    EXPECT_NE(out.find(",xts,"), std::string::npos);
    EXPECT_NE(out.find(",call, b/umod"), std::string::npos);
    // Unsigned remainder must use b/umod, not the signed b/mod / b/div or an inline A/X.
    EXPECT_EQ(out.find(",call, b/mod"), std::string::npos);
    EXPECT_EQ(out.find(",call, b/div"), std::string::npos);
    EXPECT_EQ(out.find(",a/x,"), std::string::npos);
    // The helper pops its own operand: no UTM stack adjustment around the call.
    EXPECT_EQ(out.find("15 ,utm, -1"), std::string::npos);
}

// End-to-end: full 48-bit unsigned remainder via b/umod (residue printed in octal, %o).
// b/umod computes a - (a/b)*b, so it inherits b/udiv's size-based routing; the blocks
// below exercise each branch of the underlying divide:
//   - fast path  (a,b < 2^40)            -> b/div directly
//   - 2-step     (b < 2^32, a >= 2^40)   -> base-2^8 long division via b/div / b/mod
//   - loop, b in [2^32, 2^40)            -> divisor-shift loop
//   - loop, b >= 2^40                    -> divisor-shift loop (short)
TEST_F(CodegenTest, RemainderUnsignedRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void check(unsigned a, unsigned b) { printf("%o\n", a % b); }
        void program() {
            /* fast path: both operands < 2^40 */
            check(7, 2);                    /* 1 */
            check(6, 3);                    /* 0, exact */
            check(17, 5);                   /* 2 */
            check(100, 10);                 /* 0, exact */
            check(0, 5);                    /* 0 */
            check(42, 1);                   /* 0, divide by 1 */
            check(5, 9);                    /* 5, divisor > dividend */
            check(1000, 8);                 /* 0, power-of-two divisor */
            check(0xFFFFFFFFFFU, 7);        /* (2^40-1)%7, top of b/div's exact range */

            /* 2-step path: b < 2^32 with a >= 2^40 */
            check(0x10000000000U, 7);       /* 2^40 % 7 */
            check(0xFFFFFFFFFFFFU, 1);      /* 0, -1u % 1 */
            check(0xFFFFFFFFFFFFU, 2);      /* 1, high bit set, odd */
            check(0xFFFFFFFFFFFFU, 3);      /* 0, exact */
            check(0xABCDEF123456U, 0x1011U);    /* wide % small */
            check(0x123456789ABCU, 0x1000000U); /* % 2^24 = low 24 bits */

            /* divisor-shift loop: b in [2^32, 2^40) */
            check(0xFFFFFFFFFFFFU, 0x100000000U); /* % 2^32 = low 32 bits */
            check(0x800000000000U, 0x40000000U);  /* 0, exact */

            /* divisor-shift loop: b >= 2^40 */
            check(0x800000000000U, 2);            /* 0, 2^47 even */
            check(0xFFFFFFFFFFFFU, 0x800000000000U); /* 2^48-1 - 2^47 */
            check(0xFFFFFFFFFFFFU, 0xFFFFFFFFFFFFU);  /* 0, a == b */
        }
    )");
    EXPECT_EQ(
        "1\n"
        "0\n"
        "2\n"
        "0\n"
        "0\n"
        "0\n"
        "5\n"
        "0\n"
        "1\n"
        "2\n"
        "0\n"
        "1\n"
        "0\n"
        "1477\n"
        "36115274\n"
        "37777777777\n"
        "0\n"
        "0\n"
        "3777777777777777\n"
        "0\n",
        result);
}

// ---------------------------------------------------------------------------
// Constant strength reduction (task #33).  Multiply by a constant power of two
// becomes a single ASN left shift; unsigned divide / remainder by a power of two
// become an ASN right shift / AAX mask, replacing the b/mul / b/udiv / b/umod
// helper calls.  Signed divide / remainder by 2^k stay on b/div / b/mod (they
// round toward zero, which is not a plain shift).
// ---------------------------------------------------------------------------

// Madlen shape: signed multiply by 8 (= 2^3) becomes an ASN with field 64-3 = 61 followed
// by an AAX mask to the 41-bit signed range (the left shift can spill into the exponent
// field), with no b/mul helper call and no caller-side stack pop.
TEST_F(CodegenTest, MultiplyPow2MadlenShape)
{
    std::string out = CompileToMadlen("extern int g; void foo(int a) { g = a * 8; }");
    EXPECT_NE(out.find(",asn, 61"), std::string::npos);
    EXPECT_NE(out.find(",aax, =37777777777777"), std::string::npos);
    EXPECT_EQ(out.find(",call, b/mul"), std::string::npos);
    EXPECT_EQ(out.find(",xts,"), std::string::npos);
}

// Multiply is commutative: 8 * a reduces to the same ASN as a * 8.
TEST_F(CodegenTest, MultiplyPow2CommutativeMadlenShape)
{
    std::string out = CompileToMadlen("extern int g; void foo(int a) { g = 8 * a; }");
    EXPECT_NE(out.find(",asn, 61"), std::string::npos);
    EXPECT_EQ(out.find(",call, b/mul"), std::string::npos);
}

// Unsigned multiply by a power of two reduces to a bare ASN: the full 48-bit product is
// modular, so no exponent-field mask is needed (unlike the signed case).
TEST_F(CodegenTest, MultiplyUnsignedPow2MadlenShape)
{
    std::string out = CompileToMadlen("extern unsigned g; void foo(unsigned a) { g = a * 8; }");
    EXPECT_NE(out.find(",asn, 61"), std::string::npos);
    EXPECT_EQ(out.find(",aax,"), std::string::npos);
    EXPECT_EQ(out.find(",call, b/umul"), std::string::npos);
}

// Madlen shape: unsigned divide by 4 (= 2^2) becomes a logical right shift, ASN 64+2 = 66,
// with no b/udiv helper call.
TEST_F(CodegenTest, DivideUnsignedPow2MadlenShape)
{
    std::string out = CompileToMadlen("extern unsigned g; void foo(unsigned a) { g = a / 4; }");
    EXPECT_NE(out.find(",asn, 66"), std::string::npos);
    EXPECT_EQ(out.find(",call, b/udiv"), std::string::npos);
}

// Madlen shape: unsigned remainder by 4 becomes a mask of the low 2 bits, AAX =3 (octal).
TEST_F(CodegenTest, RemainderUnsignedPow2MadlenShape)
{
    std::string out = CompileToMadlen("extern unsigned g; void foo(unsigned a) { g = a % 4; }");
    EXPECT_NE(out.find(",aax, =3"), std::string::npos);
    EXPECT_EQ(out.find(",call, b/umod"), std::string::npos);
}

// Signed divide by a power of two is NOT reduced: it must round toward zero for negative
// dividends, which a shift cannot do, so it stays on the b/div helper.
TEST_F(CodegenTest, DivideSignedPow2StillHelperMadlenShape)
{
    std::string out = CompileToMadlen("extern int g; void foo(int a) { g = a / 4; }");
    EXPECT_NE(out.find(",call, b/div"), std::string::npos);
    EXPECT_EQ(out.find(",asn,"), std::string::npos);
}

// Signed remainder by a power of two likewise stays on b/mod (sign of dividend).
TEST_F(CodegenTest, RemainderSignedPow2StillHelperMadlenShape)
{
    std::string out = CompileToMadlen("extern int g; void foo(int a) { g = a % 4; }");
    EXPECT_NE(out.find(",call, b/mod"), std::string::npos);
    EXPECT_EQ(out.find(",aax,"), std::string::npos);
}

// End-to-end: multiply by powers of two via ASN gives the same products as b/mul, including
// negative dividends (the modular low bits carry the correct two's-complement result).
TEST_F(CodegenTest, MultiplyPow2Run)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void check(int a) { printf("%d\n", a * 8); }
        void program() {
            check(3);        /* 24 */
            check(0);        /* 0 */
            check(-5);       /* -40 */
            check(125000);   /* 1000000 */
            check(-125000);  /* -1000000 */
        }
    )");
    EXPECT_EQ("24\n0\n-40\n1000000\n-1000000\n", result);
}

// End-to-end: unsigned multiply by a power of two via ASN, over the full 48-bit range.
// The product is taken mod 2^48 (the bare logical shift), printed in octal.
TEST_F(CodegenTest, MultiplyUnsignedPow2Run)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void check(unsigned a) { printf("%o\n", a * 8); }
        void program() {
            check(3);                    /* 30 octal (24) */
            check(0);                    /* 0 */
            check(0x100000000000U);      /* 2^44 * 8 = 2^47 = 4000000000000000 octal */
            check(0xFFFFFFFFFFFFU);      /* (2^48-1)*8 mod 2^48 = 2^48-8 -> 7777777777777770 */
        }
    )");
    EXPECT_EQ(
        "30\n"
        "0\n"
        "4000000000000000\n"
        "7777777777777770\n",
        result);
}

// End-to-end: unsigned divide / remainder by powers of two via ASN / AAX, over the full
// 48-bit unsigned range (high bit set, exercising the logical-shift / mask path).
TEST_F(CodegenTest, DivRemUnsignedPow2Run)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void check(unsigned a) { printf("%o %o\n", a / 8, a % 8); }
        void program() {
            check(100);                  /* 100/8=12=014o, 100%8=4 */
            check(7);                    /* 0 7 */
            check(64);                   /* 64/8=8=010o, 0 */
            check(0xFFFFFFFFFFFFU);      /* 2^48-1: /8 = 2^45-1 (15 sevens), %8 = 7 */
        }
    )");
    EXPECT_EQ(
        "14 4\n"
        "0 7\n"
        "10 0\n"
        "777777777777777 7\n",
        result);
}

// ---------------------------------------------------------------------------
// Floating-point arithmetic (task #15).  Add/Sub/Mul/Div map to the hardware
// A+X/A-X/A*X/A/X bracketed by NTR 0 (enable normalize+round) … NTR 7 (restore
// the integer suppress mode left by b/save).  float == double (single word).
// ---------------------------------------------------------------------------

// Madlen shape: double ADD brackets a+x with ntr 0 / ntr 7.
TEST_F(CodegenTest, AddDoubleMadlen)
{
    std::string output =
        CompileToMadlen("extern double g; void foo(double a, double b) { g = a + b; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
             ,ntr, 0
           6 ,a+x, 1
             ,ntr, 7
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// Madlen shape: double SUBTRACT brackets a-x with ntr 0 / ntr 7.
TEST_F(CodegenTest, SubDoubleMadlen)
{
    std::string output =
        CompileToMadlen("extern double g; void foo(double a, double b) { g = a - b; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
             ,ntr, 0
           6 ,a-x, 1
             ,ntr, 7
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// Madlen shape: double MULTIPLY uses the inline a*x (no b/mul helper) with ntr brackets.
TEST_F(CodegenTest, MulDoubleMadlen)
{
    std::string output =
        CompileToMadlen("extern double g; void foo(double a, double b) { g = a * b; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
             ,ntr, 0
           6 ,a*x, 1
             ,ntr, 7
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// Madlen shape: double DIVIDE uses the inline a/x (no b/div helper) with ntr brackets.
// A constant divisor materializes as an =r FP literal operand.
TEST_F(CodegenTest, DivDoubleMadlen)
{
    std::string output =
        CompileToMadlen("extern double g; void foo(double a, double b) { g = a / b; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
             ,ntr, 0
           6 ,a/x, 1
             ,ntr, 7
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// End-to-end: double add/sub/mul/div on the hardware FP unit.  Results are printed as
// raw octal bit patterns (%o) — the runtime has no %f formatting — so the expected
// values are the BESM-6 native FP encodings of the mathematical results.
TEST_F(CodegenTest, FpArithRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void show(double x) { printf("%o\n", x); }
        void program() {
            double a = 1.5, b = 2.5;
            show(a + b);    /* 4.0 */
            show(b - a);    /* 1.0 */
            show(a * b);    /* 3.75 */
            show(b / a);    /* 1.666... */
            show(a - b);    /* -1.0 */
            show(6.0 / 2.0);/* 3.0 (constant divisor) */
            show(0.0 + a);  /* 1.5 */
        }
    )");
    EXPECT_EQ(
        "4150000000000000\n" /* 4.0   */
        "4050000000000000\n" /* 1.0   */
        "4117000000000000\n" /* 3.75  */
        "4055252525252521\n" /* 5/3   */
        "4020000000000000\n" /* -1.0  */
        "4114000000000000\n" /* 3.0   */
        "4054000000000000\n" /* 1.5   */,
        result);
}

// End-to-end: FP compound assignment (+=, -=, *=, /=) and float (== double, single
// word) go through the same _DOUBLE ops as plain binary FP arithmetic.
TEST_F(CodegenTest, FpCompoundAssignRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void show(double x) { printf("%o\n", x); }
        void program() {
            double d = 10.0;
            d += 2.0;  show(d);   /* 12.0 */
            d -= 4.0;  show(d);   /*  8.0 */
            d *= 0.5;  show(d);   /*  4.0 */
            d /= 2.0;  show(d);   /*  2.0 */
            float f = 1.5f;
            f = f + 0.5f;         /*  2.0 */
            show(f);
        }
    )");
    EXPECT_EQ(
        "4214000000000000\n" /* 12.0 */
        "4210000000000000\n" /*  8.0 */
        "4150000000000000\n" /*  4.0 */
        "4110000000000000\n" /*  2.0 */
        "4110000000000000\n" /*  2.0 */,
        result);
}

// FP comparisons (task #16).  The four orderings <, <=, >, >= lower to the FP relational
// helpers b/flt, b/fle, b/fgt, b/fge (the integer b/lt..b/ge subtract the operands as raw
// words, which is wrong for FP).  == and != stay on the type-independent b/eq / b/ne.

// Madlen shape: double `a < b` calls b/flt (operand on the stack top, the other in A).
TEST_F(CodegenTest, FpCompareMadlen)
{
    std::string output =
        CompileToMadlen("extern int g; void foo(double a, double b) { g = a < b; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
           6 ,xts, 1
             ,call, b/flt
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// End-to-end: the six FP comparisons on doubles, printed as raw 0/1.  Operands are
// volatile so the optimizer cannot fold them; b/flt..b/fge run on the hardware FP unit.

// a > b (2.5 vs 1.5): <, <=, >, >=, ==, != → 0 0 1 1 0 1.
TEST_F(CodegenTest, FpCompareGreaterRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void program() {
            volatile double a = 2.5, b = 1.5;
            printf("%d%d%d%d%d%d\n", a < b, a <= b, a > b, a >= b, a == b, a != b);
        }
    )");
    EXPECT_EQ("001101\n", result);
}

// a < b (1.5 vs 2.5): <, <=, >, >=, ==, != → 1 1 0 0 0 1.
TEST_F(CodegenTest, FpCompareLessRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void program() {
            volatile double a = 1.5, b = 2.5;
            printf("%d%d%d%d%d%d\n", a < b, a <= b, a > b, a >= b, a == b, a != b);
        }
    )");
    EXPECT_EQ("110001\n", result);
}

// a == b (2.0 vs 2.0): the equality edge for <= and >= — a-b normalizes to exact zero.
// <, <=, >, >=, ==, != → 0 1 0 1 1 0.
TEST_F(CodegenTest, FpCompareEqualRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void program() {
            volatile double a = 2.0, b = 2.0;
            printf("%d%d%d%d%d%d\n", a < b, a <= b, a > b, a >= b, a == b, a != b);
        }
    )");
    EXPECT_EQ("010110\n", result);
}

// float ≡ double (single word), and negative operands exercise the FP sign path.
// a = -1.5, b = 0.5 → a < b: <, <=, >, >=, ==, != → 1 1 0 0 0 1.
TEST_F(CodegenTest, FpCompareFloatRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void program() {
            volatile float a = -1.5f, b = 0.5f;
            printf("%d%d%d%d%d%d\n", a < b, a <= b, a > b, a >= b, a == b, a != b);
        }
    )");
    EXPECT_EQ("110001\n", result);
}

// Mode-restoration regression: the FP ordering helpers enter full FP mode (NTR 0) for the
// subtract and must restore the integer mode (NTR 7) before returning.  If they did not,
// the integer add that follows would run in normalizing FP mode and corrupt its result.
// Here `a < b` is true (1) and the subsequent `x + y` must still compute 10 + 3 = 13.
TEST_F(CodegenTest, FpCompareRestoresIntModeRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void program() {
            volatile double a = 1.5, b = 2.5;
            volatile int x = 10, y = 3;
            int c = a < b;
            int d = x + y;
            printf("%d %d\n", c, d);
        }
    )");
    EXPECT_EQ("1 13\n", result);
}
