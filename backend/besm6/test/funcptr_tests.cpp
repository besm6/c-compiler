// Function-pointer code generation and end-to-end execution tests.
//
// Indirect calls compile to a WTC of the pointer's frame slot (C = target address) followed
// by a bare `13 ,vjm,` (VJM jumps to offset + C).  A function name used as a value decays to
// its label address via GET_ADDRESS (UTC/VTM/ITA), not a load of the function's first word.
// Direct calls remain `,call, name`.  These tests exercise both the instruction selection
// (CompileToMadlen) and the runtime behaviour under Dubna (CompileAndRun).

#include "codegen_test.h"

// ---------------------------------------------------------------------------
// Madlen-level: instruction selection
// ---------------------------------------------------------------------------

// Indirect call through a function-pointer parameter: WTC of the param slot + VJM,
// never a `,call,` to the pointer name.
TEST_F(CodegenTest, FuncPtrIndirectCallViaParam)
{
    std::string output = CompileToMadlen(R"(
        int apply(int (*op)(int, int), int x, int y) {
            return op(x, y);
        }
    )");
    EXPECT_NE(output.find(",wtc,"), std::string::npos) << output;
    EXPECT_NE(output.find("13 ,vjm,"), std::string::npos) << output;
    EXPECT_EQ(output.find(",call, op"), std::string::npos) << output;
    EXPECT_EQ(output.find(",call, *op"), std::string::npos) << output;
}

// Explicit deref `(*op)(...)` strips the DEREF of a function pointer, so it still compiles
// to a single WTC + VJM with no extra LOAD of the pointer (which would be a second `,wtc,`).
TEST_F(CodegenTest, FuncPtrIndirectCallViaDeref)
{
    std::string output = CompileToMadlen(R"(
        int apply(int (*op)(int, int), int x, int y) {
            return (*op)(x, y);
        }
    )");
    EXPECT_NE(output.find("13 ,vjm,"), std::string::npos) << output;
    EXPECT_EQ(output.find(",call, op"), std::string::npos) << output;
    EXPECT_EQ(output.find(",call, *op"), std::string::npos) << output;
    // Exactly one WTC (the indirect call); a stray LOAD of op would add another.
    size_t first = output.find(",wtc,");
    ASSERT_NE(first, std::string::npos) << output;
    EXPECT_EQ(output.find(",wtc,", first + 1), std::string::npos) << output;
}

// A function name used as a call argument decays to its address: GET_ADDRESS emits
// VTM <name>/ITA, not a UTC+XTA that would load the function's first code word.
TEST_F(CodegenTest, FuncPtrNameDecaysToAddress)
{
    std::string output = CompileToMadlen(R"(
        int add(int a, int b) { return a + b; }
        int apply(int (*op)(int, int), int x, int y);
        int call_add(int x, int y) {
            return apply(add, x, y);
        }
    )");
    // call_add takes the address of add (ITA after VTM add) and calls apply directly.
    EXPECT_NE(output.find("14 ,vtm, add"), std::string::npos) << output;
    EXPECT_EQ(output.find(",xta, add"), std::string::npos) << output;
    EXPECT_NE(output.find(",ita,"), std::string::npos) << output;
    EXPECT_NE(output.find(",call, apply"), std::string::npos) << output;
}

// ---------------------------------------------------------------------------
// Run-level: end-to-end behaviour under Dubna
// ---------------------------------------------------------------------------

// Assign a function to a pointer, then call through it.
TEST_F(CodegenTest, FuncPtrAssignAndCall)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        int add(int a, int b) { return a + b; }
        void program() {
            int (*fp)(int, int) = add;
            printf("%d\n", fp(10, 3));
        }
    )");
    EXPECT_EQ("13\n", result);
}

// Pass a function pointer as an argument (the classic apply pattern), two callees.
TEST_F(CodegenTest, FuncPtrPassAsArgument)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        int add(int a, int b) { return a + b; }
        int sub(int a, int b) { return a - b; }
        int apply(int (*op)(int, int), int x, int y) { return op(x, y); }
        void program() {
            printf("%d\n", apply(add, 10, 3));
            printf("%d\n", apply(sub, 10, 3));
        }
    )");
    EXPECT_EQ("13\n7\n", result);
}

// Select a callee at runtime into a single pointer, then call.
TEST_F(CodegenTest, FuncPtrRuntimeDispatch)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        int add(int a, int b) { return a + b; }
        int sub(int a, int b) { return a - b; }
        int dispatch(int which, int x, int y) {
            int (*fp)(int, int);
            if (which)
                fp = add;
            else
                fp = sub;
            return fp(x, y);
        }
        void program() {
            printf("%d\n", dispatch(1, 20, 5));
            printf("%d\n", dispatch(0, 20, 5));
        }
    )");
    EXPECT_EQ("25\n15\n", result);
}

// Explicit dereference call (*fp)(...) returns the same as fp(...).
TEST_F(CodegenTest, FuncPtrExplicitDerefCall)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        int sub(int a, int b) { return a - b; }
        void program() {
            int (*fp)(int, int) = sub;
            printf("%d\n", (*fp)(20, 5));
        }
    )");
    EXPECT_EQ("15\n", result);
}

