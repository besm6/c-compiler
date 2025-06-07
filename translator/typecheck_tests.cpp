//
// Below is a list of unit tests, each with a C code snippet,
// expected `symtab`, `typetab`, and AST type annotations.
// The tests are designed to cover scalar expressions, arithmetic
// promotions, pointers, arrays, structs, functions, initializers,
// and error conditions.
//
// Test Case 1: Simple Integer Variable Declaration and Expression
// Test Case 2: Struct Declaration and Member Access
// Test Case 3: Array and String Literal Initialization
// Test Case 4: Pointer Arithmetic and Dereference
// Test Case 5: Function Call with Parameters
// Test Case 6: Conditional Expression
// Test Case 7: Error - Duplicate Struct Declaration
// Test Case 8: Static Variable with Compound Initializer
// Test Case 9: Pointer to Struct and Arrow Operator
// Test Case 10: Error - Invalid Assignment
//
#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "translator.h"
#include "parser.h"
#include "scanner.h"
#include "internal.h"
#include "symtab.h"
#include "typetab.h"
#include "xalloc.h"
#include <fcntl.h>

// Test fixture
class TypecheckTest : public ::testing::Test {
    const std::string test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    FILE *input_file{};

public:
    Program *program{};

protected:
    void SetUp() override
    {
        auto filename = test_name + ".c";
        input_file = fopen(filename.c_str(), "w+");
        ASSERT_NE(nullptr, input_file);
        translator_debug = 1;
    }

    void TearDown() override
    {
        fclose(input_file);
        if (program) {
            free_program(program);
        }
        symtab_print();
        typetab_print();
        symtab_destroy();
        typetab_destroy();
        xreport_lost_memory();
        EXPECT_EQ(xtotal_allocated_size(), 0);
        xfree_all();
    }

    // Helper to create a temporary file with content
    FILE *CreateTempFile(const char *content)
    {
        fwrite(content, 1, strlen(content), input_file);
        rewind(input_file);
        return input_file;
    }

    // Helper to get external declaration from program
    void ParseProgram(const char *content)
    {
        program = parse(CreateTempFile(content));
        EXPECT_NE(nullptr, program);
        //print_program(stdout, program);
    }

    // Helper to get external declaration from program
    ExternalDecl *GetExternalDecl(const char *content)
    {
        ParseProgram(content);
        EXPECT_NE(nullptr, program->decls);
        return program->decls;
    }
};

