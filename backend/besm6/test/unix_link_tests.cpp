//
// Assemble+link acceptance tests for the Unix (b6as) path — task U3.
//
// Each test compiles a chapter-style C program through the Unix dialect
// (genbesm --unix, run in-process by CompileToUnix), assembles it with `b6as`, and links
// it with `b6ld` against the U2 libc.a staged next to the besm-tests binary.  The
// CompileAndAssembleUnix helper (codegen_test.h) asserts every step exits 0; validation
// here is "assembles and links cleanly", not execution — running the linked b.out under
// b6sim is tasks U5/U6, which still need the libc syscall leaves + crt0.
//
// The programs reference only already-implemented libc symbols (the arithmetic/compare
// helpers, printf/puts, the string/mem family, the allocator) so the link resolves fully;
// an undefined reference makes b6ld exit non-zero and fails the test.
//
// SKIP_IF_NO_UNIX_TOOLS() skips a test when the sibling v7besm b6as/b6ld are not on PATH,
// keeping `make run` green on machines without that toolchain.
//
#include "codegen_test.h"

// Pure arithmetic exercising the multiply/divide/remainder runtime helpers.
TEST_F(CodegenTest, UnixLinkArithmetic)
{
    SKIP_IF_NO_UNIX_TOOLS();
    CompileAndAssembleUnix(WrapMain(R"(
int main(void) {
    int a = 20, b = 7;
    return a * b + a / b - a % b;
}
)"));
}

// Control flow: an if, a while, and a for loop with a running accumulator.
TEST_F(CodegenTest, UnixLinkControlFlow)
{
    SKIP_IF_NO_UNIX_TOOLS();
    CompileAndAssembleUnix(WrapMain(R"(
int main(void) {
    int sum = 0;
    for (int i = 0; i < 10; i = i + 1) {
        if (i % 2 == 0)
            sum = sum + i;
    }
    int n = sum;
    while (n > 0)
        n = n - 1;
    return sum;
}
)"));
}

// Pointer dereference and indexing through a local array.
TEST_F(CodegenTest, UnixLinkPointers)
{
    SKIP_IF_NO_UNIX_TOOLS();
    CompileAndAssembleUnix(WrapMain(R"(
int main(void) {
    int v[4];
    v[0] = 3; v[1] = 5; v[2] = 7; v[3] = 9;
    int *p = v;
    int s = 0;
    for (int i = 0; i < 4; i = i + 1)
        s = s + *(p + i);
    return s;
}
)"));
}

// A small struct passed by pointer, exercising member load/store.
TEST_F(CodegenTest, UnixLinkStruct)
{
    SKIP_IF_NO_UNIX_TOOLS();
    CompileAndAssembleUnix(WrapMain(R"(
struct point { int x; int y; };
int dist2(struct point *p) { return p->x * p->x + p->y * p->y; }
int main(void) {
    struct point q;
    q.x = 3; q.y = 4;
    return dist2(&q);
}
)"));
}

// puts to stdout — links the I/O leaf chain (puts -> putbyte).
TEST_F(CodegenTest, UnixLinkPuts)
{
    SKIP_IF_NO_UNIX_TOOLS();
    CompileAndAssembleUnix(R"(
int puts(const char *s);
void program(void) {
    puts("HELLO UNIX");
}
)");
}

// printf with a format directive — links the doprnt format engine.
TEST_F(CodegenTest, UnixLinkPrintf)
{
    SKIP_IF_NO_UNIX_TOOLS();
    CompileAndAssembleUnix(R"(
int printf(const char *format, ...);
void program(void) {
    printf("VALUE %d\n", 42);
}
)");
}

// The <string.h> family: strlen, strcpy, strcmp.
TEST_F(CodegenTest, UnixLinkStringOps)
{
    SKIP_IF_NO_UNIX_TOOLS();
    CompileAndAssembleUnix(WrapMain(R"(
unsigned long strlen(const char *s);
char *strcpy(char *d, const char *s);
int strcmp(const char *a, const char *b);
int main(void) {
    char buf[16];
    strcpy(buf, "ABCDE");
    return (int)strlen(buf) + strcmp(buf, "ABCDE");
}
)"));
}

// The dynamic allocator: malloc/free.
TEST_F(CodegenTest, UnixLinkMalloc)
{
    SKIP_IF_NO_UNIX_TOOLS();
    CompileAndAssembleUnix(WrapMain(R"(
void *malloc(unsigned long n);
void free(void *p);
int main(void) {
    int *p = malloc(4 * sizeof(int));
    p[0] = 11; p[3] = 22;
    int r = p[0] + p[3];
    free(p);
    return r;
}
)"));
}
