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
        name: %x
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 42
      dst:
        kind: var
        name: %x
    - instruction:
      kind: return
      src:
        kind: var
        name: %x
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
        name: %x
    - instruction:
      kind: copy
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %y
    - instruction:
      kind: return
      src:
        kind: var
        name: %y
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
        name: %x
    - instruction:
      kind: binary
      op: add
      src1:
        kind: var
        name: %x
      src2:
        kind: constant
        const:
          kind: int
          value: 5
      dst:
        kind: var
        name: %0
    - instruction:
      kind: copy
      src:
        kind: var
        name: %0
      dst:
        kind: var
        name: %x
    - instruction:
      kind: return
      src:
        kind: var
        name: %x
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
        name: %x
    - instruction:
      kind: binary
      op: bitwise_or
      src1:
        kind: var
        name: %x
      src2:
        kind: constant
        const:
          kind: int
          value: 3
      dst:
        kind: var
        name: %0
    - instruction:
      kind: copy
      src:
        kind: var
        name: %0
      dst:
        kind: var
        name: %x
    - instruction:
      kind: return
      src:
        kind: var
        name: %x
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
        name: %x
    - instruction:
      kind: jump_if_zero
      condition:
        kind: var
        name: %x
      target: %0
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 2
      dst:
        kind: var
        name: %2
    - instruction:
      kind: jump
      target: %1
    - instruction:
      kind: label
      name: %0
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 3
      dst:
        kind: var
        name: %2
    - instruction:
      kind: label
      name: %1
    - instruction:
      kind: return
      src:
        kind: var
        name: %2
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
    - param: %x
    - param: %y
  body:
    - instruction:
      kind: binary
      op: greater_than
      src1:
        kind: var
        name: %x
      src2:
        kind: constant
        const:
          kind: int
          value: 0
      dst:
        kind: var
        name: %0
    - instruction:
      kind: jump_if_zero
      condition:
        kind: var
        name: %0
      target: %1
    - instruction:
      kind: copy
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %3
    - instruction:
      kind: jump
      target: %2
    - instruction:
      kind: label
      name: %1
    - instruction:
      kind: copy
      src:
        kind: var
        name: %y
      dst:
        kind: var
        name: %3
    - instruction:
      kind: label
      name: %2
    - instruction:
      kind: return
      src:
        kind: var
        name: %3
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
    - param: %a
    - param: %b
  body:
    - instruction:
      kind: jump_if_zero
      condition:
        kind: var
        name: %a
      target: %0
    - instruction:
      kind: binary
      op: not_equal
      src1:
        kind: var
        name: %b
      src2:
        kind: constant
        const:
          kind: int
          value: 0
      dst:
        kind: var
        name: %2
    - instruction:
      kind: jump
      target: %1
    - instruction:
      kind: label
      name: %0
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 0
      dst:
        kind: var
        name: %2
    - instruction:
      kind: label
      name: %1
    - instruction:
      kind: return
      src:
        kind: var
        name: %2
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
    - param: %a
    - param: %b
  body:
    - instruction:
      kind: jump_if_not_zero
      condition:
        kind: var
        name: %a
      target: %0
    - instruction:
      kind: binary
      op: not_equal
      src1:
        kind: var
        name: %b
      src2:
        kind: constant
        const:
          kind: int
          value: 0
      dst:
        kind: var
        name: %2
    - instruction:
      kind: jump
      target: %1
    - instruction:
      kind: label
      name: %0
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: %2
    - instruction:
      kind: label
      name: %1
    - instruction:
      kind: return
      src:
        kind: var
        name: %2
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
    - param: %x
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
    - param: %x
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
      dst: %0
      offset: 0
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 2
      dst: %0
      offset: 4
    - instruction:
      kind: get_address
      src:
        kind: var
        name: %0
      dst:
        kind: var
        name: %1
    - instruction:
      kind: add_ptr
      ptr:
        kind: var
        name: %1
      index:
        kind: constant
        const:
          kind: int
          value: 0
      scale: 1
      dst:
        kind: var
        name: %2
    - instruction:
      kind: load
      src_ptr:
        kind: var
        name: %2
      dst:
        kind: var
        name: %3
    - instruction:
      kind: return
      src:
        kind: var
        name: %3
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
      dst: %0
      offset: 0
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 20
      dst: %0
      offset: 4
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 30
      dst: %0
      offset: 8
    - instruction:
      kind: get_address
      src:
        kind: var
        name: %0
      dst:
        kind: var
        name: %1
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: long
          value: 1
      dst:
        kind: var
        name: %2
    - instruction:
      kind: add_ptr
      ptr:
        kind: var
        name: %1
      index:
        kind: var
        name: %2
      scale: 4
      dst:
        kind: var
        name: %3
    - instruction:
      kind: load
      src_ptr:
        kind: var
        name: %3
      dst:
        kind: var
        name: %4
    - instruction:
      kind: return
      src:
        kind: var
        name: %4
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

