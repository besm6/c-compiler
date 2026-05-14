#include <gtest/gtest.h>
#include <string.h>

#include "semantic.h"
#include "typetab.h"
#include "xalloc.h"

// Test fixture for TypeTab tests
class TypeTabTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        xalloc_debug = 1;
        typetab_init();
    }

    void TearDown() override
    {
        typetab_destroy();
        xreport_lost_memory();
        EXPECT_EQ(xtotal_allocated_size(), 0);
        xfree_all();
    }
};

// Test typetab_init
TEST_F(TypeTabTest, InitCreatesEmptyTable)
{
    EXPECT_FALSE(typetab_exists("any_name"));
}

// Test typetab_exists returns true after add
TEST_F(TypeTabTest, AddAndExists)
{
    Type *t = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    typetab_add("MyInt", t, 0);
    free_type(t);

    EXPECT_TRUE(typetab_exists("MyInt"));
}

// Test typetab_find returns correct name and type kind
TEST_F(TypeTabTest, AddAndFind)
{
    Type *t = new_type(TYPE_LONG, __func__, __FILE__, __LINE__);
    typetab_add("MyLong", t, 0);
    free_type(t);

    TypeDef *entry = typetab_find("MyLong");
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->name, "MyLong");
    EXPECT_EQ(entry->type->kind, TYPE_LONG);
}

// Test typetab_resolve returns the underlying type
TEST_F(TypeTabTest, AddAndResolve)
{
    Type *t = new_type(TYPE_DOUBLE, __func__, __FILE__, __LINE__);
    typetab_add("MyDouble", t, 0);
    free_type(t);

    const Type *resolved = typetab_resolve("MyDouble");
    ASSERT_NE(resolved, nullptr);
    EXPECT_EQ(resolved->kind, TYPE_DOUBLE);
}

// Test typetab_exists returns false for non-existent name
TEST_F(TypeTabTest, ExistsFalseForNonExistent)
{
    EXPECT_FALSE(typetab_exists("nonexistent"));
}

// Test typetab_destroy frees all memory and leaves table empty
TEST_F(TypeTabTest, DestroyFreesMemory)
{
    Type *t = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    typetab_add("MyInt", t, 0);
    free_type(t);

    typetab_destroy();
    typetab_init(); // Re-initialize so TearDown can safely call destroy again
    EXPECT_FALSE(typetab_exists("MyInt"));
}

// Test multiple distinct typedef names coexist
TEST_F(TypeTabTest, AddMultipleDistinctNames)
{
    Type *ti = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    Type *td = new_type(TYPE_DOUBLE, __func__, __FILE__, __LINE__);
    typetab_add("MyInt", ti, 0);
    typetab_add("MyDouble", td, 0);
    free_type(ti);
    free_type(td);

    EXPECT_TRUE(typetab_exists("MyInt"));
    EXPECT_TRUE(typetab_exists("MyDouble"));
    EXPECT_EQ(typetab_resolve("MyInt")->kind, TYPE_INT);
    EXPECT_EQ(typetab_resolve("MyDouble")->kind, TYPE_DOUBLE);
}

// Test replacing an existing typedef entry
TEST_F(TypeTabTest, ReplaceExistingEntry)
{
    Type *t1 = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    typetab_add("MyType", t1, 0);
    free_type(t1);

    Type *t2 = new_type(TYPE_LONG, __func__, __FILE__, __LINE__);
    typetab_add("MyType", t2, 0);
    free_type(t2);

    TypeDef *entry = typetab_find("MyType");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->type->kind, TYPE_LONG);
}

// Test purge removes entries at level > given level
TEST_F(TypeTabTest, PurgeRemovesLevel1Entry)
{
    Type *t = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    typetab_add("Local", t, 1);
    free_type(t);

    EXPECT_TRUE(typetab_exists("Local"));
    typetab_purge(0); // removes level > 0, so level 1 is removed
    EXPECT_FALSE(typetab_exists("Local"));
}

// Test purge keeps entries at level 0 when purging at level 0
TEST_F(TypeTabTest, PurgeKeepsLevel0Entry)
{
    Type *t = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    typetab_add("Global", t, 0);
    free_type(t);

    typetab_purge(0); // removes level > 0 only; level 0 survives
    EXPECT_TRUE(typetab_exists("Global"));
}

// Test purge with nested scopes: only removes beyond the given level
TEST_F(TypeTabTest, PurgeNestedScopes)
{
    Type *t0 = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    Type *t1 = new_type(TYPE_LONG, __func__, __FILE__, __LINE__);
    Type *t2 = new_type(TYPE_DOUBLE, __func__, __FILE__, __LINE__);
    typetab_add("Global", t0, 0);
    typetab_add("Level1", t1, 1);
    typetab_add("Level2", t2, 2);
    free_type(t0);
    free_type(t1);
    free_type(t2);

    typetab_purge(1); // removes level > 1, so only level 2 is removed
    EXPECT_TRUE(typetab_exists("Global"));
    EXPECT_TRUE(typetab_exists("Level1"));
    EXPECT_FALSE(typetab_exists("Level2"));
}

