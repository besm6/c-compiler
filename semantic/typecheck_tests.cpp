//
// Below is a list of unit tests, each with a C code snippet,
// expected `symtab`, `structtab`, and AST type annotations.
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
#include <fcntl.h>
#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "internal.h"
#include "parser.h"
#include "scanner.h"
#include "semantic.h"
#include "structtab.h"
#include "symtab.h"
#include "xalloc.h"

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
        input_file    = fopen(filename.c_str(), "w+");
        ASSERT_NE(nullptr, input_file);
        semantic_debug = 1;
    }

    void TearDown() override
    {
        fclose(input_file);
        if (program) {
            free_program(program);
        }
        symtab_print();
        structtab_print();
        symtab_destroy();
        structtab_destroy();
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
        // print_program(stdout, program);
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
// - x: {name="x", type=TYPE_INT, kind=SYM_STATIC, u.static_var={global=true,
// init_kind=INIT_INITIALIZED, init_list={kind=TAC_STATIC_INIT_I32, u.int_val=42, next=NULL}}}
// - main: {name="main", type=TYPE_FUNCTION(return_type=TYPE_INT, params=NULL), kind=SYM_FUNC,
// u.func={defined=true, global=true}}
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

    Tac_StaticInit *init = x->u.static_var.init_list;
    ASSERT_NE(init, nullptr);
    EXPECT_EQ(init->kind, TAC_STATIC_INIT_I32);
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
// - p: {name="p", type=TYPE_STRUCT("Point"), kind=SYM_STATIC, u.static_var={global=true,
// init_kind=INIT_INITIALIZED, init_list=[{kind=TAC_STATIC_INIT_I32, u.int_val=1},
// {kind=TAC_STATIC_INIT_ZERO, u.zero_bytes=4}, {kind=TAC_STATIC_INIT_DOUBLE, u.double_val=2.0}]}}
// - get_y: {name="get_y", type=TYPE_FUNCTION(return_type=TYPE_DOUBLE, params=NULL), kind=SYM_FUNC,
// u.func={defined=true, global=true}}
//
// Expected Typetab:
// - Point: {tag="Point", alignment=8, size=16, members=[{name="x", type=TYPE_INT, offset=0},
// {name="y", type=TYPE_DOUBLE, offset=8}], member_count=2}
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

    // structtab: Point with int x@0, double y@8, size=16, align=8
    const StructDef *pt = structtab_find("Point");
    ASSERT_NE(pt, nullptr);
    EXPECT_EQ(pt->alignment, 8);
    EXPECT_EQ(pt->size, 16);
    ASSERT_NE(pt->members, nullptr);
    EXPECT_STREQ(pt->members->name, "x");
    EXPECT_EQ(pt->members->type->kind, TYPE_INT);
    EXPECT_EQ(pt->members->offset, 0);
    ASSERT_NE(pt->members->next, nullptr);
    EXPECT_STREQ(pt->members->next->name, "y");
    EXPECT_EQ(pt->members->next->type->kind, TYPE_DOUBLE);
    EXPECT_EQ(pt->members->next->offset, 8);
    EXPECT_EQ(pt->members->next->next, nullptr);

    // symtab: p
    const Symbol *p = symtab_get("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->kind, SYM_STATIC);
    EXPECT_TRUE(p->u.static_var.global);
    EXPECT_EQ(p->u.static_var.init_kind, INIT_INITIALIZED);
    ASSERT_NE(p->type, nullptr);
    EXPECT_EQ(p->type->kind, TYPE_STRUCT);

    // symtab: get_y
    const Symbol *get_y = symtab_get("get_y");
    ASSERT_NE(get_y, nullptr);
    EXPECT_EQ(get_y->kind, SYM_FUNC);
    EXPECT_TRUE(get_y->u.func.defined);
    EXPECT_TRUE(get_y->u.func.global);
    ASSERT_NE(get_y->type, nullptr);
    EXPECT_EQ(get_y->type->u.function.return_type->kind, TYPE_DOUBLE);

    // AST: field access p.y has type TYPE_DOUBLE
    ExternalDecl *ext2 = program->decls->next->next;
    ASSERT_NE(ext2, nullptr);
    EXPECT_EQ(ext2->kind, EXTERNAL_DECL_FUNCTION);
    Stmt *ret = ext2->u.function.body->u.compound->u.stmt;
    EXPECT_EQ(ret->kind, STMT_RETURN);
    Expr *field = ret->u.expr;
    EXPECT_EQ(field->kind, EXPR_FIELD_ACCESS);
    ASSERT_NE(field->type, nullptr);
    EXPECT_EQ(field->type->kind, TYPE_DOUBLE);
}

//
// Test Case 3: Array and String Literal Initialization
// Tests array initialization with a string literal and subscript operation.
//
// Expected Symtab:
// - _str0: {name="_str0", type=TYPE_ARRAY(element=TYPE_CHAR, size=6), kind=SYM_CONST,
// u.const_init={kind=TAC_STATIC_INIT_STRING, u.string_val={str="hello", null_terminated=true},
// next=NULL}}
// - str: {name="str", type=TYPE_ARRAY(element=TYPE_CHAR, size=6), kind=SYM_STATIC,
// u.static_var={global=true, init_kind=INIT_INITIALIZED, init_list={kind=TAC_STATIC_INIT_STRING,
// u.string_val={str="hello", null_terminated=true}, next=NULL}}}
// - main: {name="main", type=TYPE_FUNCTION(return_type=TYPE_INT, params=NULL), kind=SYM_FUNC,
// u.func={defined=true, global=true}}
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

    // TODO: check symtab, AST
}

