#include <gtest/gtest.h>
#include <string.h>

#include "translator.h"
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
    // Table should be empty after init (already called in SetUp)
    EXPECT_FALSE(typetab_exists("any_tag"));
}

// Test typetab_add_struct with a single field
TEST_F(TypeTabTest, AddFieldDefinitionSingleMember)
{
    Type *intType   = new_type(TYPE_INT);
    FieldDef *field = new_member("x", intType, 0);

    typetab_add_struct("point", 4, 4, field);

    EXPECT_TRUE(typetab_exists("point"));
    StructDef *entry = typetab_find("point");
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

// Test typetab_add_struct with multiple fields
TEST_F(TypeTabTest, AddFieldDefinitionMultipleMembers)
{
    Type *intType    = new_type(TYPE_INT);
    Type *doubleType = new_type(TYPE_DOUBLE);
    FieldDef *field1 = new_member("x", intType, 0);
    FieldDef *field2 = new_member("y", doubleType, 8);
    field1->next     = field2;

    typetab_add_struct("vector", 8, 16, field1);

    EXPECT_TRUE(typetab_exists("vector"));
    StructDef *entry = typetab_find("vector");
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
TEST_F(TypeTabTest, ReplaceFieldDefinition)
{
    Type *intType    = new_type(TYPE_INT);
    FieldDef *field1 = new_member("x", intType, 0);
    typetab_add_struct("point", 4, 4, field1);

    Type *doubleType = new_type(TYPE_DOUBLE);
    FieldDef *field2 = new_member("y", doubleType, 0);
    typetab_add_struct("point", 8, 8, field2);

    StructDef *entry = typetab_find("point");
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

// Test typetab_exists
TEST_F(TypeTabTest, ExistsReturnsFalseForNonExistent)
{
    EXPECT_FALSE(typetab_exists("nonexistent"));
}

// Test typetab_find fails on non-existent tag
TEST_F(TypeTabTest, FindNonExistentTag)
{
    // Since typetab_find terminates on error, we can't directly test it without
    // mocking or handling the termination. Instead, we rely on typetab_exists.
    EXPECT_FALSE(typetab_exists("nonexistent"));
}

// Test typetab_destroy
TEST_F(TypeTabTest, DestroyFreesMemory)
{
    Type *intType   = new_type(TYPE_INT);
    FieldDef *field = new_member("x", intType, 0);
    typetab_add_struct("point", 4, 4, field);

    typetab_destroy();
    typetab_init(); // Re-initialize to ensure table is usable
    EXPECT_FALSE(typetab_exists("point"));
}

// Test adding struct with NULL fields
TEST_F(TypeTabTest, AddStructWithNullMembers)
{
    typetab_add_struct("empty", 4, 0, nullptr);

    EXPECT_TRUE(typetab_exists("empty"));
    StructDef *entry = typetab_find("empty");
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->tag, "empty");
    EXPECT_EQ(entry->alignment, 4);
    EXPECT_EQ(entry->size, 0);
    EXPECT_EQ(entry->members, nullptr);
}
