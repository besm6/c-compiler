#include "codegen_test.h"

//
// Task #26 framework + rule #27 (redundant reload elimination).
//
// Rule #27: an `atx reg,off` that stores A to a frame slot leaves the value in A,
// so an immediately following `xta reg,off` reload of the same slot is redundant
// and is deleted.  See docs/Peephole_Rewrites.md §5.1.
//

// The store-then-reload of the sum temporary is collapsed: the `7 ,xta,` reload
// that followed `7 ,atx,` is gone (the store remains until rule #28).
TEST_F(CodegenTest, RedundantReloadRemoved)
{
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
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// A label is a basic-block boundary: control can re-enter, so a slot stored just
// before the label must NOT license dropping a reload of it just after.  The
// if/else join stores `c` (slot 7,0) at the end of the else arm, then the join
// label `*1`, then reloads `c` for the return.  Because the label resets the
// tracked state, the `7 ,xta,` reload after `*1:` survives (it is NOT collapsed
// against the preceding `7 ,atx,`, which lies in a different basic block).
TEST_F(CodegenTest, ReloadAcrossLabelKept)
{
    std::string output = CompileToMadlen(
        "int foo(int a, int b) { int c; if (a) c = a; else c = b; return c; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
             ,uza, *0
           6 ,xta,
           7 ,atx,
             ,uj, *1
       *0:   ,bss,
           6 ,xta, 1
           7 ,atx,
       *1:   ,bss,
           7 ,xta,
             ,uj, b/ret
             ,uj, b/ret
             ,end,
)",
              output);
}

// Behavior must be unchanged with the pass on (the pass is always on): the
// store/reload that rule #27 collapses is on the a+b path of this program.
TEST_F(CodegenTest, ArithmeticBehaviorUnchanged)
{
    std::string out = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program(void) {
            int a = 17;
            int b = 25;
            printf("%d\n", a + b);
        }
    )");
    EXPECT_EQ("42\n", out);
}