// Array of function pointers, indexed dispatch.
TEST_F(CodegenTest, FuncPtrArrayDispatch)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        int add(int a, int b) { return a + b; }
        int sub(int a, int b) { return a - b; }
        void program() {
            int (*tab[2])(int, int);
            tab[0] = add;
            tab[1] = sub;
            printf("%d\n", tab[0](10, 4));
            printf("%d\n", tab[1](10, 4));
        }
    )");
    EXPECT_EQ("14\n6\n", result);
}

// A function that returns a function pointer; the result is then called.
TEST_F(CodegenTest, FuncPtrReturnedFromFunction)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        int add(int a, int b) { return a + b; }
        int sub(int a, int b) { return a - b; }
        int (*pick(int which))(int, int) {
            if (which)
                return add;
            return sub;
        }
        void program() {
            printf("%d\n", pick(1)(8, 2));
            printf("%d\n", pick(0)(8, 2));
        }
    )");
    EXPECT_EQ("10\n6\n", result);
}

// Function-pointer value semantics: comparing a pointer against a function name
// (which decays to the same address) controls a branch.
TEST_F(CodegenTest, FuncPtrEqualityCompare)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        int add(int a, int b) { return a + b; }
        int sub(int a, int b) { return a - b; }
        void program() {
            int (*fp)(int, int) = add;
            printf("%d\n", fp == add);
            printf("%d\n", fp == sub);
        }
    )");
    EXPECT_EQ("1\n0\n", result);
}

// Call a 3-argument target through a pointer: verifies the negative arg count (r14)
// and stacked arguments match a direct call.
TEST_F(CodegenTest, FuncPtrThreeArgs)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        int combine(int a, int b, int c) { return a * 100 + b * 10 + c; }
        void program() {
            int (*fp)(int, int, int) = combine;
            printf("%d\n", fp(1, 2, 3));
        }
    )");
    EXPECT_EQ("123\n", result);
}

// ---------------------------------------------------------------------------
// Variadic calls through a function pointer
// ---------------------------------------------------------------------------

// A variadic call through a pointer keeps the indirect-call shape: the args (incl. the
// variadic ones) are pushed, the negative arg count goes into r14 (14 ,vtm, -N), and the
// call is a 13 ,vjm, after a wtc of the pointer slot — never a ,call, to the pointer name.
TEST_F(CodegenTest, FuncPtrVariadicCallMadlen)
{
    std::string output = CompileToMadlen(R"(
        #include <stdio.h>
        void program() {
            int (*fp)(char *, ...) = printf;
            fp("%d %d\n", 1, 2);
        }
    )");
    EXPECT_NE(output.find("13 ,vjm,"), std::string::npos) << output;
    EXPECT_NE(output.find("14 ,vtm, -"), std::string::npos) << output;
    EXPECT_EQ(output.find(",call, fp"), std::string::npos) << output;
}

// End-to-end: assign printf (known-working, observable) to a variadic function pointer and
// call through it.  Exercises pushing the variadic args and the negative arg count for an
// indirect target.
TEST_F(CodegenTest, FuncPtrVariadicCall)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        void program() {
            int (*fp)(char *, ...) = printf;
            fp("%d %d\n", 1, 2);
        }
    )");
    EXPECT_EQ("1 2\n", result);
}

// ---------------------------------------------------------------------------
// Struct-by-value through a function pointer.
//
// A multi-word struct returned by value uses the hidden-pointer (sret) ABI, lowered in
// the translator: the caller passes the address of the result slot as an implicit first
// argument and the callee writes the struct through it.  Returning a struct works for
// both indirect (here) and direct calls.
//
// Passing a multi-word struct *as an argument* by value uses the true by-value ABI: the
// translator marshals the struct as N consecutive machine-word arguments and the callee
// reserves N contiguous param slots, so the negative arg count in r14 and the param
// layout agree.
// ---------------------------------------------------------------------------

// Pass a 2-member struct by value through a function pointer: both words are marshalled
// (14 ,vtm, -2 for a 2-word struct), so the callee reads both members.  Output: "7\n".
TEST_F(CodegenTest, FuncPtrStructByValueArg)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        struct P { int x; int y; };
        int sum(struct P p) { return p.x + p.y; }
        void program() {
            struct P q;
            q.x = 3;
            q.y = 4;
            int (*fp)(struct P) = sum;
            printf("%d\n", fp(q));
        }
    )");
    EXPECT_EQ("7\n", result);
}

// Returning a 2-member struct by value through a function pointer: the sret ABI carries
// both words into the caller's slot.  Output: "3 4\n".
TEST_F(CodegenTest, FuncPtrStructByValueReturn)
{
    std::string result = CompileAndRun(R"(
        #include <stdio.h>
        struct P { int x; int y; };
        struct P mk(int a, int b) { struct P p; p.x = a; p.y = b; return p; }
        void program() {
            struct P (*fp)(int, int) = mk;
            struct P r = fp(3, 4);
            printf("%d %d\n", r.x, r.y);
        }
    )");
    EXPECT_EQ("3 4\n", result);
}