//
// Test Case 4: Pointer Arithmetic and Dereference
// Tests array-to-pointer conversion, pointer initialization, and dereference.
//
// Expected Symtab:
// - arr: {name="arr", type=TYPE_ARRAY(element=TYPE_INT, size=5), kind=SYM_STATIC,
// u.static_var={global=true, init_kind=INIT_INITIALIZED, init_list=[{kind=TAC_STATIC_INIT_I32,
// u.int_val=1}, {kind=TAC_STATIC_INIT_I32, u.int_val=2}, {kind=TAC_STATIC_INIT_I32, u.int_val=3},
// {kind=TAC_STATIC_INIT_I32, u.int_val=4}, {kind=TAC_STATIC_INIT_I32, u.int_val=5}]}}
// - ptr: {name="ptr", type=TYPE_POINTER(target=TYPE_INT), kind=SYM_STATIC,
// u.static_var={global=true, init_kind=INIT_INITIALIZED, init_list={kind=TAC_STATIC_INIT_POINTER,
// u.ptr_id="arr", next=NULL}}}
// - main: {name="main", type=TYPE_FUNCTION(return_type=TYPE_INT, params=NULL), kind=SYM_FUNC,
// u.func={defined=true, global=true}}
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

    // TODO: check symtab, AST
}

//
// Test Case 5: Function Call with Parameters
// Tests function declaration, parameter passing, and arithmetic promotion.
//
// Expected Symtab:
// - add: {name="add", type=TYPE_FUNCTION(return_type=TYPE_INT, params=[TYPE_INT, TYPE_DOUBLE]),
// kind=SYM_FUNC, u.func={defined=true, global=true}}
// - main: {name="main", type=TYPE_FUNCTION(return_type=TYPE_INT, params=NULL), kind=SYM_FUNC,
// u.func={defined=true, global=true}}
//
// Expected Typetab:
// - Empty.
//
// Expected AST Type Fields:
// - Program->decls[0] (function add):
//   - ExternalDecl->u.function.type: TYPE_FUNCTION(return_type=TYPE_INT, params=[TYPE_INT,
//   TYPE_DOUBLE])
//   - Stmt(STMT_RETURN)->u.expr->type: TYPE_INT
//   - Expr(EXPR_BINARY_OP, ADD)->type: TYPE_DOUBLE (common type)
//   - Expr(EXPR_VAR, "a")->type: TYPE_INT
//   - Expr(EXPR_VAR, "b")->type: TYPE_DOUBLE
//   - Expr(EXPR_CAST)->type: TYPE_INT (result cast to return type)
// - Program->decls[1] (function main):
//   - ExternalDecl->u.function.type: TYPE_FUNCTION(return_type=TYPE_INT, params=NULL)
//   - Stmt(STMT_RETURN)->u.expr->type: TYPE_INT (for add(1, 2.0))
//   - Expr(EXPR_CALL)->type: TYPE_INT
//   - Expr(EXPR_VAR, "add")->type: TYPE_FUNCTION(return_type=TYPE_INT, params=[TYPE_INT,
//   TYPE_DOUBLE])
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

    // TODO: check symtab, AST
}

