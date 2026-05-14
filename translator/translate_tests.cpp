#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "parser.h"
#include "semantic.h"
#include "structtab.h"
#include "symtab.h"
#include "typetab.h"
#include "tac.h"
#include "translate.h"
#include "xalloc.h"

class TranslateTest : public ::testing::Test {
    FILE *input_file{};

protected:
    Program *program{};

    void SetUp() override
    {
        input_file = tmpfile();
        ASSERT_NE(nullptr, input_file);
    }

    void TearDown() override
    {
        fclose(input_file);
        if (program) {
            free_program(program);
        }
        symtab_destroy();
        structtab_destroy();
        typetab_destroy();
        nametab_destroy();
        xreport_lost_memory();
        EXPECT_EQ(xtotal_allocated_size(), 0);
        xfree_all();
    }

    // Parse C source, run the full tacker pipeline on each declaration, and
    // return the concatenated YAML output for every translated toplevel.
    std::string CompileToYaml(const char *src)
    {
        fwrite(src, 1, strlen(src), input_file);
        rewind(input_file);
        program = parse(input_file);
        EXPECT_NE(nullptr, program);

        std::string result;
        ExternalDecl *decls = program->decls;
        program->decls      = nullptr;
        while (decls) {
            ExternalDecl *next = decls->next;
            decls->next        = nullptr;
            typecheck_global_decl(decls);
            label_loops(decls);
            Tac_TopLevel *tac = translate(decls);
            free_external_decl(decls);
            if (tac) {
                FILE *f = tmpfile();
                EXPECT_NE(nullptr, f);
                for (const Tac_TopLevel *t = tac; t; t = t->next)
                    tac_export_yaml(f, t);
                long len = ftell(f);
                rewind(f);
                std::string yaml(static_cast<size_t>(len), '\0');
                EXPECT_TRUE(fread(&yaml[0], 1, static_cast<size_t>(len), f));
                fclose(f);
                result += yaml;
                tac_free_toplevel(tac);
            }
            decls = next;
        }
        return result;
    }
};

// ---------------------------------------------------------------------------
// Local variable declarations — task #1
// ---------------------------------------------------------------------------

// A variable with no initializer emits no COPY; only the return is generated.
TEST_F(TranslateTest, LocalVarNoInit)
{
    std::string yaml = CompileToYaml("int f(void) { int x; return 0; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: return
      src:
        kind: constant
        const:
          kind: int
          value: 0
)");
}

// A variable with a constant initializer emits COPY dst ← constant.
TEST_F(TranslateTest, LocalVarWithIntInit)
{
    std::string yaml = CompileToYaml("int f(void) { int x = 42; return x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 42
      dst:
        kind: var
        name: x
    - instruction:
      kind: return
      src:
        kind: var
        name: x
)");
}

// Two local variables each emit their own COPY.
TEST_F(TranslateTest, TwoLocalVars)
{
    std::string yaml = CompileToYaml("int f(void) { int x = 1; int y = 2; return x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: x
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 2
      dst:
        kind: var
        name: y
    - instruction:
      kind: return
      src:
        kind: var
        name: x
)");
}

// Initialized variable used in an expression produces COPY then BINARY.
TEST_F(TranslateTest, LocalVarUsedInBinaryExpr)
{
    std::string yaml = CompileToYaml("int f(void) { int x = 10; return x + 1; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 10
      dst:
        kind: var
        name: x
    - instruction:
      kind: binary
      op: add
      src1:
        kind: var
        name: x
      src2:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: t.0
    - instruction:
      kind: return
      src:
        kind: var
        name: t.0
)");
}

// ---------------------------------------------------------------------------
// Char literals — task #1
// ---------------------------------------------------------------------------

TEST_F(TranslateTest, CharLiteralReturnedDirectly)
{
    std::string yaml = CompileToYaml("int f(void) { return 'A'; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: return
      src:
        kind: constant
        const:
          kind: int
          value: 65
)");
}

// ---------------------------------------------------------------------------
// String literals — task #1
// ---------------------------------------------------------------------------

// A string literal emits a static_constant toplevel node followed by a
// get_address instruction that yields the pointer result.
TEST_F(TranslateTest, StringLiteralReturned)
{
    std::string yaml = CompileToYaml(R"(char *f(void) { return "hi"; })");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: static_constant
  name: _str0
  type:
    kind: array
    elem_type:
      kind: char
    size: 5
  init:
    kind: string
    value: "hi"
    null_terminated: true
- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: get_address
      src:
        kind: var
        name: _str0
      dst:
        kind: var
        name: t.0
    - instruction:
      kind: return
      src:
        kind: var
        name: t.0
)");
}