//
// Test Case 1: Simple Integer Variable Declaration and Expression
// Tests a global variable declaration, integer addition, and function return.
//
// Expected Symtab:
// - x: {name="x", type=TYPE_INT, kind=SYM_STATIC, u.static_var={global=true, init_kind=INIT_INITIALIZED, init_list={kind=INIT_INT, u.int_val=42, next=NULL}}}
// - main: {name="main", type=TYPE_FUNCTION(return_type=TYPE_INT, params=NULL), kind=SYM_FUNC, u.func={defined=true, global=true}}
//
// Expected Typetab:
// - Empty (no structs).
//
// Expected AST Type Fields:
// - Program->decls[0] (var x):
//   - InitDeclarator->type: TYPE_INT
//   - Initializer->type: TYPE_INT (for 42)
//   - Initializer->u.expr->type: TYPE_INT
// - Program->decls[1] (function main):
//   - ExternalDecl->u.function.type: TYPE_FUNCTION(return_type=TYPE_INT, params=NULL)
//   - Stmt(STMT_RETURN)->u.expr->type: TYPE_INT (for x + 1)
//   - Expr(EXPR_BINARY_OP, ADD)->type: TYPE_INT
//   - Expr(EXPR_VAR, "x")->type: TYPE_INT
//   - Expr(EXPR_LITERAL, 1)->type: TYPE_INT
//
TEST_F(TypecheckTest, TypecheckIntVarExpr)
{
    ParseProgram(R"(
        int x = 42;
        int main() {
            return x + 1;
        }
    )");
    typecheck_program(program);

    // Check symbol x.
    const Symbol *x = symtab_get("x");
    ASSERT_NE(x, nullptr);
    EXPECT_STREQ(x->name, "x");
    EXPECT_EQ(x->kind, SYM_STATIC);
    EXPECT_TRUE(x->u.static_var.global);
    EXPECT_EQ(x->u.static_var.init_kind, INIT_INITIALIZED);

    ASSERT_NE(x->type, nullptr);
    EXPECT_EQ(x->type->kind, TYPE_INT);

    StaticInitializer *init = x->u.static_var.init_list;
    ASSERT_NE(init, nullptr);
    EXPECT_EQ(init->kind, INIT_INT);
    EXPECT_EQ(init->u.int_val, 42);
    EXPECT_EQ(init->next, nullptr);

    // Check symbol main.
    const Symbol *main = symtab_get("main");
    ASSERT_NE(main, nullptr);
    EXPECT_STREQ(main->name, "main");
    EXPECT_EQ(main->kind, SYM_FUNC);
    EXPECT_TRUE(main->u.func.defined);
    EXPECT_TRUE(main->u.func.global);

    ASSERT_NE(main->type, nullptr);
    EXPECT_EQ(main->type->kind, TYPE_FUNCTION);
    EXPECT_EQ(main->type->u.function.params, nullptr);
    EXPECT_FALSE(main->type->u.function.variadic);

    const Type *type = main->type->u.function.return_type;
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->kind, TYPE_INT);

    // Check types in AST.
    ExternalDecl *ext0 = program->decls;
    ExternalDecl *ext1 = ext0->next;
    ASSERT_NE(ext0, nullptr);
    ASSERT_NE(ext1, nullptr);
    EXPECT_EQ(ext1->next, nullptr);
    EXPECT_EQ(ext0->kind, EXTERNAL_DECL_DECLARATION);
    EXPECT_EQ(ext1->kind, EXTERNAL_DECL_FUNCTION);

    // Program->decls[0] (var x):
    Declaration *decl0 = ext0->u.declaration;
    EXPECT_EQ(decl0->kind, DECL_VAR);
    EXPECT_EQ(decl0->u.var.declarators->type->kind, TYPE_INT);
    EXPECT_STREQ(decl0->u.var.declarators->name, "x");
    EXPECT_EQ(decl0->u.var.declarators->init, nullptr);

    // Program->decls[1] (function main):
    EXPECT_EQ(ext1->u.function.type->kind, TYPE_FUNCTION);
    EXPECT_EQ(ext1->u.function.body->kind, STMT_COMPOUND);
    EXPECT_EQ(ext1->u.function.body->u.compound->kind, DECL_OR_STMT_STMT);

    Stmt *stmt = ext1->u.function.body->u.compound->u.stmt;
    EXPECT_EQ(stmt->kind, STMT_RETURN);

    Expr *expr = stmt->u.expr;
    EXPECT_EQ(expr->type->kind, TYPE_INT);
    EXPECT_EQ(expr->kind, EXPR_BINARY_OP);
    EXPECT_EQ(expr->u.binary_op.op, BINARY_ADD);
    EXPECT_EQ(expr->u.binary_op.left->type->kind, TYPE_INT);
    EXPECT_EQ(expr->u.binary_op.right->type->kind, TYPE_INT);
}

