#include "codegen_test.h"

TEST_F(CodegenTest, LabelJump)
{
    // goto end; end: g = x — the trivial forward goto is folded away by the optimizer
    // (end: is immediately next), leaving just the param→global copy
    std::string output = CompileToMadlen("extern int g; void foo(int x) { goto end; end: g = x; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// label_loops() resets label_seq=0 per function; STMT_WHILE allocates .L0 (end)
// then .L1 (continue), so the continue label (loop top) is .L1 and end is .L0.
TEST_F(CodegenTest, WhileLoopJumpIfZero)
{
    std::string output = CompileToMadlen("void foo(int x) { while (x) {} }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
      *L1:   ,bss,
           6 ,xta,
             ,uza, *L0
             ,uj, *L1
      *L0:   ,bss,
             ,uj, b/ret
             ,end,
)", output);
}

// new_temp() allocates ".0" for the do-while loop-top label;
// label_loops assigns .L0 (end) and .L1 (continue) — both dead (no break/continue),
// so the optimizer removes them; bar() call keeps the loop body non-empty.
TEST_F(CodegenTest, DoWhileJumpIfNotZero)
{
    std::string output = CompileToMadlen(
        "void bar(void); void foo(int x) { do { bar(); } while (x); }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
       *0:   ,bss,
             ,call, bar
           6 ,xta,
             ,u1a, *0
             ,uj, b/ret
             ,end,
)", output);
}

// --- switch statement (task #5) ---------------------------------------------
// switch has no dedicated TAC node: it lowers to a COPY + a chain of
// BINARY(equal)/JUMP_IF_NOT_ZERO compares + a JUMP to default/end + inline
// LABELs (translator/stmt.c). These end-to-end tests confirm the dispatch
// chain runs correctly under the simulator for dense, sparse, default,
// fall-through, break, and no-match cases.

// Dense, contiguous case values plus a default. Each value dispatches to its
// own arm; a non-matching value reaches the default.
TEST_F(CodegenTest, SwitchDense)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        int classify(int x) {
            switch (x) {
                case 0: return 10;
                case 1: return 11;
                case 2: return 12;
                default: return 99;
            }
        }
        void program() {
            printf("%d %d %d %d\n",
                   classify(0), classify(1), classify(2), classify(5));
        }
    )");
    EXPECT_EQ("10 11 12 99\n", result);
}

// Sparse, non-contiguous case constants exercise the same linear compare chain.
TEST_F(CodegenTest, SwitchSparse)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        int classify(int x) {
            switch (x) {
                case 1:    return 1;
                case 100:  return 2;
                case 1000: return 3;
                default:   return 0;
            }
        }
        void program() {
            printf("%d %d %d %d\n",
                   classify(1), classify(100), classify(1000), classify(50));
        }
    )");
    EXPECT_EQ("1 2 3 0\n", result);
}

// A default listed between cases is taken only on no match, regardless of its
// textual position among the cases.
TEST_F(CodegenTest, SwitchDefaultMiddle)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        int classify(int x) {
            switch (x) {
                case 1:  return 11;
                default: return 99;
                case 2:  return 22;
            }
        }
        void program() {
            printf("%d %d %d\n", classify(1), classify(2), classify(7));
        }
    )");
    EXPECT_EQ("11 22 99\n", result);
}

// Cases without break fall through into the following arm(s). Accumulate into
// a volatile local so the dispatch can't be folded away.
TEST_F(CodegenTest, SwitchFallthrough)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        int sum_from(int x) {
            volatile int r = 0;
            switch (x) {
                case 1: r += 1;
                case 2: r += 2;
                case 3: r += 4;
            }
            return r;
        }
        void program() {
            printf("%d %d %d %d\n",
                   sum_from(1), sum_from(2), sum_from(3), sum_from(9));
        }
    )");
    EXPECT_EQ("7 6 4 0\n", result);
}

// break after each arm prevents fall-through, isolating the results.
TEST_F(CodegenTest, SwitchBreak)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        int pick(int x) {
            volatile int r = 0;
            switch (x) {
                case 1: r = 1; break;
                case 2: r = 2; break;
                case 3: r = 3; break;
                default: r = 9; break;
            }
            return r;
        }
        void program() {
            printf("%d %d %d %d\n", pick(1), pick(2), pick(3), pick(8));
        }
    )");
    EXPECT_EQ("1 2 3 9\n", result);
}

// With no default, a value matching no case skips the whole body and leaves an
// initialized local untouched.
TEST_F(CodegenTest, SwitchNoDefault)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        int pick(int x) {
            volatile int r = -1;
            switch (x) {
                case 1: r = 1; break;
                case 2: r = 2; break;
            }
            return r;
        }
        void program() {
            printf("%d %d %d\n", pick(1), pick(2), pick(7));
        }
    )");
    EXPECT_EQ("1 2 -1\n", result);
}
