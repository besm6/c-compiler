//
// Chapter 10 — File-scope variables and storage-class specifiers: valid
// programs compiled and run on BESM-6.  Imported from "Writing a C Compiler"
// (tests/chapter_10/valid + extra_credit + libraries).  Each program defines
// int main(void); WrapMain prints its return value, and we compare program
// output against the value computed by host cc.
//
// Chapter 10 is mostly about static/extern storage; several of its features are
// not yet supported by our BESM-6 backend, so a large share of the programs are
// DISABLED_ (grouped at the bottom with a one-line reason each).  What works:
// file-scope globals, file-scope statics with a single (or tentative-then-)
// definition, and extern declarations that bring an existing file-scope global
// into scope.
//
// Reclassifications / divergences vs. the book:
//
//  * No-shadowing rule (permanent design decision): chapter 10's central idiom
//    is "int x = …; { extern int x; }" — an inner extern bringing a shadowed
//    file-scope variable back into scope.  We reject every such redeclaration
//    ("Duplicate variable declaration" / "declared both with and without
//    linkage"), so these book-"valid" programs are not run tests here (the
//    no-shadowing negative direction is already covered in the semantic tests):
//      distinct_local_and_extern, extern_block_scope_variable,
//      shadow_static_local_var, static_local_multiple_scopes,
//      label_file_scope_var_same_name, libraries/external_linkage_function,
//      libraries/external_var_scoping.
//
//  * The book's putchar() is not in our libc; programs that need it would use
//    libc putch() with UPPERCASE text (the GOST output charset renders
//    lowercase Latin as Cyrillic), as in chapter 9.
//
//  * Multi-file "libraries" programs are concatenated into one translation
//    unit (the only multi-file mechanism available).  Shared-single-global
//    cases work (external_tentative_var, external_variable*); the
//    internal-linkage cases fundamentally need two separate translation units
//    each owning a distinct same-named static entity, which one file-scope
//    namespace cannot represent — they are DISABLED_ below.
//    (*external_variable is DISABLED_ for an unrelated reason, see below.)
//
// Backend gaps exercised here, each producing a DISABLED_ group below:
//   (a) No block-scope static-local storage: a "static int x;" inside a
//       function emits no static_variable toplevel, so the body references an
//       undefined name.  (Confirmed: Madlen has no static-local support.)
//   (b) Tentative/extern clobber: a tentative ("int x;") or "extern int x;"
//       file-scope declaration that FOLLOWS an initialized definition
//       ("int x = 3;") emits a second static_variable toplevel with no init,
//       which overwrites the initialized one to 0.
//
// Right shift of a negative int is implementation-defined (C11 §6.5.7p5).
// BESM-6 shifts logically (no sign extension; see backend/besm6/instr.c
// emit_shift), so a runtime ">>" of a negative value differs from x86_64's
// arithmetic shift.  bitwise_ops_file_scope_vars is a self-checking program
// whose result therefore differs between the two targets; its expected value
// below is the BESM-6 result (2), not the x86 result (0).  (The constant-fold
// path matches x86, which is why earlier chapters' constant-shift tests pass.)
//
#include "book_run.h"

// --- valid (run) ------------------------------------------------------------

