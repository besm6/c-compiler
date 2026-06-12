#include <gtest/gtest.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
void _Noreturn fatal_error(const char *message, ...)
{
    fprintf(stderr, "Fatal error: ");
    va_list ap;
    va_start(ap, message);
    vfprintf(stderr, message, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

#include "optimize.h"
#include "parser.h"
#include "semantic.h"
#include "structtab.h"
#include "symtab.h"
#include "tac.h"
#include "translate.h"
#include "typetab.h"
#include "xalloc.h"
}

// ---------------------------------------------------------------------------
// Test fixture: compile C source, optimize the function body, return YAML.
// ---------------------------------------------------------------------------

class PipelineTest : public ::testing::Test {
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
        if (program)
            free_program(program);
        symtab_destroy();
        structtab_destroy();
        typetab_destroy();
        nametab_destroy();
        xreport_lost_memory();
        EXPECT_EQ(xtotal_allocated_size(), 0);
        xfree_all();
    }

    static std::string capture_instructions(const Tac_Instruction *body)
    {
        FILE *f = tmpfile();
        EXPECT_NE(f, nullptr);
        tac_export_yaml_instruction_list(f, body, 0);
        long len = ftell(f);
        rewind(f);
        std::string yaml(static_cast<size_t>(len), '\0');
        EXPECT_TRUE(fread(&yaml[0], 1, static_cast<size_t>(len), f));
        fclose(f);
        return yaml;
    }

    std::string OptimizeYaml(const char *src,
                             OptFlags flags = opt_flags_default())
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
            typecheck_decl(decls);
            Tac_TopLevel *tac = translate(decls, flags);
            free_external_decl(decls);
            if (tac) {
                for (const Tac_TopLevel *t = tac; t; t = t->next) {
                    if (t->kind == TAC_TOPLEVEL_FUNCTION && t->u.function.body)
                        result += capture_instructions(t->u.function.body);
                }
                tac_free_toplevel(tac);
            }
            decls = next;
        }
        return result;
    }
};

// ---------------------------------------------------------------------------
// Constant folding
// ---------------------------------------------------------------------------

