//
// Canonical initializer form produced by normalize_init: brace elision and braced scalars.
//
#include "typecheck.h"
#include "typecheck_fixture.h"

class NormalizeTest : public TypecheckTest {
protected:
    // Parse src, typecheck every external declaration but the last, and normalize the
    // last one's initializer in static mode (leaves stay raw, so no symbols are needed).
    const Initializer *Normalize(const char *src)
    {
        ParseProgram(src);
        ExternalDecl *d = program->decls;
        for (; d->next; d = d->next)
            typecheck_global_decl(d);
        InitDeclarator *id = d->u.declaration->u.var.declarators;
        id->init           = normalize_init(id->type, id->init, INIT_STATIC);
        type               = id->type;
        return id->init;
    }

    const Type *type{};
};

static size_t item_count(const Initializer *init)
{
    size_t n = 0;
    for (const InitItem *item = init->u.items; item; item = item->next)
        n++;
    return n;
}

static const Initializer *item_at(const Initializer *init, size_t i)
{
    const InitItem *item = init->u.items;
    for (; i > 0; i--)
        item = item->next;
    return item->init;
}

static long int_value(const Initializer *init)
{
    EXPECT_EQ(init->kind, INITIALIZER_SINGLE);
    EXPECT_EQ(init->u.expr->kind, EXPR_LITERAL);
    return init->u.expr->u.literal->u.int_val;
}

// A braced scalar is unwrapped (§6.7.9p11).
TEST_F(NormalizeTest, BracedScalar)
{
    const Initializer *init = Normalize("int x = { 5 };");
    EXPECT_EQ(int_value(init), 5);
}

// Missing elements are NULL holes; the node has exactly N items.
TEST_F(NormalizeTest, ArrayHoles)
{
    const Initializer *init = Normalize("int a[4] = { 1, 2 };");
    ASSERT_EQ(init->kind, INITIALIZER_COMPOUND);
    ASSERT_EQ(item_count(init), 4u);
    EXPECT_EQ(int_value(item_at(init, 0)), 1);
    EXPECT_EQ(int_value(item_at(init, 1)), 2);
    EXPECT_EQ(item_at(init, 2), nullptr);
    EXPECT_EQ(item_at(init, 3), nullptr);
}

// Brace elision fills a 2-D array row by row; a partial last row keeps a hole.
TEST_F(NormalizeTest, ElidedMatrix)
{
    const Initializer *init = Normalize("int a[2][2] = { 1, 2, 3 };");
    ASSERT_EQ(item_count(init), 2u);
    const Initializer *row0 = item_at(init, 0);
    const Initializer *row1 = item_at(init, 1);
    ASSERT_EQ(row0->kind, INITIALIZER_COMPOUND);
    ASSERT_EQ(row1->kind, INITIALIZER_COMPOUND);
    EXPECT_EQ(int_value(item_at(row0, 0)), 1);
    EXPECT_EQ(int_value(item_at(row0, 1)), 2);
    EXPECT_EQ(int_value(item_at(row1, 0)), 3);
    EXPECT_EQ(item_at(row1, 1), nullptr);
}

// An elided unsized array of structs gets its length from the rows filled.
TEST_F(NormalizeTest, ElidedStructArray)
{
    const Initializer *init =
        Normalize("struct s { char *name; int v; }; struct s tab[] = { \"AB\", 1, \"CD\" };");
    EXPECT_EQ(get_array_size(type), 2u);
    ASSERT_EQ(item_count(init), 2u);
    const Initializer *second = item_at(init, 1);
    ASSERT_EQ(item_count(second), 2u);
    EXPECT_EQ(item_at(second, 0)->u.expr->u.literal->kind, LITERAL_STRING);
    EXPECT_EQ(item_at(second, 1), nullptr);
}

// Elision stops at the end of the inner array; the next value goes to the next member.
TEST_F(NormalizeTest, ElisionBoundary)
{
    const Initializer *init =
        Normalize("struct s { int a[2]; int b; }; struct s g = { 1, 2, 3 };");
    ASSERT_EQ(item_count(init), 2u);
    const Initializer *a = item_at(init, 0);
    EXPECT_EQ(int_value(item_at(a, 0)), 1);
    EXPECT_EQ(int_value(item_at(a, 1)), 2);
    EXPECT_EQ(int_value(item_at(init, 1)), 3);
}

