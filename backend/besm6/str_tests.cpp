// Run-tests for the str* functions in libc (strlen, strcpy, strncpy, strcat,
// strncat, strcmp, strncmp, strchr, strrchr, strstr, strtok, strerror).
//
// Output is KOI7 with case folding, so printed letters fold to upper case;
// expected strings use UPPERCASE ASCII.  Where a letter is awkward, the test
// compares numeric results with %d instead.  Each test compiles a tiny
// program(), runs it under Dubna, and checks the captured stdout.
#include "codegen_test.h"

// ---- strlen --------------------------------------------------------------

TEST_F(CodegenTest, StrlenBasic)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            printf("%d\n", (int)strlen("HELLO"));
        }
    )");
    EXPECT_EQ("5\n", result);
}

TEST_F(CodegenTest, StrlenEmpty)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            printf("%d\n", (int)strlen(""));
        }
    )");
    EXPECT_EQ("0\n", result);
}

TEST_F(CodegenTest, StrlenCrossesWord)
{
    // 8 characters span the 6-byte word boundary.
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            printf("%d\n", (int)strlen("ABCDEFGH"));
        }
    )");
    EXPECT_EQ("8\n", result);
}

// ---- strcpy / strncpy ----------------------------------------------------

TEST_F(CodegenTest, StrcpyBasic)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char dst[10];
            strcpy(dst, "ABCDEFG");
            printf("%s\n", dst);
        }
    )");
    EXPECT_EQ("ABCDEFG\n", result);
}

TEST_F(CodegenTest, StrcpyReturnsDest)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char dst[4];
            char *r = strcpy(dst, "XY");
            printf("%d\n", r == dst);
        }
    )");
    EXPECT_EQ("1\n", result);
}

TEST_F(CodegenTest, StrncpyTruncates)
{
    // n shorter than the source: no terminator is written, so place one.
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char dst[8];
            strncpy(dst, "ABCDEF", 3);
            dst[3] = 0;
            printf("%s\n", dst);
        }
    )");
    EXPECT_EQ("ABC\n", result);
}

TEST_F(CodegenTest, StrncpyPads)
{
    // n longer than the source: the tail is zero-filled.
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char dst[8];
            int i;
            for (i = 0; i < 8; i++) dst[i] = 'Z';
            strncpy(dst, "AB", 6);
            dst[6] = 0;
            /* "AB" then four NULs; printed length stops at first NUL. */
            printf("%d\n", (int)strlen(dst));
        }
    )");
    EXPECT_EQ("2\n", result);
}

// ---- strcat / strncat ----------------------------------------------------

TEST_F(CodegenTest, StrcatBasic)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char dst[10];
            strcpy(dst, "ABC");
            strcat(dst, "DEF");
            printf("%s\n", dst);
        }
    )");
    EXPECT_EQ("ABCDEF\n", result);
}

TEST_F(CodegenTest, StrncatBounded)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char dst[10];
            strcpy(dst, "ABC");
            strncat(dst, "DEFGH", 2);
            printf("%s\n", dst);
        }
    )");
    EXPECT_EQ("ABCDE\n", result);
}

// ---- strcmp / strncmp ----------------------------------------------------

TEST_F(CodegenTest, StrcmpEqual)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            printf("%d\n", strcmp("ABCD", "ABCD"));
        }
    )");
    EXPECT_EQ("0\n", result);
}

TEST_F(CodegenTest, StrcmpLess)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            // 'A'(65) - 'B'(66) = -1
            printf("%d\n", strcmp("ABA", "ABB"));
        }
    )");
    EXPECT_EQ("-1\n", result);
}

TEST_F(CodegenTest, StrcmpGreaterByLength)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            // "ABCD" > "ABC": first extra byte 'D'(68) - '\0'(0) = 68
            printf("%d\n", strcmp("ABCD", "ABC"));
        }
    )");
    EXPECT_EQ("68\n", result);
}

TEST_F(CodegenTest, StrncmpBoundedEqual)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            // Differ at index 3, only 3 compared -> equal.
            printf("%d\n", strncmp("ABCX", "ABCY", 3));
        }
    )");
    EXPECT_EQ("0\n", result);
}

// ---- strchr / strrchr ----------------------------------------------------

TEST_F(CodegenTest, StrchrFound)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            const char *s = "ABCDEF";
            char *p = strchr(s, 'C');
            printf("%d\n", (int)(p - s));
        }
    )");
    EXPECT_EQ("2\n", result);
}

TEST_F(CodegenTest, StrchrNotFound)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char *p = strchr("ABCDEF", 'Z');
            printf("%d\n", p == 0);
        }
    )");
    EXPECT_EQ("1\n", result);
}

TEST_F(CodegenTest, StrchrFindsNul)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            const char *s = "ABC";
            char *p = strchr(s, 0);
            printf("%d\n", (int)(p - s));
        }
    )");
    EXPECT_EQ("3\n", result);
}

TEST_F(CodegenTest, StrrchrLast)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            const char *s = "ABCABC";
            char *p = strrchr(s, 'B');
            printf("%d\n", (int)(p - s));
        }
    )");
    EXPECT_EQ("4\n", result);
}

// ---- strstr --------------------------------------------------------------

TEST_F(CodegenTest, StrstrFound)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            const char *s = "ABCDEF";
            char *p = strstr(s, "CDE");
            printf("%d\n", (int)(p - s));
        }
    )");
    EXPECT_EQ("2\n", result);
}

TEST_F(CodegenTest, StrstrNotFound)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char *p = strstr("ABCDEF", "XYZ");
            printf("%d\n", p == 0);
        }
    )");
    EXPECT_EQ("1\n", result);
}

TEST_F(CodegenTest, StrstrEmptyNeedle)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            const char *s = "ABCDEF";
            char *p = strstr(s, "");
            printf("%d\n", p == s);
        }
    )");
    EXPECT_EQ("1\n", result);
}

// ---- strtok --------------------------------------------------------------

TEST_F(CodegenTest, StrtokMultiToken)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char buf[16];
            strcpy(buf, "AB,CD,EF");
            char *t = strtok(buf, ",");
            while (t != 0) {
                printf("%s\n", t);
                t = strtok(0, ",");
            }
        }
    )");
    EXPECT_EQ("AB\nCD\nEF\n", result);
}

TEST_F(CodegenTest, StrtokLeadingDelims)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char buf[16];
            strcpy(buf, ",,AB,,CD,,");
            char *t = strtok(buf, ",");
            while (t != 0) {
                printf("%s\n", t);
                t = strtok(0, ",");
            }
        }
    )");
    EXPECT_EQ("AB\nCD\n", result);
}

// ---- strerror ------------------------------------------------------------

TEST_F(CodegenTest, StrerrorKnown)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            printf("%s\n", strerror(5));
        }
    )");
    EXPECT_EQ("OUT OF MEMORY\n", result);
}

TEST_F(CodegenTest, StrerrorUnknown)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            printf("%s\n", strerror(99));
        }
    )");
    EXPECT_EQ("UNKNOWN ERROR\n", result);
}
