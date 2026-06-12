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
        name: .x
    - instruction:
      kind: copy
      src:
        kind: var
        name: .x
      dst:
        kind: var
        name: .0
    - instruction:
      kind: binary
      op: add
      src1:
        kind: var
        name: .x
      src2:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: .1
    - instruction:
      kind: copy
      src:
        kind: var
        name: .1
      dst:
        kind: var
        name: .x
    - instruction:
      kind: return
      src:
        kind: var
        name: .0
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
        name: .x
    - instruction:
      kind: copy
      src:
        kind: var
        name: .x
      dst:
        kind: var
        name: .0
    - instruction:
      kind: binary
      op: subtract
      src1:
        kind: var
        name: .x
      src2:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: .1
    - instruction:
      kind: copy
      src:
        kind: var
        name: .1
      dst:
        kind: var
        name: .x
    - instruction:
      kind: return
      src:
        kind: var
        name: .0
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
        name: .x
    - instruction:
      kind: binary
      op: add
      src1:
        kind: var
        name: .x
      src2:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: .0
    - instruction:
      kind: copy
      src:
        kind: var
        name: .0
      dst:
        kind: var
        name: .x
    - instruction:
      kind: return
      src:
        kind: var
        name: .0
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
        name: .x
    - instruction:
      kind: binary
      op: subtract
      src1:
        kind: var
        name: .x
      src2:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: .0
    - instruction:
      kind: copy
      src:
        kind: var
        name: .0
      dst:
        kind: var
        name: .x
    - instruction:
      kind: return
      src:
        kind: var
        name: .0
)");
}