//
// Test Case 2: Struct Declaration and Member Access
// Tests struct definition, initialization, and dot operator.
//
// Expected Symtab:
// - p: {name="p", type=TYPE_STRUCT("Point"), kind=SYM_STATIC, u.static_var={global=true, init_kind=INIT_INITIALIZED, init_list=[{kind=INIT_INT, u.int_val=1}, {kind=INIT_ZERO, u.zero_bytes=4}, {kind=INIT_DOUBLE, u.double_val=2.0}]}}
// - get_y: {name="get_y", type=TYPE_FUNCTION(return_type=TYPE_DOUBLE, params=NULL), kind=SYM_FUNC, u.func={defined=true, global=true}}
//
// Expected Typetab:
// - Point: {tag="Point", alignment=8, size=16, members=[{name="x", type=TYPE_INT, offset=0}, {name="y", type=TYPE_DOUBLE, offset=8}], member_count=2}
//
// Expected AST Type Fields:
// - Program->decls[0] (struct Point):
//   - Declaration->u.var.specifiers->type: TYPE_STRUCT("Point")
// - Program->decls[1] (var p):
//   - InitDeclarator->type: TYPE_STRUCT("Point")
//   - Initializer->type: TYPE_STRUCT("Point")
//   - Initializer->u.items[0]->init->type: TYPE_INT (for 1)
//   - Initializer->u.items[0]->init->u.expr->type: TYPE_INT
//   - Initializer->u.items[1]->init->type: TYPE_DOUBLE (for 2.0)
//   - Initializer->u.items[1]->init->u.expr->type: TYPE_DOUBLE
// - Program->decls[2] (function get_y):
//   - ExternalDecl->u.function.type: TYPE_FUNCTION(return_type=TYPE_DOUBLE, params=NULL)
//   - Stmt(STMT_RETURN)->u.expr->type: TYPE_DOUBLE (for p.y)
//   - Expr(EXPR_FIELD_ACCESS, "y")->type: TYPE_DOUBLE
//   - Expr(EXPR_VAR, "p")->type: TYPE_STRUCT("Point")
//
TEST_F(TypecheckTest, TypecheckStructDeclMemberAccess)
{
    ParseProgram(R"(
        struct Point {
            int x;
            double y;
        };
        struct Point p = {1, 2.0};
        double get_y() {
            return p.y;
        }
    )");
    typecheck_program(program);
    //TODO
}

//
// Test Case 3: Array and String Literal Initialization
// Tests array initialization with a string literal and subscript operation.
//
// Expected Symtab:
// - _str0: {name="_str0", type=TYPE_ARRAY(element=TYPE_CHAR, size=6), kind=SYM_CONST, u.const_init={kind=INIT_STRING, u.string_val={str="hello", null_terminated=true}, next=NULL}}
// - str: {name="str", type=TYPE_ARRAY(element=TYPE_CHAR, size=6), kind=SYM_STATIC, u.static_var={global=true, init_kind=INIT_INITIALIZED, init_list={kind=INIT_STRING, u.string_val={str="hello", null_terminated=true}, next=NULL}}}
// - main: {name="main", type=TYPE_FUNCTION(return_type=TYPE_INT, params=NULL), kind=SYM_FUNC, u.func={defined=true, global=true}}
//
// Expected Typetab:
// - Empty.
//
// Expected AST Type Fields:
// - Program->decls[0] (var str):
//   - InitDeclarator->type: TYPE_ARRAY(element=TYPE_CHAR, size=6)
//   - Initializer->type: TYPE_ARRAY(element=TYPE_CHAR, size=6)
//   - Initializer->u.expr->type: TYPE_ARRAY(element=TYPE_CHAR, size=6)
// - Program->decls[1] (function main):
//   - ExternalDecl->u.function.type: TYPE_FUNCTION(return_type=TYPE_INT, params=NULL)
//   - Stmt(STMT_RETURN)->u.expr->type: TYPE_CHAR (for str[0])
//   - Expr(EXPR_SUBSCRIPT)->type: TYPE_CHAR
//   - Expr(EXPR_VAR, "str")->type: TYPE_POINTER(target=TYPE_CHAR) (array-to-pointer)
//   - Expr(EXPR_LITERAL, 0)->type: TYPE_LONG (index converted to long)
//
TEST_F(TypecheckTest, TypecheckArrayStringLiteralInit)
{
    ParseProgram(R"(
        char str[] = "hello";
        int main() {
            return str[0];
        }
    )");
    typecheck_program(program);
    //TODO
}

