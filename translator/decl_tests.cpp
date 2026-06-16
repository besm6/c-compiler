#include "translate_test.h"

// ---------------------------------------------------------------------------
// Local variable declarations — task #1
// ---------------------------------------------------------------------------

// A variable with no initializer emits no COPY; only the return is generated.
TEST_F(TranslateTest, LocalVarNoInit)
{
    std::string yaml = CompileToYaml("int f(void) { int x; return 0; }");
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
          value: 0
)");
}

// A variable with a constant initializer emits COPY dst ← constant.
TEST_F(TranslateTest, LocalVarWithIntInit)
{
    std::string yaml = CompileToYaml("int f(void) { int x = 42; return x; }");
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

// Two local variables each emit their own COPY.
TEST_F(TranslateTest, TwoLocalVars)
{
    std::string yaml = CompileToYaml("int f(void) { int x = 1; int y = 2; return x; }");
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
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 2
      dst:
        kind: var
        name: %y
    - instruction:
      kind: return
      src:
        kind: var
        name: %x
)");
}

// Initialized variable used in an expression produces COPY then BINARY.
TEST_F(TranslateTest, LocalVarUsedInBinaryExpr)
{
    std::string yaml = CompileToYaml("int f(void) { int x = 10; return x + 1; }");
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
          value: 1
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

// ---------------------------------------------------------------------------
// Char literals — task #1
// ---------------------------------------------------------------------------

TEST_F(TranslateTest, CharLiteralReturnedDirectly)
{
    std::string yaml = CompileToYaml("int f(void) { return 'A'; }");
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
          value: 65
)");
}

// ---------------------------------------------------------------------------
// String literals — task #1
// ---------------------------------------------------------------------------

// A string literal emits a static_constant toplevel node followed by a
// get_address instruction that yields the pointer result.
TEST_F(TranslateTest, StringLiteralReturned)
{
    std::string yaml = CompileToYaml(R"(char *f(void) { return "hi"; })");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: static_constant
  name: _str0
  type:
    kind: array
    elem_type:
      kind: char
    size: 3
  init:
    kind: string
    value: hi
    null_terminated: true
- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: get_address_decay
      src:
        kind: var
        name: _str0
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

// Two functions with distinct string literals each get their own
// static_constant node with unique names.
TEST_F(TranslateTest, TwoFunctionsDistinctStringLiterals)
{
    std::string yaml = CompileToYaml(
        "char *f(void) { return \"yes\"; }\n"
        "char *g(void) { return \"no\"; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: static_constant
  name: _str0
  type:
    kind: array
    elem_type:
      kind: char
    size: 4
  init:
    kind: string
    value: yes
    null_terminated: true
- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: get_address_decay
      src:
        kind: var
        name: _str0
      dst:
        kind: var
        name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %0
- toplevel:
  kind: static_constant
  name: _str1
  type:
    kind: array
    elem_type:
      kind: char
    size: 3
  init:
    kind: string
    value: no
    null_terminated: true
- toplevel:
  kind: function
  name: g
  global: true
  body:
    - instruction:
      kind: get_address_decay
      src:
        kind: var
        name: _str1
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

// ---------------------------------------------------------------------------
// Enum constant literals — task #1
// ---------------------------------------------------------------------------

// Enumerators without explicit values start at 0 and auto-increment.
TEST_F(TranslateTest, EnumConstDefaultValues)
{
    std::string yaml = CompileToYaml("enum Color { RED, GREEN }; int f(void) { return GREEN; }");
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
          value: 1
)");
}

// An explicit initializer overrides the auto-increment.
TEST_F(TranslateTest, EnumConstExplicitValue)
{
    std::string yaml = CompileToYaml("enum Color { RED = 5, GREEN }; int f(void) { return RED; }");
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
          value: 5
)");
}

// Enum declared inside a function body is scoped to that body.
TEST_F(TranslateTest, EnumConstLocalDecl)
{
    std::string yaml = CompileToYaml("int f(void) { enum Dir { UP = 10, DOWN }; return DOWN; }");
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
          value: 11
)");
}

// for-init declaration emits COPY before the loop test label.
TEST_F(TranslateTest, ForLoopInitDecl)
{
    std::string yaml = CompileToYaml("int f(void) { for (int i = 5; ; ) { return i; } return 0; }");
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
        name: %i
    - instruction:
      kind: label
      name: %0
    - instruction:
      kind: return
      src:
        kind: var
        name: %i
    - instruction:
      kind: label
      name: %L1
    - instruction:
      kind: jump
      target: %0
    - instruction:
      kind: label
      name: %L0
    - instruction:
      kind: return
      src:
        kind: constant
        const:
          kind: int
          value: 0
)");
}

// ---------------------------------------------------------------------------
// Global declarations
// ---------------------------------------------------------------------------

// Tentative global variable (no initializer) emits static_variable with no init_list.
TEST_F(TranslateTest, GlobalVarTentative)
{
    std::string yaml = CompileToYaml("int x;");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: static_variable
  name: x
  global: true
  type:
    kind: int
)");
}

// Initialized global variable emits static_variable with i32 init.
TEST_F(TranslateTest, GlobalVarInitialized)
{
    std::string yaml = CompileToYaml("int x = 42;");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: static_variable
  name: x
  global: true
  type:
    kind: int
  init_list:
    - init:
      kind: i32
      value: 42
)");
}

// Initialized global short variable emits static_variable with i16 init.
TEST_F(TranslateTest, GlobalVarShortInitialized)
{
    std::string yaml = CompileToYaml("short x = 42;");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: static_variable
  name: x
  global: true
  type:
    kind: short
  init_list:
    - init:
      kind: i16
      value: 42
)");
}

