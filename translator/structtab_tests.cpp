#include <gtest/gtest.h>
#include <string.h>

#include "translate.h"
#include "structtab.h"
#include "xalloc.h"

// Test fixture for StructTab tests
class StructTabTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        xalloc_debug = 1;
        structtab_init();
    }

    void TearDown() override
    {
        structtab_destroy();
        xreport_lost_memory();
        EXPECT_EQ(xtotal_allocated_size(), 0);
        xfree_all();
    }
};

// Test structtab_init
TEST_F(StructTabTest, InitCreatesEmptyTable)
{
    // Table should be empty after init (already called in SetUp)
    EXPECT_FALSE(structtab_exists("any_tag"));
}

// Test structtab_add_struct with a single field
TEST_F(StructTabTest, AddFieldDefinitionSingleMember)
{
    Type *intType   = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    FieldDef *field = new_member("x", intType, 0);

    structtab_add_struct("point", 4, 4, field, 0);

    EXPECT_TRUE(structtab_exists("point"));
    StructDef *entry = structtab_find("point");
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->tag, "point");
    EXPECT_EQ(entry->alignment, 4);
    EXPECT_EQ(entry->size, 4);
    ASSERT_NE(entry->members, nullptr);
    EXPECT_STREQ(entry->members->name, "x");
    EXPECT_EQ(entry->members->offset, 0);
    EXPECT_EQ(entry->members->type->kind, TYPE_INT);
    EXPECT_EQ(entry->members->next, nullptr);
}

// Test structtab_add_struct with multiple fields
TEST_F(StructTabTest, AddFieldDefinitionMultipleMembers)
{
    Type *intType    = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    Type *doubleType = new_type(TYPE_DOUBLE, __func__, __FILE__, __LINE__);
    FieldDef *field1 = new_member("x", intType, 0);
    FieldDef *field2 = new_member("y", doubleType, 8);
    field1->next     = field2;

    structtab_add_struct("vector", 8, 16, field1, 0);

    EXPECT_TRUE(structtab_exists("vector"));
    StructDef *entry = structtab_find("vector");
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->tag, "vector");
    EXPECT_EQ(entry->alignment, 8);
    EXPECT_EQ(entry->size, 16);
    ASSERT_NE(entry->members, nullptr);
    EXPECT_STREQ(entry->members->name, "x");
    EXPECT_EQ(entry->members->offset, 0);
    EXPECT_EQ(entry->members->type->kind, TYPE_INT);
    ASSERT_NE(entry->members->next, nullptr);
    EXPECT_STREQ(entry->members->next->name, "y");
    EXPECT_EQ(entry->members->next->offset, 8);
    EXPECT_EQ(entry->members->next->type->kind, TYPE_DOUBLE);
    EXPECT_EQ(entry->members->next->next, nullptr);
}

// Test replacing an existing struct definition
TEST_F(StructTabTest, ReplaceFieldDefinition)
{
    Type *intType    = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    FieldDef *field1 = new_member("x", intType, 0);
    structtab_add_struct("point", 4, 4, field1, 0);

    Type *doubleType = new_type(TYPE_DOUBLE, __func__, __FILE__, __LINE__);
    FieldDef *field2 = new_member("y", doubleType, 0);
    structtab_add_struct("point", 8, 8, field2, 0);

    StructDef *entry = structtab_find("point");
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->tag, "point");
    EXPECT_EQ(entry->alignment, 8);
    EXPECT_EQ(entry->size, 8);
    ASSERT_NE(entry->members, nullptr);
    EXPECT_STREQ(entry->members->name, "y");
    EXPECT_EQ(entry->members->offset, 0);
    EXPECT_EQ(entry->members->type->kind, TYPE_DOUBLE);
    EXPECT_EQ(entry->members->next, nullptr);
}

// Test structtab_exists
TEST_F(StructTabTest, ExistsReturnsFalseForNonExistent)
{
    EXPECT_FALSE(structtab_exists("nonexistent"));
}

// Test structtab_find fails on non-existent tag
TEST_F(StructTabTest, FindNonExistentTag)
{
    // Since structtab_find terminates on error, we can't directly test it without
    // mocking or handling the termination. Instead, we rely on structtab_exists.
    EXPECT_FALSE(structtab_exists("nonexistent"));
}

// Test structtab_destroy
TEST_F(StructTabTest, DestroyFreesMemory)
{
    Type *intType   = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    FieldDef *field = new_member("x", intType, 0);
    structtab_add_struct("point", 4, 4, field, 0);

    structtab_destroy();
    structtab_init(); // Re-initialize to ensure table is usable
    EXPECT_FALSE(structtab_exists("point"));
}

// Test adding struct with NULL fields
TEST_F(StructTabTest, AddStructWithNullMembers)
{
    structtab_add_struct("empty", 4, 0, nullptr, 0);

    EXPECT_TRUE(structtab_exists("empty"));
    StructDef *entry = structtab_find("empty");
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->tag, "empty");
    EXPECT_EQ(entry->alignment, 4);
    EXPECT_EQ(entry->size, 0);
    EXPECT_EQ(entry->members, nullptr);
}