// Two functions with distinct string literals each get their own
// static_constant node with unique names.
TEST_F(TranslateTest, TwoFunctionsDistinctStringLiterals)
{
    std::string yaml = CompileToYaml(
        "char *f(void) { return \"yes\"; }\n"
        "char *g(void) { return \"no\"; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: static_constant
  name: _str0
  type:
    kind: array
    elem_type:
      kind: char
    size: 6
  init:
    kind: string
    value: "yes"
    null_terminated: true
- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: get_address
      src:
        kind: var
        name: _str0
      dst:
        kind: var
        name: t.0
    - instruction:
      kind: return
      src:
        kind: var
        name: t.0
- toplevel:
  kind: static_constant
  name: _str1
  type:
    kind: array
    elem_type:
      kind: char
    size: 5
  init:
    kind: string
    value: "no"
    null_terminated: true
- toplevel:
  kind: function
  name: g
  global: true
  body:
    - instruction:
      kind: get_address
      src:
        kind: var
        name: _str1
      dst:
        kind: var
        name: t.0
    - instruction:
      kind: return
      src:
        kind: var
        name: t.0
)");
}

// ---------------------------------------------------------------------------
// Unary plus — task #3
// ---------------------------------------------------------------------------

// Unary + is a no-op: it must not emit any instruction.
TEST_F(TranslateTest, UnaryPlusNoOp)
{
    std::string yaml = CompileToYaml("int f(void) { return +42; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: return
      src:
        kind: constant
        const:
          kind: int
          value: 42
)");
}

// ---------------------------------------------------------------------------
// Ternary conditional ?:  — task #3
// ---------------------------------------------------------------------------

// Simple ternary: variable condition, integer constant branches.
TEST_F(TranslateTest, TernaryConstantBranches)
{
    std::string yaml = CompileToYaml("int f(void) { int x = 1; return x ? 2 : 3; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: x
    - instruction:
      kind: jump_if_zero
      condition:
        kind: var
        name: x
      target: t.0
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 2
      dst:
        kind: var
        name: t.2
    - instruction:
      kind: jump
      target: t.1
    - instruction:
      kind: label
      name: t.0
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 3
      dst:
        kind: var
        name: t.2
    - instruction:
      kind: label
      name: t.1
    - instruction:
      kind: return
      src:
        kind: var
        name: t.2
)");
}

// Ternary with expression condition and variable branches.
TEST_F(TranslateTest, TernaryExprCondVarBranches)
{
    std::string yaml = CompileToYaml("int f(int x, int y) { return x > 0 ? x : y; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: x
    - param: y
  body:
    - instruction:
      kind: binary
      op: greater_than
      src1:
        kind: var
        name: x
      src2:
        kind: constant
        const:
          kind: int
          value: 0
      dst:
        kind: var
        name: t.0
    - instruction:
      kind: jump_if_zero
      condition:
        kind: var
        name: t.0
      target: t.1
    - instruction:
      kind: copy
      src:
        kind: var
        name: x
      dst:
        kind: var
        name: t.3
    - instruction:
      kind: jump
      target: t.2
    - instruction:
      kind: label
      name: t.1
    - instruction:
      kind: copy
      src:
        kind: var
        name: y
      dst:
        kind: var
        name: t.3
    - instruction:
      kind: label
      name: t.2
    - instruction:
      kind: return
      src:
        kind: var
        name: t.3
)");
}

// ---------------------------------------------------------------------------
// Short-circuit && and || — task #4
// ---------------------------------------------------------------------------

TEST_F(TranslateTest, LogicalAndShortCircuit)
{
    std::string yaml = CompileToYaml("int f(int a, int b) { return a && b; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: a
    - param: b
  body:
    - instruction:
      kind: jump_if_zero
      condition:
        kind: var
        name: a
      target: t.0
    - instruction:
      kind: binary
      op: not_equal
      src1:
        kind: var
        name: b
      src2:
        kind: constant
        const:
          kind: int
          value: 0
      dst:
        kind: var
        name: t.2
    - instruction:
      kind: jump
      target: t.1
    - instruction:
      kind: label
      name: t.0
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 0
      dst:
        kind: var
        name: t.2
    - instruction:
      kind: label
      name: t.1
    - instruction:
      kind: return
      src:
        kind: var
        name: t.2
)");
}

