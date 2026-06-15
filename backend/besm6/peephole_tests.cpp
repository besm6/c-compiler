#include "codegen_test.h"

//
// Task #26 framework + rule #27 (redundant reload elimination) + rule #28 (dead
// temp-store elimination).
//
// Rule #27: an `atx reg,off` that stores A to a frame slot leaves the value in A,
// so an immediately following `xta reg,off` reload of the same slot is redundant
// and is deleted.  See docs/Peephole_Rewrites.md §5.1.
//
// Rule #28: once #27 removes the reload, the `atx` of a single-use '%'-temporary is
// dead — nothing reads the slot before the block ends or it is overwritten — and is
// removed too, leaving the result live only in A.  See docs/Peephole_Rewrites.md §5.2.
//

// The sum temporary's store and reload are both gone: #27 drops the `7 ,xta,` reload
// and #28 drops the now-dead `7 ,atx,` store, so the sum flows straight from A into
// the global `g`.
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

// Rule #28 in full: `return a + b;` routes the sum through a single-use temporary.
// #27 drops the reload, #28 drops the dead store, and the sum is returned straight
// from A — no `7 ,atx,` temp store survives.  (The temp's frame slot is still counted
// in `15 ,utm, 1`; reclaiming it is task #35.)
TEST_F(CodegenTest, DeadTempStoreRemoved)
{
    std::string output = CompileToMadlen("int foo(int a, int b) { return a + b; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
           6 ,a+x, 1
             ,uj, b/ret
             ,uj, b/ret
             ,end,
)",
              output);
}

// Multi-block guard: a temporary that is live across a basic-block boundary must keep
// its store.  The ternary `a ? b : c` writes its result temporary (slot 7,0) at the end
// of each arm and reloads it after the join label `*1`.  Because that temporary is
// referenced in three basic blocks, rule #28 must NOT treat either `7 ,atx,` as dead —
// both stores survive.
TEST_F(CodegenTest, TempLiveAcrossBranchKept)
{
    std::string output = CompileToMadlen("int foo(int a, int b, int c) { return a ? b : c; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
             ,uza, *0
           6 ,xta, 1
           7 ,atx,
             ,uj, *1
       *0:   ,bss,
           6 ,xta, 2
           7 ,atx,
       *1:   ,bss,
           7 ,xta,
             ,uj, b/ret
             ,uj, b/ret
             ,end,
)",
              output);
}

// Behavior guard for the multi-block case: dead temp-store elimination must not
// corrupt a value that is live across a branch.  Both ternary results must compute.
TEST_F(CodegenTest, TempAcrossBranchBehaviorUnchanged)
{
    std::string out = CompileAndRun(R"(
        int printf(const char *format, ...);
        int pick(int a, int b, int c) { return a ? b : c; }
        void program(void) {
            printf("%d %d\n", pick(1, 17, 25), pick(0, 17, 25));
        }
    )");
    EXPECT_EQ("17 25\n", out);
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
