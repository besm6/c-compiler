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
