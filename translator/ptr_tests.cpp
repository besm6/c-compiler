#include "translate_test.h"

// ---------------------------------------------------------------------------
// Pointer operations — tasks #1+
// ---------------------------------------------------------------------------

// &x on a local variable emits GET_ADDRESS into a fresh temp,
// then the initializer COPY stores the temp into p.
TEST_F(TranslateTest, AddressOfLocalVar)
{
    std::string yaml = CompileToYaml("void f(void) { int x; int *p = &x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: get_address
      src:
        kind: var
        name: x
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
        name: p
)");
}

// *p in rvalue context emits LOAD src_ptr←p, yielding the loaded value.
TEST_F(TranslateTest, DerefPointer)
{
    std::string yaml = CompileToYaml("void f(void) { int *p; int y = *p; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: load
      src_ptr:
        kind: var
        name: p
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
        name: y
)");
}

// int **pp; int y = **pp; — two LOADs (load pointer, load through it) then COPY.
TEST_F(TranslateTest, DerefDerefLocalVar)
{
    std::string yaml = CompileToYaml("void f(void) { int **pp; int y = **pp; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: load
      src_ptr:
        kind: var
        name: pp
      dst:
        kind: var
        name: t.0
    - instruction:
      kind: load
      src_ptr:
        kind: var
        name: t.0
      dst:
        kind: var
        name: t.1
    - instruction:
      kind: copy
      src:
        kind: var
        name: t.1
      dst:
        kind: var
        name: y
)");
}

// *p = 5 through a pointer parameter emits a single STORE.
TEST_F(TranslateTest, AssignThroughPointer)
{
    std::string yaml = CompileToYaml("void f(int *p) { *p = 5; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: p
  body:
    - instruction:
      kind: store
      src:
        kind: constant
        const:
          kind: int
          value: 5
      dst_ptr:
        kind: var
        name: p
)");
}

// (*p)++ emits LOAD + BINARY(add 1) + STORE; returns the old value.
TEST_F(TranslateTest, PostIncThroughPointer)
{
    std::string yaml = CompileToYaml("void f(int *p) { (*p)++; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: p
  body:
    - instruction:
      kind: load
      src_ptr:
        kind: var
        name: p
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
      kind: store
      src:
        kind: var
        name: t.1
      dst_ptr:
        kind: var
        name: p
)");
}

// *p += 3 emits LOAD + BINARY(add 3) + STORE.
TEST_F(TranslateTest, CompoundAssignThroughPointer)
{
    std::string yaml = CompileToYaml("void f(int *p) { *p += 3; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: p
  body:
    - instruction:
      kind: load
      src_ptr:
        kind: var
        name: p
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
          value: 3
      dst:
        kind: var
        name: t.1
    - instruction:
      kind: store
      src:
        kind: var
        name: t.1
      dst_ptr:
        kind: var
        name: p
)");
}

// **pp emits two LOADs, validating the mutual recursion between gen_lval and gen_expr.
TEST_F(TranslateTest, DerefDerefPointer)
{
    std::string yaml = CompileToYaml("void f(int **pp) { int y = **pp; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: pp
  body:
    - instruction:
      kind: load
      src_ptr:
        kind: var
        name: pp
      dst:
        kind: var
        name: t.0
    - instruction:
      kind: load
      src_ptr:
        kind: var
        name: t.0
      dst:
        kind: var
        name: t.1
    - instruction:
      kind: copy
      src:
        kind: var
        name: t.1
      dst:
        kind: var
        name: y
)");
}

// ---------------------------------------------------------------------------
// Array subscript — task #4
// ---------------------------------------------------------------------------

// a[i] in rvalue context: ADD_PTR + LOAD + COPY into the local.
TEST_F(TranslateTest, SubscriptRead)
{
    std::string yaml = CompileToYaml("void f(int *a, long i) { int y = a[i]; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: a
    - param: i
  body:
    - instruction:
      kind: add_ptr
      ptr:
        kind: var
        name: a
      index:
        kind: var
        name: i
      scale: 4
      dst:
        kind: var
        name: t.0
    - instruction:
      kind: load
      src_ptr:
        kind: var
        name: t.0
      dst:
        kind: var
        name: t.1
    - instruction:
      kind: copy
      src:
        kind: var
        name: t.1
      dst:
        kind: var
        name: y
)");
}

// a[i] = 5: ADD_PTR + STORE.
TEST_F(TranslateTest, SubscriptWrite)
{
    std::string yaml = CompileToYaml("void f(int *a, long i) { a[i] = 5; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: a
    - param: i
  body:
    - instruction:
      kind: add_ptr
      ptr:
        kind: var
        name: a
      index:
        kind: var
        name: i
      scale: 4
      dst:
        kind: var
        name: t.0
    - instruction:
      kind: store
      src:
        kind: constant
        const:
          kind: int
          value: 5
      dst_ptr:
        kind: var
        name: t.0
)");
}

// a[i]++: ADD_PTR + LOAD + BINARY(add 1) + STORE; returns old value (discarded).
TEST_F(TranslateTest, SubscriptPostInc)
{
    std::string yaml = CompileToYaml("void f(int *a, long i) { a[i]++; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: a
    - param: i
  body:
    - instruction:
      kind: add_ptr
      ptr:
        kind: var
        name: a
      index:
        kind: var
        name: i
      scale: 4
      dst:
        kind: var
        name: t.0
    - instruction:
      kind: load
      src_ptr:
        kind: var
        name: t.0
      dst:
        kind: var
        name: t.1
    - instruction:
      kind: binary
      op: add
      src1:
        kind: var
        name: t.1
      src2:
        kind: constant
        const:
          kind: int
          value: 1
      dst:
        kind: var
        name: t.2
    - instruction:
      kind: store
      src:
        kind: var
        name: t.2
      dst_ptr:
        kind: var
        name: t.0
)");
}
