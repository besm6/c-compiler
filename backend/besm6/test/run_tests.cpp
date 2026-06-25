#include "codegen_test.h"

TEST_F(CodegenTest, FuncArg1)
{
    std::string output = CompileToMadlen(R"(
        void foo(int bar);
        void quz() {
            foo(42);
        }
    )");
    EXPECT_EQ(R"(c
      quz:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save0
             ,xta, =52
          14 ,vtm, -1
             ,call, foo
             ,uj, b/ret
             ,end,
)",
              output);
}

TEST_F(CodegenTest, RunRev)
{
    char in_path[] = "/tmp/rev_input_XXXXXX";
    int in_fd      = mkstemp(in_path);
    ASSERT_GE(in_fd, 0);
    const char *input = "hello\nworld\nfoo\n";
    write(in_fd, input, strlen(input));
    close(in_fd);

    char out_path[] = "/tmp/rev_output_XXXXXX";
    int out_fd      = mkstemp(out_path);
    ASSERT_GE(out_fd, 0);
    close(out_fd);

    RunExternalProgram("rev", { in_path }, out_path);

    FILE *f = fopen(out_path, "r");
    ASSERT_NE(nullptr, f);
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    std::string got(static_cast<size_t>(len), '\0');
    fread(&got[0], 1, static_cast<size_t>(len), f);
    fclose(f);

    EXPECT_EQ(got, "olleh\ndlrow\noof\n");

    unlink(in_path);
    unlink(out_path);
}

TEST_F(CodegenTest, EmptyProgram)
{
    std::string result = CompileAndRun("void program() {}");
    EXPECT_EQ("", result);
}

TEST_F(CodegenTest, PrintChar)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void program() {
            putbyte('Q');
            putbyte('\n');
        }
    )");
    EXPECT_EQ("Q\n", result);
}

// putchar writes one byte and returns it; print the byte then its returned value.
TEST_F(CodegenTest, PutChar)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void program() {
            int r = putchar('Q');
            putchar('\n');
            printf("%d\n", r);
        }
    )");
    EXPECT_EQ("Q\n81\n", result);
}

TEST_F(CodegenTest, PrintDecimal)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void program() {
            printf("%d\n", 42);
        }
    )");
    EXPECT_EQ("42\n", result);
}

// Multi-character constants: a static global initializer (exercising the
// INIT_I64 static-data path) and an inline argument, both byte-packed.
//   'ab' = 0x6162 = 24930,  'cd' = 0x6364 = 25444
TEST_F(CodegenTest, MultiCharConstant)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        int g = 'ab';
        void program() {
            printf("%d\n", g);
            printf("%d\n", 'cd');
        }
    )");
    EXPECT_EQ("24930\n25444\n", result);
}

TEST_F(CodegenTest, PrintTwoLines)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void program() {
            printf("First line.\nSecond line.\n");
        }
    )");
    EXPECT_EQ("FIRST LINE.\nSECOND LINE.\n", result);
}

// End-to-end: a parameterless _Noreturn function (no b/save0 prologue) runs correctly
// on the simulator — it prints, then tail-calls _Noreturn exit().
TEST_F(CodegenTest, NoreturnNoParamsRun)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        #include <stdlib.h>
        _Noreturn void die(void) {
            printf("BYE\n");
            exit(0);
        }
        void program() {
            printf("HELLO\n");
            die();
        }
    )");
    // die() runs with the elided prologue (no b/save0), prints, then tail-calls exit().
    // exit() halts the job mid-stream, so the monitor's "*END FILE" marker is captured
    // in the listing (a normal return ends after the "----" separator and is truncated).
    EXPECT_EQ("HELLO\nBYE\n\n*END FILE\n", result);
}

// Float constant in a function-call argument.
// 1.5 as double → "=r1.5" literal.
TEST_F(CodegenTest, FuncArgFloat)
{
    std::string output = CompileToMadlen(R"(
        void foo(double x);
        void quz(void) { foo(1.5); }
    )");
    EXPECT_EQ(R"(c
      quz:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save0
             ,xta, =r1.5
          14 ,vtm, -1
             ,call, foo
             ,uj, b/ret
             ,end,
)",
              output);
}

TEST_F(CodegenTest, PrintFormatDecimal)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void program() {
            printf("foo = %d, bar = %d\n", 123, -456);
        }
    )");
    EXPECT_EQ("FOO = 123, BAR = -456\n", result);
}

TEST_F(CodegenTest, PrintFormatOctal)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void program() {
            printf("foo = %o, bar = %o\n", 123, -456);
        }
    )");
    EXPECT_EQ("FOO = 173, BAR = 37777777777070\n", result);
}

TEST_F(CodegenTest, PrintFormatString)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void program() {
            printf("hello %s\n", "world");
        }
    )");
    EXPECT_EQ("HELLO WORLD\n", result);
}

TEST_F(CodegenTest, PrintFormatChar)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void program() {
            printf("hello %c%c%c%c%c\n", '(', '-', '_', '-', ')');
        }
    )");
    EXPECT_EQ("HELLO (-_-)\n", result);
}

