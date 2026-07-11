//
// End-to-end run tests for the Bemsh (Cyrillic autocode) dialect (task B3).
//
// Each test compiles C through the Bemsh path (genbesm --bemsh), wraps the output in a
// `*bemsh` Dubna job linking the Bemsh runtime library libbem.bin, runs it on the `dubna`
// simulator, and asserts the captured stdout — identical to the Madlen `CompileAndRun`
// path (see run_tests.cpp).  These are the first tests where compiler-generated *and*
// C-libc Bemsh code actually executes, exercising the calling convention (_save/_save0/
// _ret), the arithmetic helpers, the `внешн` call-target declarations, libbem linking, and
// the compiled stdout chain putbyte -> flush -> _tout.
//
// The runtime entry point is `void program()` (the Dubna-monitor convention); `*main progra`
// designates it (bemsh_mangle("program") == "progra").  Printed lowercase Latin renders as
// Cyrillic on the listing, so output tests use UPPERCASE ASCII (see run_tests.cpp).
//
#include "codegen_test.h"

// Calling convention only: an empty program links _save0/_ret and returns cleanly (no output).
TEST_F(CodegenTest, BemshEmptyProgramRun)
{
    std::string result = CompileAndRunBemsh("void program() {}");
    EXPECT_EQ("", result);
}

// Arithmetic helpers: a multiply routes through _mul; the program computes and returns with
// no output.  A missing _mul export or a link failure would diverge from the empty result.
TEST_F(CodegenTest, BemshArithmeticRun)
{
    std::string result = CompileAndRunBemsh(R"(
        int f(int a, int b) { return a * b; }
        void program() { volatile int x = f(6, 7); (void)x; }
    )");
    EXPECT_EQ("", result);
}

// Visible output: the compiled putbyte -> flush -> _tout chain writes bytes to stdout.
// Mirrors the Madlen PrintChar test; '\n' flushes the KOI7 line buffer.
TEST_F(CodegenTest, BemshPutByteRun)
{
    std::string result = CompileAndRunBemsh(R"(
        #include <stdio.h>
        void program() {
            putbyte('H');
            putbyte('I');
            putbyte('\n');
        }
    )");
    EXPECT_EQ("HI\n", result);
}

// putchar wraps putbyte; two characters then a flushing newline.
TEST_F(CodegenTest, BemshPutCharRun)
{
    std::string result = CompileAndRunBemsh(R"(
        #include <stdio.h>
        void program() {
            putchar('H');
            putchar('\n');
        }
    )");
    EXPECT_EQ("H\n", result);
}
