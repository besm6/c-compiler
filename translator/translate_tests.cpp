#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "parser.h"
#include "structtab.h"
#include "symtab.h"
#include "tac.h"
#include "translate.h"
#include "xalloc.h"

class TranslateTest : public ::testing::Test {
    FILE *input_file{};

protected:
    Program *program{};

    void SetUp() override
    {
        input_file = tmpfile();
        ASSERT_NE(nullptr, input_file);
    }

    void TearDown() override
    {
        fclose(input_file);
        if (program) {
            free_program(program);
        }
        symtab_destroy();
        structtab_destroy();
        xreport_lost_memory();
        EXPECT_EQ(xtotal_allocated_size(), 0);
        xfree_all();
    }

    // Parse C source, run the full tacker pipeline on each declaration, and
    // return the concatenated YAML output for every translated toplevel.
    std::string CompileToYaml(const char *src)
    {
        fwrite(src, 1, strlen(src), input_file);
        rewind(input_file);
        program = parse(input_file);
        EXPECT_NE(nullptr, program);

        std::string result;
        ExternalDecl *decls = program->decls;
        program->decls      = nullptr;
        while (decls) {
            ExternalDecl *next = decls->next;
            decls->next        = nullptr;
            typecheck_global_decl(decls);
            label_loops(decls);
            Tac_TopLevel *tac = translate(decls);
            free_external_decl(decls);
            if (tac) {
                FILE *f = tmpfile();
                EXPECT_NE(nullptr, f);
                tac_export_yaml(f, tac);
                long len = ftell(f);
                rewind(f);
                std::string yaml(static_cast<size_t>(len), '\0');
                fread(&yaml[0], 1, static_cast<size_t>(len), f);
                fclose(f);
                result += yaml;
                tac_free_toplevel(tac);
            }
            decls = next;
        }
        return result;
    }
};

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
        name: x
    - instruction:
      kind: return
      src:
        kind: var
        name: x
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
        name: x
    - instruction:
      kind: copy
      src:
        kind: constant
        const:
          kind: int
          value: 2
      dst:
        kind: var
        name: y
    - instruction:
      kind: return
      src:
        kind: var
        name: x
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
        name: x
    - instruction:
      kind: binary
      op: add
      src1:
        kind: var
        name: x
      src2:
        kind: constant
        const:
          kind: int
          value: 1
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

// for-init declaration emits COPY before the loop test label.
TEST_F(TranslateTest, ForLoopInitDecl)
{
    std::string yaml = CompileToYaml(
        "int f(void) { for (int i = 5; ; ) { return i; } return 0; }");
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
        name: i
    - instruction:
      kind: label
      name: t.0
    - instruction:
      kind: return
      src:
        kind: var
        name: i
    - instruction:
      kind: label
      name: .L1
    - instruction:
      kind: jump
      target: t.0
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