//
// Test Case 4: Pointer Arithmetic and Dereference
// Tests array-to-pointer conversion, pointer initialization, and dereference.
//
// Expected Symtab:
// - arr: {name="arr", type=TYPE_ARRAY(element=TYPE_INT, size=5), kind=SYM_STATIC, u.static_var={global=true, init_kind=INIT_INITIALIZED, init_list=[{kind=INIT_INT, u.int_val=1}, {kind=INIT_INT, u.int_val=2}, {kind=INIT_INT, u.int_val=3}, {kind=INIT_INT, u.int_val=4}, {kind=INIT_INT, u.int_val=5}]}}
// - ptr: {name="ptr", type=TYPE_POINTER(target=TYPE_INT), kind=SYM_STATIC, u.static_var={global=true, init_kind=INIT_INITIALIZED, init_list={kind=INIT_POINTER, u.ptr_id="arr", next=NULL}}}
// - main: {name="main", type=TYPE_FUNCTION(return_type=TYPE_INT, params=NULL), kind=SYM_FUNC, u.func={defined=true, global=true}}
//
// Expected Typetab:
// - Empty.
//
// Expected AST Type Fields:
// - Program->decls[0] (var arr):
//   - InitDeclarator->type: TYPE_ARRAY(element=TYPE_INT, size=5)
//   - Initializer->type: TYPE_ARRAY(element=TYPE_INT, size=5)
//   - Initializer->u.items[0..4]->init->type: TYPE_INT
//   - Initializer->u.items[0..4]->init->u.expr->type: TYPE_INT
// - Program->decls[1] (var ptr):
//   - InitDeclarator->type: TYPE_POINTER(target=TYPE_INT)
//   - Initializer->type: TYPE_POINTER(target=TYPE_INT)
//   - Initializer->u.expr->type: TYPE_POINTER(target=TYPE_INT) (for arr)
// - Program->decls[2] (function main):
//   - ExternalDecl->u.function.type: TYPE_FUNCTION(return_type=TYPE_INT, params=NULL)
//   - Stmt(STMT_RETURN)->u.expr->type: TYPE_INT (for *(ptr + 1))
//   - Expr(EXPR_DEREF)->type: TYPE_INT
//   - Expr(EXPR_BINARY_OP, ADD)->type: TYPE_POINTER(target=TYPE_INT)
//   - Expr(EXPR_VAR, "ptr")->type: TYPE_POINTER(target=TYPE_INT)
//   - Expr(EXPR_LITERAL, 1)->type: TYPE_LONG
//
TEST_F(TypecheckTest, PointerArithmeticDereference)
{
    ParseProgram(R"(
        int arr[5] = {1, 2, 3, 4, 5};
        int *ptr = arr;
        int main() {
            return *(ptr + 1);
        }
    )");
    typecheck_program(program);
    //TODO
}

//
// Test Case 5: Function Call with Parameters
// Tests function declaration, parameter passing, and arithmetic promotion.
//
// Expected Symtab:
// - add: {name="add", type=TYPE_FUNCTION(return_type=TYPE_INT, params=[TYPE_INT, TYPE_DOUBLE]), kind=SYM_FUNC, u.func={defined=true, global=true}}
// - main: {name="main", type=TYPE_FUNCTION(return_type=TYPE_INT, params=NULL), kind=SYM_FUNC, u.func={defined=true, global=true}}
//
// Expected Typetab:
// - Empty.
//
// Expected AST Type Fields:
// - Program->decls[0] (function add):
//   - ExternalDecl->u.function.type: TYPE_FUNCTION(return_type=TYPE_INT, params=[TYPE_INT, TYPE_DOUBLE])
//   - Stmt(STMT_RETURN)->u.expr->type: TYPE_INT
//   - Expr(EXPR_BINARY_OP, ADD)->type: TYPE_DOUBLE (common type)
//   - Expr(EXPR_VAR, "a")->type: TYPE_INT
//   - Expr(EXPR_VAR, "b")->type: TYPE_DOUBLE
//   - Expr(EXPR_CAST)->type: TYPE_INT (result cast to return type)
// - Program->decls[1] (function main):
//   - ExternalDecl->u.function.type: TYPE_FUNCTION(return_type=TYPE_INT, params=NULL)
//   - Stmt(STMT_RETURN)->u.expr->type: TYPE_INT (for add(1, 2.0))
//   - Expr(EXPR_CALL)->type: TYPE_INT
//   - Expr(EXPR_VAR, "add")->type: TYPE_FUNCTION(return_type=TYPE_INT, params=[TYPE_INT, TYPE_DOUBLE])
//   - Expr(EXPR_LITERAL, 1)->type: TYPE_INT
//   - Expr(EXPR_LITERAL, 2.0)->type: TYPE_DOUBLE
//
TEST_F(TypecheckTest, FunctionCallWithParameters)
{
    ParseProgram(R"(
        int add(int a, double b) {
            return a + b;
        }
        int main() {
            return add(1, 2.0);
        }
    )");
    typecheck_program(program);
    //TODO
}

