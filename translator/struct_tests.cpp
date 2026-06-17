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
      kind: allocate_local
      name: %s
      size: 12
      alignment: 6
    - instruction:
      kind: copy_from_offset
      src: %s
      offset: 0
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
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
      kind: allocate_local
      name: %s
      size: 12
      alignment: 6
    - instruction:
      kind: copy_from_offset
      src: %s
      offset: 6
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
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
      kind: allocate_local
      name: %s
      size: 12
      alignment: 6
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 5
      dst: %s
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
    - param: %p
  body:
    - instruction:
      kind: add_ptr
      ptr:
        kind: var
        name: %p
      index:
        kind: constant
        const:
          kind: int
          value: 0
      scale: 1
      dst:
        kind: var
        name: %0
    - instruction:
      kind: load
      src_ptr:
        kind: var
        name: %0
      dst:
        kind: var
        name: %1
    - instruction:
      kind: return
      src:
        kind: var
        name: %1
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
    - param: %p
  body:
    - instruction:
      kind: add_ptr
      ptr:
        kind: var
        name: %p
      index:
        kind: constant
        const:
          kind: int
          value: 0
      scale: 1
      dst:
        kind: var
        name: %0
    - instruction:
      kind: store
      src:
        kind: constant
        const:
          kind: int
          value: 5
      dst_ptr:
        kind: var
        name: %0
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
      kind: allocate_local
      name: %s
      size: 12
      alignment: 6
    - instruction:
      kind: get_address
      src:
        kind: var
        name: %s
      dst:
        kind: var
        name: %0
    - instruction:
      kind: add_ptr
      ptr:
        kind: var
        name: %0
      index:
        kind: constant
        const:
          kind: int
          value: 0
      scale: 1
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
        name: %p
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
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 0
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
        name: %quz
    - instruction:
      kind: add_ptr
      ptr:
        kind: var
        name: %quz
      index:
        kind: constant
        const:
          kind: int
          value: 0
      scale: 1
      dst:
        kind: var
        name: %1
    - instruction:
      kind: load
      src_ptr:
        kind: var
        name: %1
      dst:
        kind: var
        name: %2
    - instruction:
      kind: return
      src:
        kind: var
        name: %2
)");
}

// ---------------------------------------------------------------------------
// Struct-by-value return: hidden-pointer (sret) ABI, lowered in the translator.
// On the BESM-6 target a struct larger than one 6-byte word (here struct T = two ints =
// 12 bytes) is returned through a hidden pointer; a one-word struct stays on the
// accumulator path.
// ---------------------------------------------------------------------------

// Callee: a multi-word struct return prepends a hidden pointer param (%.ret), copies the
// result word by word into *%.ret via STORE, and returns the pointer itself.
TEST_F(TranslateTest, StructByValueReturnCallee)
{
    std::string yaml = CompileToYaml(
        "struct T { int a; int b; };"
        "struct T mk(void) { struct T t; t.a = 1; t.b = 2; return t; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: mk
  global: true
  params:
    - param: %.ret
  body:
    - instruction:
      kind: allocate_local
      name: %t
      size: 12
      alignment: 6
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 1
      dst: %t
      offset: 0
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 2
      dst: %t
      offset: 6
    - instruction:
      kind: copy_from_offset
      src: %t
      offset: 0
      dst:
        kind: var
        name: %0
    - instruction:
      kind: add_ptr
      ptr:
        kind: var
        name: %.ret
      index:
        kind: constant
        const:
          kind: int
          value: 0
      scale: 6
      dst:
        kind: var
        name: %1
    - instruction:
      kind: store
      src:
        kind: var
        name: %0
      dst_ptr:
        kind: var
        name: %1
    - instruction:
      kind: copy_from_offset
      src: %t
      offset: 6
      dst:
        kind: var
        name: %2
    - instruction:
      kind: add_ptr
      ptr:
        kind: var
        name: %.ret
      index:
        kind: constant
        const:
          kind: int
          value: 1
      scale: 6
      dst:
        kind: var
        name: %3
    - instruction:
      kind: store
      src:
        kind: var
        name: %2
      dst_ptr:
        kind: var
        name: %3
    - instruction:
      kind: return
      src:
        kind: var
        name: %.ret
)");
}

// Caller: a call returning a multi-word struct allocates a result slot, passes its
// address as the hidden first argument, and the call carries no scalar destination; the
// struct then lives in the slot and is copied into the destination word by word.
TEST_F(TranslateTest, StructByValueReturnCaller)
{
    std::string yaml = CompileToYaml(
        "struct T { int a; int b; };"
        "struct T mk(void);"
        "void use(void) { struct T r = mk(); }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: use
  global: true
  body:
    - instruction:
      kind: allocate_local
      name: %r
      size: 12
      alignment: 6
    - instruction:
      kind: allocate_local
      name: %0
      size: 12
      alignment: 6
    - instruction:
      kind: get_address
      src:
        kind: var
        name: %0
      dst:
        kind: var
        name: %1
    - instruction:
      kind: fun_call
      fun_name: mk
      args:
        - val:
          kind: var
          name: %1
    - instruction:
      kind: copy_from_offset
      src: %0
      offset: 0
      dst:
        kind: var
        name: %2
    - instruction:
      kind: copy_to_offset
      src:
        kind: var
        name: %2
      dst: %r
      offset: 0
    - instruction:
      kind: copy_from_offset
      src: %0
      offset: 6
      dst:
        kind: var
        name: %3
    - instruction:
      kind: copy_to_offset
      src:
        kind: var
        name: %3
      dst: %r
      offset: 6
)");
}

// A struct that fits in one word still returns through the accumulator (no hidden
// param, no STORE) — the sret rewrite must not trigger.
TEST_F(TranslateTest, StructByValueReturnOneWordUsesAccumulator)
{
    std::string yaml = CompileToYaml(
        "struct S { int a; };"
        "struct S mk(void) { struct S s; s.a = 7; return s; }");
    EXPECT_EQ(yaml.find("param: %.ret"), std::string::npos);
    EXPECT_EQ(yaml.find("kind: store"), std::string::npos);
    EXPECT_NE(yaml.find("name: %s"), std::string::npos); // returns the struct slot directly
}