// Test typedef of a pointer type
TEST_F(TypeTabTest, TypedefToPointerType)
{
    Type *target = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    Type *ptr    = new_type(TYPE_POINTER, __func__, __FILE__, __LINE__);
    ptr->u.pointer.target = target;
    typetab_add("IntPtr", ptr, 0);
    free_type(ptr);

    const Type *resolved = typetab_resolve("IntPtr");
    ASSERT_NE(resolved, nullptr);
    EXPECT_EQ(resolved->kind, TYPE_POINTER);
    ASSERT_NE(resolved->u.pointer.target, nullptr);
    EXPECT_EQ(resolved->u.pointer.target->kind, TYPE_INT);
}

// Test typedef of an array type
TEST_F(TypeTabTest, TypedefToArrayType)
{
    Type *elem  = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    Type *arr   = new_type(TYPE_ARRAY, __func__, __FILE__, __LINE__);
    arr->u.array.element = elem;
    arr->u.array.size    = nullptr;
    typetab_add("IntArray", arr, 0);
    free_type(arr);

    const Type *resolved = typetab_resolve("IntArray");
    ASSERT_NE(resolved, nullptr);
    EXPECT_EQ(resolved->kind, TYPE_ARRAY);
    ASSERT_NE(resolved->u.array.element, nullptr);
    EXPECT_EQ(resolved->u.array.element->kind, TYPE_INT);
}

// Test chained typedef: typedef of a TYPE_TYPEDEF_NAME
TEST_F(TypeTabTest, TypedefToTypedefName)
{
    // First typedef: MyInt -> int
    Type *ti = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    typetab_add("MyInt", ti, 0);
    free_type(ti);

    // Second typedef: MyInt2 -> MyInt (i.e. TYPE_TYPEDEF_NAME "MyInt")
    Type *tname = new_type(TYPE_TYPEDEF_NAME, __func__, __FILE__, __LINE__);
    tname->u.typedef_name.name = xstrdup("MyInt");
    typetab_add("MyInt2", tname, 0);
    free_type(tname);

    // Resolving MyInt2 yields the TYPE_TYPEDEF_NAME node (one level)
    const Type *r = typetab_resolve("MyInt2");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->kind, TYPE_TYPEDEF_NAME);
    EXPECT_STREQ(r->u.typedef_name.name, "MyInt");

    // Resolving MyInt yields TYPE_INT
    const Type *r2 = typetab_resolve(r->u.typedef_name.name);
    ASSERT_NE(r2, nullptr);
    EXPECT_EQ(r2->kind, TYPE_INT);
}

// Test that the stored type is a clone (independent copy)
TEST_F(TypeTabTest, AddPreservesOriginalType)
{
    Type *t = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    typetab_add("MyInt", t, 0);
    // Change the original after adding
    t->kind = TYPE_LONG;
    free_type(t);

    // The stored entry must still be TYPE_INT
    EXPECT_EQ(typetab_resolve("MyInt")->kind, TYPE_INT);
}

// Test multiple purge cycles at different levels
TEST_F(TypeTabTest, MultiplePurgeCycles)
{
    Type *tg = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    Type *tl = new_type(TYPE_LONG, __func__, __FILE__, __LINE__);
    typetab_add("Global", tg, 0);
    typetab_add("Local", tl, 1);
    free_type(tg);
    free_type(tl);

    typetab_purge(0); // removes level 1 Local
    EXPECT_TRUE(typetab_exists("Global"));
    EXPECT_FALSE(typetab_exists("Local"));

    // Add a new local entry
    Type *tl2 = new_type(TYPE_DOUBLE, __func__, __FILE__, __LINE__);
    typetab_add("Local2", tl2, 1);
    free_type(tl2);

    EXPECT_TRUE(typetab_exists("Local2"));
    typetab_purge(0); // purge again
    EXPECT_FALSE(typetab_exists("Local2"));
    EXPECT_TRUE(typetab_exists("Global"));
}

// Test that find returns the correct TypeDef name string
TEST_F(TypeTabTest, FindReturnsCorrectName)
{
    Type *t = new_type(TYPE_CHAR, __func__, __FILE__, __LINE__);
    typetab_add("MyChar", t, 0);
    free_type(t);

    TypeDef *entry = typetab_find("MyChar");
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->name, "MyChar");
}

// Test struct type can be stored as a typedef
TEST_F(TypeTabTest, TypedefToStructType)
{
    Type *s = new_type(TYPE_STRUCT, __func__, __FILE__, __LINE__);
    s->u.struct_t.name   = xstrdup("point");
    s->u.struct_t.fields = nullptr;
    typetab_add("Point", s, 0);
    free_type(s);

    const Type *resolved = typetab_resolve("Point");
    ASSERT_NE(resolved, nullptr);
    EXPECT_EQ(resolved->kind, TYPE_STRUCT);
    EXPECT_STREQ(resolved->u.struct_t.name, "point");
}

// Test purge of empty table is a no-op (does not crash)
TEST_F(TypeTabTest, PurgeEmptyTableIsNoop)
{
    typetab_purge(0);
    EXPECT_FALSE(typetab_exists("anything"));
}
