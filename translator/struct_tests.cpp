#include "translate_test.h"

// ---------------------------------------------------------------------------
// Struct field read/write — task #6
// ---------------------------------------------------------------------------

// s.x in rvalue context emits COPY_FROM_OFFSET for the first field (offset 0).
TEST_F(TranslateTest, StructFieldReadFirst)
{
    std::string yaml = CompileToYaml(
        "struct Foo { int x; int y; };"
        "int f(void) { struct Foo s; return s.x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: copy_from_offset
      src: s
      offset: 0
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

// s.y in rvalue context emits COPY_FROM_OFFSET with the correct byte offset (4).
TEST_F(TranslateTest, StructFieldReadSecond)
{
    std::string yaml = CompileToYaml(
        "struct Foo { int x; int y; };"
        "int f(void) { struct Foo s; return s.y; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: copy_from_offset
      src: s
      offset: 4
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

// s.x = val emits COPY_TO_OFFSET (direct-variable optimization).
TEST_F(TranslateTest, StructFieldWrite)
{
    std::string yaml = CompileToYaml(
        "struct Foo { int x; int y; };"
        "void f(void) { struct Foo s; s.x = 5; }");
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
          value: 5
      dst: s
      offset: 0
)");
}

// ---------------------------------------------------------------------------
// Pointer-to-struct field access — task #7
// ---------------------------------------------------------------------------

// p->x in rvalue context emits ADD_PTR + LOAD.
TEST_F(TranslateTest, PtrFieldRead)
{
    std::string yaml = CompileToYaml(
        "struct Foo { int x; int y; };"
        "int f(struct Foo *p) { return p->x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: p
  body:
    - instruction:
      kind: add_ptr
      ptr:
        kind: var
        name: p
      index:
        kind: constant
        const:
          kind: int
          value: 0
      scale: 1
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
      kind: return
      src:
        kind: var
        name: t.1
)");
}

// p->x = 5 emits ADD_PTR + STORE.
TEST_F(TranslateTest, PtrFieldWrite)
{
    std::string yaml = CompileToYaml(
        "struct Foo { int x; int y; };"
        "void f(struct Foo *p) { p->x = 5; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  params:
    - param: p
  body:
    - instruction:
      kind: add_ptr
      ptr:
        kind: var
        name: p
      index:
        kind: constant
        const:
          kind: int
          value: 0
      scale: 1
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

// &s.x emits GET_ADDRESS then ADD_PTR (scale=1, index=byte offset).
TEST_F(TranslateTest, StructFieldAddressOf)
{
    std::string yaml = CompileToYaml(
        "struct Foo { int x; int y; };"
        "void f(void) { struct Foo s; int *p = &s.x; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: get_address
      src:
        kind: var
        name: s
      dst:
        kind: var
        name: t.0
    - instruction:
      kind: add_ptr
      ptr:
        kind: var
        name: t.0
      index:
        kind: constant
        const:
          kind: int
          value: 0
      scale: 1
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
        name: p
)");
}

TEST_F(TranslateTest, LocalStructCast)
{
    std::string yaml = CompileToYaml(R"(
        char *foo()
        {
            struct a {
                char *bar;
            } *quz;
            quz = (struct a *)0;
            return quz->bar;
        }
    )");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: foo
  global: true
  body:
    - instruction:
      kind: sign_extend
      src:
        kind: constant
        const:
          kind: int
          value: 0
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
        name: quz
    - instruction:
      kind: add_ptr
      ptr:
        kind: var
        name: quz
      index:
        kind: constant
        const:
          kind: int
          value: 0
      scale: 1
      dst:
        kind: var
        name: t.1
    - instruction:
      kind: load
      src_ptr:
        kind: var
        name: t.1
      dst:
        kind: var
        name: t.2
    - instruction:
      kind: return
      src:
        kind: var
        name: t.2
)");
}
