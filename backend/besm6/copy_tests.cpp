#include "codegen_test.h"

// Global → local: x = g where x is never used — mark it as volatile.
TEST_F(CodegenTest, CopyGlobalToLocal)
{
    std::string output = CompileToMadlen(R"(
        extern int g;
        void foo(void) {
            volatile int x;
            x = g;
        }
    )");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save0
          15 ,utm, 1
             ,utc, g
             ,xta,
           7 ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// Local → global: COPY a → g (Case C)
TEST_F(CodegenTest, CopyLocalToGlobal)
{
    std::string output = CompileToMadlen("extern int g; void foo(int a) { g = a; }");
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
)",
              output);
}

// Global → global: COPY g → h (Case D)
TEST_F(CodegenTest, CopyGlobalToGlobal)
{
    std::string output = CompileToMadlen("extern int g, h; void foo(void) { h = g; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
        h:   ,subp,
             ,its, 13
             ,call, b/save0
             ,utc, g
             ,xta,
             ,utc, h
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// Runtime: write to global, copy global→global, read back via local.
TEST_F(CodegenTest, GlobalScalarReadWrite)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        int g, h;
        void copy_to_h(int v) { g = v; h = g; }
        void write_h(void) { int x; x = h; writeb(x); writeb('\n'); }
        void program() { copy_to_h('M'); write_h(); }
    )");
    EXPECT_EQ("M\n", result);
}

// COPY from integer constant to a local variable.
// 42 decimal = 52 octal → literal =52.
TEST_F(CodegenTest, CopyConstToLocal)
{
    std::string output = CompileToMadlen(R"(
        void foo(void) {
            volatile int x;
            x = 42;
        }
    )");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save0
          15 ,utm, 1
             ,xta, =52
           7 ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// COPY from integer constant 0 to a global variable.
TEST_F(CodegenTest, CopyConstToGlobal)
{
    std::string output = CompileToMadlen("extern int g; void foo(void) { g = 0; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save0
             ,xta, =0
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// COPY from negative integer constant: -1 must be masked to 41 bits
// (0x1FFFFFFFFFF = 37777777777777 octal), not emitted as a 64-bit pattern.
TEST_F(CodegenTest, CopyNegConst)
{
    std::string output = CompileToMadlen("extern int g; void foo(void) { g = -1; }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save0
             ,xta, =37777777777777
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}