TEST_F(TranslateTest, LogicalOrShortCircuit)
{
    std::string yaml = CompileToYaml("int f(int a, int b) { return a || b; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: a
    - param: b
  body:
    - instruction:
      kind: jump_if_not_zero
      condition:
        kind: var
        name: a
      target: t.0
    - instruction:
      kind: binary
      op: not_equal
      src1:
        kind: var
        name: b
      src2:
        kind: constant
        const:
          kind: int
          value: 0
      dst:
        kind: var
        name: t.2
    - instruction:
      kind: jump
      target: t.1
    - instruction:
      kind: label
      name: t.0
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: t.2
    - instruction:
      kind: label
      name: t.1
    - instruction:
      kind: return
      src:
        kind: var
        name: t.2
)");
}

// ---------------------------------------------------------------------------
// goto statement — task #4
// ---------------------------------------------------------------------------

TEST_F(TranslateTest, GotoEmitsJump)
{
    std::string yaml = CompileToYaml("int f(void) { goto end; end: return 0; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: jump
      target: end
    - instruction:
      kind: label
      name: end
    - instruction:
      kind: return
      src:
        kind: constant
        const:
          kind: int
          value: 0
)");
}

// ---------------------------------------------------------------------------
// Labeled statement — task #5
// ---------------------------------------------------------------------------

TEST_F(TranslateTest, LabeledStatementEmitsLabel)
{
    std::string yaml = CompileToYaml("int f(void) { int x = 1; loop: x = 2; return x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: x
    - instruction:
      kind: label
      name: loop
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 2
      dst:
        kind: var
        name: x
    - instruction:
      kind: return
      src:
        kind: var
        name: x
)");
}

// ---------------------------------------------------------------------------
// Assignment expressions — task #2
// ---------------------------------------------------------------------------

// Simple assignment emits COPY into the target variable and returns it.
TEST_F(TranslateTest, AssignSimple)
{
    std::string yaml = CompileToYaml("int f(void) { int x = 0; x = 42; return x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 0
      dst:
        kind: var
        name: x
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 42
      dst:
        kind: var
        name: x
    - instruction:
      kind: return
      src:
        kind: var
        name: x
)");
}

// Assignment is an expression — its result can be used as an initializer.
TEST_F(TranslateTest, AssignUsedAsExpr)
{
    std::string yaml = CompileToYaml("int f(void) { int x; int y = (x = 7); return y; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 7
      dst:
        kind: var
        name: x
    - instruction:
      kind: copy
      src:
        kind: var
        name: x
      dst:
        kind: var
        name: y
    - instruction:
      kind: return
      src:
        kind: var
        name: y
)");
}

// Compound add-assign: reads target, adds rhs, stores back.
TEST_F(TranslateTest, CompoundAssignAdd)
{
    std::string yaml = CompileToYaml("int f(void) { int x = 10; x += 5; return x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 10
      dst:
        kind: var
        name: x
    - instruction:
      kind: binary
      op: add
      src1:
        kind: var
        name: x
      src2:
        kind: constant
        const:
          kind: int
          value: 5
      dst:
        kind: var
        name: t.0
    - instruction:
      kind: copy
      src:
        kind: var
        name: t.0
      dst:
        kind: var
        name: x
    - instruction:
      kind: return
      src:
        kind: var
        name: x
)");
}

// Compound bitwise-or-assign exercises the bitwise_or TAC binary operator.
TEST_F(TranslateTest, CompoundAssignBitwiseOr)
{
    std::string yaml = CompileToYaml("int f(void) { int x = 6; x |= 3; return x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 6
      dst:
        kind: var
        name: x
    - instruction:
      kind: binary
      op: bitwise_or
      src1:
        kind: var
        name: x
      src2:
        kind: constant
        const:
          kind: int
          value: 3
      dst:
        kind: var
        name: t.0
    - instruction:
      kind: copy
      src:
        kind: var
        name: t.0
      dst:
        kind: var
        name: x
    - instruction:
      kind: return
      src:
        kind: var
        name: x
)");
}

// ---------------------------------------------------------------------------
// Enum constant literals — task #1
// ---------------------------------------------------------------------------

// Enumerators without explicit values start at 0 and auto-increment.
TEST_F(TranslateTest, EnumConstDefaultValues)
{
    std::string yaml = CompileToYaml("enum Color { RED, GREEN }; int f(void) { return GREEN; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: return
      src:
        kind: constant
        const:
          kind: int
          value: 1
)");
}

