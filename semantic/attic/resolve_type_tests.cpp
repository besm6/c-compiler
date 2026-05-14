#include <gtest/gtest.h>

#include "ast.h"
#include "hash_table.h"
#include "resolve.c" // Include resolve.c for resolve_type and type_table

// Fixture for resolve_type tests
class ResolveTypeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        type_table = create_hash_table();
        // Add a known struct tag
        StructEntry *entry               = (StructEntry *)malloc(sizeof(StructEntry));
        entry->unique_tag                = strdup("struct_123");
        entry->struct_from_current_scope = 1;
        hash_table_insert(type_table, "my_struct", entry);
    }

    void TearDown() override
    {
        hash_table_free(type_table);
        type_table = NULL;
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

    // Helper to create a TYPE_POINTER
    Type *make_pointer_type(Type *target)
    {
        Type *t                 = (Type *)malloc(sizeof(Type));
        t->kind                 = TYPE_POINTER;
        t->u.pointer.target     = target;
        t->u.pointer.qualifiers = NULL;
        t->qualifiers           = NULL;
        return t;
    }

    // Helper to create a TYPE_ARRAY
    Type *make_array_type(Type *element, Expr *size)
    {
        Type *t               = (Type *)malloc(sizeof(Type));
        t->kind               = TYPE_ARRAY;
        t->u.array.element    = element;
        t->u.array.size       = size;
        t->u.array.qualifiers = NULL;
        t->u.array.is_static  = false;
        t->qualifiers         = NULL;
        return t;
    }

    // Helper to create a TYPE_FUNCTION
    Type *make_function_type(Type *return_type, Param *params)
    {
        Type *t                   = (Type *)malloc(sizeof(Type));
        t->kind                   = TYPE_FUNCTION;
        t->u.function.return_type = return_type;
        t->u.function.params      = params;
        t->u.function.variadic    = false;
        t->qualifiers             = NULL;
        return t;
    }

    // Helper to create a Param
    Param *make_param(const char *name, Type *type)
    {
        Param *p      = (Param *)malloc(sizeof(Param));
        p->name       = strdup(name);
        p->type       = type;
        p->specifiers = NULL;
        p->next       = NULL;
        return p;
    }

    // Helper to create a TYPE_INT
    Type *make_int_type()
    {
        Type *t                 = (Type *)malloc(sizeof(Type));
        t->kind                 = TYPE_INT;
        t->u.integer.signedness = SIGNED_SIGNED;
        t->qualifiers           = NULL;
        return t;
    }

    // Helper to free a Type (simplified, assumes no deep freeing)
    void free_type(Type *t)
    {
        if (!t)
            return;
        if (t->kind == TYPE_STRUCT) {
            free(t->u.struct_t.name);
        } else if (t->kind == TYPE_POINTER) {
            free_type(t->u.pointer.target);
        } else if (t->kind == TYPE_ARRAY) {
            free_type(t->u.array.element);
            // Skip u.array.size as it's not resolved
        } else if (t->kind == TYPE_FUNCTION) {
            free_type(t->u.function.return_type);
            Param *p = t->u.function.params;
            while (p) {
                Param *next = p->next;
                free(p->name);
                free_type(p->type);
                free(p);
                p = next;
            }
        }
        free(t);
    }
};

TEST_F(ResolveTypeTest, ResolveStructType_KnownTag)
{
    Type *t = make_struct_type("my_struct");
    resolve_type(t);
    EXPECT_STREQ(t->u.struct_t.name, "struct_123");
    free_type(t);
}

TEST_F(ResolveTypeTest, ResolveStructType_UnknownTag)
{
    Type *t = make_struct_type("unknown_struct");
    EXPECT_EXIT(
        {
            resolve_type(t);
            exit(0); // Should not reach here
        },
        ::testing::ExitedWithCode(1), "Undeclared structure type unknown_struct");
    free_type(t);
}

TEST_F(ResolveTypeTest, ResolvePointerType)
{
    Type *struct_type = make_struct_type("my_struct");
    Type *t           = make_pointer_type(struct_type);
    resolve_type(t);
    EXPECT_STREQ(t->u.pointer.target->u.struct_t.name, "struct_123");
    free_type(t);
}

TEST_F(ResolveTypeTest, ResolveArrayType)
{
    Type *struct_type = make_struct_type("my_struct");
    Type *t           = make_array_type(struct_type, NULL); // No size expr
    resolve_type(t);
    EXPECT_STREQ(t->u.array.element->u.struct_t.name, "struct_123");
    free_type(t);
}

TEST_F(ResolveTypeTest, ResolveFunctionType)
{
    Type *return_type = make_struct_type("my_struct");
    Type *param_type  = make_struct_type("my_struct");
    Param *param      = make_param("p1", param_type);
    Type *t           = make_function_type(return_type, param);
    resolve_type(t);
    EXPECT_STREQ(t->u.function.return_type->u.struct_t.name, "struct_123");
    EXPECT_STREQ(t->u.function.params->type->u.struct_t.name, "struct_123");
    free_type(t);
}

TEST_F(ResolveTypeTest, ResolvePrimitiveType)
{
    Type *t    = make_int_type();
    Type *orig = (Type *)malloc(sizeof(Type));
    *orig      = *t; // Copy for comparison
    resolve_type(t);
    EXPECT_EQ(t->kind, TYPE_INT);
    EXPECT_EQ(t->u.integer.signedness, orig->u.integer.signedness);
    free_type(t);
    free(orig);
}

TEST_F(ResolveTypeTest, ResolveNullType)
{
    Type *t = NULL;
    resolve_type(t);
    EXPECT_EQ(t, nullptr); // No crash, no change
}

TEST_F(ResolveTypeTest, ResolveNestedTypes)
{
    Type *struct_type  = make_struct_type("my_struct");
    Type *array_type   = make_array_type(struct_type, NULL);
    Type *pointer_type = make_pointer_type(array_type);
    resolve_type(pointer_type);
    EXPECT_STREQ(pointer_type->u.pointer.target->u.array.element->u.struct_t.name, "struct_123");
    free_type(pointer_type);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    testing::GTEST_FLAG(death_test_style) = "threadsafe";
    return RUN_ALL_TESTS();
}