//
// Test Case 6: Conditional Expression
// Tests local variable, conditional expression, and common type resolution.
//
// Expected Symtab:
// - main: {name="main", type=TYPE_FUNCTION(return_type=TYPE_INT, params=NULL), kind=SYM_FUNC,
// u.func={defined=true, global=true}}
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

    // TODO: check symtab, AST
}

TEST_F(TypecheckTest, GenericExpressionTypeMatch)
{
    ParseProgram(R"(
        int main() {
            int x = 1;
            return _Generic(x, double: 0, int: 42);
        }
    )");
    typecheck_program(program);
}

TEST_F(TypecheckTest, GenericExpressionDefault)
{
    ParseProgram(R"(
        int main() {
            double x = 1.0;
            return _Generic(x, int: 0, default: 42);
        }
    )");
    typecheck_program(program);
}

TEST_F(TypecheckTest, CompoundLiteralArray)
{
    ParseProgram(R"(
        int f(void) {
            return ((int[1]){42})[0];
        }
    )");
    typecheck_program(program);
}

TEST_F(TypecheckTest, CompoundLiteralStruct)
{
    ParseProgram(R"(
        struct S { int x; int y; };
        int f(void) {
            struct S s = (struct S){1, 2};
            return s.x;
        }
    )");
    typecheck_program(program);
}

TEST_F(TypecheckTest, NestedStruct)
{
    ParseProgram(R"(
        struct foo {
            struct {
                int bar;
            } quz;
        };
    )");
    typecheck_program(program);
}

//
// Test Case 7: Error - Duplicate Struct Declaration
// Tests error handling for duplicate struct definitions.
//
// Expected Symtab:
// - Empty (error occurs before any symbols).
//
// Expected Typetab:
// - Empty (error occurs during structtab_add_struct).
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
    ASSERT_EXIT(typecheck_program(program), ::testing::ExitedWithCode(1),
                "Structure S was already declared");
}

//
// Test Case 8: Static Variable with Compound Initializer
// Tests static variable with struct initializer.
//
// Expected Symtab:
// - s: {name="s", type=TYPE_STRUCT("S"), kind=SYM_STATIC, u.static_var={global=false,
// init_kind=INIT_INITIALIZED, init_list=[{kind=TAC_STATIC_INIT_I32, u.int_val=1},
// {kind=TAC_STATIC_INIT_I32, u.int_val=2}]}}
//
// Expected Typetab:
// - S: {tag="S", alignment=4, size=8, members=[{name="x", type=TYPE_INT, offset=0}, {name="y",
// type=TYPE_INT, offset=4}], member_count=2}
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

    // structtab: S with int x@0, int y@4, size=8, align=4
    const StructDef *sd = structtab_find("S");
    ASSERT_NE(sd, nullptr);
    EXPECT_EQ(sd->alignment, 4);
    EXPECT_EQ(sd->size, 8);
    ASSERT_NE(sd->members, nullptr);
    EXPECT_STREQ(sd->members->name, "x");
    EXPECT_EQ(sd->members->offset, 0);
    ASSERT_NE(sd->members->next, nullptr);
    EXPECT_STREQ(sd->members->next->name, "y");
    EXPECT_EQ(sd->members->next->offset, 4);

    // symtab: s — static (not global), initialized
    const Symbol *s = symtab_get("s");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->kind, SYM_STATIC);
    EXPECT_FALSE(s->u.static_var.global);
    EXPECT_EQ(s->u.static_var.init_kind, INIT_INITIALIZED);
    ASSERT_NE(s->type, nullptr);
    EXPECT_EQ(s->type->kind, TYPE_STRUCT);

    Tac_StaticInit *init = s->u.static_var.init_list;
    ASSERT_NE(init, nullptr);
    EXPECT_EQ(init->kind, TAC_STATIC_INIT_I32);
    EXPECT_EQ(init->u.int_val, 1);
    ASSERT_NE(init->next, nullptr);
    EXPECT_EQ(init->next->kind, TAC_STATIC_INIT_I32);
    EXPECT_EQ(init->next->u.int_val, 2);
    EXPECT_EQ(init->next->next, nullptr);
}

