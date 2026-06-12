#include "translate_test.h"

// ---------------------------------------------------------------------------
// switch / case / default lowering — task #8
// ---------------------------------------------------------------------------

// Two cases plus default: full dispatch chain with copy, binary-equal pairs,
// jump-if-not-zero pairs, default jump, inline labels, and end label.
TEST_F(TranslateTest, SwitchWithDefaultAndCases)
{
    std::string yaml = CompileToYaml(
        "int f(int x) {"
        "  switch (x) {"
        "    case 1: return 1;"
        "    case 2: return 2;"
        "    default: return 0;"
        "  }"
        "}");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: .x
  body:
    - instruction:
      kind: copy
      src:
        kind: var
        name: .x
      dst:
        kind: var
        name: .3
    - instruction:
      kind: binary
      op: equal
      src1:
        kind: var
        name: .3
      src2:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: .4
    - instruction:
      kind: jump_if_not_zero
      condition:
        kind: var
        name: .4
      target: .0
    - instruction:
      kind: binary
      op: equal
      src1:
        kind: var
        name: .3
      src2:
        kind: constant
        const:
          kind: int
          value: 2
      dst:
        kind: var
        name: .5
    - instruction:
      kind: jump_if_not_zero
      condition:
        kind: var
        name: .5
      target: .1
    - instruction:
      kind: jump
      target: .2
    - instruction:
      kind: label
      name: .0
    - instruction:
      kind: return
      src:
        kind: constant
        const:
          kind: int
          value: 1
    - instruction:
      kind: label
      name: .1
    - instruction:
      kind: return
      src:
        kind: constant
        const:
          kind: int
          value: 2
    - instruction:
      kind: label
      name: .2
    - instruction:
      kind: return
      src:
        kind: constant
        const:
          kind: int
          value: 0
    - instruction:
      kind: label
      name: .L0
)");
}

// One case, no default: dispatch jumps to end label on miss.
TEST_F(TranslateTest, SwitchNoDefault)
{
    std::string yaml = CompileToYaml(
        "int f(int x) {"
        "  switch (x) { case 1: return 1; }"
        "  return 0;"
        "}");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: .x
  body:
    - instruction:
      kind: copy
      src:
        kind: var
        name: .x
      dst:
        kind: var
        name: .1
    - instruction:
      kind: binary
      op: equal
      src1:
        kind: var
        name: .1
      src2:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: .2
    - instruction:
      kind: jump_if_not_zero
      condition:
        kind: var
        name: .2
      target: .0
    - instruction:
      kind: jump
      target: .L0
    - instruction:
      kind: label
      name: .0
    - instruction:
      kind: return
      src:
        kind: constant
        const:
          kind: int
          value: 1
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

// Two consecutive case labels with no break between them: both labels are emitted
// before the shared body — classic fall-through.
TEST_F(TranslateTest, SwitchFallthrough)
{
    std::string yaml = CompileToYaml(
        "int f(int x) {"
        "  int r = 0;"
        "  switch (x) { case 1: case 2: r = 1; }"
        "  return r;"
        "}");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: .x
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
        name: .r
    - instruction:
      kind: copy
      src:
        kind: var
        name: .x
      dst:
        kind: var
        name: .2
    - instruction:
      kind: binary
      op: equal
      src1:
        kind: var
        name: .2
      src2:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: .3
    - instruction:
      kind: jump_if_not_zero
      condition:
        kind: var
        name: .3
      target: .0
    - instruction:
      kind: binary
      op: equal
      src1:
        kind: var
        name: .2
      src2:
        kind: constant
        const:
          kind: int
          value: 2
      dst:
        kind: var
        name: .4
    - instruction:
      kind: jump_if_not_zero
      condition:
        kind: var
        name: .4
      target: .1
    - instruction:
      kind: jump
      target: .L0
    - instruction:
      kind: label
      name: .0
    - instruction:
      kind: label
      name: .1
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: .r
    - instruction:
      kind: label
      name: .L0
    - instruction:
      kind: return
      src:
        kind: var
        name: .r
)");
}

// Explicit break in each case: break emits a jump to the end label (.L0),
// preventing fall-through.
TEST_F(TranslateTest, SwitchBreakExitsEarly)
{
    std::string yaml = CompileToYaml(
        "int f(int x) {"
        "  int r = 0;"
        "  switch (x) {"
        "    case 1: r = 1; break;"
        "    case 2: r = 2; break;"
        "  }"
        "  return r;"
        "}");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: .x
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
        name: .r
    - instruction:
      kind: copy
      src:
        kind: var
        name: .x
      dst:
        kind: var
        name: .2
    - instruction:
      kind: binary
      op: equal
      src1:
        kind: var
        name: .2
      src2:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: .3
    - instruction:
      kind: jump_if_not_zero
      condition:
        kind: var
        name: .3
      target: .0
    - instruction:
      kind: binary
      op: equal
      src1:
        kind: var
        name: .2
      src2:
        kind: constant
        const:
          kind: int
          value: 2
      dst:
        kind: var
        name: .4
    - instruction:
      kind: jump_if_not_zero
      condition:
        kind: var
        name: .4
      target: .1
    - instruction:
      kind: jump
      target: .L0
    - instruction:
      kind: label
      name: .0
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: .r
    - instruction:
      kind: jump
      target: .L0
    - instruction:
      kind: label
      name: .1
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 2
      dst:
        kind: var
        name: .r
    - instruction:
      kind: jump
      target: .L0
    - instruction:
      kind: label
      name: .L0
    - instruction:
      kind: return
      src:
        kind: var
        name: .r
)");
}

// Empty switch body: dispatch chain has no comparisons, just copy + jump to end.
TEST_F(TranslateTest, SwitchEmptyBody)
{
    std::string yaml = CompileToYaml("void f(int x) { switch (x) {} }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: .x
  body:
    - instruction:
      kind: copy
      src:
        kind: var
        name: .x
      dst:
        kind: var
        name: .0
    - instruction:
      kind: jump
      target: .L0
    - instruction:
      kind: label
      name: .L0
)");
}
