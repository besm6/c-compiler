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
// A local aggregate (struct/array) reserves a contiguous multi-word frame slot via
// an AllocateLocal pseudo-instruction (task #23), so its members no longer overlap
// adjacent auto slots.  The frame prologue (15 ,utm, N) extends the stack by the full
// aggregate size, and member access addresses word offset = byte_offset / 6.
//

// Local struct, write second member: the 2-word slot extends the frame by 2 words
// (15 ,utm, 2); s.y is word offset 1 of the slot at offset 0 -> 7 ,atx, 1.
TEST_F(CodegenTest, CopyToOffsetLocalWrite)
{
    std::string output =
        CompileToMadlen("struct Foo { int x; int y; }; void f(void){ struct Foo s; s.y = 5; }");
    EXPECT_EQ(R"(c
        f:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save0
          15 ,utm, 2
             ,xta, =5
           7 ,atx, 1
             ,uj, b/ret
             ,end,
)",
              output);
}

// Local struct, read second member: the 2-word slot occupies offsets 0..1 and the
// return temp lands at offset 2, so the frame extends by 3 words (15 ,utm, 3).
TEST_F(CodegenTest, CopyFromOffsetLocalRead)
{
    std::string output =
        CompileToMadlen("struct Foo { int x; int y; }; int f(void){ struct Foo s; return s.y; }");
    EXPECT_EQ(R"(c
        f:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save0
          15 ,utm, 2
           7 ,xta, 1
             ,uj, b/ret
             ,end,
)",
              output);
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
             ,xta, =7
             ,utc, g
             ,atx,
             ,utc, g
             ,xta, 1
             ,uj, b/ret
             ,end,
)",
              output);
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

// Runtime: a *local* struct now occupies a contiguous 2-word slot (task #23), so
// writing both members and reading them back yields independent values — they no
// longer overlap.  Returns 18 = 7 + 11.
TEST_F(CodegenTest, LocalStructRun)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        struct Foo { int x; int y; };
        void program() {
            struct Foo s;
            s.x = 7;
            s.y = 11;
            printf("%d\n", s.x + s.y);
        }
    )");
    EXPECT_EQ("18\n", result);
}
