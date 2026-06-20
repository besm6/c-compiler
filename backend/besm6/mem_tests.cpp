// Run-tests for memcpy / memset / memcmp in libc.
//
// Output is KOI7 with case folding, so printed letters fold to upper case;
// expected strings use UPPERCASE ASCII.  Where a letter is awkward, the test
// compares numeric results with %d instead.  Each test compiles a tiny
// program(), runs it under Dubna, and checks the captured stdout.
#include "codegen_test.h"

// ---- memset --------------------------------------------------------------

TEST_F(CodegenTest, MemsetFillChar)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char buf[9];
            memset(buf, 'A', 8);
            buf[8] = 0;
            printf("%s\n", buf);
        }
    )");
    EXPECT_EQ("AAAAAAAA\n", result);
}

TEST_F(CodegenTest, MemsetReturnsDest)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char buf[4];
            char *r = memset(buf, 'X', 4);
            printf("%d\n", r == buf);
        }
    )");
    EXPECT_EQ("1\n", result);
}

TEST_F(CodegenTest, MemsetZeroBounded)
{
    // Fill only the middle, leaving the sentinels intact.
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char buf[6];
            buf[0] = 'A';
            buf[1] = 'B';
            buf[2] = 'C';
            buf[3] = 'D';
            buf[4] = 'E';
            buf[5] = 0;
            memset(buf + 1, 'Z', 3);
            printf("%s\n", buf);
        }
    )");
    EXPECT_EQ("AZZZE\n", result);
}

// ---- memcpy --------------------------------------------------------------

TEST_F(CodegenTest, MemcpyString)
{
    // n = 8 crosses the 6-byte word boundary.
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char dst[9];
            memcpy(dst, "ABCDEFGH", 8);
            dst[8] = 0;
            printf("%s\n", dst);
        }
    )");
    EXPECT_EQ("ABCDEFGH\n", result);
}

TEST_F(CodegenTest, MemcpyReturnsDest)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char dst[4];
            char *r = memcpy(dst, "XYZ", 3);
            printf("%d\n", r == dst);
        }
    )");
    EXPECT_EQ("1\n", result);
}

TEST_F(CodegenTest, MemcpyPartial)
{
    // Copy only 3 bytes; the rest of dst keeps its prior content.
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            char dst[7];
            memset(dst, 'Z', 6);
            dst[6] = 0;
            memcpy(dst, "ABCDEF", 3);
            printf("%s\n", dst);
        }
    )");
    EXPECT_EQ("ABCZZZ\n", result);
}

// ---- memcmp --------------------------------------------------------------

TEST_F(CodegenTest, MemcmpEqual)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            printf("%d\n", memcmp("ABCD", "ABCD", 4));
        }
    )");
    EXPECT_EQ("0\n", result);
}

TEST_F(CodegenTest, MemcmpLess)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            // 'A'(65) - 'B'(66) = -1
            printf("%d\n", memcmp("AAAA", "ABAA", 4));
        }
    )");
    EXPECT_EQ("-1\n", result);
}

TEST_F(CodegenTest, MemcmpGreater)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            // 'B'(66) - 'A'(65) = 1
            printf("%d\n", memcmp("ABAA", "AAAA", 4));
        }
    )");
    EXPECT_EQ("1\n", result);
}

TEST_F(CodegenTest, MemcmpBoundedPrefix)
{
    // Differ at index 3, but only the first 3 bytes are compared -> equal.
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <string.h>
        void program() {
            printf("%d\n", memcmp("ABCX", "ABCY", 3));
        }
    )");
    EXPECT_EQ("0\n", result);
}
