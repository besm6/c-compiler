// Run-tests for the <math.h> exponent pair frexp() / ldexp() in libc.
//
// frexp(value, &e) splits value == fraction * 2^e with 0.5 <= |fraction| < 1
// (or 0 when value == 0); ldexp(x, n) is the inverse, x * 2^n.  Both are pure
// exponent-field surgery on the BESM-6 48-bit float word (no rounding), so the
// %f round trips are exact.  Output is KOI7 case-folded, so literal text is
// UPPERCASE; %f prints 6 fraction digits.  Each test compiles a tiny program(),
// runs it under Dubna, and compares the captured stdout.
#include "codegen_test.h"

// ---- frexp: fraction in [0.5, 1) and the binary exponent ------------------

TEST_F(CodegenTest, FrexpBasic)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <math.h>
        void program() {
            int e;
            double m = frexp(12.0, &e);   /* 12 = 0.75 * 2^4 */
            printf("M=%f E=%d\n", m, e);
        }
    )");
    EXPECT_EQ("M=0.750000 E=4\n", result);
}

TEST_F(CodegenTest, FrexpPowersOfTwo)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <math.h>
        void program() {
            int e1, e2, e3;
            double m1 = frexp(8.0, &e1);    /* 0.5 * 2^4  */
            double m2 = frexp(1.0, &e2);    /* 0.5 * 2^1  */
            double m3 = frexp(0.25, &e3);   /* 0.5 * 2^-1 */
            printf("%f %d %f %d %f %d\n", m1, e1, m2, e2, m3, e3);
        }
    )");
    EXPECT_EQ("0.500000 4 0.500000 1 0.500000 -1\n", result);
}

TEST_F(CodegenTest, FrexpNegativePreservesSign)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <math.h>
        void program() {
            int e;
            double m = frexp(-12.0, &e);   /* fraction in (-1, -0.5] */
            printf("%f %d\n", m, e);
        }
    )");
    EXPECT_EQ("-0.750000 4\n", result);
}

TEST_F(CodegenTest, FrexpZero)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <math.h>
        void program() {
            int e;
            double m = frexp(0.0, &e);   /* returns 0.0, *e = 0 (branchless path) */
            printf("%f %d\n", m, e);
        }
    )");
    EXPECT_EQ("0.000000 0\n", result);
}

// ---- ldexp: scale by a power of two ---------------------------------------

TEST_F(CodegenTest, LdexpBasic)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <math.h>
        void program() {
            printf("%f %f %f\n", ldexp(1.0, 3), ldexp(0.75, 4), ldexp(1.0, -2));
        }
    )");
    EXPECT_EQ("8.000000 12.000000 0.250000\n", result);
}

TEST_F(CodegenTest, LdexpZero)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <math.h>
        void program() {
            printf("%f\n", ldexp(0.0, 5));   /* scaling zero stays zero */
        }
    )");
    EXPECT_EQ("0.000000\n", result);
}

// ---- frexp / ldexp are inverses -------------------------------------------

TEST_F(CodegenTest, FrexpLdexpRoundTrip)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <math.h>
        void program() {
            int e;
            double m = frexp(12.0, &e);
            printf("M=%f E=%d\n", m, e);
            printf("BACK=%f\n", ldexp(m, e));
            double n = frexp(100.0, &e);       /* not a power of two */
            printf("BACK=%f\n", ldexp(n, e));
        }
    )");
    EXPECT_EQ("M=0.750000 E=4\nBACK=12.000000\nBACK=100.000000\n", result);
}

// Bit-exact round trip across several magnitudes and signs.  Returns 0 only if
// every ldexp(frexp(x, &e), e) == x exactly, so a %f-rounded display cannot mask
// a wrong exponent bit.
TEST_F(CodegenTest, FrexpLdexpExactRoundTrip)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <math.h>
        void program() {
            double xs[6];
            xs[0] = 12.0;
            xs[1] = -12.0;
            xs[2] = 0.25;
            xs[3] = 100.0;
            xs[4] = 1.0;
            xs[5] = 0.0;
            int i, e;
            for (i = 0; i < 6; i = i + 1) {
                double x = xs[i];
                double y = ldexp(frexp(x, &e), e);
                if (y != x) {
                    printf("FAIL %d\n", i);
                    return;
                }
            }
            printf("OK\n");
        }
    )");
    EXPECT_EQ("OK\n", result);
}
