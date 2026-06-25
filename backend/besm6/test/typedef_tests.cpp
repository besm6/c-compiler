// End-to-end run-tests for *global* (file-scope) typedefs.
//
// resolve_typedef_names() leaves a global typedef name in the type tree as a
// lightweight TYPE_TYPEDEF_NAME reference (only local typedefs are expanded), so
// every downstream type read must look through it via unalias().  These tests
// drive a global typedef through each audited path — integer promotion, pointer
// deref/subscript, struct member access, function-pointer call, typedef'd array,
// nested typedefs in pointers/structs — and check the runtime result, so a missed
// unalias surfaces as a wrong answer rather than passing silently.
//
// Output is KOI7 with case folding, so printed letters are UPPER CASE.
#include "codegen_test.h"

// A global typedef of a narrow integer type must still undergo integer promotion
// in arithmetic/shift expressions (the ->kind == TYPE_SHORT reads in expressions.c).
TEST_F(CodegenTest, TypedefShortPromotion)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        typedef short S;
        void program() {
            S a = 3;
            S b = 4;
            printf("%d\n", (a << 2) + b);
        }
    )");
    EXPECT_EQ("16\n", result);
}

// A global typedef of a pointer type: deref and subscript must look through the
// typedef to find the pointee (the u.pointer.target reads in expressions/translate).
TEST_F(CodegenTest, TypedefPointerDerefAndSubscript)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        typedef int *IP;
        void program() {
            int a[3];
            a[0] = 10; a[1] = 20; a[2] = 30;
            IP p = a;
            printf("%d %d\n", *p, p[2]);
        }
    )");
    EXPECT_EQ("10 30\n", result);
}

// A global typedef of a struct used directly: member access and whole-struct
// assignment must resolve the tag (the u.struct_t.name reads).
TEST_F(CodegenTest, TypedefStructMemberAndAssign)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        typedef struct { int x; int y; } Point;
        void program() {
            Point a;
            a.x = 7; a.y = 9;
            Point b = a;
            printf("%d\n", b.x + b.y);
        }
    )");
    EXPECT_EQ("16\n", result);
}

// A global typedef nested as a pointer-to-typedef and as a struct member type:
// the nested TYPE_TYPEDEF_NAME must be unaliased at every level.
TEST_F(CodegenTest, TypedefNestedPointerAndField)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        typedef int Word;
        typedef struct { Word w; } Box;
        void program() {
            Box b;
            b.w = 5;
            Box *bp = &b;
            Word *wp = &b.w;
            *wp = *wp + 1;
            printf("%d\n", bp->w);
        }
    )");
    EXPECT_EQ("6\n", result);
}

// A global typedef of an array type, including sizeof and indexing.
TEST_F(CodegenTest, TypedefArray)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        typedef int Vec[3];
        void program() {
            Vec v;
            v[0] = 1; v[1] = 2; v[2] = 3;
            printf("%d %d\n", v[0] + v[1] + v[2], (int)sizeof(Vec));
        }
    )");
    EXPECT_EQ("6 18\n", result);
}

// A global typedef of a function-pointer type, called indirectly.
TEST_F(CodegenTest, TypedefFunctionPointer)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        int add(int a, int b) { return a + b; }
        typedef int (*BinOp)(int, int);
        void program() {
            BinOp op = add;
            printf("%d\n", op(40, 2));
        }
    )");
    EXPECT_EQ("42\n", result);
}

// A chain of global typedefs (A -> B -> C) must resolve through every link.
TEST_F(CodegenTest, TypedefChain)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        typedef int A;
        typedef A B;
        typedef B C;
        void program() {
            C c = 21;
            printf("%d\n", c + c);
        }
    )");
    EXPECT_EQ("42\n", result);
}

// A *local* typedef must still be expanded eagerly (its typetab entry is purged on
// block exit); using it within its block and an unrelated global typedef after it
// must both behave correctly.
TEST_F(CodegenTest, TypedefLocalExpanded)
{
    std::string result = CompileAndRun(R"(
#include <stdio.h>
        typedef int G;
        void program() {
            {
                typedef int *LP;
                int n = 8;
                LP q = &n;
                *q = *q + 1;
                printf("%d\n", n);
            }
            G g = 33;
            printf("%d\n", g);
        }
    )");
    EXPECT_EQ("9\n33\n", result);
}
