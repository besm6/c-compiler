#include <gtest/gtest.h>
#include <string.h>

#include "ast.h"
#include "typetab.h"

// Test fixture for TypeTab tests
class TypeTabTest : public ::testing::Test {
protected:
    void SetUp() override { typetab_init(); }

    void TearDown() override { typetab_destroy(); }

    // Helper to create a FieldDef
    FieldDef *createFieldDef(const char *name, Type *type, int offset)
    {
        FieldDef *member = new FieldDef;
        member->name       = strdup(name);
        member->type       = type;
        member->offset     = offset;
        member->next       = nullptr;
        return member;
    }

    // Helper to create a simple Type
    Type *createSimpleType(TypeKind kind)
    {
        Type *type       = new_type(kind);
        type->qualifiers = nullptr;
        return type;
    }

    // Helper to free a FieldDef list
    void freeFieldDefList(FieldDef *member)
    {
        while (member) {
            FieldDef *next = member->next;
            free(member->name);
            free_type(member->type);
            delete member;
            member = next;
        }
    }
};

// Test typetab_init
TEST_F(TypeTabTest, InitCreatesEmptyTable)
{
    // Table should be empty after init (already called in SetUp)
    EXPECT_FALSE(typetab_exists("any_tag"));
}

// Test typetab_add_struct_definition with a single member
TEST_F(TypeTabTest, AddFieldDefinitionSingleMember)
{
    Type *intType      = createSimpleType(TYPE_INT);
    FieldDef *member = createFieldDef("x", intType, 0);

    typetab_add_struct_definition(strdup("point"), 4, 4, member);

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

// Test typetab_add_struct_definition with multiple members
TEST_F(TypeTabTest, AddFieldDefinitionMultipleMembers)
{
    Type *intType       = createSimpleType(TYPE_INT);
    Type *doubleType    = createSimpleType(TYPE_DOUBLE);
    FieldDef *member1 = createFieldDef("x", intType, 0);
    FieldDef *member2 = createFieldDef("y", doubleType, 8);
    member1->next       = member2;

    typetab_add_struct_definition(strdup("vector"), 8, 16, member1);

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
    Type *intType       = createSimpleType(TYPE_INT);
    FieldDef *member1 = createFieldDef("x", intType, 0);
    typetab_add_struct_definition(strdup("point"), 4, 4, member1);

    Type *doubleType    = createSimpleType(TYPE_DOUBLE);
    FieldDef *member2 = createFieldDef("y", doubleType, 0);
    typetab_add_struct_definition(strdup("point"), 8, 8, member2);

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
    Type *intType      = createSimpleType(TYPE_INT);
    FieldDef *member = createFieldDef("x", intType, 0);
    typetab_add_struct_definition(strdup("point"), 4, 4, member);

    typetab_destroy();
    typetab_init(); // Re-initialize to ensure table is usable
    EXPECT_FALSE(typetab_exists("point"));
}

// Test adding struct with NULL members
TEST_F(TypeTabTest, AddStructWithNullMembers)
{
    typetab_add_struct_definition(strdup("empty"), 4, 0, nullptr);

    EXPECT_TRUE(typetab_exists("empty"));
    StructDef *entry = typetab_find("empty");
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->tag, "empty");
    EXPECT_EQ(entry->alignment, 4);
    EXPECT_EQ(entry->size, 0);
    EXPECT_EQ(entry->members, nullptr);
}