// Initialized global unsigned short variable emits static_variable with u16 init.
TEST_F(TranslateTest, GlobalVarUshortInitialized)
{
    std::string yaml = CompileToYaml("unsigned short x = 1000;");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: static_variable
  name: x
  global: true
  type:
    kind: ushort
  init_list:
    - init:
      kind: u16
      value: 1000
)");
}

// Static global variable sets global=false.
TEST_F(TranslateTest, GlobalVarStatic)
{
    std::string yaml = CompileToYaml("static int x = 5;");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: static_variable
  name: x
  global: false
  type:
    kind: int
  init_list:
    - init:
      kind: i32
      value: 5
)");
}

// extern declaration emits no TAC — no storage is allocated.
TEST_F(TranslateTest, GlobalVarExtern)
{
    std::string yaml = CompileToYaml("extern int x;");
    EXPECT_EQ(yaml, "");
}

// Inline struct definition inside an extern array declaration allocates no
// storage, but emits a declare_array so a backend knows the name is an array
// (decays to its label address) rather than a pointer.
TEST_F(TranslateTest, ExternArrayOfInlineStruct)
{
    std::string yaml = CompileToYaml("extern struct S { int x; } arr[];");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: declare_array
  name: arr
  size: 0
)");
}

// Incomplete extern array declaration allocates no storage, but emits a
// declare_array (size 0, since the bound is unknown).
TEST_F(TranslateTest, GlobalVarExternIncompleteArray)
{
    std::string yaml = CompileToYaml("extern int icode[];");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: declare_array
  name: icode
  size: 0
)");
}

// Multi-declarator global declaration emits one static_variable per declarator.
TEST_F(TranslateTest, GlobalVarMultiDeclarator)
{
    std::string yaml = CompileToYaml("int x = 1, y = 2;");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: static_variable
  name: x
  global: true
  type:
    kind: int
  init_list:
    - init:
      kind: i32
      value: 1
- toplevel:
  kind: static_variable
  name: y
  global: true
  type:
    kind: int
  init_list:
    - init:
      kind: i32
      value: 2
)");
}

// Struct/enum/typedef-only declarations produce no TAC output.
TEST_F(TranslateTest, StructDeclOnly)
{
    EXPECT_EQ(CompileToYaml("struct Foo { int x; };"), "");
}

TEST_F(TranslateTest, EnumDeclOnly)
{
    EXPECT_EQ(CompileToYaml("enum Color { RED, GREEN, BLUE };"), "");
}

TEST_F(TranslateTest, TypedefOnly)
{
    EXPECT_EQ(CompileToYaml("typedef int MyInt;"), "");
}

TEST_F(TranslateTest, TypedefLocalVar)
{
    std::string yaml = CompileToYaml(
        "typedef int myint;"
        "int f(void) { myint x = 42; return x; }");
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

// ---------------------------------------------------------------------------
// Compound local-variable initializers — task #2
// ---------------------------------------------------------------------------

// struct Foo s = {1, 2}; emits two COPY_TO_OFFSET instructions.
TEST_F(TranslateTest, LocalStructCompoundInit)
{
    std::string yaml = CompileToYaml(
        "struct Foo { int x; int y; };"
        "void f(void) { struct Foo s = {1, 2}; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: allocate_local
      name: %s
      size: 8
      alignment: 4
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 1
      dst: %s
      offset: 0
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 2
      dst: %s
      offset: 4
)");
}

// int arr[3] = {10, 20, 30}; emits three COPY_TO_OFFSET instructions.
TEST_F(TranslateTest, LocalArrayCompoundInit)
{
    std::string yaml = CompileToYaml("void f(void) { int arr[3] = {10, 20, 30}; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: allocate_local
      name: %arr
      size: 12
      alignment: 4
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 10
      dst: %arr
      offset: 0
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 20
      dst: %arr
      offset: 4
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 30
      dst: %arr
      offset: 8
)");
}

// struct Outer o = {{1, 2}, 3}; — nested struct — emits COPY_TO_OFFSET at offsets 0, 4, 8.
TEST_F(TranslateTest, LocalNestedStructInit)
{
    std::string yaml = CompileToYaml(
        "struct Inner { int a; int b; };"
        "struct Outer { struct Inner in; int c; };"
        "void f(void) { struct Outer o = {{1, 2}, 3}; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: allocate_local
      name: %o
      size: 12
      alignment: 4
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 1
      dst: %o
      offset: 0
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 2
      dst: %o
      offset: 4
    - instruction:
      kind: copy_to_offset
      src:
        kind: constant
        const:
          kind: int
          value: 3
      dst: %o
      offset: 8
)");
}

// An uninitialized local aggregate still emits AllocateLocal so the backend reserves
// its full frame slot (task #23). Size/alignment are in target bytes (x86_64 default
// here): int a[10] -> 40 bytes; struct Foo {int;int} -> 8 bytes; both 4-byte aligned.
TEST_F(TranslateTest, LocalAggregateAllocateLocal)
{
    std::string yaml = CompileToYaml(
        "struct Foo { int x; int y; };"
        "void f(void) { int a[10]; struct Foo s; }");
    EXPECT_EQ(yaml, R"(- toplevel:
  kind: function
  name: f
  global: true
  body:
    - instruction:
      kind: allocate_local
      name: %a
      size: 40
      alignment: 4
    - instruction:
      kind: allocate_local
      name: %s
      size: 8
      alignment: 4
)");
}
