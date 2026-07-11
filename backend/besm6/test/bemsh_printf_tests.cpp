//
// End-to-end run tests for the full C libc on the Bemsh (Cyrillic autocode) dialect (task B5).
//
// Task B3 compiled only the char-level stdout chain (putbyte/flush/putchar/putch) into
// libbem.bin.  Task B5 compiles the rest of LIBC_C_PORTABLE — printf/doprnt, sprintf/snprintf,
// the string/mem/math routines and atoi — with `genbesm --bemsh`, so a Bemsh program has the
// same hosted-libc surface as the Madlen path.  These tests exercise that surface end-to-end:
// each compiles a program() through the Bemsh path, links libbem.bin under a `*bemsh` Dubna
// job, runs it on `dubna`, and asserts the captured stdout (the counterpart of the Madlen
// printf_tests.cpp / str_tests.cpp / math_tests.cpp).  A representative subset, not a mirror
// of the whole Madlen suite — enough to prove the library links and the major format/string
// paths run.
//
// As on the other Bemsh run tests: the entry point is `void program()`, and printed lowercase
// Latin renders as Cyrillic on the listing, so output uses UPPERCASE ASCII (see
// bemsh_run_tests.cpp).  KOI7 case folding also makes %x/%X indistinguishable (both upper).
//
#include "codegen_test.h"

// ---- printf: integer conversions -----------------------------------------

TEST_F(CodegenTest, BemshPrintfDecimal)
{
    std::string result = CompileAndRunBemsh(R"(
#include <stdio.h>
        void program() {
            printf("%d %d %d\n", 0, 42, -2345);
        }
    )");
    EXPECT_EQ("0 42 -2345\n", result);
}

TEST_F(CodegenTest, BemshPrintfUnsignedOctalHex)
{
    std::string result = CompileAndRunBemsh(R"(
#include <stdio.h>
        void program() {
            printf("%u %o %x %X\n", 100, 64, 255, 255);
        }
    )");
    EXPECT_EQ("100 100 FF FF\n", result);
}

TEST_F(CodegenTest, BemshPrintfFlagsWidthPrecision)
{
    std::string result = CompileAndRunBemsh(R"(
#include <stdio.h>
        void program() {
            printf("%+d % d %05d %.5d\n", 42, 42, 42, 42);
        }
    )");
    EXPECT_EQ("+42  42 00042 00042\n", result);
}

// ---- printf: char, string, percent ---------------------------------------

TEST_F(CodegenTest, BemshPrintfCharStringPercent)
{
    std::string result = CompileAndRunBemsh(R"(
#include <stdio.h>
        void program() {
            printf("%c%c %s %d%%\n", 'H', 'I', "ABC", 100);
        }
    )");
    EXPECT_EQ("HI ABC 100%\n", result);
}

TEST_F(CodegenTest, BemshPrintfStringWidthPrecision)
{
    std::string result = CompileAndRunBemsh(R"(
#include <stdio.h>
        void program() {
            printf("[%5s][%-5s][%.2s]\n", "AB", "AB", "ABCDE");
        }
    )");
    EXPECT_EQ("[   AB][AB   ][AB]\n", result);
}

// ---- printf: floating point ----------------------------------------------

TEST_F(CodegenTest, BemshPrintfFloat)
{
    std::string result = CompileAndRunBemsh(R"(
#include <stdio.h>
        void program() {
            printf("%f %.2f %g\n", 3.14, 2.5, 100.0);
        }
    )");
    EXPECT_EQ("3.140000 2.50 100\n", result);
}

// ---- sprintf / snprintf ---------------------------------------------------

TEST_F(CodegenTest, BemshSprintf)
{
    std::string result = CompileAndRunBemsh(R"(
#include <stdio.h>
        void program() {
            char buf[16];
            sprintf(buf, "X=%d Y=%X", 42, 255);
            puts(buf);
        }
    )");
    EXPECT_EQ("X=42 Y=FF\n", result);
}

TEST_F(CodegenTest, BemshSnprintfTruncation)
{
    std::string result = CompileAndRunBemsh(R"(
#include <stdio.h>
        void program() {
            char buf[8];
            int n = snprintf(buf, 4, "%d", 12345);
            printf("%s %d\n", buf, n);
        }
    )");
    // snprintf writes at most 3 chars + NUL but returns the full would-be length.
    EXPECT_EQ("123 5\n", result);
}

// ---- string / mem routines ------------------------------------------------

TEST_F(CodegenTest, BemshStringRoutines)
{
    std::string result = CompileAndRunBemsh(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char buf[16];
            strcpy(buf, "ABC");
            strcat(buf, "DE");
            printf("%s %d %d %d\n", buf, (int)strlen(buf),
                   strcmp("AB", "AB"), strcmp("AB", "AC"));
        }
    )");
    EXPECT_EQ("ABCDE 5 0 -1\n", result);
}

TEST_F(CodegenTest, BemshMemcpy)
{
    std::string result = CompileAndRunBemsh(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char a[8] = "ABCDE";
            char b[8];
            memcpy(b, a, 6);
            puts(b);
        }
    )");
    EXPECT_EQ("ABCDE\n", result);
}

// ---- atoi -----------------------------------------------------------------

TEST_F(CodegenTest, BemshAtoi)
{
    std::string result = CompileAndRunBemsh(R"(
#include <stdio.h>
#include <stdlib.h>
        void program() {
            printf("%d %d\n", atoi("1234"), atoi("-56"));
        }
    )");
    EXPECT_EQ("1234 -56\n", result);
}
