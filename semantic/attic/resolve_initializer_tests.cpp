#include <gtest/gtest.h>

#include "ast.h"
#include "resolve.h"

// Fixture for resolve_initializer tests
class ResolveInitializerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        type_table   = create_hash_table();
        symbol_table = create_hash_table();
        // Add a known struct tag
        StructEntry *struct_entry               = (StructEntry *)malloc(sizeof(StructEntry));
        struct_entry->unique_tag                = strdup("struct_123");
        struct_entry->struct_from_current_scope = 1;
        hash_table_insert(type_table, "my_struct", struct_entry);
        // Add a known variable
        VarEntry *var_entry           = (VarEntry *)malloc(sizeof(VarEntry));
        var_entry->unique_name        = strdup("var_123");
        var_entry->from_current_scope = 1;
        var_entry->has_linkage        = 0;
        hash_table_insert(symbol_table, "my_var", var_entry);
    }

    void TearDown() override
    {
        hash_table_free(type_table);
        hash_table_free(symbol_table);
        type_table   = NULL;
        symbol_table = NULL;
    }

    // Helper to create a TYPE_STRUCT
    Type *make_struct_type(const char *name)
    {
        Type *t              = (Type *)malloc(sizeof(Type));
        t->kind              = TYPE_STRUCT;
        t->u.struct_t.name   = strdup(name);
        t->u.struct_t.fields = NULL;
        t->qualifiers        = NULL;
        return t;
    }

    // Helper to create an EXPR_VAR
    Expr *make_var_expr(const char *name)
    {
        Expr *e  = (Expr *)malloc(sizeof(Expr));
        e->kind  = EXPR_VAR;
        e->u.var = strdup(name);
        e->next  = NULL;
        e->type  = NULL;
        return e;
    }

    // Helper to create an EXPR_LITERAL
    Expr *make_literal_expr(int value)
    {
        Expr *e        = (Expr *)malloc(sizeof(Expr));
        e->kind        = EXPR_LITERAL;
        Literal *lit   = (Literal *)malloc(sizeof(Literal));
        lit->kind      = LITERAL_INT;
        lit->u.int_val = value;
        e->u.literal   = lit;
        e->next        = NULL;
        e->type        = NULL;
        return e;
    }

    // Helper to create an INITIALIZER_SINGLE
    Initializer *make_single_initializer(Expr *expr)
    {
        Initializer *init = (Initializer *)malloc(sizeof(Initializer));
        init->kind        = INITIALIZER_SINGLE;
        init->u.expr      = expr;
        return init;
    }

    // Helper to create an INITIALIZER_COMPOUND
    Initializer *make_compound_initializer(InitItem *items)
    {
        Initializer *init = (Initializer *)malloc(sizeof(Initializer));
        init->kind        = INITIALIZER_COMPOUND;
        init->u.items     = items;
        return init;
    }

    // Helper to create an InitItem
    InitItem *make_init_item(Initializer *init, Designator *designators = NULL)
    {
        InitItem *item    = (InitItem *)malloc(sizeof(InitItem));
        item->init        = init;
        item->designators = designators;
        item->next        = NULL;
        return item;
    }

    // Helper to create an array Designator
    Designator *make_array_designator(Expr *expr)
    {
        Designator *d = (Designator *)malloc(sizeof(Designator));
        d->kind       = DESIGNATOR_ARRAY;
        d->u.expr     = expr;
        d->next       = NULL;
        return d;
    }

    // Helper to create a field Designator
    Designator *make_field_designator(const char *name)
    {
        Designator *d = (Designator *)malloc(sizeof(Designator));
        d->kind       = DESIGNATOR_FIELD;
        d->u.name     = strdup(name);
        d->next       = NULL;
        return d;
    }

    // Helper to free an Initializer
    void free_initializer(Initializer *init)
    {
        if (!init)
            return;
        if (init->kind == INITIALIZER_SINGLE) {
            free_expr(init->u.expr);
        } else {
            for (InitItem *item = init->u.items; item;) {
                InitItem *next = item->next;
                free_initializer(item->init);
                free_designator(item->designators);
                free(item);
                item = next;
            }
        }
        free(init);
    }

    // Helper to free a Designator
    void free_designator(Designator *d)
    {
        while (d) {
            Designator *next = d->next;
            if (d->kind == DESIGNATOR_ARRAY) {
                free_expr(d->u.expr);
            } else {
                free(d->u.name);
            }
            free(d);
            d = next;
        }
    }

    // Helper to free an Expr
    void free_expr(Expr *e)
    {
        if (!e)
            return;
        switch (e->kind) {
        case EXPR_LITERAL:
            free(e->u.literal);
            break;
        case EXPR_VAR:
            free(e->u.var);
            break;
        default:
            // Minimal freeing for test purposes
            break;
        }
        free(e);
    }

    // Helper to free a Type
    void free_type(Type *t)
    {
        if (!t)
            return;
        if (t->kind == TYPE_STRUCT) {
            free(t->u.struct_t.name);
        }
        free(t);
    }
};