//
// Test Case 9: Pointer to Struct and Arrow Operator
// Tests struct pointer, address-of operator, and arrow operator.
//
// Expected Symtab:
// - s: {name="s", type=TYPE_STRUCT("S"), kind=SYM_STATIC, u.static_var={global=true,
// init_kind=INIT_INITIALIZED, init_list={kind=TAC_STATIC_INIT_I32, u.int_val=42}}}
// - p: {name="p", type=TYPE_POINTER(target=TYPE_STRUCT("S")), kind=SYM_LOCAL, u={}}
// - main: {name="main", type=TYPE_FUNCTION(return_type=TYPE_INT, params=NULL), kind=SYM_FUNC,
// u.func={defined=true, global=true}}
//
// Expected Typetab:
// - S: {tag="S", alignment=4, size=4, members=[{name="x", type=TYPE_INT, offset=0}],
// member_count=1}
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

    // structtab: S with int x@0, size=4, align=4
    const StructDef *sd = structtab_find("S");
    ASSERT_NE(sd, nullptr);
    EXPECT_EQ(sd->alignment, 4);
    EXPECT_EQ(sd->size, 4);
    ASSERT_NE(sd->members, nullptr);
    EXPECT_STREQ(sd->members->name, "x");
    EXPECT_EQ(sd->members->offset, 0);
    EXPECT_EQ(sd->members->next, nullptr);

    // symtab: s
    const Symbol *s = symtab_get("s");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->kind, SYM_STATIC);
    EXPECT_TRUE(s->u.static_var.global);
    EXPECT_EQ(s->u.static_var.init_kind, INIT_INITIALIZED);
    Tac_StaticInit *init = s->u.static_var.init_list;
    ASSERT_NE(init, nullptr);
    EXPECT_EQ(init->kind, TAC_STATIC_INIT_I32);
    EXPECT_EQ(init->u.int_val, 42);

    // symtab: main
    const Symbol *main_sym = symtab_get("main");
    ASSERT_NE(main_sym, nullptr);
    EXPECT_EQ(main_sym->kind, SYM_FUNC);
    EXPECT_TRUE(main_sym->u.func.defined);

    // AST: arrow expression p->x has type TYPE_INT
    ExternalDecl *ext2 = program->decls->next->next;
    ASSERT_NE(ext2, nullptr);
    EXPECT_EQ(ext2->kind, EXTERNAL_DECL_FUNCTION);
    DeclOrStmt *ds = ext2->u.function.body->u.compound;
    ASSERT_NE(ds, nullptr);       // first: decl for p
    ASSERT_NE(ds->next, nullptr); // second: return stmt
    Stmt *ret = ds->next->u.stmt;
    EXPECT_EQ(ret->kind, STMT_RETURN);
    Expr *arrow = ret->u.expr;
    EXPECT_EQ(arrow->kind, EXPR_PTR_ACCESS);
    ASSERT_NE(arrow->type, nullptr);
    EXPECT_EQ(arrow->type->kind, TYPE_INT);
}

//
// Test Case 10: Error - Invalid Assignment
// Tests error handling for type mismatch in assignment.
//
// Expected Symtab:
// - main: {name="main", type=TYPE_FUNCTION(return_type=TYPE_INT, params=NULL), kind=SYM_FUNC,
// u.func={defined=true, global=true}}
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
    ASSERT_EXIT(typecheck_program(program), ::testing::ExitedWithCode(1),
                "Cannot convert type for assignment");
}

