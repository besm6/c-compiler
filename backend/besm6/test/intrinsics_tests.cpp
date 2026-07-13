//
// The <besm6.h> compiler intrinsics (docs/Besm6_Intrinsics.md), Tier 2: the five
// bit-manipulation instructions that have no C equivalent — gather, scatter, population
// count, highest set bit, and the machine's own end-around-carry add.
//
// Each is a call in the IR and an inline instruction in the machine code: the golden tests
// below pin that no `,call,` (and no `,subp,`) survives — which matters more than it looks,
// because all nine intrinsic names truncate to the same 8-character Madlen symbol, so one
// left uninterecepted would silently alias another rather than fail to link.  The run tests
// pin the values the hardware actually produces, on all three paths.
//
#include "codegen_test.h"

//
// Instruction selection — Madlen.
//
// b6_popcount(a) is __besm6_acx(a, 0), and a zero operand needs no literal at all: the
// `,acx,` is left with an empty address field, so EA = 0 and it reads memory word 0, which
// always reads as zero.
//
TEST_F(CodegenTest, IntrinsicPopcountMadlen)
{
    std::string output = CompileToMadlen(R"(
        #include <besm6.h>
        unsigned pcnt(unsigned a) { return b6_popcount(a); }
    )");
    EXPECT_EQ(R"(c
     pcnt:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
             ,acx,
             ,uj, b/ret
             ,end,
)",
              output);
}

// Gather and scatter: one instruction each, the mask as the memory operand.
TEST_F(CodegenTest, IntrinsicGatherScatterMadlen)
{
    std::string output = CompileToMadlen(R"(
        #include <besm6.h>
        unsigned gather(unsigned a, unsigned m) { return __besm6_apx(a, m); }
        unsigned scatter(unsigned a, unsigned m) { return __besm6_aux(a, m); }
    )");
    EXPECT_EQ(R"(c
   gather:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
           6 ,apx, 1
             ,uj, b/ret
             ,end,
c
  scatter:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
           6 ,aux, 1
             ,uj, b/ret
             ,end,
)",
              output);
}

//
// Instruction selection — Unix (b6as).
//
TEST_F(CodegenTest, IntrinsicHighbitUnix)
{
    std::string output = CompileToUnix(R"(
        #include <besm6.h>
        unsigned highbit(unsigned a) { return b6_highbit(a); }
    )");
    EXPECT_EQ(R"(    .text
    .globl highbit
highbit:
    its 13
 13 vjm b$save
  6 xta
    anx
    uj b$ret
)",
              output);
}

// ARX is the one that leaves multiplicative ω, so it is trailed by the no-op `aox` — OR in
// memory word 0: A unchanged, ω back to logical.
TEST_F(CodegenTest, IntrinsicCyclicAddUnix)
{
    std::string output = CompileToUnix(R"(
        #include <besm6.h>
        unsigned cyc(unsigned a, unsigned b) { return __besm6_arx(a, b); }
    )");
    EXPECT_EQ(R"(    .text
    .globl cyc
cyc:
    its 13
 13 vjm b$save
  6 xta
  6 arx 1
    aox
    uj b$ret
)",
              output);
}

//
// Instruction selection — Bemsh.  Same two instructions under their Cyrillic mnemonics
// (чед = acx, слц = arx, или = aox), and again no `внешн` for the intrinsic itself.
//
TEST_F(CodegenTest, IntrinsicPopcountBemsh)
{
    std::string output = CompileToBemsh(R"(
        #include <besm6.h>
        unsigned pcnt(unsigned a) { return b6_popcount(a); }
    )");
    EXPECT_EQ(R"(ввд$$$
*
pcnt   старт 1
_save  внешн ._save
_ret   внешн ._ret
       счим 13
       пв _save(13)
       сч (6)
       чед
       пб _ret
       финиш
квч$$$
трн$$$
0-0
блмак
бтмалф
кнц$$$
)",
              output);
}

TEST_F(CodegenTest, IntrinsicCyclicAddBemsh)
{
    std::string output = CompileToBemsh(R"(
        #include <besm6.h>
        unsigned cyc(unsigned a, unsigned b) { return __besm6_arx(a, b); }
    )");
    EXPECT_EQ(R"(ввд$$$
*
cyc    старт 1
_save  внешн ._save
_ret   внешн ._ret
       счим 13
       пв _save(13)
       сч (6)
       слц 1(6)
       или
       пб _ret
       финиш
квч$$$
трн$$$
0-0
блмак
бтмалф
кнц$$$
)",
              output);
}

//
// The ω correction, pinned at the instruction level.
//
// Peephole rules #27 and #28 drop the store/reload of the boolean, so the branch consumes
// the accumulator the ARX left — `,arx,` then `,uza,` with nothing in between.  Under
// multiplicative ω that `uza` would test abs(A) < 0.5 (i.e. bit 48) instead of A ≠ 0.  The
// `,aox,` that lands between them is what makes the branch test the value.
//
TEST_F(CodegenTest, IntrinsicCyclicAddBranchOmega)
{
    std::string output = CompileToMadlen(R"(
        #include <stdio.h>
        #include <besm6.h>
        void f(unsigned a, unsigned b)
        {
            if (__besm6_arx(a, b))
                puts("NONZERO");
            else
                puts("ZERO");
        }
    )");
    EXPECT_EQ(R"(c
        f:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
           6 ,arx, 1
             ,aox,
             ,uza, *1
          14 ,vtm, *str0
             ,ita, 14
             ,aox, =:64
          14 ,vtm, -1
             ,call, puts
             ,uj, *2
       *1:   ,bss,
          14 ,vtm, *str1
             ,ita, 14
             ,aox, =:64
          14 ,vtm, -1
             ,call, puts
       *2:   ,bss,
             ,uj, b/ret
    *str1:   ,log, 2644252223600000
    *str0:   ,log, 2344751626442522
             ,log, 2360000000000000
             ,end,
)",
              output);
}

