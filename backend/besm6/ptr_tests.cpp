#include "codegen_test.h"

//
// ADD_PTR — pointer/array index scaling (task #19).
//
// dst = ptr + index * scale.  `scale` is the element size in bytes; the backend
// converts it to a word scale (scale / 6) since BESM-6 is word-addressed.
//

// Global array, variable index, variable source: word scale 1 (plain), array base.
// The element address is materialized via the index register on UTC:
//   xta i / ati 1 / 1 ,utc, arr / 14 ,vtm, / ita 14
TEST_F(CodegenTest, AddPtrGlobalArrayStore)
{
    std::string output = CompileToMadlen("int arr[3]; void f(long i, int v){ arr[i] = v; }");
    EXPECT_EQ(R"(c
      arr:   ,name,
             ,bss, 3
             ,end,
c
        f:   ,name,
    b/ret:   ,subp,
      arr:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
             ,ati, 1
           1 ,utc, arr
          14 ,vtm, 0
             ,ita, 14
           7 ,atx,
             ,ati, 1
           6 ,xta, 1
           1 ,atx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// Pointer parameter, variable index: word scale 1, pointer base — the pointer's
// stored value is the base address, so the scaled index is added with a plain A+X.
TEST_F(CodegenTest, AddPtrPointerLoad)
{
    std::string output = CompileToMadlen("int f(int *a, long i){ return a[i]; }");
    EXPECT_EQ(R"(c
        f:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 2
           6 ,xta, 1
           6 ,a+x,
           7 ,atx,
             ,ati, 1
           1 ,xta,
           7 ,atx, 1
             ,uj, b/ret
             ,uj, b/ret
             ,end,
)",
              output);
}

// Two-word element (long long, 12 bytes → word scale 2): the index is scaled by a
// left shift (,asn, 63 = shift left 1) before adding the pointer base.
TEST_F(CodegenTest, AddPtrPowerOfTwoScale)
{
    std::string output = CompileToMadlen("long long *f(long long *p, long i){ return &p[i]; }");
    EXPECT_EQ(R"(c
        f:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta, 1
             ,asn, 63
           6 ,a+x,
           7 ,atx,
             ,uj, b/ret
             ,uj, b/ret
             ,end,
)",
              output);
}

// Runtime: store through a variable index, then load it back.
TEST_F(CodegenTest, AddPtrVarIndexRun)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        int arr[3];
        void program() {
            int v = 42;
            long i = 1;
            arr[i] = v;
            printf("%d\n", arr[i]);
        }
    )");
    EXPECT_EQ("42\n", result);
}

// Runtime: fill a global array element-by-element, then read every element back.
TEST_F(CodegenTest, AddPtrGlobalArrayRun)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        int arr[3];
        void program() {
            int a = 10, b = 20, c = 30;
            arr[0] = a;
            arr[1] = b;
            arr[2] = c;
            printf("%d %d %d\n", arr[0], arr[1], arr[2]);
        }
    )");
    EXPECT_EQ("10 20 30\n", result);
}

// A local array reserves a contiguous multi-word frame slot (task #23): int a[4]
// occupies 4 words, so the frame prologue extends the stack by exactly 4 (no other
// autos in this function).  Previously frame.c reserved a single word per name.
TEST_F(CodegenTest, LocalArrayFrameSize)
{
    std::string output = CompileToMadlen("void f(void){ int a[4]; }");
    EXPECT_NE(output.find("15 ,utm, 4"), std::string::npos) << output;
}

// Runtime: a local array indexes into its own contiguous frame slot, so distinct
// elements hold distinct values.  Returns 30 = 10 + 20.
TEST_F(CodegenTest, LocalArrayRun)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            int a[4];
            a[0] = 10;
            a[3] = 20;
            printf("%d\n", a[0] + a[3]);
        }
    )");
    EXPECT_EQ("30\n", result);
}