// ---------------------------------------------------------------------------
// PipelineTest — mirrors the lower main loop: per-decl typecheck
// ---------------------------------------------------------------------------

class PipelineTest : public TypecheckTest {
protected:
    void RunPipeline(const char *src)
    {
        ParseProgram(src);
        typecheck_program(program);
    }
};

// Struct definition through the full pipeline.
// Verifies that struct registration does not double-register in structtab.
TEST_F(PipelineTest, StructDecl)
{
    RunPipeline("struct S { int x; double y; };");

    const StructDef *sd = structtab_find("S");
    ASSERT_NE(sd, nullptr);
    EXPECT_EQ(sd->alignment, 8);
    EXPECT_EQ(sd->size, 16);
    ASSERT_NE(sd->members, nullptr);
    EXPECT_STREQ(sd->members->name, "x");
    EXPECT_EQ(sd->members->type->kind, TYPE_INT);
    EXPECT_EQ(sd->members->offset, 0);
    ASSERT_NE(sd->members->next, nullptr);
    EXPECT_STREQ(sd->members->next->name, "y");
    EXPECT_EQ(sd->members->next->type->kind, TYPE_DOUBLE);
    EXPECT_EQ(sd->members->next->offset, 8);
    EXPECT_EQ(sd->members->next->next, nullptr);
}

// Struct definition followed by a variable of that struct type.
TEST_F(PipelineTest, StructUsedInVar)
{
    RunPipeline("struct S { int x; }; struct S s;");

    EXPECT_TRUE(structtab_exists("S"));

    const Symbol *s = symtab_get("s");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->kind, SYM_STATIC);
    EXPECT_TRUE(s->u.static_var.global);
    EXPECT_EQ(s->u.static_var.init_kind, INIT_TENTATIVE);
    ASSERT_NE(s->type, nullptr);
    EXPECT_EQ(s->type->kind, TYPE_STRUCT);
}

// Function prototype followed by definition.
// Verifies that symtab_add_fun() sets has_linkage so the redeclaration
// does not fatal with "Duplicate declaration".
TEST_F(PipelineTest, FunctionPrototypeThenDefinition)
{
    RunPipeline("int f(void); int f(void) { return 1; }");

    const Symbol *f = symtab_get("f");
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->kind, SYM_FUNC);
    EXPECT_TRUE(f->u.func.defined);
    EXPECT_TRUE(f->u.func.global);
    ASSERT_NE(f->type, nullptr);
    EXPECT_EQ(f->type->kind, TYPE_FUNCTION);
}

// File-scope declaration with multiple declarators.
// Verifies that typecheck_file_scope_var_decl() loops over all InitDeclarators.
TEST_F(PipelineTest, FileVarMultipleDeclarators)
{
    RunPipeline("int x = 1, y = 2;");

    const Symbol *x = symtab_get("x");
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(x->kind, SYM_STATIC);
    EXPECT_TRUE(x->u.static_var.global);
    EXPECT_EQ(x->u.static_var.init_kind, INIT_INITIALIZED);
    ASSERT_NE(x->u.static_var.init_list, nullptr);
    EXPECT_EQ(x->u.static_var.init_list->kind, TAC_STATIC_INIT_I32);
    EXPECT_EQ(x->u.static_var.init_list->u.int_val, 1);

    const Symbol *y = symtab_get("y");
    ASSERT_NE(y, nullptr);
    EXPECT_EQ(y->kind, SYM_STATIC);
    EXPECT_TRUE(y->u.static_var.global);
    EXPECT_EQ(y->u.static_var.init_kind, INIT_INITIALIZED);
    ASSERT_NE(y->u.static_var.init_list, nullptr);
    EXPECT_EQ(y->u.static_var.init_list->kind, TAC_STATIC_INIT_I32);
    EXPECT_EQ(y->u.static_var.init_list->u.int_val, 2);
}

