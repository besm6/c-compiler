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

// Rule #29 — NTR mode coalescing.  Two double additions in one basic block emit a
// `ntr 7` (restore) immediately chased by a `ntr 0` (re-enter FP mode).  Rule #29(b)
// drops the dead `ntr 7` and #29(a) drops the now-redundant `ntr 0`, so R = 0 is held
// across both `a+x` and restored once at the end: a single `,ntr, 0` … `,ntr, 7`
// bracket.  See docs/Peephole_Rewrites.md §5.3.
TEST_F(CodegenTest, ConsecutiveFpOpsCoalesceNtr)
{
    std::string output = CompileToMadlen(
        "double f(double a, double b, double c) { double e = a + b; return e + c; }");
    EXPECT_EQ(R"(c
        f:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 2
           6 ,xta,
             ,ntr, 0
           6 ,a+x, 1
           6 ,a+x, 2
             ,ntr, 7
             ,uj, b/ret
             ,uj, b/ret
             ,end,
)",
              output);
}

// Behavior guard for rule #29: coalescing the NTR brackets must not change the FP
// result.  Two consecutive FP adds, (a + b) + c, with normalization/rounding active
// throughout.  The result is printed as its raw octal bit pattern (the runtime has no
// %f formatting); 8.0 encodes as 4210000000000000.
TEST_F(CodegenTest, FpCoalesceBehaviorUnchanged)
{
    std::string out = CompileAndRun(R"(
        int printf(const char *format, ...);
        double add3(double a, double b, double c) { double e = a + b; return e + c; }
        void program(void) {
            printf("%o\n", add3(1.5, 2.5, 4.0));
        }
    )");
    EXPECT_EQ("4210000000000000\n", out);
}

// Task #30 — compare → branch fusion.  A relational helper's 0/1 result that feeds a
// JUMP_IF_ZERO is consumed directly by the conditional branch: no boolean temp is stored
// or reloaded.  This is the emergent product of rule #27 (which drops the reload) and rule
// #28 (which drops the now-dead store), made correct by the runtime helpers' logical-ω
// exit contract — every comparison helper leaves ω consistent with its returned A, so the
// `,uza,` tests the result without a reload.  See docs/Peephole_Rewrites.md §5.4 and the
// "ω mode and the AU mode register R" section of docs/Besm6_Runtime_Library.md.
//
// The fused shape is `xta / xts / ,call, b/lt / ,uza,` with no `,atx,`/`,xta,` of the
// comparison temporary between the call and the branch.
TEST_F(CodegenTest, CompareBranchFused)
{
    std::string output =
        CompileToMadlen("extern int g; void foo(int a, int b) { if (a < b) g = 1; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
           6 ,xts, 1
             ,call, b/lt
             ,uza, *1
             ,xta, =1
             ,utc, g
             ,atx,
             ,uj, *2
       *1:   ,bss,
       *2:   ,bss,
             ,uj, b/ret
             ,end,
)",
              output);
}

// Behavior guard for the compare → branch fusion across signedness and FP: each
// comparison drives an if/else and prints a distinguishing digit (the listing path
// upper-cases output, so letters cannot encode a true/false distinction).  The expected
// string 0101110 is, in order: a<b (7<4) 0, a>b 1, a==b 0, a!=b 1, ua<ub (3<9) 1,
// x<y (1.5<2.5) 1, x>=y 0 — confirming every helper's ω feeds the branch correctly.
TEST_F(CodegenTest, CompareBranchFusedBehaviorUnchanged)
{
    std::string out = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program(void) {
            volatile int a = 7, b = 4;
            volatile unsigned ua = 3u, ub = 9u;
            volatile double x = 1.5, y = 2.5;
            printf("%d", (a < b) ? 1 : 0);
            if (a > b)   printf("1"); else printf("0");
            if (a == b)  printf("1"); else printf("0");
            if (a != b)  printf("1"); else printf("0");
            if (ua < ub) printf("1"); else printf("0");
            if (x < y)   printf("1"); else printf("0");
            if (x >= y)  printf("1"); else printf("0");
            printf("\n");
        }
    )");
    EXPECT_EQ("0101110\n", out);
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
