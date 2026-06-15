#include "codegen_test.h"

//
// Fat-pointer char access (task #21).
//
// char*/void* are fat pointers: bit 48 set (marker), bits 47-45 the byte offset,
// bits 15-1 the word address.  Dereferencing extracts one byte (WTC/XTA/ASX/AAX);
// storing read-modify-writes the containing word via the b/stb helper; taking the
// address of a char sets the fat marker; int*->char* sets marker + offset 5.
//

// &c where c is a char yields a fat pointer: the GET_ADDRESS sets bit 48 (offset 0).
TEST_F(CodegenTest, GetAddressCharSetsFatMarker)
{
    DisableOptimization();
    std::string output = CompileToMadlen("char c; char *p; void f(void){ p = &c; }");
    EXPECT_NE(output.find(",aox, =4000000000000000"), std::string::npos) << output;
}

// Dereferencing a char* emits the byte-load sequence wtc / xta / asx / aax =377.
TEST_F(CodegenTest, LoadCharEmitsByteSequence)
{
    DisableOptimization();
    std::string output = CompileToMadlen("char c; int f(void){ char *p = &c; return *p; }");
    EXPECT_NE(output.find(",wtc,"), std::string::npos) << output;
    EXPECT_NE(output.find(",asx,"), std::string::npos) << output;
    EXPECT_NE(output.find(",aax, =377"), std::string::npos) << output;
}

// Storing through a char* calls the b/stb runtime helper.
TEST_F(CodegenTest, StoreCharCallsStb)
{
    DisableOptimization();
    std::string output = CompileToMadlen("char c; void f(void){ char *p = &c; *p = 'Q'; }");
    EXPECT_NE(output.find(",call, b/stb"), std::string::npos) << output;
}

// int*->char* cast sets the fat marker plus offset 5 (points at the first/MSB byte).
TEST_F(CodegenTest, IntPtrToCharPtrSetsOffset5)
{
    DisableOptimization();
    std::string output =
        CompileToMadlen("char *f(int *q){ return (char*)q; }");
    EXPECT_NE(output.find(",aox, =6400000000000000"), std::string::npos) << output;
}

// Regression: an int* load still uses the plain word sequence (no byte machinery).
TEST_F(CodegenTest, LoadIntStillWord)
{
    DisableOptimization();
    std::string output = CompileToMadlen("int v; int f(void){ int *p = &v; return *p; }");
    EXPECT_EQ(output.find(",wtc,"), std::string::npos) << output;
    EXPECT_EQ(output.find(",asx,"), std::string::npos) << output;
}

// Runtime: store and load a byte through a fat pointer at offset 0 (standalone char).
TEST_F(CodegenTest, CharStoreLoadRoundtrip)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        void program() {
            char c;
            char *p = &c;
            *p = 'Q';
            writeb(*p);
            writeb('\n');
        }
    )");
    EXPECT_EQ("Q\n", result);
}

// Runtime: compound assignment through a char* (byte load + byte store).
TEST_F(CodegenTest, CharCompoundAssign)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        void program() {
            char c;
            char *p = &c;
            *p = 'A';
            *p += 1;
            writeb(*p);
            writeb('\n');
        }
    )");
    EXPECT_EQ("B\n", result);
}

// Runtime: post-increment through a char* returns the old byte and stores the new one.
TEST_F(CodegenTest, CharPostIncrement)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        void program() {
            char c;
            char *p = &c;
            *p = 'A';
            char old = (*p)++;
            writeb(old);
            writeb(*p);
            writeb('\n');
        }
    )");
    EXPECT_EQ("AB\n", result);
}

// Runtime: int*->char* cast yields a fat pointer at offset 5 (the int's MSB byte).
// Storing and loading through it exercises the byte shift by 40 bits (offset 5).
TEST_F(CodegenTest, CharCastOffset5Roundtrip)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        void program() {
            int x = 0;
            char *p = (char*)&x;
            *p = 'Z';
            writeb(*p);
            writeb('\n');
        }
    )");
    EXPECT_EQ("Z\n", result);
}