// long double literal (L suffix) → TAC_CONST_LONG_DOUBLE
TEST_F(TranslateTest, LongDoubleLiteral)
{
    std::string yaml = CompileToYaml("long double f(void) { return 1.5L; }");
    EXPECT_NE(yaml.find("kind: long_double"), std::string::npos);
    // The hex float representation of 1.5L is platform-dependent (%La format):
    // on macOS/ARM64 it prints as 0xcp-3; on Linux x86-64 it may differ.
    // Verify the value is present but don't hard-code the exact hex string.
    EXPECT_NE(yaml.find("value: "), std::string::npos);
}

// ---------------------------------------------------------------------------
// long long / long / ulong / ulong_long literals
// ---------------------------------------------------------------------------

// Small LL literal gets TAC kind long_long, not int.
TEST_F(TranslateTest, LongLongLiteralSmall)
{
    std::string yaml = CompileToYaml("long long f(void) { return 10LL; }");
    EXPECT_NE(yaml.find("kind: long_long"), std::string::npos);
    EXPECT_NE(yaml.find("value: 10"), std::string::npos);
}

// Large LL literal preserves all 64 bits.
TEST_F(TranslateTest, LongLongLiteralLarge)
{
    std::string yaml = CompileToYaml(
        "long long f(void) { long long x = 9999999999LL; return x; }");
    EXPECT_NE(yaml.find("kind: long_long"), std::string::npos);
    EXPECT_NE(yaml.find("value: 9999999999"), std::string::npos);
}

// ULL literal gets TAC kind ulong_long.
TEST_F(TranslateTest, ULongLongLiteral)
{
    std::string yaml = CompileToYaml("unsigned long long f(void) { return 10ULL; }");
    EXPECT_NE(yaml.find("kind: ulong_long"), std::string::npos);
    EXPECT_NE(yaml.find("value: 10"), std::string::npos);
}

// L suffix literal gets TAC kind long.
TEST_F(TranslateTest, LongLiteral)
{
    std::string yaml = CompileToYaml("long f(void) { return 10L; }");
    EXPECT_NE(yaml.find("kind: long"), std::string::npos);
    EXPECT_NE(yaml.find("value: 10"), std::string::npos);
}

// UL suffix literal gets TAC kind ulong.
TEST_F(TranslateTest, ULongLiteral)
{
    std::string yaml = CompileToYaml("unsigned long f(void) { return 10UL; }");
    EXPECT_NE(yaml.find("kind: ulong"), std::string::npos);
    EXPECT_NE(yaml.find("value: 10"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Unsigned / logical TAC op kinds — task #1 (Phase E)
// ---------------------------------------------------------------------------

// Unsigned operands select divide_unsigned, not divide.
TEST_F(TranslateTest, UnsignedDivide)
{
    std::string yaml =
        CompileToYaml("unsigned int f(unsigned int a, unsigned int b) { return a / b; }");
    EXPECT_NE(yaml.find("op: divide_unsigned"), std::string::npos);
    EXPECT_EQ(yaml.find("op: divide\n"), std::string::npos);
}

// Unsigned operands select less_than_unsigned, not less_than.
TEST_F(TranslateTest, UnsignedLessThan)
{
    std::string yaml =
        CompileToYaml("int f(unsigned int a, unsigned int b) { return a < b; }");
    EXPECT_NE(yaml.find("op: less_than_unsigned"), std::string::npos);
    EXPECT_EQ(yaml.find("op: less_than\n"), std::string::npos);
}

// Unsigned left operand selects right_shift_logical, not right_shift.
TEST_F(TranslateTest, LogicalRightShift)
{
    std::string yaml = CompileToYaml("unsigned int f(unsigned int a) { return a >> 2; }");
    EXPECT_NE(yaml.find("op: right_shift_logical"), std::string::npos);
    EXPECT_EQ(yaml.find("op: right_shift\n"), std::string::npos);
}

// Unsigned operands select add_unsigned, not add (true 48-bit modular add on BESM-6).
TEST_F(TranslateTest, UnsignedAdd)
{
    std::string yaml =
        CompileToYaml("unsigned int f(unsigned int a, unsigned int b) { return a + b; }");
    EXPECT_NE(yaml.find("op: add_unsigned"), std::string::npos);
    EXPECT_EQ(yaml.find("op: add\n"), std::string::npos);
}

// Signed operands still select the signed variants.
TEST_F(TranslateTest, SignedDivideUnchanged)
{
    std::string yaml = CompileToYaml("int f(int a, int b) { return a / b; }");
    EXPECT_NE(yaml.find("op: divide\n"), std::string::npos);
    EXPECT_EQ(yaml.find("op: divide_unsigned"), std::string::npos);
}

// Signed add still selects plain add, not add_unsigned.
TEST_F(TranslateTest, SignedAddUnchanged)
{
    std::string yaml = CompileToYaml("int f(int a, int b) { return a + b; }");
    EXPECT_NE(yaml.find("op: add\n"), std::string::npos);
    EXPECT_EQ(yaml.find("op: add_unsigned"), std::string::npos);
}
