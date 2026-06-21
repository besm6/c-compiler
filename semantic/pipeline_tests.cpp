#include "typecheck_fixture.h"

// Struct definition through the full pipeline.
// Verifies that struct registration does not double-register in structtab.
TEST_F(PipelineTest, StructDecl)
{
    RunPipeline("struct S { int x; double y; };");

    const StructDef *sd = structtab_find("S");
    ASSERT_NE(sd, nullptr);
    EXPECT_EQ(sd->alignment, 8);
    EXPECT_EQ(sd->size, 16);
    ASSERT_NE(sd->members, nullptr);
    EXPECT_STREQ(sd->members->name, "x");
    EXPECT_EQ(sd->members->type->kind, TYPE_INT);
    EXPECT_EQ(sd->members->offset, 0);
    ASSERT_NE(sd->members->next, nullptr);
    EXPECT_STREQ(sd->members->next->name, "y");
    EXPECT_EQ(sd->members->next->type->kind, TYPE_DOUBLE);
    EXPECT_EQ(sd->members->next->offset, 8);
    EXPECT_EQ(sd->members->next->next, nullptr);
}

// Struct definition followed by a variable of that struct type.
TEST_F(PipelineTest, StructUsedInVar)
{
    RunPipeline("struct S { int x; }; struct S s;");

    EXPECT_TRUE(structtab_exists("S"));

    const Symbol *s = symtab_get("s");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->kind, SYM_STATIC);
    EXPECT_TRUE(s->u.static_var.global);
    EXPECT_EQ(s->u.static_var.init_kind, INIT_TENTATIVE);
    ASSERT_NE(s->type, nullptr);
    EXPECT_EQ(s->type->kind, TYPE_STRUCT);
}

// Function prototype followed by definition.
// Verifies that symtab_add_fun() sets has_linkage so the redeclaration
// does not fatal with "Duplicate declaration".
TEST_F(PipelineTest, FunctionPrototypeThenDefinition)
{
    RunPipeline("int f(void); int f(void) { return 1; }");

    const Symbol *f = symtab_get("f");
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->kind, SYM_FUNC);
    EXPECT_TRUE(f->u.func.defined);
    EXPECT_TRUE(f->u.func.global);
    ASSERT_NE(f->type, nullptr);
    EXPECT_EQ(f->type->kind, TYPE_FUNCTION);
}

// File-scope declaration with multiple declarators.
// Verifies that typecheck_file_scope_var_decl() loops over all InitDeclarators.
TEST_F(PipelineTest, FileVarMultipleDeclarators)
{
    RunPipeline("int x = 1, y = 2;");

    const Symbol *x = symtab_get("x");
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(x->kind, SYM_STATIC);
    EXPECT_TRUE(x->u.static_var.global);
    EXPECT_EQ(x->u.static_var.init_kind, INIT_INITIALIZED);
    ASSERT_NE(x->u.static_var.init_list, nullptr);
    EXPECT_EQ(x->u.static_var.init_list->kind, TAC_STATIC_INIT_I64);
    EXPECT_EQ(x->u.static_var.init_list->u.long_val, 1);

    const Symbol *y = symtab_get("y");
    ASSERT_NE(y, nullptr);
    EXPECT_EQ(y->kind, SYM_STATIC);
    EXPECT_TRUE(y->u.static_var.global);
    EXPECT_EQ(y->u.static_var.init_kind, INIT_INITIALIZED);
    ASSERT_NE(y->u.static_var.init_list, nullptr);
    EXPECT_EQ(y->u.static_var.init_list->kind, TAC_STATIC_INIT_I64);
    EXPECT_EQ(y->u.static_var.init_list->u.long_val, 2);
}

