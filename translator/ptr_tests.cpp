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

// Parser bug: local-variable declarators strip all pointer stars beyond the first.
// `int **pp;` gets type `int *` instead of `int **`, so typecheck rejects the second
// dereference with "Tried to dereference non-pointer".
// Re-enable and remove the workaround in DerefDerefPointer once the parser is fixed.
TEST_F(TranslateTest, DISABLED_DerefDerefLocalVar)
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

// **pp emits two LOADs, validating the mutual recursion between gen_lval and gen_expr.
// Uses a function parameter for int** because local-var declarations lose the second
// pointer level in the current parser (see DISABLED_DerefDerefLocalVar).
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