//
// char* arithmetic & packed char members (task #22).
//
// Pointer ± integer on a char*/void* adjusts the 3-bit byte offset of the fat pointer
// (ADD_PTR scale 1): a constant ±1 uses b/pinc / b/pdec, any other delta uses b/padd.
// A char array / string literal decays to a fat pointer at byte#0 (offset_enc 5).  Packed
// char struct members are read with a byte extract and written via the b/stb helper.
//

// Runtime: index a local char array (decay + ADD_PTR scale 1 via b/padd).
TEST_F(CodegenTest, CharArrayIndexReadWrite)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        void program() {
            char a[6];
            a[0] = 'H';
            a[5] = '!';
            writeb(a[0]);
            writeb(a[5]);
            writeb('\n');
        }
    )");
    EXPECT_EQ("H!\n", result);
}

// Runtime: a string literal decays to a char* at its first byte (MSB); deref walks it.
TEST_F(CodegenTest, StringDecayDeref)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        void program() {
            char *s = "HI";
            writeb(*s);
            writeb(*(s + 1));
            writeb('\n');
        }
    )");
    EXPECT_EQ("HI\n", result);
}

// Runtime: char*++ across a word boundary (b/pinc offset_enc 0 -> 5 with word carry).
TEST_F(CodegenTest, CharPtrIncWordBoundary)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        void program() {
            char a[8];
            for (int i = 0; i < 7; i++)
                a[i] = 'A' + i;
            char *p = a;
            p += 5;          /* a[5] = 'F' */
            writeb(*p);
            p++;             /* crosses into the next word: a[6] = 'G' */
            writeb(*p);
            writeb('\n');
        }
    )");
    EXPECT_EQ("FG\n", result);
}

// Runtime: char*-- across a word boundary backward (b/pdec offset_enc 5 -> 0).
TEST_F(CodegenTest, CharPtrDecWordBoundary)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        void program() {
            char a[8];
            for (int i = 0; i < 7; i++)
                a[i] = 'A' + i;
            char *p = a;
            p += 6;          /* a[6] = 'G' */
            writeb(*p);
            p--;             /* a[5] = 'F' (crosses back a word) */
            writeb(*p);
            writeb('\n');
        }
    )");
    EXPECT_EQ("GF\n", result);
}

// Runtime: char* + n with a carry across a word (b/padd floored division by 6).
TEST_F(CodegenTest, CharPtrPlusNCarry)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        void program() {
            char a[12];
            for (int i = 0; i < 10; i++)
                a[i] = '0' + i;
            char *p = a;
            p = p + 8;       /* a[8] = '8' */
            writeb(*p);
            writeb('\n');
        }
    )");
    EXPECT_EQ("8\n", result);
}

// Runtime: char* - n with a negative byte delta (b/padd floored division, negative path).
TEST_F(CodegenTest, CharPtrMinusN)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        void program() {
            char a[12];
            for (int i = 0; i < 10; i++)
                a[i] = '0' + i;
            char *p = a + 9; /* a[9] = '9' */
            p = p - 7;       /* a[2] = '2' */
            writeb(*p);
            writeb('\n');
        }
    )");
    EXPECT_EQ("2\n", result);
}

// Runtime: packed char struct members at byte offsets 0..5 plus a word member after them.
// The char at offset 0 has offset%6==0 but must still use byte access (driven by the flag).
TEST_F(CodegenTest, PackedStructCharMembers)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        struct S { char a; char b; char c; char d; char e; char f; int g; };
        void program() {
            struct S s;
            s.a = 'A'; s.b = 'B'; s.c = 'C';
            s.d = 'D'; s.e = 'E'; s.f = 'F';
            writeb(s.a); writeb(s.b); writeb(s.c);
            writeb(s.d); writeb(s.e); writeb(s.f);
            writeb('\n');
        }
    )");
    EXPECT_EQ("ABCDEF\n", result);
}

// Runtime: a subscript-array base and a decayed fat pointer agree on byte#0.
TEST_F(CodegenTest, MixedSubscriptAndDecayConsistency)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        void program() {
            char a[3];
            a[0] = 'X';
            char *p = a;
            writeb(p[0]);
            writeb(a[0]);
            writeb('\n');
        }
    )");
    EXPECT_EQ("XX\n", result);
}

