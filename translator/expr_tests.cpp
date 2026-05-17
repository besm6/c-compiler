#include "translate_test.h"

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
// Ternary conditional ?: — task #3
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
// sizeof and _Alignof operators — task #5
// ---------------------------------------------------------------------------

TEST_F(TranslateTest, SizeofType_Int)
{
    std::string yaml = CompileToYaml("unsigned long f(void) { return sizeof(int); }");
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
          value: 4
)");
}

TEST_F(TranslateTest, SizeofType_Long)
{
    std::string yaml = CompileToYaml("unsigned long f(void) { return sizeof(long); }");
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
          value: 8
)");
}

TEST_F(TranslateTest, SizeofExpr)
{
    std::string yaml = CompileToYaml("unsigned long f(void) { int x; return sizeof(x); }");
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
          value: 4
)");
}

TEST_F(TranslateTest, AlignofDouble)
{
    std::string yaml = CompileToYaml("unsigned long f(void) { return _Alignof(double); }");
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
          value: 8
)");
}

// ---------------------------------------------------------------------------
// _Generic expressions — task #5
// ---------------------------------------------------------------------------

// Controlling expression type matches a typed association.
TEST_F(TranslateTest, GenericTypeMatch)
{
    std::string yaml = CompileToYaml("int f(int x) { return _Generic(x, double: 0, int: 42); }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: x
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

// No type matches; falls back to the default association.
TEST_F(TranslateTest, GenericDefault)
{
    std::string yaml = CompileToYaml("int f(double x) { return _Generic(x, int: 0, default: 99); }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: x
  body:
    - instruction:
      kind: return
      src:
        kind: constant
        const:
          kind: int
          value: 99
)");
}

// ---------------------------------------------------------------------------
// Compound literal expressions — task #7
// ---------------------------------------------------------------------------

// Scalar compound literal (int){42} — value returned directly, no temp.
TEST_F(TranslateTest, CompoundLiteralScalar)
{
    std::string yaml = CompileToYaml("int f(void) { return (int){42}; }");
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

// Struct compound literal field access: (struct Foo){1, 2}.x
TEST_F(TranslateTest, CompoundLiteralStructField)
{
    std::string yaml = CompileToYaml(
        "struct Foo { int x; int y; };"
        "int f(void) { return (struct Foo){1, 2}.x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 1
      dst: t.0
      offset: 0
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 2
      dst: t.0
      offset: 4
    - instruction:
      kind: get_address
      src:
        kind: var
        name: t.0
      dst:
        kind: var
        name: t.1
    - instruction:
      kind: add_ptr
      ptr:
        kind: var
        name: t.1
      index:
        kind: constant
        const:
          kind: int
          value: 0
      scale: 1
      dst:
        kind: var
        name: t.2
    - instruction:
      kind: load
      src_ptr:
        kind: var
        name: t.2
      dst:
        kind: var
        name: t.3
    - instruction:
      kind: return
      src:
        kind: var
        name: t.3
)");
}

// Array compound literal subscript: (int[3]){10, 20, 30}[1]
TEST_F(TranslateTest, CompoundLiteralArraySubscript)
{
    std::string yaml = CompileToYaml("int f(void) { return (int[3]){10, 20, 30}[1]; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 10
      dst: t.0
      offset: 0
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 20
      dst: t.0
      offset: 4
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 30
      dst: t.0
      offset: 8
    - instruction:
      kind: get_address
      src:
        kind: var
        name: t.0
      dst:
        kind: var
        name: t.1
    - instruction:
      kind: sign_extend
      src:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: t.2
    - instruction:
      kind: add_ptr
      ptr:
        kind: var
        name: t.1
      index:
        kind: var
        name: t.2
      scale: 4
      dst:
        kind: var
        name: t.3
    - instruction:
      kind: load
      src_ptr:
        kind: var
        name: t.3
      dst:
        kind: var
        name: t.4
    - instruction:
      kind: return
      src:
        kind: var
        name: t.4
)");
}

// float literal (f suffix) → TAC_CONST_FLOAT
TEST_F(TranslateTest, FloatLiteral)
{
    std::string yaml = CompileToYaml("float f(void) { return 1.5f; }");
    EXPECT_NE(yaml.find("kind: float"), std::string::npos);
    EXPECT_NE(yaml.find("value: 0x1.8p+0"), std::string::npos);
}

// double literal (no suffix) → TAC_CONST_DOUBLE
TEST_F(TranslateTest, DoubleLiteral)
{
    std::string yaml = CompileToYaml("double f(void) { return 1.5; }");
    EXPECT_NE(yaml.find("kind: double"), std::string::npos);
    EXPECT_NE(yaml.find("value: 0x1.8p+0"), std::string::npos);
}