// Static and extern file-scope variables: linkage and init_kind.
TEST_F(PipelineTest, StaticAndExternVars)
{
    RunPipeline("static int a; extern int b;");

    const Symbol *a = symtab_get("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->kind, SYM_STATIC);
    EXPECT_FALSE(a->u.static_var.global);
    EXPECT_EQ(a->u.static_var.init_kind, INIT_TENTATIVE);

    const Symbol *b = symtab_get("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->kind, SYM_STATIC);
    EXPECT_TRUE(b->u.static_var.global);
    EXPECT_EQ(b->u.static_var.init_kind, INIT_NONE);
}

// _Static_assert with a true condition inside a struct is accepted.
TEST_F(PipelineTest, StaticAssertInStructPasses)
{
    RunPipeline("struct S { _Static_assert(1, \"ok\"); int x; };");

    const StructDef *sd = structtab_find("S");
    ASSERT_NE(sd, nullptr);
    ASSERT_NE(sd->members, nullptr);
    EXPECT_STREQ(sd->members->name, "x");
    EXPECT_EQ(sd->members->type->kind, TYPE_INT);
}

// _Static_assert with a false condition inside a struct is a compile-time error.
TEST_F(PipelineTest, StaticAssertInStructFails)
{
    ParseProgram("struct S { _Static_assert(0, \"bad\"); int x; };");
    ASSERT_EXIT(typecheck_program(program), ::testing::ExitedWithCode(1),
                "_Static_assert failed: bad");
}

// _Static_assert with a true condition inside a union is accepted.
TEST_F(PipelineTest, StaticAssertInUnionPasses)
{
    RunPipeline("union U { _Static_assert(1, \"ok\"); int x; };");

    const StructDef *sd = structtab_find("U");
    ASSERT_NE(sd, nullptr);
    ASSERT_NE(sd->members, nullptr);
    EXPECT_STREQ(sd->members->name, "x");
}

// --- Missing-return diagnostic & main's implicit return 0 -------------------

// A non-void, non-main function whose body can fall off the end is rejected.
TEST_F(PipelineTest, NonVoidFallsOffEnd_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int f(void) {
    int x = 1;
}
)"),
                 "may fall off the end");
}

// A function that returns on every path is accepted.
TEST_F(PipelineTest, NonVoidAllPathsReturn_Ok)
{
    RunPipeline(R"(int f(int x) {
    if (x)
        return 1;
    else
        return 2;
})");
    EXPECT_NE(program, nullptr);
}

// An infinite loop with no exit makes the end unreachable — accepted.
TEST_F(PipelineTest, NonVoidInfiniteLoop_Ok)
{
    RunPipeline(R"(int f(int x) {
    for (;;) {
        if (x)
            return x;
    }
})");
    EXPECT_NE(program, nullptr);
}

// A switch is conservatively treated as not falling through, so an exhaustive
// switch is never flagged (matches the runtime library's strerror()).
TEST_F(PipelineTest, ExhaustiveSwitch_Ok)
{
    RunPipeline(R"(int f(int x) {
    switch (x) {
    case 1:
        return 1;
    default:
        return 0;
    }
})");
    EXPECT_NE(program, nullptr);
}

// A void function may fall off the end with no diagnostic.
TEST_F(PipelineTest, VoidFallsOffEnd_Ok)
{
    RunPipeline(R"(void f(void) {
    int x = 1;
})");
    EXPECT_NE(program, nullptr);
}

// main() falling off the end is not an error; the typechecker appends an
// implicit `return 0;` (C11 §5.1.2.2.3).
TEST_F(PipelineTest, MainFallsOffEndGetsImplicitReturnZero)
{
    RunPipeline("int main(void) { }");

    ExternalDecl *fn = program->decls;
    ASSERT_EQ(fn->kind, EXTERNAL_DECL_FUNCTION);
    DeclOrStmt *it = fn->u.function.body->u.compound;
    ASSERT_NE(it, nullptr);
    while (it->next) {
        it = it->next;
    }
    ASSERT_EQ(it->kind, DECL_OR_STMT_STMT);
    Stmt *last = it->u.stmt;
    ASSERT_EQ(last->kind, STMT_RETURN);
    ASSERT_NE(last->u.expr, nullptr);
    EXPECT_EQ(last->u.expr->kind, EXPR_LITERAL);
    EXPECT_EQ(last->u.expr->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(last->u.expr->u.literal->u.int_val, 0);
}