TEST_F(PipelineTest, DivisionConstantFolded)
{
    EXPECT_EQ(OptimizeYaml("int f(void) { return 6/2; }"),
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 3\n");
}

TEST_F(PipelineTest, AdditionConstantFolded)
{
    EXPECT_EQ(OptimizeYaml("int f(void) { return 2+2; }"),
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 4\n");
}

TEST_F(PipelineTest, SubtractionConstantFolded)
{
    EXPECT_EQ(OptimizeYaml("int f(void) { return 10-3; }"),
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 7\n");
}

TEST_F(PipelineTest, MultiplicationConstantFolded)
{
    EXPECT_EQ(OptimizeYaml("int f(void) { return 3*4; }"),
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 12\n");
}

TEST_F(PipelineTest, UnaryNotFolded)
{
    EXPECT_EQ(OptimizeYaml("int f(void) { return !0; }"),
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 1\n");
}

TEST_F(PipelineTest, UnaryComplementFolded)
{
    EXPECT_EQ(OptimizeYaml("int f(void) { return ~0; }"),
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: -1\n");
}

TEST_F(PipelineTest, LongConstantFolded)
{
    EXPECT_EQ(OptimizeYaml("long f(void) { return 3L * 4L; }"),
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: long\n"
        "      value: 12\n");
}

// ---------------------------------------------------------------------------
// Multi-pass interactions
// ---------------------------------------------------------------------------

// Two optimizer iterations needed: first folds (2+3)→5, second folds (5*4)→20.
TEST_F(PipelineTest, TwoPassFolded)
{
    EXPECT_EQ(OptimizeYaml("int f(void) { int a = 2+3; return a*4; }"),
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 20\n");
}

// ---------------------------------------------------------------------------
// Copy propagation
// ---------------------------------------------------------------------------

// Copy(x, t) → Return(t): copy prop rewrites to Return(x); dead store removes Copy.
TEST_F(PipelineTest, CopyPropSimple)
{
    EXPECT_EQ(OptimizeYaml("int g(int x) { int t = x; return t; }"),
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: var\n"
        "    name: %x\n");
}

// Chain a=x, b=a → Return(b) fully propagated to Return(x).
TEST_F(PipelineTest, CopyPropChain)
{
    EXPECT_EQ(OptimizeYaml("int g(int x) { int a = x; int b = a; return b; }"),
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: var\n"
        "    name: %x\n");
}

// ---------------------------------------------------------------------------
// Dead store elimination
// ---------------------------------------------------------------------------

// Copy(3, t) is dead (overwritten); Copy(4, t) propagated into Return; then also dead.
TEST_F(PipelineTest, DeadStoreOverwrite)
{
    EXPECT_EQ(OptimizeYaml("int f(void) { int t = 3; t = 4; return t; }"),
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 4\n");
}

// ---------------------------------------------------------------------------
// Dead branch elimination
// ---------------------------------------------------------------------------

// if(0): fold JIZ(0,"L")→Jump("L"); then-block unreachable → removed.
TEST_F(PipelineTest, DeadBranchIfZero)
{
    EXPECT_EQ(OptimizeYaml("int h(int x) { if (0) return 1; return 2; }"),
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 2\n");
}

// if(1): fold JIZ(1,"L")→deleted; else-block unreachable → removed.
TEST_F(PipelineTest, DeadBranchIfOne)
{
    EXPECT_EQ(OptimizeYaml("int h(int x) { if (1) return 42; return 0; }"),
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 42\n");
}

// ---------------------------------------------------------------------------
// No optimization needed
// ---------------------------------------------------------------------------

// Direct return of a parameter: no temps, optimizer is a pass-through.
TEST_F(PipelineTest, NoOptNeeded)
{
    EXPECT_EQ(OptimizeYaml("int f(int x) { return x; }"),
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: var\n"
        "    name: %x\n");
}

// ---------------------------------------------------------------------------
// Dead store elimination must not remove loads used as indirect call targets
// ---------------------------------------------------------------------------

// load fp→t.0 must survive: t.0 is the callee in an indirect fun_call.
// Bug: DSE does not count fun_call.fun_name as a use when it is a variable.
TEST_F(PipelineTest, DeadStoreKeepsIndirectCallTarget)
{
    EXPECT_EQ(OptimizeYaml("int f(int (*fp)(int)) { return (*fp)(42); }"),
        "- instruction:\n"
        "  kind: load\n"
        "  src_ptr:\n"
        "    kind: var\n"
        "    name: %fp\n"
        "  dst:\n"
        "    kind: var\n"
        "    name: %0\n"
        "- instruction:\n"
        "  kind: fun_call\n"
        "  fun_name: %0\n"
        "  args:\n"
        "    - val:\n"
        "      kind: constant\n"
        "      const:\n"
        "        kind: int\n"
        "        value: 42\n"
        "  dst:\n"
        "    kind: var\n"
        "    name: %1\n"
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: var\n"
        "    name: %1\n");
}

// ---------------------------------------------------------------------------
// Writes to observable variables survive dead-store elimination
// ---------------------------------------------------------------------------

// Regression: a write to a file-scope global must survive even though the
// global is declared in a separate external declaration. The optimizer learns
// that "g" is not a local of f (f has no automatic locals named g) and so keeps
// the store.
TEST_F(PipelineTest, GlobalWriteSurvivesDeadStore)
{
    EXPECT_EQ(OptimizeYaml("int g; void f(void) { g = 5; }"),
        "- instruction:\n"
        "  kind: copy\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 5\n"
        "  dst:\n"
        "    kind: var\n"
        "    name: g\n");
}

// An `extern` global has no static_variable toplevel at all, yet the per-function
// classification still recognises it as non-local and preserves the store.
TEST_F(PipelineTest, ExternGlobalWriteSurvives)
{
    EXPECT_EQ(OptimizeYaml("extern int g; void f(void) { g = 5; }"),
        "- instruction:\n"
        "  kind: copy\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 5\n"
        "  dst:\n"
        "    kind: var\n"
        "    name: g\n");
}

// Negative control: the dead local `x` is still removed; only the global write
// survives. Guards against over-preserving once globals are kept.
TEST_F(PipelineTest, DeadLocalRemovedAlongsideGlobalWrite)
{
    EXPECT_EQ(OptimizeYaml("int g; void f(void) { int x = 7; g = 5; }"),
        "- instruction:\n"
        "  kind: copy\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 5\n"
        "  dst:\n"
        "    kind: var\n"
        "    name: g\n");
}