// An explicit initializer overrides the auto-increment.
TEST_F(TranslateTest, EnumConstExplicitValue)
{
    std::string yaml = CompileToYaml("enum Color { RED = 5, GREEN }; int f(void) { return RED; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: return
      src:
        kind: constant
        const:
          kind: int
          value: 5
)");
}

// Enum declared inside a function body is scoped to that body.
TEST_F(TranslateTest, EnumConstLocalDecl)
{
    std::string yaml = CompileToYaml("int f(void) { enum Dir { UP = 10, DOWN }; return DOWN; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: return
      src:
        kind: constant
        const:
          kind: int
          value: 11
)");
}

// for-init declaration emits COPY before the loop test label.
TEST_F(TranslateTest, ForLoopInitDecl)
{
    std::string yaml = CompileToYaml("int f(void) { for (int i = 5; ; ) { return i; } return 0; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 5
      dst:
        kind: var
        name: i
    - instruction:
      kind: label
      name: t.0
    - instruction:
      kind: return
      src:
        kind: var
        name: i
    - instruction:
      kind: label
      name: .L1
    - instruction:
      kind: jump
      target: t.0
    - instruction:
      kind: label
      name: .L0
    - instruction:
      kind: return
      src:
        kind: constant
        const:
          kind: int
          value: 0
)");
}

// ---------------------------------------------------------------------------
// Global declarations — task #N
// ---------------------------------------------------------------------------

// Tentative global variable (no initializer) emits static_variable with no init_list.
TEST_F(TranslateTest, GlobalVarTentative)
{
    std::string yaml = CompileToYaml("int x;");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: static_variable
  name: x
  global: true
  type:
    kind: int
)");
}

// Initialized global variable emits static_variable with i32 init.
TEST_F(TranslateTest, GlobalVarInitialized)
{
    std::string yaml = CompileToYaml("int x = 42;");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: static_variable
  name: x
  global: true
  type:
    kind: int
  init_list:
    - init:
      kind: i32
      value: 42
)");
}

// Static global variable sets global=false.
TEST_F(TranslateTest, GlobalVarStatic)
{
    std::string yaml = CompileToYaml("static int x = 5;");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: static_variable
  name: x
  global: false
  type:
    kind: int
  init_list:
    - init:
      kind: i32
      value: 5
)");
}

// extern declaration emits static_variable with global=true and no init_list.
TEST_F(TranslateTest, GlobalVarExtern)
{
    std::string yaml = CompileToYaml("extern int x;");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: static_variable
  name: x
  global: true
  type:
    kind: int
)");
}

// Function prototype emits TAC_TOPLEVEL_FUNCTION with params but no body.
TEST_F(TranslateTest, FunctionPrototype)
{
    std::string yaml = CompileToYaml("int foo(int a, int b);");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: foo
  global: true
  params:
    - param: a
    - param: b
)");
}

// Static void prototype: global=false, no params (void sentinel stripped), no body.
TEST_F(TranslateTest, FunctionPrototypeStatic)
{
    std::string yaml = CompileToYaml("static void bar(void);");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: bar
  global: false
)");
}

// Multi-declarator global declaration emits one static_variable per declarator.
TEST_F(TranslateTest, GlobalVarMultiDeclarator)
{
    std::string yaml = CompileToYaml("int x = 1, y = 2;");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: static_variable
  name: x
  global: true
  type:
    kind: int
  init_list:
    - init:
      kind: i32
      value: 1
- toplevel:
  kind: static_variable
  name: y
  global: true
  type:
    kind: int
  init_list:
    - init:
      kind: i32
      value: 2
)");
}

// Struct/enum/typedef-only declarations produce no TAC output.
TEST_F(TranslateTest, StructDeclOnly)
{
    EXPECT_EQ(CompileToYaml("struct Foo { int x; };"), "");
}

TEST_F(TranslateTest, EnumDeclOnly)
{
    EXPECT_EQ(CompileToYaml("enum Color { RED, GREEN, BLUE };"), "");
}

TEST_F(TranslateTest, TypedefOnly)
{
    EXPECT_EQ(CompileToYaml("typedef int MyInt;"), "");
}

// Static function definition must emit global=false (regression: was hardcoded true).
TEST_F(TranslateTest, StaticFunctionGlobalFalse)
{
    std::string yaml = CompileToYaml("static int f(void) { return 0; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: false
  body:
    - instruction:
      kind: return
      src:
        kind: constant
        const:
          kind: int
          value: 0
)");
}
