// Run-tests for the full-featured printf / sprintf / snprintf in libc.
//
// All output is KOI7 with case folding, so every letter prints UPPER CASE and
// %x/%X, %e/%E, %g/%G are indistinguishable.  Expected strings are upper case.
// Each test compiles a tiny program(), runs it under Dubna, and compares the
// captured stdout.  Floating-point expectations reflect the BESM-6 48-bit native
// format (~12 significant decimal digits).
#include "codegen_test.h"

// ---- integer conversions -------------------------------------------------

TEST_F(CodegenTest, PrintfDecimal)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("%d %d %d\n", 0, 42, -2345);
        }
    )");
    EXPECT_EQ("0 42 -2345\n", result);
}

TEST_F(CodegenTest, PrintfUnsignedOctalHex)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("%u %o %x %X\n", 100, 64, 255, 255);
        }
    )");
    EXPECT_EQ("100 100 FF FF\n", result);
}

TEST_F(CodegenTest, PrintfSharpFlag)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("%#x %#o\n", 255, 64);
        }
    )");
    EXPECT_EQ("0XFF 0100\n", result);
}

TEST_F(CodegenTest, PrintfSignFlags)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("%+d % d %+d\n", 42, 42, -42);
        }
    )");
    EXPECT_EQ("+42  42 -42\n", result);
}

TEST_F(CodegenTest, PrintfIntPrecision)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("%.5d %05.3d\n", 42, 42);
        }
    )");
    EXPECT_EQ("00042   042\n", result);
}

// A negative int read through an unsigned conversion shows the 41-bit pattern.
TEST_F(CodegenTest, PrintfNegativeHex)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("%x\n", -1);
        }
    )");
    EXPECT_EQ("1FFFFFFFFFF\n", result);
}

// ---- width / alignment ---------------------------------------------------

TEST_F(CodegenTest, PrintfWidth)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("[%5d][%-5d][%05d]\n", 42, 42, 42);
        }
    )");
    EXPECT_EQ("[   42][42   ][00042]\n", result);
}

TEST_F(CodegenTest, PrintfStarWidthPrecision)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("[%*d][%.*f]\n", 6, 42, 2, 3.14159);
        }
    )");
    EXPECT_EQ("[    42][3.14]\n", result);
}

// ---- characters and strings ----------------------------------------------

TEST_F(CodegenTest, PrintfChar)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("%c%c%c\n", 'A', 'B', 'C');
        }
    )");
    EXPECT_EQ("ABC\n", result);
}

TEST_F(CodegenTest, PrintfPercent)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("100%%\n");
        }
    )");
    EXPECT_EQ("100%\n", result);
}

TEST_F(CodegenTest, PrintfString)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("[%s]\n", "hello");
        }
    )");
    EXPECT_EQ("[HELLO]\n", result);
}

TEST_F(CodegenTest, PrintfStringWidthPrecision)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("[%10s][%-10s][%.3s]\n", "hi", "hi", "hello");
        }
    )");
    EXPECT_EQ("[        HI][HI        ][HEL]\n", result);
}

TEST_F(CodegenTest, PrintfNullString)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("[%s]\n", (char *)0);
        }
    )");
    EXPECT_EQ("[(NULL)]\n", result);
}

// ---- floating point ------------------------------------------------------

TEST_F(CodegenTest, PrintfFloatF)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("%f %f %f\n", 1.5, 0.0, -2.5);
        }
    )");
    EXPECT_EQ("1.500000 0.000000 -2.500000\n", result);
}

TEST_F(CodegenTest, PrintfFloatPrecisionWidth)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("[%.2f][%10.3f][%-8.3f]\n", 3.14159, 3.14159, 3.14159);
        }
    )");
    EXPECT_EQ("[3.14][     3.142][3.142   ]\n", result);
}

TEST_F(CodegenTest, PrintfFloatExp)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("%e\n", 12.34);
        }
    )");
    EXPECT_EQ("1.234000E+01\n", result);
}

TEST_F(CodegenTest, PrintfFloatG)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("%g %g\n", 12.34, 100000.0);
        }
    )");
    EXPECT_EQ("12.34 100000\n", result);
}

// ---- sprintf / snprintf --------------------------------------------------

TEST_F(CodegenTest, Snprintf)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        int snprintf(char *buf, int size, const char *format, ...);
        void program() {
            char buf[32];
            int n = snprintf(buf, 32, "n=%d s=%s", 7, "abc");
            printf("[%s](%d)\n", buf, n);
        }
    )");
    EXPECT_EQ("[N=7 S=ABC](9)\n", result);
}

TEST_F(CodegenTest, Sprintf)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        int sprintf(char *buf, const char *format, ...);
        void program() {
            char buf[40];
            int n = sprintf(buf, "%d-%x-%c", 5, 250, 'Z');
            printf("[%s](%d)\n", buf, n);
        }
    )");
    EXPECT_EQ("[5-FA-Z](6)\n", result);
}

// snprintf truncates to size-1 chars plus NUL, but returns the full length.
TEST_F(CodegenTest, SnprintfTruncation)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        int snprintf(char *buf, int size, const char *format, ...);
        void program() {
            char buf[5];
            int n = snprintf(buf, 5, "%s", "abcdefg");
            printf("[%s](%d)\n", buf, n);
        }
    )");
    EXPECT_EQ("[ABCD](7)\n", result);
}