// A partial inner brace ends that subobject; the next value goes to the next member.
TEST_F(NormalizeTest, PartialInnerBrace)
{
    const Initializer *init =
        Normalize("struct s { int a[2]; int b; }; struct s g = { { 1 }, 2 };");
    const Initializer *a = item_at(init, 0);
    EXPECT_EQ(int_value(item_at(a, 0)), 1);
    EXPECT_EQ(item_at(a, 1), nullptr);
    EXPECT_EQ(int_value(item_at(init, 1)), 2);
}

// A string literal for a char array member is kept whole, not elided into chars.
TEST_F(NormalizeTest, StringForCharArrayMember)
{
    const Initializer *init =
        Normalize("struct s { char n[4]; int v; }; struct s g[] = { \"AB\", 1, \"CD\", 2 };");
    ASSERT_EQ(item_count(init), 2u);
    const Initializer *n = item_at(item_at(init, 1), 0);
    ASSERT_EQ(n->kind, INITIALIZER_SINGLE);
    EXPECT_EQ(n->u.expr->u.literal->kind, LITERAL_STRING);
}

// A braced string for a char array is unwrapped (§6.7.9p14).
TEST_F(NormalizeTest, BracedString)
{
    const Initializer *init = Normalize("char s[4] = { \"AB\" };");
    ASSERT_EQ(init->kind, INITIALIZER_SINGLE);
    EXPECT_EQ(init->u.expr->u.literal->kind, LITERAL_STRING);
}

// A union takes a single item, for its first member.
TEST_F(NormalizeTest, UnionElision)
{
    const Initializer *init =
        Normalize("union u { int i[2]; char *p; }; union u g = { 1, 2 };");
    ASSERT_EQ(item_count(init), 1u);
    const Initializer *i = item_at(init, 0);
    EXPECT_EQ(int_value(item_at(i, 0)), 1);
    EXPECT_EQ(int_value(item_at(i, 1)), 2);
}

TEST_F(NormalizeTest, ExcessElidedDies)
{
    EXPECT_DEATH(Normalize("int a[2][2] = { 1, 2, 3, 4, 5 };"),
                 "Too many elements in array initializer");
}

TEST_F(NormalizeTest, ExcessBracedDies)
{
    EXPECT_DEATH(Normalize("int a[2][2] = { { 1, 2, 3 } };"),
                 "Too many elements in array initializer");
}

TEST_F(NormalizeTest, EmptyScalarDies)
{
    EXPECT_DEATH(Normalize("int x = { };"), "Empty scalar initializer");
}

TEST_F(NormalizeTest, DesignatorDies)
{
    EXPECT_DEATH(Normalize("int a[2] = { [1] = 5 };"),
                 "Designated initializers are not supported yet");
}

// Automatic mode typechecks each leaf once, including one first seen at an aggregate
// slot (a struct value vs. brace elision); the fixture checks nothing leaks.
TEST_F(PipelineTest, NormalizeAutomatic)
{
    RunPipeline(R"(struct s { char *name; int v; };
struct t { int a[2]; int b; };
int f(void)
{
    struct s one = { "AB", 1 };
    struct s tab[] = { one, "CD", 2, { "EF" } };
    struct t x = { 1, 2, 3 };
    int m[2][2] = { 1, 2, 3 };
    int y = { 4 };
    char n[4] = { "GH" };
    return tab[1].v + x.b + m[1][0] + y + n[0];
}
)");
}

TEST_F(PipelineTest, NormalizeAutomaticExcessElidedDies)
{
    EXPECT_DEATH(RunPipeline("void f(void) { int a[2][2] = { 1, 2, 3, 4, 5 }; }"),
                 "Too many elements in array initializer");
}

TEST_F(PipelineTest, NormalizeAutomaticExcessStructDies)
{
    EXPECT_DEATH(RunPipeline("struct s { int a; }; void f(void) { struct s x[1] = { { 1, 2 } }; }"),
                 "Too many elements in struct initializer");
}

TEST_F(PipelineTest, NormalizeAutomaticEmptyScalarDies)
{
    EXPECT_DEATH(RunPipeline("void f(void) { int x = { }; }"), "Empty scalar initializer");
}
