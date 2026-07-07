//
// Unix-path run tests: compile → b6as → b6ld (crt0.o first) → b6sim, capturing the
// program's stdout.  Unlike the Madlen-on-dubna path — whose KOI7 output device folds
// text to upper case — the Unix (b6as) path is transparent: no KOI7 conversion, so
// bytes reach the host stdout verbatim and lower-case source text stays lower case.
// The expected strings therefore mirror the source text as written.  Task U6.
//
// Unlike the Madlen libc (whose startup calls `void program()`), the Unix crt0 calls
// `int main(void)` (see libc/besm6/unix/crt0.s), so these programs define main() and do
// their I/O directly; the printed stdout is what matters for the parity check.
//
// These need the sibling v7besm toolchain (b6as/b6ld/b6sim) on PATH; each test skips
// cleanly when it is absent, so `make run` stays green on machines without it.
//
#include "codegen_test.h"

TEST_F(CodegenTest, UnixRunEmptyProgram)
{
    SKIP_IF_NO_UNIX_RUN_TOOLS();
    EXPECT_EQ("", CompileAndRunUnix("int main(void) { return 0; }"));
}

TEST_F(CodegenTest, UnixRunPrintChar)
{
    SKIP_IF_NO_UNIX_RUN_TOOLS();
    std::string result = CompileAndRunUnix(R"(
        #include <stdio.h>
        int main(void) {
            putbyte('Q');
            putbyte('\n');
            return 0;
        }
    )");
    EXPECT_EQ("Q\n", result);
}

TEST_F(CodegenTest, UnixRunPrintDecimal)
{
    SKIP_IF_NO_UNIX_RUN_TOOLS();
    std::string result = CompileAndRunUnix(R"(
        #include <stdio.h>
        int main(void) {
            printf("%d\n", 42);
            return 0;
        }
    )");
    EXPECT_EQ("42\n", result);
}

// putchar writes one byte and returns it; print the byte then its returned value.
TEST_F(CodegenTest, UnixRunPutChar)
{
    SKIP_IF_NO_UNIX_RUN_TOOLS();
    std::string result = CompileAndRunUnix(R"(
        #include <stdio.h>
        int main(void) {
            int r = putchar('Q');
            putchar('\n');
            printf("%d\n", r);
            return 0;
        }
    )");
    EXPECT_EQ("Q\n81\n", result);
}

TEST_F(CodegenTest, UnixRunPrintFormatDecimal)
{
    SKIP_IF_NO_UNIX_RUN_TOOLS();
    std::string result = CompileAndRunUnix(R"(
        #include <stdio.h>
        int main(void) {
            printf("foo = %d, bar = %d\n", 123, -456);
            return 0;
        }
    )");
    EXPECT_EQ("foo = 123, bar = -456\n", result);
}

TEST_F(CodegenTest, UnixRunPrintFormatString)
{
    SKIP_IF_NO_UNIX_RUN_TOOLS();
    std::string result = CompileAndRunUnix(R"(
        #include <stdio.h>
        int main(void) {
            printf("hello %s\n", "world");
            return 0;
        }
    )");
    EXPECT_EQ("hello world\n", result);
}

TEST_F(CodegenTest, UnixRunPrintFormatChar)
{
    SKIP_IF_NO_UNIX_RUN_TOOLS();
    std::string result = CompileAndRunUnix(R"(
        #include <stdio.h>
        int main(void) {
            printf("hello %c%c%c%c%c\n", '(', '-', '_', '-', ')');
            return 0;
        }
    )");
    EXPECT_EQ("hello (-_-)\n", result);
}

// Exercise the multiply/divide/remainder runtime helpers end-to-end, printing the
// computed value.  20*7 + 20/7 - 20%7 = 140 + 2 - 6 = 136.
TEST_F(CodegenTest, UnixRunArithmetic)
{
    SKIP_IF_NO_UNIX_RUN_TOOLS();
    std::string result = CompileAndRunUnix(R"(
        #include <stdio.h>
        int main(void) {
            int a = 20, b = 7;
            printf("%d\n", a * b + a / b - a % b);
            return 0;
        }
    )");
    EXPECT_EQ("136\n", result);
}