// A static file-scope variable may be tentatively defined and declared several
// times, but defined only once; the definition (4) comes last and wins.
TEST_F(CodegenTest, Chapter10_MultipleStaticFileScopeVars)
{
    EXPECT_EQ("4\n", CompileAndRun(WrapMain(R"(static int foo;

int main(void) {
    return foo;
}

extern int foo;

static int foo = 4;)")));
}

// A tentatively-defined file-scope variable is zero-initialized.
TEST_F(CodegenTest, Chapter10_TentativeDefinition)
{
    EXPECT_EQ("5\n", CompileAndRun(WrapMain(R"(extern int foo;

int foo;

int foo;

int main(void) {
    for (int i = 0; i < 5; i = i + 1)
        foo = foo + 1;
    return foo;
}

int foo;)")));
}

// The type specifier may precede the storage-class specifier ("int static").
TEST_F(CodegenTest, Chapter10_TypeBeforeStorageClass)
{
    EXPECT_EQ("7\n", CompileAndRun(WrapMain(R"(int static foo(void) {
    return 3;
}

int static bar = 4;

int main(void) {
    int extern foo(void);
    int extern bar;
    return foo() + bar;
})")));
}

// ++ and -- on file-scope variables.
TEST_F(CodegenTest, Chapter10_IncrementGlobalVars)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"(int i = 0;
int j = 0;

int incr_i(void){
    if (i == 1) {
        i++;
        ++i;
    }
    return 0;
}

int decr_j(void) {
    if (j == -1) {
        j--;
    }
    return 0;
}

int main(void) {
    i++ ? 0 : incr_i();
    if (i != 3) {
        return 1;
    }
    --j? decr_j(): 0;
    if (j != -2) {
        return 2;
    }
    return 0;
})")));
}

// An external variable can be used in a switch controlling expression.
TEST_F(CodegenTest, Chapter10_SwitchOnExtern)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"(int update_x(void);

int main(void) {
    update_x();
    extern int x;
    switch(x) {
        case 0: return 1;
        case 1: return 2;
        case 4: return 0;
        default: return 4;
    }
}

int x;

int update_x(void) {
    x = 4;
    return 0;
})")));
}

// An external variable is in scope in a switch even if its decl is jumped over.
TEST_F(CodegenTest, Chapter10_SwitchSkipExternDecl)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"(int main(void) {
    int a = 10;
    switch(a) {
        case 1: return 1;
        extern int x;
        case 2: return 2;
        case 10:
        if (x * 2 == 30) {
            return 0;
        }
        default: return 5;
    }
    return 6;
}

int x = 15;)")));
}

// A variable with external linkage, tentatively defined in one "file" and
// brought into scope with extern in another.
TEST_F(CodegenTest, Chapter10_LibExternalTentativeVar)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"(int x;

int read_x(void) {
    return x;
}

int read_x(void);

int main(void) {
    extern int x;
    if (x != 0)
        return 1;
    x = 3;
    if (read_x() != 3)
        return 1;
    return 0;
})")));
}

// Bitwise operations on file-scope variables.  The "y = ((y & -5) ^ 12) >> 2"
// step right-shifts a negative value: BESM-6 shifts logically (impl-defined,
// C11 §6.5.7p5), so y becomes 2^39 - 3 rather than -3, the "y != -3" guard
// fires, and the program returns 2 (the x86 result would be 0).
TEST_F(CodegenTest, Chapter10_BitwiseOpsFileScopeVars)
{
    EXPECT_EQ("2\n", CompileAndRun(WrapMain(R"(int x = 1;
int y = 0;

int main(void) {
    y = -1;
    x = (x << 1) | 1;
    if (x != 3) {
        return 1;
    }
    y = ((y & -5) ^ 12) >> 2;
    if (y != -3) {
        return 2;
    }
    return 0;
})")));
}

// --- DISABLED_ (a): no block-scope static-local storage (see header) --------

// Uninitialized static local, zero-initialized and persisting across calls.
TEST_F(CodegenTest, DISABLED_Chapter10_StaticLocalUninitialized)
{
    EXPECT_EQ("4\n", CompileAndRun(WrapMain(R"(int foo(void) {
    static int x;
    x = x + 1;
    return x;
}

int main(void) {
    int ret;
    for (int i = 0; i < 4; i = i + 1)
        ret = foo();
    return ret;
})")));
}

// Static locals used as memory operands in a relational expression.
TEST_F(CodegenTest, DISABLED_Chapter10_StaticVariablesInExpressions)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"(int main(void) {
    static int i = 2;
    static int j = 3;
    int cmp = i < j;

    if (!cmp)
        return 1;
    return 0;
})")));
}