//
// Test Case 6: Conditional Expression
// Tests local variable, conditional expression, and common type resolution.
//
// Expected Symtab:
// - main: {name="main", type=TYPE_FUNCTION(return_type=TYPE_INT, params=NULL), kind=SYM_FUNC, u.func={defined=true, global=true}}
// - x: {name="x", type=TYPE_INT, kind=SYM_LOCAL, u={}}
//
// Expected Typetab:
// - Empty.
//
// Expected AST Type Fields:
// - Program->decls[0] (function main):
//   - ExternalDecl->u.function.type: TYPE_FUNCTION(return_type=TYPE_INT, params=NULL)
//   - DeclOrStmt(DECL)->u.decl->u.var.declarators->type: TYPE_INT (for x)
//   - Initializer->type: TYPE_INT
//   - Initializer->u.expr->type: TYPE_INT (for 1)
//   - Stmt(STMT_RETURN)->u.expr->type: TYPE_INT (for x ? 10 : 20)
//   - Expr(EXPR_COND)->type: TYPE_INT
//   - Expr(EXPR_VAR, "x")->type: TYPE_INT
//   - Expr(EXPR_LITERAL, 10)->type: TYPE_INT
//   - Expr(EXPR_LITERAL, 20)->type: TYPE_INT
//
TEST_F(TypecheckTest, ConditionalExpression)
{
    ParseProgram(R"(
        int main() {
            int x = 1;
            return x ? 10 : 20;
        }
    )");
    typecheck_program(program);
    //TODO
}

//
// Test Case 7: Error - Duplicate Struct Declaration
// Tests error handling for duplicate struct definitions.
//
// Expected Symtab:
// - Empty (error occurs before any symbols).
//
// Expected Typetab:
// - Empty (error occurs during typetab_add_struct).
//
// Expected AST Type Fields:
// - N/A (program terminates with error: "Structure S was already declared").
//
TEST_F(TypecheckTest, DuplicateStructDeclaration)
{
    ParseProgram(R"(
        struct S { int x; };
        struct S { int y; };
    )");
    typecheck_program(program);
    //TODO
}

//
// Test Case 8: Static Variable with Compound Initializer
// Tests static variable with struct initializer.
//
// Expected Symtab:
// - s: {name="s", type=TYPE_STRUCT("S"), kind=SYM_STATIC, u.static_var={global=false, init_kind=INIT_INITIALIZED, init_list=[{kind=INIT_INT, u.int_val=1}, {kind=INIT_INT, u.int_val=2}]}}
//
// Expected Typetab:
// - S: {tag="S", alignment=4, size=8, members=[{name="x", type=TYPE_INT, offset=0}, {name="y", type=TYPE_INT, offset=4}], member_count=2}
//
// Expected AST Type Fields:
// - Program->decls[0] (struct S):
//   - Declaration->u.var.specifiers->type: TYPE_STRUCT("S")
// - Program->decls[1] (var s):
//   - InitDeclarator->type: TYPE_STRUCT("S")
//   - Initializer->type: TYPE_STRUCT("S")
//   - Initializer->u.items[0]->init->type: TYPE_INT (for 1)
//   - Initializer->u.items[0]->init->u.expr->type: TYPE_INT
//   - Initializer->u.items[1]->init->type: TYPE_INT (for 2)
//   - Initializer->u.items[1]->init->u.expr->type: TYPE_INT
//
TEST_F(TypecheckTest, StaticVariableCompoundInitializer)
{
    ParseProgram(R"(
        struct S { int x; int y; };
        static struct S s = {1, 2};
    )");
    typecheck_program(program);
    //TODO
}