TEST_F(ResolveInitializerTest, ResolveSingleInitializer_KnownVar)
{
    Expr *expr        = make_var_expr("my_var");
    Initializer *init = make_single_initializer(expr);
    resolve_initializer(init);
    EXPECT_STREQ(init->u.expr->u.var, "var_123");
    free_initializer(init);
}

TEST_F(ResolveInitializerTest, ResolveSingleInitializer_UnknownVar)
{
    Expr *expr        = make_var_expr("unknown_var");
    Initializer *init = make_single_initializer(expr);
    EXPECT_EXIT(
        {
            resolve_initializer(init);
            exit(0);
        },
        ::testing::ExitedWithCode(1), "Undeclared variable unknown_var");
    free_initializer(init);
}

TEST_F(ResolveInitializerTest, ResolveCompoundInitializer_SingleItem)
{
    Expr *expr               = make_var_expr("my_var");
    Initializer *single_init = make_single_initializer(expr);
    InitItem *item           = make_init_item(single_init);
    Initializer *init        = make_compound_initializer(item);
    resolve_initializer(init);
    EXPECT_STREQ(init->u.items->init->u.expr->u.var, "var_123");
    free_initializer(init);
}

TEST_F(ResolveInitializerTest, ResolveCompoundInitializer_MultipleItems)
{
    Expr *expr1        = make_var_expr("my_var");
    Expr *expr2        = make_var_expr("my_var");
    Initializer *init1 = make_single_initializer(expr1);
    Initializer *init2 = make_single_initializer(expr2);
    InitItem *item1    = make_init_item(init1);
    InitItem *item2    = make_init_item(init2);
    item1->next        = item2;
    Initializer *init  = make_compound_initializer(item1);
    resolve_initializer(init);
    EXPECT_STREQ(init->u.items->init->u.expr->u.var, "var_123");
    EXPECT_STREQ(init->u.items->next->init->u.expr->u.var, "var_123");
    free_initializer(init);
}

TEST_F(ResolveInitializerTest, ResolveCompoundInitializer_NestedCompound)
{
    Expr *inner_expr         = make_var_expr("my_var");
    Initializer *inner_init  = make_single_initializer(inner_expr);
    InitItem *inner_item     = make_init_item(inner_init);
    Initializer *nested_init = make_compound_initializer(inner_item);
    InitItem *outer_item     = make_init_item(nested_init);
    Initializer *init        = make_compound_initializer(outer_item);
    resolve_initializer(init);
    EXPECT_STREQ(init->u.items->init->u.items->init->u.expr->u.var, "var_123");
    free_initializer(init);
}

TEST_F(ResolveInitializerTest, ResolveCompoundInitializer_ArrayDesignator)
{
    Expr *designator_expr    = make_var_expr("my_var");
    Designator *designator   = make_array_designator(designator_expr);
    Expr *init_expr          = make_var_expr("my_var");
    Initializer *single_init = make_single_initializer(init_expr);
    InitItem *item           = make_init_item(single_init, designator);
    Initializer *init        = make_compound_initializer(item);
    resolve_initializer(init);
    EXPECT_STREQ(init->u.items->designators->u.expr->u.var, "var_123");
    EXPECT_STREQ(init->u.items->init->u.expr->u.var, "var_123");
    free_initializer(init);
}

TEST_F(ResolveInitializerTest, ResolveCompoundInitializer_FieldDesignator)
{
    Designator *designator   = make_field_designator("field");
    Expr *init_expr          = make_var_expr("my_var");
    Initializer *single_init = make_single_initializer(init_expr);
    InitItem *item           = make_init_item(single_init, designator);
    Initializer *init        = make_compound_initializer(item);
    resolve_initializer(init);
    EXPECT_STREQ(init->u.items->designators->u.name, "field");
    EXPECT_STREQ(init->u.items->init->u.expr->u.var, "var_123");
    free_initializer(init);
}

TEST_F(ResolveInitializerTest, ResolveNullInitializer)
{
    Initializer *init = NULL;
    resolve_initializer(init);
    EXPECT_EQ(init, nullptr);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    testing::GTEST_FLAG(death_test_style) = "threadsafe";
    return RUN_ALL_TESTS();
}
