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
#include <stdio.h>
        void program() {
            printf("%d %d %d\n", 0, 42, -2345);
        }
    )");
    EXPECT_EQ("0 42 -2345\n", result);
}

TEST_F(CodegenTest, PrintfUnsignedOctalHex)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        void program() {
            printf("%u %o %x %X\n", 100, 64, 255, 255);
        }
    )");
    EXPECT_EQ("100 100 FF FF\n", result);
}

TEST_F(CodegenTest, PrintfSharpFlag)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        void program() {
            printf("%#x %#o\n", 255, 64);
        }
    )");
    EXPECT_EQ("0XFF 0100\n", result);
}

TEST_F(CodegenTest, PrintfSignFlags)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        void program() {
            printf("%+d % d %+d\n", 42, 42, -42);
        }
    )");
    EXPECT_EQ("+42  42 -42\n", result);
}

TEST_F(CodegenTest, PrintfIntPrecision)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
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
#include <stdio.h>
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
#include <stdio.h>
        void program() {
            printf("[%5d][%-5d][%05d]\n", 42, 42, 42);
        }
    )");
    EXPECT_EQ("[   42][42   ][00042]\n", result);
}

TEST_F(CodegenTest, PrintfStarWidthPrecision)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
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
#include <stdio.h>
        void program() {
            printf("%c%c%c\n", 'A', 'B', 'C');
        }
    )");
    EXPECT_EQ("ABC\n", result);
}

TEST_F(CodegenTest, PrintfPercent)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        void program() {
            printf("100%%\n");
        }
    )");
    EXPECT_EQ("100%\n", result);
}

TEST_F(CodegenTest, PrintfString)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        void program() {
            printf("[%s]\n", "hello");
        }
    )");
    EXPECT_EQ("[HELLO]\n", result);
}

TEST_F(CodegenTest, PrintfStringWidthPrecision)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        void program() {
            printf("[%10s][%-10s][%.3s]\n", "hi", "hi", "hello");
        }
    )");
    EXPECT_EQ("[        HI][HI        ][HEL]\n", result);
}

TEST_F(CodegenTest, PrintfNullString)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
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
#include <stdio.h>
        void program() {
            printf("%f %f %f\n", 1.5, 0.0, -2.5);
        }
    )");
    EXPECT_EQ("1.500000 0.000000 -2.500000\n", result);
}

TEST_F(CodegenTest, PrintfFloatPrecisionWidth)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        void program() {
            printf("[%.2f][%10.3f][%-8.3f]\n", 3.14159, 3.14159, 3.14159);
        }
    )");
    EXPECT_EQ("[3.14][     3.142][3.142   ]\n", result);
}

TEST_F(CodegenTest, PrintfFloatExp)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        void program() {
            printf("%e\n", 12.34);
        }
    )");
    EXPECT_EQ("1.234000E+01\n", result);
}

TEST_F(CodegenTest, PrintfFloatG)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        void program() {
            printf("%g %g\n", 12.34, 100000.0);
        }
    )");
    EXPECT_EQ("12.34 100000\n", result);
}

// Rounding carry in cvt/cvtround: 0.5 rounds the lone '0' up to "1"; 9.5 carries out of
// the leading digit, exercising the reserved-slot prepend (cvtround moves *startp back).
// Guards the char* pointer rewrite of the float formatter.  Both values are exact in
// binary, so there is no representation noise.
TEST_F(CodegenTest, PrintfFloatRoundCarry)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        void program() {
            printf("%.0f %.0f\n", 0.5, 9.5);
        }
    )");
    EXPECT_EQ("1 10\n", result);
}

// ---- sprintf / snprintf --------------------------------------------------

TEST_F(CodegenTest, Snprintf)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
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
#include <stdio.h>
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
#include <stdio.h>
        void program() {
            char buf[5];
            int n = snprintf(buf, 5, "%s", "abcdefg");
            printf("[%s](%d)\n", buf, n);
        }
    )");
    EXPECT_EQ("[ABCD](7)\n", result);
}

