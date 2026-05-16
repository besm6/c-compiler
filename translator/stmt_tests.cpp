#include "translate_test.h"

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
// Function calls — task #5
// ---------------------------------------------------------------------------

// Void call with no arguments emits fun_call with no args and no dst.
TEST_F(TranslateTest, CallVoidNoArgs)
{
    std::string yaml = CompileToYaml("void g(void); void f(void) { g(); }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: g
  global: true
- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: fun_call
      fun_name: g
)");
}

// Non-void call with no arguments emits fun_call with a dst temp, then return.
TEST_F(TranslateTest, CallReturningInt)
{
    std::string yaml = CompileToYaml("int g(void); int f(void) { return g(); }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: g
  global: true
- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: fun_call
      fun_name: g
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

// Call with two integer arguments emits an args list in the fun_call.
TEST_F(TranslateTest, CallWithArgs)
{
    std::string yaml = CompileToYaml("int add(int a, int b); int f(void) { return add(1, 2); }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: add
  global: true
  params:
    - param: a
    - param: b
- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: fun_call
      fun_name: add
      args:
        - val:
          kind: constant
          const:
            kind: int
            value: 1
        - val:
          kind: constant
          const:
            kind: int
            value: 2
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

// Call result used inside a binary expression: fun_call → t.0, binary → t.1.
TEST_F(TranslateTest, CallResultInExpression)
{
    std::string yaml = CompileToYaml("int g(void); int f(void) { return g() + 1; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: g
  global: true
- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: fun_call
      fun_name: g
      dst:
        kind: var
        name: t.0
    - instruction:
      kind: binary
      op: add
      src1:
        kind: var
        name: t.0
      src2:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: t.1
    - instruction:
      kind: return
      src:
        kind: var
        name: t.1
)");
}

// Indirect call through a function pointer parameter: fun_name is the pointer variable.
TEST_F(TranslateTest, IndirectCallViaFunctionPointer)
{
    std::string yaml = CompileToYaml("int f(int (*fp)(int)) { return fp(42); }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: fp
  body:
    - instruction:
      kind: fun_call
      fun_name: fp
      args:
        - val:
          kind: constant
          const:
            kind: int
            value: 42
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

// (*fp)(42): callee is type-checked with decay, so (*fp) has pointer type and
// lowering emits LOAD then indirect fun_call (unlike fp(42), which calls via fp).
TEST_F(TranslateTest, IndirectCallViaExplicitDeref)
{
    std::string yaml = CompileToYaml("int f(int (*fp)(int)) { return (*fp)(42); }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: fp
  body:
    - instruction:
      kind: load
      src_ptr:
        kind: var
        name: fp
      dst:
        kind: var
        name: t.0
    - instruction:
      kind: fun_call
      fun_name: t.0
      args:
        - val:
          kind: constant
          const:
            kind: int
            value: 42
      dst:
        kind: var
        name: t.1
    - instruction:
      kind: return
      src:
        kind: var
        name: t.1
)");
}

// ---------------------------------------------------------------------------
// Global linkage — task #4
// ---------------------------------------------------------------------------

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