// Shape: char* + i lowers to ADD_PTR scale 1 -> the b/padd helper.
TEST_F(CodegenTest, CharPtrPlusIntCallsPadd)
{
    DisableOptimization();
    std::string output = CompileToMadlen("char *f(char *p, int i){ return p + i; }");
    EXPECT_NE(output.find(",call, b/padd"), std::string::npos) << output;
}

// Shape: char*++ uses the b/pinc helper (constant +1 byte step).
TEST_F(CodegenTest, CharPtrIncCallsPinc)
{
    DisableOptimization();
    std::string inc = CompileToMadlen("char *f(char *p){ p++; return p; }");
    EXPECT_NE(inc.find(",call, b/pinc"), std::string::npos) << inc;
}

// Shape: char*-- uses the b/pdec helper (constant -1 byte step).
TEST_F(CodegenTest, CharPtrDecCallsPdec)
{
    DisableOptimization();
    std::string dec = CompileToMadlen("char *f(char *p){ p--; return p; }");
    EXPECT_NE(dec.find(",call, b/pdec"), std::string::npos) << dec;
}

// Shape: a char array / string decays to a fat pointer at offset_enc 5 (MSB byte#0).
TEST_F(CodegenTest, ArrayDecaySetsOffset5)
{
    DisableOptimization();
    std::string output = CompileToMadlen("char g[6]; char *f(void){ return g; }");
    EXPECT_NE(output.find(",aox, =6400000000000000"), std::string::npos) << output;
}

// Shape: reading a packed char member emits a byte extract (asx-style shift + mask =377).
TEST_F(CodegenTest, StructCharMemberReadIsByte)
{
    DisableOptimization();
    std::string output = CompileToMadlen(
        "struct S { char a; char b; int c; }; int f(struct S s){ return s.b; }");
    EXPECT_NE(output.find(",aax, =377"), std::string::npos) << output;
}

// Shape: writing a packed char member calls the b/stb read-modify-write helper.
TEST_F(CodegenTest, StructCharMemberWriteCallsStb)
{
    DisableOptimization();
    std::string output = CompileToMadlen(
        "struct S { char a; char b; int c; }; void f(struct S *s){ s->b = 'X'; }");
    EXPECT_NE(output.find(",call, b/stb"), std::string::npos) << output;
}

// Regression: a word pointer ++ stays on the inline integer path (no b/pinc / b/padd).
TEST_F(CodegenTest, WordPtrIncStaysInline)
{
    DisableOptimization();
    std::string output = CompileToMadlen("int *f(int *p){ p++; return p; }");
    EXPECT_EQ(output.find(",call, b/pinc"), std::string::npos) << output;
    EXPECT_EQ(output.find(",call, b/padd"), std::string::npos) << output;
}

// Regression: a word struct member stays a plain word load (no byte extract / b/stb).
TEST_F(CodegenTest, StructIntMemberStaysWord)
{
    DisableOptimization();
    std::string output = CompileToMadlen(
        "struct S { char a; char b; int c; }; int f(struct S s){ return s.c; }");
    EXPECT_EQ(output.find(",call, b/stb"), std::string::npos) << output;
}

// Runtime: a global char array decays and indexes correctly through b/padd.
TEST_F(CodegenTest, GlobalCharArrayIndexRun)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        char g[6];
        void program() {
            g[0] = 'P'; g[3] = 'Q';
            char *p = g;
            writeb(p[0]);
            writeb(p[3]);
            writeb('\n');
        }
    )");
    EXPECT_EQ("PQ\n", result);
}

// Runtime: compound += through a char* lvalue (load fat pointer, b/padd, store).
TEST_F(CodegenTest, CharPtrLvalueCompoundAdd)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        void program() {
            char a[6];
            for (int i = 0; i < 6; i++)
                a[i] = 'A' + i;
            char *p = a;
            char **pp = &p;
            *pp += 4;        /* p now points at a[4] = 'E' */
            writeb(*p);
            writeb('\n');
        }
    )");
    EXPECT_EQ("E\n", result);
}