//
// Execution — Madlen / dubna.  (The KOI7 output device folds letters to upper case, so the
// printed text and the expectations are UPPERCASE.)
//
// popcount(0377) = 8; popcount(0) = 0.  anx numbers from the MSB — bit 48 is position 1 and
// bit 1 is position 48 — so highbit(1) = 48 and highbit(2^47) = 1.  Zero has no highest set
// bit and there is no distinguished "not found" value: the result is just x, here 0.
//
TEST_F(CodegenTest, IntrinsicPopcountRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        #include <besm6.h>
        unsigned pcnt(unsigned a) { return b6_popcount(a); }
        unsigned hbit(unsigned a) { return b6_highbit(a); }
        void program()
        {
            printf("%d %d\n", pcnt(0377), pcnt(0));
            printf("%d %d %d\n", hbit(1), hbit(04000000000000000u), hbit(0));
        }
    )");
    EXPECT_EQ("8 0\n" "48 1 0\n", result);
}

//
// apx gathers the bits of A selected by the mask, aligned to the MSB; aux scatters them
// back into the mask's positions.  They are exact inverses, so a round trip through one
// mask reproduces exactly the masked bits of the original — and nothing else.
//
TEST_F(CodegenTest, IntrinsicGatherScatterRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        #include <besm6.h>
        void program()
        {
            unsigned a = 0525252525252525u;
            unsigned m = 0770077007700770u;
            unsigned r = __besm6_aux(__besm6_apx(a, m), m);
            printf("%o\n", r);
            if (r == (a & m))
                puts("SAME");
            else
                puts("DIFFERENT");
        }
    )");
    EXPECT_EQ("520052005200520\n" "SAME\n", result);
}

//
// arx is the machine's own integer add: a carry out of bit 48 comes back into bit 1.  This
// is the one place it differs from C's `+` on unsigned, which would wrap to 0.
//
TEST_F(CodegenTest, IntrinsicCyclicAddRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        #include <besm6.h>
        void program()
        {
            unsigned all = 07777777777777777u;   /* 48 one-bits */
            printf("%d %d\n", __besm6_arx(all, 1), __besm6_arx(2, 3));
        }
    )");
    EXPECT_EQ("1 5\n", result);
}

//
// The ω correction, executed.  Both globals are zero, so the cyclic add yields zero and the
// `if` must not be taken.  Without the `aox` the `uza` inherits multiplicative ω from the
// arx, tests bit 48 of a zero accumulator, finds it clear — and takes the branch, printing
// NONZERO.  Deleting the aox in intrinsics.c must flip this test.
//
TEST_F(CodegenTest, IntrinsicCyclicAddBranchRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        #include <besm6.h>
        unsigned ga, gb;
        void program()
        {
            if (__besm6_arx(ga, gb))
                puts("NONZERO");
            else
                puts("ZERO");
            gb = 1;
            if (__besm6_arx(ga, gb))
                puts("NONZERO");
            else
                puts("ZERO");
        }
    )");
    EXPECT_EQ("ZERO\n" "NONZERO\n", result);
}

//
// Execution — Unix (b6as/b6ld/b6sim).  Entry point is main(), and b6sim's output is not
// case-folded, so the expectations are verbatim.
//
TEST_F(CodegenTest, UnixRunIntrinsics)
{
    SKIP_IF_NO_UNIX_RUN_TOOLS();
    std::string result = CompileAndRunUnix(R"(
        #include <stdio.h>
        #include <besm6.h>
        unsigned ga, gb;
        int main(void)
        {
            unsigned a = 0525252525252525u;
            unsigned m = 0770077007700770u;
            printf("%d %d\n", b6_popcount(0377), b6_highbit(1));
            printf("%d\n", __besm6_aux(__besm6_apx(a, m), m) == (a & m));
            printf("%d\n", __besm6_arx(07777777777777777u, 1));
            if (__besm6_arx(ga, gb))
                puts("nonzero");
            else
                puts("zero");
            return 0;
        }
    )");
    EXPECT_EQ("8 48\n" "1\n" "1\n" "zero\n", result);
}

//
// Execution — Bemsh / dubna (the Cyrillic mnemonics, assembled by the Bemsh translator and
// linked against libbem.bin).  Upper case again, for the same reason as the Madlen path.
//
TEST_F(CodegenTest, BemshIntrinsicsRun)
{
    std::string result = CompileAndRunBemsh(R"(
        #include <stdio.h>
        #include <besm6.h>
        unsigned ga, gb;
        void program()
        {
            unsigned a = 0525252525252525u;
            unsigned m = 0770077007700770u;
            printf("%d %d\n", b6_popcount(0377), b6_highbit(1));
            printf("%d\n", __besm6_aux(__besm6_apx(a, m), m) == (a & m));
            printf("%d\n", __besm6_arx(07777777777777777u, 1));
            if (__besm6_arx(ga, gb))
                puts("NONZERO");
            else
                puts("ZERO");
        }
    )");
    EXPECT_EQ("8 48\n" "1\n" "1\n" "ZERO\n", result);
}