//
// Test Case 9: Pointer to Struct and Arrow Operator
// Tests struct pointer, address-of operator, and arrow operator.
//
// Expected Symtab:
// - s: {name="s", type=TYPE_STRUCT("S"), kind=SYM_STATIC, u.static_var={global=true, init_kind=INIT_INITIALIZED, init_list={kind=INIT_INT, u.int_val=42}}}
// - p: {name="p", type=TYPE_POINTER(target=TYPE_STRUCT("S")), kind=SYM_LOCAL, u={}}
// - main: {name="main", type=TYPE_FUNCTION(return_type=TYPE_INT, params=NULL), kind=SYM_FUNC, u.func={defined=true, global=true}}
//
// Expected Typetab:
// - S: {tag="S", alignment=4, size=4, members=[{name="x", type=TYPE_INT, offset=0}], member_count=1}
//
// Expected AST Type Fields:
// - Program->decls[0] (struct S):
//   - Declaration->u.var.specifiers->type: TYPE_STRUCT("S")
// - Program->decls[1] (var s):
//   - InitDeclarator->type: TYPE_STRUCT("S")
//   - Initializer->type: TYPE_STRUCT("S")
//   - Initializer->u.items[0]->init->type: TYPE_INT
//   - Initializer->u.items[0]->init->u.expr->type: TYPE_INT
// - Program->decls[2] (function main):
//   - ExternalDecl->u.function.type: TYPE_FUNCTION(return_type=TYPE_INT, params=NULL)
//   - DeclOrStmt(DECL)->u.decl->u.var.declarators->type: TYPE_POINTER(target=TYPE_STRUCT("S"))
//   - Initializer->type: TYPE_POINTER(target=TYPE_STRUCT("S"))
//   - Initializer->u.expr->type: TYPE_POINTER(target=TYPE_STRUCT("S")) (for &s)
//   - Expr(EXPR_UNARY_OP, UNARY_ADDRESS)->type: TYPE_POINTER(target=TYPE_STRUCT("S"))
//   - Expr(EXPR_VAR, "s")->type: TYPE_STRUCT("S")
//   - Stmt(STMT_RETURN)->u.expr->type: TYPE_INT (for p->x)
//   - Expr(EXPR_PTR_ACCESS, "x")->type: TYPE_INT
//   - Expr(EXPR_VAR, "p")->type: TYPE_POINTER(target=TYPE_STRUCT("S"))
//
TEST_F(TypecheckTest, PointerStructArrowOperator)
{
    ParseProgram(R"(
        struct S { int x; };
        struct S s = {42};
        int main() {
            struct S *p = &s;
            return p->x;
        }
    )");
    typecheck_program(program);
    //TODO
}

//
// Test Case 10: Error - Invalid Assignment
// Tests error handling for type mismatch in assignment.
//
// Expected Symtab:
// - main: {name="main", type=TYPE_FUNCTION(return_type=TYPE_INT, params=NULL), kind=SYM_FUNC, u.func={defined=true, global=true}}
// - x: {name="x", type=TYPE_INT, kind=SYM_LOCAL, u={}}
//
// Expected Typetab:
// - Empty.
//
// Expected AST Type Fields:
// - N/A (terminates with error: "Cannot convert type for assignment" in convert_by_assignment).
//
TEST_F(TypecheckTest, InvalidAssignment)
{
    ParseProgram(R"(
        int main() {
            int x = 1;
            x = "string";
        }
    )");
    typecheck_program(program);
    //TODO
}