// Static and extern file-scope variables: linkage and init_kind.
TEST_F(PipelineTest, StaticAndExternVars)
{
    RunPipeline("static int a; extern int b;");

    const Symbol *a = symtab_get("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->kind, SYM_STATIC);
    EXPECT_FALSE(a->u.static_var.global);
    EXPECT_EQ(a->u.static_var.init_kind, INIT_TENTATIVE);

    const Symbol *b = symtab_get("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->kind, SYM_STATIC);
    EXPECT_TRUE(b->u.static_var.global);
    EXPECT_EQ(b->u.static_var.init_kind, INIT_NONE);
}

// ---------------------------------------------------------------------------
// Switch semantic validation (TypecheckTest)
// ---------------------------------------------------------------------------

// Float controlling expression is rejected.
TEST_F(TypecheckTest, SwitchFloatExpr)
{
    ParseProgram("double f(double x) { switch (x) {} return 0; }");
    ASSERT_EXIT(typecheck_program(program), ::testing::ExitedWithCode(1), "integer type");
}

// Pointer controlling expression is rejected.
TEST_F(TypecheckTest, SwitchPointerExpr)
{
    ParseProgram("int f(int *p) { switch (p) {} return 0; }");
    ASSERT_EXIT(typecheck_program(program), ::testing::ExitedWithCode(1), "integer type");
}

// Float case expression is rejected.
TEST_F(TypecheckTest, CaseFloatValue)
{
    ParseProgram("int f(int x) { switch (x) { case 1.5: break; } return 0; }");
    ASSERT_EXIT(typecheck_program(program), ::testing::ExitedWithCode(1), "integer type");
}

// Duplicate integer case values are rejected.
TEST_F(TypecheckTest, CaseDuplicate)
{
    ParseProgram("int f(int x) { switch (x) { case 1: break; case 1: break; } return 0; }");
    ASSERT_EXIT(typecheck_program(program), ::testing::ExitedWithCode(1), "Duplicate case value");
}

// char 'a' and integer 97 should be the same value — duplicate.
// Disabled: the parser stores all char literals as LITERAL_INT 0 via strtoul,
// so 'a' and 97 are not detected as equal until the parser is fixed.
TEST_F(TypecheckTest, CaseDuplicateCharAndInt)
{
    ParseProgram("int f(int x) { switch (x) { case 'a': break; case 97: break; } return 0; }");
    ASSERT_EXIT(typecheck_program(program), ::testing::ExitedWithCode(1), "Duplicate case value");
}

// Two default labels in one switch are rejected.
TEST_F(TypecheckTest, MultipleDefaultLabels)
{
    ParseProgram("int f(int x) { switch (x) { default: break; default: break; } return 0; }");
    ASSERT_EXIT(typecheck_program(program), ::testing::ExitedWithCode(1), "Multiple default");
}

// Basic switch with distinct integer case values is accepted.
TEST_F(TypecheckTest, SwitchIntBasic)
{
    ParseProgram("int f(int x) { switch (x) { case 0: break; case 1: break; } return 0; }");
    typecheck_program(program);
}

// Char controlling expression is accepted (gets integer-promoted).
TEST_F(TypecheckTest, SwitchCharExpr)
{
    ParseProgram("int f(char c) { switch (c) { case 'a': break; } return 0; }");
    typecheck_program(program);
}

// Switch with cases and a default label is accepted.
TEST_F(TypecheckTest, SwitchWithDefault)
{
    ParseProgram("int f(int x) { switch (x) { case 0: break; default: break; } return 0; }");
    typecheck_program(program);
}

// Switch with no cases is accepted.
TEST_F(TypecheckTest, SwitchEmptyBody)
{
    ParseProgram("int f(int x) { switch (x) {} return 0; }");
    typecheck_program(program);
}

// Nested switches each with case 1 are accepted — contexts are independent.
TEST_F(TypecheckTest, SwitchNestedContexts)
{
    ParseProgram(R"(
        int f(int x, int y) {
            switch (x) {
                case 1: switch (y) { case 1: break; } break;
            }
            return 0;
        }
    )");
    typecheck_program(program);
}

// ---------------------------------------------------------------------------
// LabelLoopsTest — full pipeline: typecheck + label_loops
// ---------------------------------------------------------------------------

class LabelLoopsTest : public PipelineTest {
protected:
    void RunLabelLoops(const char *src)
    {
        ParseProgram(src);
        typecheck_program(program);
    }
};

// Return the first statement in a function body.
static Stmt *fn_first_stmt(ExternalDecl *fn)
{
    return fn->u.function.body->u.compound->u.stmt;
}

// Return the first statement in a compound statement.
static Stmt *compound_first(Stmt *s)
{
    return s->u.compound->u.stmt;
}

// Return the second statement in a compound statement.
static Stmt *compound_second(Stmt *s)
{
    return s->u.compound->next->u.stmt;
}

// while(1){} gets end and continue labels.
TEST_F(LabelLoopsTest, WhileLoopLabels)
{
    RunLabelLoops("int f(void) { while (1) {} }");

    Stmt *ws = fn_first_stmt(program->decls);
    ASSERT_EQ(ws->kind, STMT_WHILE);
    EXPECT_STREQ(ws->loop_end_label, ".L0");
    EXPECT_STREQ(ws->loop_continue_label, ".L1");
}

// for(;;){} gets end and continue labels.
TEST_F(LabelLoopsTest, ForLoopLabels)
{
    RunLabelLoops("int f(void) { for (;;) {} }");

    Stmt *fs = fn_first_stmt(program->decls);
    ASSERT_EQ(fs->kind, STMT_FOR);
    EXPECT_STREQ(fs->loop_end_label, ".L0");
    EXPECT_STREQ(fs->loop_continue_label, ".L1");
}

// do{}while(1); gets end and continue labels.
TEST_F(LabelLoopsTest, DoWhileLoopLabels)
{
    RunLabelLoops("int f(void) { do {} while (1); }");

    Stmt *ds = fn_first_stmt(program->decls);
    ASSERT_EQ(ds->kind, STMT_DO_WHILE);
    EXPECT_STREQ(ds->loop_end_label, ".L0");
    EXPECT_STREQ(ds->loop_continue_label, ".L1");
}

// switch gets only an end label; continue label stays NULL.
TEST_F(LabelLoopsTest, SwitchLabels)
{
    RunLabelLoops("int f(void) { switch (1) {} }");

    Stmt *ss = fn_first_stmt(program->decls);
    ASSERT_EQ(ss->kind, STMT_SWITCH);
    EXPECT_STREQ(ss->loop_end_label, ".L0");
    EXPECT_EQ(ss->loop_continue_label, nullptr);
}

// break inside while targets the loop's end label.
TEST_F(LabelLoopsTest, BreakInWhile)
{
    RunLabelLoops("int f(void) { while (1) { break; } }");

    Stmt *ws  = fn_first_stmt(program->decls);
    Stmt *brk = compound_first(ws->u.while_stmt.body);
    ASSERT_EQ(brk->kind, STMT_BREAK);
    EXPECT_STREQ(brk->branch_target_label, ws->loop_end_label);
}

// continue inside while targets the loop's continue label.
TEST_F(LabelLoopsTest, ContinueInWhile)
{
    RunLabelLoops("int f(void) { while (1) { continue; } }");

    Stmt *ws   = fn_first_stmt(program->decls);
    Stmt *cont = compound_first(ws->u.while_stmt.body);
    ASSERT_EQ(cont->kind, STMT_CONTINUE);
    EXPECT_STREQ(cont->branch_target_label, ws->loop_continue_label);
}

// break inside a switch case targets the switch's end label.
TEST_F(LabelLoopsTest, BreakInSwitch)
{
    RunLabelLoops("int f(void) { switch (1) { case 1: break; } }");

    Stmt *sw = fn_first_stmt(program->decls);
    ASSERT_EQ(sw->kind, STMT_SWITCH);
    Stmt *cs = compound_first(sw->u.switch_stmt.body);
    ASSERT_EQ(cs->kind, STMT_CASE);
    Stmt *brk = cs->u.case_stmt.stmt;
    ASSERT_EQ(brk->kind, STMT_BREAK);
    EXPECT_STREQ(brk->branch_target_label, sw->loop_end_label);
}

// break inside an inner for (nested in a while) targets only the inner loop.
TEST_F(LabelLoopsTest, NestedLoopBreakInner)
{
    RunLabelLoops("int f(void) { while (1) { for (;;) { break; } } }");

    Stmt *ws = fn_first_stmt(program->decls);
    ASSERT_EQ(ws->kind, STMT_WHILE);
    Stmt *fs = compound_first(ws->u.while_stmt.body);
    ASSERT_EQ(fs->kind, STMT_FOR);
    Stmt *brk = compound_first(fs->u.for_stmt.body);
    ASSERT_EQ(brk->kind, STMT_BREAK);
    EXPECT_STREQ(brk->branch_target_label, fs->loop_end_label);
    EXPECT_STRNE(brk->branch_target_label, ws->loop_end_label);
}

// continue after an inner for loop targets the outer while's continue label.
TEST_F(LabelLoopsTest, NestedLoopContinueOuter)
{
    RunLabelLoops("int f(void) { while (1) { for (;;) {} continue; } }");

    Stmt *ws = fn_first_stmt(program->decls);
    ASSERT_EQ(ws->kind, STMT_WHILE);
    Stmt *cont = compound_second(ws->u.while_stmt.body);
    ASSERT_EQ(cont->kind, STMT_CONTINUE);
    EXPECT_STREQ(cont->branch_target_label, ws->loop_continue_label);
}

// continue in a switch (nested in a while) skips the switch and targets the loop.
TEST_F(LabelLoopsTest, ContinueInSwitchInsideLoop)
{
    RunLabelLoops("int f(void) { while (1) { switch (1) { case 1: continue; } } }");

    Stmt *ws = fn_first_stmt(program->decls);
    ASSERT_EQ(ws->kind, STMT_WHILE);
    Stmt *sw = compound_first(ws->u.while_stmt.body);
    ASSERT_EQ(sw->kind, STMT_SWITCH);
    Stmt *cs = compound_first(sw->u.switch_stmt.body);
    ASSERT_EQ(cs->kind, STMT_CASE);
    Stmt *cont = cs->u.case_stmt.stmt;
    ASSERT_EQ(cont->kind, STMT_CONTINUE);
    EXPECT_STREQ(cont->branch_target_label, ws->loop_continue_label);
}

// break outside any loop or switch is a fatal error.
TEST_F(LabelLoopsTest, BreakOutsideLoop)
{
    ASSERT_EXIT(RunLabelLoops("int f(void) { break; }"), ::testing::ExitedWithCode(1),
                "break statement not inside loop or switch");
}

// continue outside any loop is a fatal error.
TEST_F(LabelLoopsTest, ContinueOutsideLoop)
{
    ASSERT_EXIT(RunLabelLoops("int f(void) { continue; }"), ::testing::ExitedWithCode(1),
                "continue statement not inside loop");
}

// _Static_assert with a true condition inside a struct is accepted.
TEST_F(PipelineTest, StaticAssertInStructPasses)
{
    RunPipeline("struct S { _Static_assert(1, \"ok\"); int x; };");

    const StructDef *sd = structtab_find("S");
    ASSERT_NE(sd, nullptr);
    ASSERT_NE(sd->members, nullptr);
    EXPECT_STREQ(sd->members->name, "x");
    EXPECT_EQ(sd->members->type->kind, TYPE_INT);
}

// _Static_assert with a false condition inside a struct is a compile-time error.
TEST_F(PipelineTest, StaticAssertInStructFails)
{
    ParseProgram("struct S { _Static_assert(0, \"bad\"); int x; };");
    ASSERT_EXIT(typecheck_program(program), ::testing::ExitedWithCode(1),
                "_Static_assert failed: bad");
}

// _Static_assert with a true condition inside a union is accepted.
TEST_F(PipelineTest, StaticAssertInUnionPasses)
{
    RunPipeline("union U { _Static_assert(1, \"ok\"); int x; };");

    const StructDef *sd = structtab_find("U");
    ASSERT_NE(sd, nullptr);
    ASSERT_NE(sd->members, nullptr);
    EXPECT_STREQ(sd->members->name, "x");
}
