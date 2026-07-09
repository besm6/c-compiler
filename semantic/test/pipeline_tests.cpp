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

// A non-void body ending in a call to a user-defined `_Noreturn` function does
// not fall off the end — accepted (the noreturn flag is threaded onto the symbol).
TEST_F(PipelineTest, NonVoidEndsInNoreturnCall_Ok)
{
    RunPipeline(R"(_Noreturn void die(void);
int f(int x) {
    die();
})");
    EXPECT_NE(program, nullptr);
}

// The same body calling a function that is NOT _Noreturn is still rejected.
TEST_F(PipelineTest, NonVoidEndsInPlainCall_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void g(void);
int f(int x) {
    g();
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

// --- Constant folding of real and mixed expressions --------------------------

// Returns the sole static initializer of a file-scope variable.
static const Tac_StaticInit *sole_init(const char *name)
{
    const Symbol *sym = symtab_get(name);
    EXPECT_NE(sym, nullptr);
    EXPECT_EQ(sym->u.static_var.init_kind, INIT_INITIALIZED);
    return sym->u.static_var.init_list;
}

// A real static initializer that is not a bare literal must still fold.  Each of these
// parses as EXPR_UNARY_OP / EXPR_BINARY_OP / EXPR_CAST over a literal, none of which the
// folder used to reach for a floating target.
TEST_F(PipelineTest, RealStaticInitConstExpr)
{
    RunPipeline(R"(double neg = -0.5;
double neg_int = -1;
double plus = +2.5;
double quotient = 1.0 / 4.0;
double mixed = 2.5 * 2;
double compare = (1.5 < 2.0);
double lognot = !0.0;
double cast = (double)-1;
float negf = -0.5f;
)");

    ASSERT_NE(sole_init("neg"), nullptr);
    EXPECT_EQ(sole_init("neg")->kind, TAC_STATIC_INIT_DOUBLE);
    EXPECT_EQ(sole_init("neg")->u.double_val, -0.5);
    EXPECT_EQ(sole_init("neg_int")->u.double_val, -1.0);
    EXPECT_EQ(sole_init("plus")->u.double_val, 2.5);
    EXPECT_EQ(sole_init("quotient")->u.double_val, 0.25);
    EXPECT_EQ(sole_init("mixed")->u.double_val, 5.0);
    // A comparison and a logical NOT yield an int, which converts to the real target.
    EXPECT_EQ(sole_init("compare")->u.double_val, 1.0);
    EXPECT_EQ(sole_init("lognot")->u.double_val, 1.0);
    EXPECT_EQ(sole_init("cast")->u.double_val, -1.0);

    ASSERT_NE(sole_init("negf"), nullptr);
    EXPECT_EQ(sole_init("negf")->kind, TAC_STATIC_INIT_FLOAT);
    EXPECT_EQ(sole_init("negf")->u.float_val, -0.5);
}

// A real element inside an aggregate initializer folds through the same recursion.
TEST_F(PipelineTest, RealArrayStaticInitConstExpr)
{
    RunPipeline("double a[] = { -1.5, 2.5 };");

    const Tac_StaticInit *first = sole_init("a");
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->kind, TAC_STATIC_INIT_DOUBLE);
    EXPECT_EQ(first->u.double_val, -1.5);
    ASSERT_NE(first->next, nullptr);
    EXPECT_EQ(first->next->u.double_val, 2.5);
}

// A real constant expression converts to an integer target by truncation toward zero.
TEST_F(PipelineTest, IntStaticInitFromRealConstExpr)
{
    RunPipeline("int trunc = -1.5; int cast = (int)2.9;");

    EXPECT_EQ(sole_init("trunc")->u.long_val, -1);
    EXPECT_EQ(sole_init("cast")->u.long_val, 2);
}

// A cast wraps to the cast type's own width and signedness, not the target's.  Both
// initializers are wider than the cast, so an unnarrowed fold would survive downstream
// truncation and show up here: 300 instead of 44, and -1 instead of 4294967295.
TEST_F(PipelineTest, ConstExprCastNarrowsToCastType)
{
    RunPipeline("int narrowed = (char)300; long widened = (unsigned)-1;");

    EXPECT_EQ(sole_init("narrowed")->u.long_val, 44);
    EXPECT_EQ(sole_init("widened")->u.long_val, 4294967295L); // x86_64: unsigned is 32-bit
}

// A cast and a real operand in a constant expression.  A struct-member _Static_assert is
// the vehicle: its condition goes through try_eval_const_int, so a wrong fold either fails
// the assert or reports "not a constant expression".  (A file-scope _Static_assert parses
// but is never evaluated, so it would silently pass whatever we wrote.)
TEST_F(PipelineTest, ConstExprFoldsCastsAndReals)
{
    RunPipeline(R"(struct S {
    _Static_assert((char)300 == 44, "cast narrows to char");
    _Static_assert((int)1.5 == 1, "real converts to int");
    _Static_assert((int)-1.9 == -1, "real truncates toward zero");
    _Static_assert(1.5 < 2.0, "real comparison yields int");
    _Static_assert(2.5 * 2.0 == 5.0, "real arithmetic");
    _Static_assert(!0, "logical not on int");
    _Static_assert(!1.5 == 0, "logical not on real");
    _Static_assert(!0.0 == 1, "logical not yields int");
    int x;
};
)");
    EXPECT_TRUE(structtab_exists("S"));
}

// `!` folds, so it is usable where an integer constant expression is required.
TEST_F(PipelineTest, LogicalNotFoldsAsArraySize)
{
    RunPipeline("int a[!0];");

    const Symbol *a = symtab_get("a");
    ASSERT_NE(a, nullptr);
    ASSERT_EQ(a->type->kind, TYPE_ARRAY);
    ASSERT_NE(a->type->u.array.size, nullptr);
    ASSERT_EQ(a->type->u.array.size->kind, EXPR_LITERAL);
    EXPECT_EQ(a->type->u.array.size->u.literal->u.int_val, 1);
}

// A bare real is not an *integer* constant expression (C11 §6.6p6); only a cast makes
// it one.  Guards the `is_real` rejection in the try_eval_const_int wrapper.
TEST_F(PipelineTest, BareRealIsNotIntegerConstExpr_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(struct S {
    _Static_assert(1.5, "not an integer constant expression");
    int x;
};
)"),
                 "_Static_assert condition is not a constant expression");
}

// The new floating-scalar path must not accept a non-constant initializer.
TEST_F(PipelineTest, RealStaticInitFromVariable_Neg)
{
    EXPECT_DEATH(RunPipeline("double a; double b = -a;"), "Static initializer is not a constant");
}

// Division by zero is not a constant expression: reject rather than fold an infinity.
TEST_F(PipelineTest, RealStaticInitDivideByZero_Neg)
{
    EXPECT_DEATH(RunPipeline("double z = 1.0 / 0.0;"), "Static initializer is not a constant");
}