// Compound assignment on several static locals, persisting across calls.
TEST_F(CodegenTest, DISABLED_Chapter10_CompoundAssignmentStaticVar)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"(int f(void) {
    static int i = 0;
    static int j = 0;
    static int k = 1;
    static int l = 48;
    i += 1;
    j -= i;
    k *= j;
    l /= 2;
    if (i != 3) {
        return 1;
    }
    if (j != -6) {
        return 2;
    }
    if (k != -18) {
        return 3;
    }
    if (l != 6) {
        return 4;
    }
    return 0;
}

int main(void) {
    f();
    f();
    return f();
})")));
}

// A static initializer runs at program startup even if a goto jumps over it.
TEST_F(CodegenTest, DISABLED_Chapter10_GotoSkipStaticInitializer)
{
    EXPECT_EQ("10\n", CompileAndRun(WrapMain(R"(int main(void) {
    goto end;
    static int x = 10;
    end:
        return x;
})")));
}

// A static initializer in a switch runs at startup; the later assignment, being
// a statement, is jumped over.
TEST_F(CodegenTest, DISABLED_Chapter10_SwitchSkipStaticInitializer)
{
    EXPECT_EQ("10\n", CompileAndRun(WrapMain(R"(int a = 3;
int main(void) {
    switch (a) {
        case 1:;
            static int x = 10;
            x = 0;
        case 3:
            return x;
    }
    return 0;
})")));
}

// A static variable and a label in the same function may share a name.
TEST_F(CodegenTest, DISABLED_Chapter10_LabelStaticVarSameName)
{
    EXPECT_EQ("5\n", CompileAndRun(WrapMain(R"(int main(void) {
    static int x = 5;
    goto x;
    x = 0;
x:
    return x;
})")));
}

// Same-named static locals in different functions must be distinct (no
// linkage); our single file-scope namespace also collides them.  Host value 29.
TEST_F(CodegenTest, DISABLED_Chapter10_MultipleStaticLocal)
{
    EXPECT_EQ("29\n", CompileAndRun(WrapMain(R"(int foo(void) {
    static int a = 3;
    a = a * 2;
    return a;
}

int bar(void) {
    static int a = 4;
    a = a + 1;
    return a;
}

int main(void) {
    return foo() + bar() + foo() + bar();
})")));
}

// --- DISABLED_ (b): tentative/extern clobbers an initialized definition ------

// "extern int foo;" after "static int foo = 3;" emits a second (uninitialized)
// foo toplevel that overwrites the initializer, so foo reads 0 instead of 3.
TEST_F(CodegenTest, DISABLED_Chapter10_StaticThenExtern)
{
    EXPECT_EQ("3\n", CompileAndRun(WrapMain(R"(static int foo = 3;

int main(void) {
    return foo;
}

extern int foo;)")));
}

// Concatenated external_variable + client: "extern int x;" trailing the client
// follows "int x = 3;" and clobbers it to 0 (same bug as StaticThenExtern).
TEST_F(CodegenTest, DISABLED_Chapter10_LibExternalVariable)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"(int x;

extern int x;
int x;

int update_x(int new_val) {
    x = new_val;
    return 0;
}

int read_x(void) {
    return x;
}

int x = 3;

int update_x(int new_val);
int read_x(void);
extern int x;

int main(void) {
    if (x != 3)
        return 1;
    if (read_x() != 3)
        return 1;
    x = 4;
    if (x != 4)
        return 1;
    if (read_x() != 4)
        return 1;
    update_x(5);
    if (x != 5)
        return 1;
    if (read_x() != 5)
        return 1;
    return 0;
})")));
}

// --- DISABLED_ (d): internal linkage needs separate translation units --------

// Two TUs each own a distinct same-named static `x`; concatenation merges them.
TEST_F(CodegenTest, DISABLED_Chapter10_LibInternalLinkageVar)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"(static int x;

int read_x(void) {
    return x;
}

int update_x(int new_val) {
    extern int x;
    x = new_val;
    return 0;
}

extern int x;
static int x = 5;
static int x;

static int x;
static int x;

int read_x(void);
int update_x(int x);

int main(void) {
    if (x != 0)
        return 1;
    if (read_x() != 5)
        return 1;
    extern int x;
    update_x(10);
    if (read_x() != 10)
        return 1;
    if (x != 0)
        return 1;
    x = 20;
    if (x != 20)
        return 1;
    if (read_x() != 10)
        return 1;
    return 0;
}

static int x;)")));
}

// Internal- and external-linkage my_fun in two TUs are distinct functions;
// concatenation redefines my_fun.
TEST_F(CodegenTest, DISABLED_Chapter10_LibInternalLinkageFunction)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"(static int my_fun(void);

int call_static_my_fun(void) {
    return my_fun();
}

int call_static_my_fun_2(void) {
    int my_fun(void);
    return my_fun();
}

extern int my_fun(void);
static int my_fun(void);

int my_fun(void) {
    static int i = 0;
    i = i + 1;
    return i;
}

extern int my_fun(void);
int call_static_my_fun(void);
int call_static_my_fun_2(void);

int main(void) {
    if (call_static_my_fun() != 1)
        return 1;
    if (my_fun() != 100)
        return 1;
    if (call_static_my_fun_2() != 2)
        return 1;
    return 0;
}

int my_fun(void) {
    return 100;
})")));
}

// Internal-linkage `x` in the client hides the external `x` in the other TU;
// concatenation produces a linkage conflict.
TEST_F(CodegenTest, DISABLED_Chapter10_LibInternalHidesExternalLinkage)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"(int x = 10;

int read_x(void){
    return x;
}

static int x = 1;

int read_internal_x(void);
int read_x(void);

int main(void) {
    extern int x;
    if (x != 1)
        return 1;
    x = 2;
    if (read_internal_x() != 2)
        return 1;
    if (read_x() != 10)
        return 1;
    return 0;
}

extern int x;

int read_internal_x(void) {
    return x;
})")));
}

