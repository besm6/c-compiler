// Run-tests for C11 _Bool.
//
// Two things are checked end to end: that a _Bool object which needs storage emitted
// reaches the translator at all (a file-scope/static/array/pointer/typedef _Bool used to
// abort b6lower), and that every conversion to _Bool normalizes to 0 or 1 (C11 §6.3.1.2).
// _Bool is one machine word here — the char sizes mean byte-packed storage and fat byte
// pointers on this target, which is the wrong representation for a one-bit type.
//
// Output is KOI7 with case folding, so printed letters must be UPPER CASE.
#include "codegen_test.h"

// C11 §6.3.1.2 on every runtime conversion path: initialization of an automatic object,
// assignment, argument passing, return, and the explicit cast.
TEST_F(CodegenTest, BoolNormalizesToZeroOrOne)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        static int sink(_Bool b) { return b; }
        static _Bool ret(int x) { return x; }
        void program() {
            _Bool a = 5;
            _Bool c;
            c = 42;
            printf("INIT %d\n", a);
            printf("ASSIGN %d\n", c);
            printf("PARAM %d\n", sink(7));
            printf("RETURN %d\n", ret(9));
            printf("CAST %d\n", (int)(_Bool)9);
            printf("ZERO %d\n", (int)(_Bool)0);
        }
    )");
    EXPECT_EQ("INIT 1\nASSIGN 1\nPARAM 1\nRETURN 1\nCAST 1\nZERO 0\n", result);
}

// A _Bool object with static storage duration, an array of _Bool, a pointer to one and a
// typedef of one each hand the object's own type to the translator, which is what used to
// fail ("ast_type_to_tac_type: unsupported type kind 1").
TEST_F(CodegenTest, BoolStaticArrayPointerTypedef)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        typedef _Bool mybool;
        _Bool g;
        _Bool gi = 5;
        static _Bool s;
        _Bool arr[4];
        _Bool *p = &g;
        mybool t;
        void program() {
            *p = 42;
            s = 1;
            t = 300;
            arr[2] = 7;
            printf("G %d GI %d S %d T %d\n", g, gi, s, t);
            printf("ARR %d %d %d %d\n", arr[0], arr[1], arr[2], arr[3]);
            printf("PTR %d\n", *p);
        }
    )");
    EXPECT_EQ("G 1 GI 1 S 1 T 1\nARR 0 0 1 0\nPTR 1\n", result);
}

// A _Bool really is two-valued: read out as an integer it is only ever 0 or 1.
TEST_F(CodegenTest, BoolIsTwoValuedInArithmetic)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        void program() {
            _Bool b = 5;
            printf("EQ1 %d EQ5 %d SUM %d\n", b == 1, b == 5, b + b);
        }
    )");
    EXPECT_EQ("EQ1 1 EQ5 0 SUM 2\n", result);
}

// ++/-- and the compound operators bypass the cast path, so each re-normalizes on its own:
// ++/-- in gen_step, `op=` through the promoted compute-in-int-and-convert-back path.
TEST_F(CodegenTest, BoolIncrementAndCompoundAssign)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        void program() {
            _Bool b = 1;
            b++;                    /* 1+1 = 2 -> 1 */
            printf("INC %d\n", b);
            b = 0; b--;             /* 0-1 = -1 -> 1 */
            printf("DEC %d\n", b);
            b = 1; b += 3;          /* 4 -> 1 */
            printf("ADDEQ %d\n", b);
            b = 1; b <<= 4;         /* 16 -> 1 */
            printf("SHLEQ %d\n", b);
            b = 1; b |= 4;          /* 5 -> 1 */
            printf("OREQ %d\n", b);
            b = 1; b -= 1;          /* 0 */
            printf("SUBEQ %d\n", b);
        }
    )");
    EXPECT_EQ("INC 1\nDEC 1\nADDEQ 1\nSHLEQ 1\nOREQ 1\nSUBEQ 0\n", result);
}

// _Bool is one machine word: word-sized, word-aligned, word-strided.
TEST_F(CodegenTest, BoolIsOneWord)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        struct s { char c; _Bool b; };
        void program() {
            _Bool a[4];
            printf("SIZE %d ALIGN %d\n", (int)sizeof(_Bool), (int)_Alignof(_Bool));
            printf("ARRAY %d STRUCT %d\n", (int)sizeof a, (int)sizeof(struct s));
            printf("STRIDE %d\n", (int)((char *)&a[1] - (char *)&a[0]));
        }
    )");
    EXPECT_EQ("SIZE 6 ALIGN 6\nARRAY 24 STRUCT 12\nSTRIDE 6\n", result);
}

// The two special cases of the zero test: a fat char*/void*, whose null still carries a
// byte-offset marker and so must be reduced to its address word first, and a floating
// point value, which is bit-compared against a zero of its own kind.
TEST_F(CodegenTest, BoolFromPointerAndFloat)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        char buf[4];
        void program() {
            char *cp = 0;
            void *vp = buf;
            int *ip = 0;
            double d = 0.5;
            printf("NULLCHAR %d PTR %d NULLINT %d\n", (int)(_Bool)cp, (int)(_Bool)vp,
                   (int)(_Bool)ip);
            printf("HALF %d ZEROF %d\n", (int)(_Bool)d, (int)(_Bool)0.0);
        }
    )");
    EXPECT_EQ("NULLCHAR 0 PTR 1 NULLINT 0\nHALF 1 ZEROF 0\n", result);
}

// <stdbool.h> spells the same type `bool`, and a _Bool converts to a double the same way
// it converts to an int — through its normalized 0/1 value.
TEST_F(CodegenTest, BoolStdboolAndFloatConversion)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
#include <stdbool.h>
        bool flag = true;
        void program() {
            bool off = false;
            double d = flag;
            printf("FLAG %d OFF %d\n", flag, off);
            printf("DOUBLE %.1f\n", d);
            if (flag && !off)
                printf("LOGIC OK\n");
        }
    )");
    EXPECT_EQ("FLAG 1 OFF 0\nDOUBLE 1.0\nLOGIC OK\n", result);
}
