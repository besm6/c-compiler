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