// A char*/void* null must test as false even when it carries the fat-pointer marker.
// Casting a null int* to char* (PTR_TO_CHAR_PTR) ORs in the marker bit, so the stored
// word is 0x6400000000000000 rather than all-zero.  Null tests (truthiness, ==/!=, !)
// must compare only the word-address part, so the marker-tagged null still reads as
// null; a real pointer (nonzero address, any byte offset) must read as non-null.
TEST_F(CodegenTest, FatPointerNullTest)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        int *ret_null(void) { return 0; }   // null int*, opaque to the optimizer
        void program() {
            char buf[6];
            char *nul = (char*)ret_null();   // marker-tagged null fat pointer
            char *p0  = buf;                 // real pointer, byte offset 0
            char *p3  = &buf[3];             // real pointer, byte offset within word
            printf("%c%c%c%c%c%c%c%c\n",
                nul ? 'T' : 'F', (nul == 0) ? 'E' : 'N', (nul != 0) ? 'N' : 'E', !nul ? 'Y' : 'F',
                p0 ? 'T' : 'F', (p0 == 0) ? 'E' : 'N',
                p3 ? 'T' : 'F', (p3 == 0) ? 'E' : 'N');
        }
    )");
    EXPECT_EQ("FEEYTNTN\n", result);
}

// Passing a null as a char*/void* argument must arrive as null in the callee.  A literal
// 0 / NULL is a null-pointer constant (lowered to COPY 0, all-zero), but passing a null
// int* to a void* parameter goes through the implicit int*->void* conversion
// (PTR_TO_CHAR_PTR), which marker-tags the null (0x6400000000000000) -- the same shape as
// free(nullintptr).  The callee's null test must still see it as null.
TEST_F(CodegenTest, FatPointerNullArgument)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        int *ret_null(void) { return 0; }      // null int*, opaque to the optimizer
        char check_charp(char *p) { return p ? 'T' : 'F'; }
        char check_voidp(void *p) { return (p == 0) ? 'E' : 'N'; }
        void program() {
            char buf[6];
            char a = check_charp(0);            // literal 0 -> char* param
            char b = check_voidp(0);            // literal 0 -> void* param
            char c = check_voidp(ret_null());   // null int* -> void* (marker-tagged)
            char d = check_charp(buf);          // real pointer -> char* param
            printf("%c%c%c%c\n", a, b, c, d);
        }
    )");
    EXPECT_EQ("FEET\n", result);
}

// TODO: re-enable once the BESM-6 basing/linking issue with malloc.c's
// module-global pointer statics is resolved (the job loops to the instruction
// cap with empty output; loader reports "long address" warnings).
TEST_F(CodegenTest, DISABLED_MallocHeapRoundTrip)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        #include <stdlib.h>
        #include <malloc.h>

        static long arena[200];

        void program() {
            heap_setup(arena, 200);
            long avail0 = heap_available();

            int *a = malloc(10 * sizeof(int));
            int *b = malloc(5 * sizeof(int));
            for (int i = 0; i < 10; i++) a[i] = i * i;
            int sum = 0;
            for (int i = 0; i < 10; i++) sum += a[i];
            printf("SUM %d\n", sum);

            free(a);
            int *c = malloc(8 * sizeof(int));
            printf("REUSE %d\n", c == a);

            free(b);
            free(c);
            printf("FULL %d\n", heap_available() == avail0);

            int *d = calloc(4, sizeof(int));
            printf("ZERO %d\n", d[0] + d[1] + d[2] + d[3]);
            d = realloc(d, 12 * sizeof(int));
            d[11] = 7;
            printf("REALLOC %d\n", d[11]);
            free(d);
        }
    )");
    EXPECT_EQ("SUM 285\nREUSE 1\nFULL 1\nZERO 0\nREALLOC 7\n", result);
}

// main() falling off the end gets an implicit `return 0;` (C11 §5.1.2.2.3), so
// its return value is a well-defined 0 rather than indeterminate accumulator
// garbage.  program() prints main()'s value.
TEST_F(CodegenTest, MainImplicitReturnZero)
{
    std::string result = CompileAndRun(
        "int printf(const char *format, ...);\n"
        "int main(void) { }\n"
        "void program(void) { printf(\"%d\\n\", main()); }\n");
    EXPECT_EQ("0\n", result);
}

// Same, with a non-empty body that still falls off the end.
TEST_F(CodegenTest, MainImplicitReturnZeroNonEmptyBody)
{
    std::string result = CompileAndRun(
        "int printf(const char *format, ...);\n"
        "int main(void) { int a = 3; a = a + 5; }\n"
        "void program(void) { printf(\"%d\\n\", main()); }\n");
    EXPECT_EQ("0\n", result);
}

// A local whose struct/union tag is declared *inside* the function body (BESM-6 backend
// TODO #62).  The tag is purged from structtab on block exit, so the translator can no
// longer resolve the type's size/alignment; validate_type caches both on the AST node while
// the tag is live, and get_size/get_alignment fall back to that cache.  Here dtoi
// reinterprets the bits of the double 1.0 as an int, and "%o" prints that word, so the
// expected output is the octal of 1.0's representation.
TEST_F(CodegenTest, LocalBlockUnionType)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        int dtoi(double d) {
            union u {
                double d;
                int i;
            };
            union u x;
            x.d = d;
            return x.i;
        }
        void program() {
            printf("%o\n", dtoi(1.0));
        }
    )");
    EXPECT_EQ("4050000000000000\n", result);
}

// A `static` local whose struct tag is declared inside the function body (TODO #62, the
// path the StaticLocalRec borrows a Type* through).  The static struct keeps its value
// across calls; each call prints the previous {a,b} then stores the new one.
TEST_F(CodegenTest, StaticLocalBlockStructType)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void bump(int a, int b) {
            struct s {
                int a;
                int b;
            };
            static struct s acc;
            printf("%d %d\n", acc.a, acc.b);
            acc.a = a;
            acc.b = b;
        }
        void program() {
            bump(1, 2);
            bump(3, 4);
            bump(5, 6);
        }
    )");
    EXPECT_EQ("0 0\n1 2\n3 4\n", result);
}
