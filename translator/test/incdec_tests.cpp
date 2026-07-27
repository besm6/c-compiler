#include "translate_test.h"

// ---------------------------------------------------------------------------
// Pre/post increment and decrement — task #7
// ---------------------------------------------------------------------------

// x++ saves the old value, adds 1, writes back, and returns the old value.
TEST_F(TranslateTest, PostIncReturnOldValue)
{
    std::string yaml = CompileToYaml("int f(void) { int x = 5; return x++; }");
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
        name: %x
    - instruction:
      kind: copy
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
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
          value: 1
      dst:
        kind: var
        name: %1
    - instruction:
      kind: copy
      src:
        kind: var
        name: %1
      dst:
        kind: var
        name: %x
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// x-- saves the old value, subtracts 1, writes back, and returns the old value.
TEST_F(TranslateTest, PostDecReturnOldValue)
{
    std::string yaml = CompileToYaml("int f(void) { int x = 5; return x--; }");
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
        name: %x
    - instruction:
      kind: copy
      src:
        kind: var
        name: %x
      dst:
        kind: var
        name: %0
    - instruction:
      kind: binary
      op: subtract
      src1:
        kind: var
        name: %x
      src2:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: %1
    - instruction:
      kind: copy
      src:
        kind: var
        name: %1
      dst:
        kind: var
        name: %x
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
)");
}

// ++x adds 1, writes back, and returns the new value.
TEST_F(TranslateTest, PreIncReturnNewValue)
{
    std::string yaml = CompileToYaml("int f(void) { int x = 5; return ++x; }");
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
          value: 1
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
        name: %0
)");
}

// --x subtracts 1, writes back, and returns the new value.
TEST_F(TranslateTest, PreDecReturnNewValue)
{
    std::string yaml = CompileToYaml("int f(void) { int x = 5; return --x; }");
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
        name: %x
    - instruction:
      kind: binary
      op: subtract
      src1:
        kind: var
        name: %x
      src2:
        kind: constant
        const:
          kind: int
          value: 1
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
        name: %0
)");
}

// C11 §6.5.2.4p2: `b++` is `b = b + 1` with the sum converted back to the operand's
// type, so a _Bool ends at 1, not 2.  ++/-- computes the step in the operand's own
// type and stores it directly — it never builds a cast node — so the §6.3.1.2
// normalization has to be emitted by the step itself.
TEST_F(TranslateTest, PostIncBoolNormalizes)
{
    std::string yaml = CompileToYaml("_Bool f(_Bool b) { b++; return b; }");
    EXPECT_NE(yaml.find("op: add"), std::string::npos);
    EXPECT_NE(yaml.find("op: not_equal"), std::string::npos);
    EXPECT_LT(yaml.find("op: add"), yaml.find("op: not_equal"));
}
