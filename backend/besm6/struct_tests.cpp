#include "codegen_test.h"

//
// CopyToOffset / CopyFromOffset — aggregate member access on a named struct
// variable (task #20).
//
// `s.field` (read) lowers to copy_from_offset; `s.field = …` (write) lowers to
// copy_to_offset.  The instruction carries a *byte* offset; for the besm6 target a
// word-sized member (int/long/pointer/float/double) is 6 bytes, so the backend
// addresses the member at word offset = byte_offset / 6.  Pointer-based access
// (`p->field`) is handled by ADD_PTR + LOAD/STORE instead (task #19).
//
// Multi-word *local* struct runtime tests await task #23: frame.c still reserves one
// word per local name, so a local struct's higher members overlap adjacent auto slots.
// The runtime round-trip below therefore uses a global struct, which gets full storage.
//

// Local struct, write second member: 7 ,atx, <slot + 1> (word offset 1 for s.y).
TEST_F(CodegenTest, CopyToOffsetLocalWrite)
{
    std::string output = CompileToMadlen(
        "struct Foo { int x; int y; }; void f(void){ struct Foo s; s.y = 5; }");
    EXPECT_EQ(R"(c
        f:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save0
          15 ,utm, 1
             ,xta, =5
           7 ,atx, 1
             ,uj, b/ret
             ,end,
)", output);
}

// Local struct, read second member: 7 ,xta, <slot + 1>.
TEST_F(CodegenTest, CopyFromOffsetLocalRead)
{
    std::string output = CompileToMadlen(
        "struct Foo { int x; int y; }; int f(void){ struct Foo s; return s.y; }");
    EXPECT_EQ(R"(c
        f:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save0
          15 ,utm, 2
           7 ,xta, 1
           7 ,atx, 1
           7 ,xta, 1
             ,uj, b/ret
             ,uj, b/ret
             ,end,
)", output);
}

// Global struct: ,utc, g then ,atx, 1 / ,xta, 1, plus a ,subp, g declaration.
TEST_F(CodegenTest, CopyOffsetGlobalStruct)
{
    std::string output = CompileToMadlen(
        "struct Foo { int x; int y; }; struct Foo g; int f(void){ g.x = 7; return g.y; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 2
             ,end,
c
        f:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save0
          15 ,utm, 1
             ,xta, =7
             ,utc, g
             ,atx,
             ,utc, g
             ,xta, 1
           7 ,atx,
           7 ,xta,
             ,uj, b/ret
             ,uj, b/ret
             ,end,
)", output);
}

// Runtime: write both members of a global struct, read them back and add.
TEST_F(CodegenTest, CopyOffsetGlobalRun)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        struct Foo { int x; int y; };
        struct Foo g;
        void program() {
            g.x = 7;
            g.y = 35;
            printf("%d\n", g.x + g.y);
        }
    )");
    EXPECT_EQ("42\n", result);
}
