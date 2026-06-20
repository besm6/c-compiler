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