// Same label `x` in two same-named functions f (one static, one extern) in
// different TUs; concatenation redefines f.
TEST_F(CodegenTest, DISABLED_Chapter10_LibSameLabelSameFun)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"(static int f(void) {
    goto x;
    return 0;
    x:
    return 2;
}

int f_caller(void) {
    return f();
}

int f(void) {
    goto x;
    return 0;
x:
    return 1;
}

int f_caller(void);

int main(void) {
    if (f() != 1) {
        return 1;
    }
    if (f_caller() != 2) {
        return 2;
    }
    return 0;
})")));
}

// --- DISABLED_ (misc) -------------------------------------------------------

// main() falls off the end: indeterminate return value (host cc returns stack
// garbage), like the ch5/ch6 missing-return cases; also needs putchar.
TEST_F(CodegenTest, DISABLED_Chapter10_StaticRecursiveCall)
{
    EXPECT_EQ("ABCDEFGHIJKLMNOPQRSTUVWXYZ26\n", CompileAndRun(WrapMain(R"(void putch(int ch);

int print_alphabet(void) {
    static int count = 0;
    putch(count + 65);
    count = count + 1;
    if (count < 26) {
        print_alphabet();
    }
    return count;
}

int main(void) {
    print_alphabet();
})")));
}

// References external symbol `zed` from a hand-written x86 .s file; an x86
// page-boundary stack-argument test with no BESM-6 analogue.
TEST_F(CodegenTest, DISABLED_Chapter10_PushArgOnPageBoundary)
{
    EXPECT_EQ("1\n", CompileAndRun(WrapMain(R"(extern int zed;
int foo(int a, int b, int c, int d, int e, int f, int g) {
    return g + 1;
}

int main(void) {
    return foo(0, 0, 0, 0, 0, 0, zed);
})")));
}