// ==========================================================================
// Known BESM-6 backend bugs, surfaced while implementing printf.
//
// These are DISABLED reproducers (run with --gtest_also_run_disabled_tests).
// printf works around each one; the tests document the underlying defect and
// should be enabled once the backend is fixed.  Each EXPECT asserts the
// CORRECT behaviour, so a disabled test currently fails and will pass after a
// fix.
//
// Note: the "null char* is a truthy fat pointer" symptom reported during
// development could NOT be reproduced in isolation — a null char* tests false
// correctly in every standalone form tried.  The printf null-string garbage it
// was blamed on is in fact the string-constant collision below
// (DISABLED_StringConstantNameNotGloballyUnique): the library "(NULL)" literal
// resolved to the caller's _str0.  So there is no separate test for it.
// ==========================================================================

// Bug: a function parameter that is both modified (--n) and tested in a loop
// condition (while (n > 0)) is read from an uninitialized auto slot rather than
// its parameter slot, so the loop misbehaves (emit_pad infinite-looped on this
// pattern; countdown drops/garbles iterations).
// Correct output: "321\n".  Buggy output: "21\n".
TEST_F(CodegenTest, MutatedParameterInLoop)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        void countdown(int n) {
            while (n > 0) {
                putbyte('0' + n);
                --n;
            }
        }
        void program() { countdown(3); putbyte('\n'); }
    )");
    EXPECT_EQ("321\n", result);
}

// Bug: char* relational comparison and decrement are unreliable (the fat-pointer
// encoding is compared as a raw word).  This loop walks a char* backwards over a
// buffer; with the bug the loop body never executes.
// Correct output: "ABC\n".  Buggy output: "\n".
TEST_F(CodegenTest, CharPtrRelationalCompare)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        void program() {
            char b[4];
            b[0] = 'C'; b[1] = 'B'; b[2] = 'A'; b[3] = 0;
            char *p = b + 2;
            while (p >= b) {
                putbyte(*p);
                --p;
            }
            putbyte('\n');
        }
    )");
    EXPECT_EQ("ABC\n", result);
}

// String-constant labels are numbered per compilation unit (the _strN counter
// restarts at 0 for every module).  When each "_str0" was emitted as its own global
// ,name, module it collided with another unit's "_str0" at link time — the library
// "(NULL)" literal resolved to the caller's first string constant.  The fix folds
// every string constant into the single module that references it, as a module-local
// label: the data is still emitted, but there is no external SUBP or standalone
// ,name, module, so the name can no longer collide across separately assembled units.
TEST_F(CodegenTest, StringConstantNameNotGloballyUnique)
{
    std::string madlen = CompileToMadlen(R"( char *f(void) { return "ABC"; } )");
    // The string is folded into f's module: its packed data word is present...
    EXPECT_NE(std::string::npos, madlen.find("2024110300000000"));
    // ...but "*str0" is local, not a global symbol: no external SUBP, no ,name, module.
    EXPECT_EQ(std::string::npos, madlen.find("*str0:   ,subp,"));
    EXPECT_EQ(std::string::npos, madlen.find("*str0:   ,name,"));
}

// Bug: an enum constant used as an array dimension is left as a LITERAL_ENUM
// (only enum constants in expressions are resolved to LITERAL_INT, by
// typecheck_literal).  get_size() then reads the enum-name pointer as the array
// length, yielding a garbage, non-deterministic size.  As a local array this
// corrupts the stack frame; here sizeof exposes it directly.  doprnt.c works
// around it by sizing nbuf with a literal instead of MAXNBUF.
// Correct output: "4\n".  Buggy output: a garbage number (the name pointer).
TEST_F(CodegenTest, EnumArrayDimension)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        enum { N = 4 };
        void program() { printf("%d\n", (int)sizeof(char[N])); }
    )");
    EXPECT_EQ("4\n", result);
}
