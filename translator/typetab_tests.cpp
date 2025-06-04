#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "translator.h"
#include "typetab.h"
#include "internal.h"
#include "xalloc.h"

// Helper to create a TypeMember
TypeMember create_member(const char *name, Type *type, int offset)
{
    TypeMember m;
    m.name   = xstrdup(name);
    m.type   = type;
    m.offset = offset;
    return m;
}

// Helper to free a TypeMember
void free_member(TypeMember *m)
{
    free(m->name);
    // Type freed elsewhere
}

// Helper to compare TypeMember arrays
bool compare_members(const TypeMember *a, int a_count, const TypeMember *b, int b_count)
{
    if (a_count != b_count)
        return false;
    for (int i = 0; i < a_count; i++) {
        if (strcmp(a[i].name, b[i].name) != 0)
            return false;
        if (!compare_type(a[i].type, b[i].type))
            return false;
        if (a[i].offset != b[i].offset)
            return false;
    }
    return true;
}

// Test fixture for typetab tests
class TypetabTest : public ::testing::Test {
protected:
    void SetUp() override { typetab_init(); }
    void TearDown() override { typetab_destroy(); }
};

// Test typetab_init
TEST_F(TypetabTest, InitEmptyTable)
{
    ASSERT_EQ(typetab_size(), 0u);
    ASSERT_FALSE(typetab_mem("S"));
}

// Test typetab_destroy
TEST_F(TypetabTest, DestroyEmptyTable)
{
    typetab_destroy();
    typetab_init(); // Re-init to ensure no corruption
    ASSERT_EQ(typetab_size(), 0u);
}

// Test typetab_add_struct_definition
TEST_F(TypetabTest, AddStructDefinition)
{
    Type *int_type       = new_type(TYPE_INT);
    TypeMember members[] = { create_member("x", int_type, 0) };

    typetab_add_struct_definition("S", 4, 4, members, 1);

    TypeEntry *entry = typetab_find("S");
    ASSERT_STREQ(entry->tag, "S");
    ASSERT_EQ(entry->alignment, 4);
    ASSERT_EQ(entry->size, 4);
    ASSERT_EQ(entry->member_count, 1);
    ASSERT_TRUE(compare_members(entry->members, entry->member_count, members, 1));

    ASSERT_EQ(typetab_size(), 1u);

    free_member(&members[0]);
    free_type(int_type);
}

// Test typetab_add_struct_definition with multiple members
TEST_F(TypetabTest, AddStructDefinitionMultipleMembers)
{
    Type *int_type       = new_type(TYPE_INT);
    Type *double_type    = new_type(TYPE_DOUBLE);
    TypeMember members[] = { create_member("x", int_type, 0), create_member("y", double_type, 8) };

    typetab_add_struct_definition("Point", 8, 16, members, 2);

    TypeEntry *entry = typetab_find("Point");
    ASSERT_STREQ(entry->tag, "Point");
    ASSERT_EQ(entry->alignment, 8);
    ASSERT_EQ(entry->size, 16);
    ASSERT_EQ(entry->member_count, 2);
    ASSERT_TRUE(compare_members(entry->members, entry->member_count, members, 2));

    ASSERT_EQ(typetab_size(), 1u);

    free_member(&members[0]);
    free_member(&members[1]);
    free_type(int_type);
    free_type(double_type);
}

// Test typetab_add_struct_definition overwrite
TEST_F(TypetabTest, AddStructDefinitionOverwrite)
{
    Type *int_type        = new_type(TYPE_INT);
    TypeMember members1[] = { create_member("x", int_type, 0) };
    TypeMember members2[] = { create_member("y", int_type, 0) };

    typetab_add_struct_definition("S", 4, 4, members1, 1);
    typetab_add_struct_definition("S", 4, 4, members2, 1);

    TypeEntry *entry = typetab_find("S");
    ASSERT_STREQ(entry->tag, "S");
    ASSERT_TRUE(compare_members(entry->members, entry->member_count, members2, 1));

    ASSERT_EQ(typetab_size(), 1u);

    free_member(&members1[0]);
    free_member(&members2[0]);
    free_type(int_type);
}

// Test typetab_add_struct_definition empty struct
TEST_F(TypetabTest, AddEmptyStructDefinition)
{
    typetab_add_struct_definition("Empty", 1, 0, NULL, 0);

    TypeEntry *entry = typetab_find("Empty");
    ASSERT_STREQ(entry->tag, "Empty");
    ASSERT_EQ(entry->alignment, 1);
    ASSERT_EQ(entry->size, 0);
    ASSERT_EQ(entry->member_count, 0);
    ASSERT_EQ(entry->members, nullptr);

    ASSERT_EQ(typetab_size(), 1u);
}

// Test typetab_mem
TEST_F(TypetabTest, MemExistentAndNonExistent)
{
    Type *int_type       = new_type(TYPE_INT);
    TypeMember members[] = { create_member("x", int_type, 0) };

    typetab_add_struct_definition("S", 4, 4, members, 1);

    ASSERT_TRUE(typetab_mem("S"));
    ASSERT_FALSE(typetab_mem("T"));

    free_member(&members[0]);
    free_type(int_type);
}

// Test typetab_find
TEST_F(TypetabTest, FindNonExistent)
{
    ASSERT_EXIT(typetab_find("S"), ::testing::ExitedWithCode(1), "Struct S not found");
}

// Test typetab_get_members
TEST_F(TypetabTest, GetMembersSorted)
{
    Type *int_type       = new_type(TYPE_INT);
    TypeMember members[] = { create_member("y", int_type, 8), // Out of order
                             create_member("x", int_type, 0) };

    typetab_add_struct_definition("S", 4, 12, members, 2);

    int count;
    TypeMember *retrieved = typetab_get_members("S", &count);
    ASSERT_EQ(count, 2);
    TypeMember expected[] = { create_member("x", int_type, 0), create_member("y", int_type, 8) };
    ASSERT_TRUE(compare_members(retrieved, count, expected, 2));

    for (int i = 0; i < count; i++)
        free_member(&retrieved[i]);
    free(retrieved);
    free_member(&members[0]);
    free_member(&members[1]);
    free_member(&expected[0]);
    free_member(&expected[1]);
    free_type(int_type);
}

// Test typetab_get_member_types
TEST_F(TypetabTest, GetMemberTypes)
{
    Type *int_type       = new_type(TYPE_INT);
    Type *double_type    = new_type(TYPE_DOUBLE);
    TypeMember members[] = { create_member("x", int_type, 0), create_member("y", double_type, 8) };

    typetab_add_struct_definition("S", 8, 16, members, 2);

    int count;
    Type **types = typetab_get_member_types("S", &count);
    ASSERT_EQ(count, 2);
    ASSERT_TRUE(compare_type(types[0], int_type));
    ASSERT_TRUE(compare_type(types[1], double_type));

    free(types); // Caller frees array, not types
    free_member(&members[0]);
    free_member(&members[1]);
    free_type(int_type);
    free_type(double_type);
}

// Test typetab_size
TEST_F(TypetabTest, SizeMultipleStructs)
{
    Type *int_type       = new_type(TYPE_INT);
    TypeMember members[] = { create_member("x", int_type, 0) };

    typetab_add_struct_definition("S", 4, 4, members, 1);
    typetab_add_struct_definition("T", 4, 4, members, 1);

    ASSERT_EQ(typetab_size(), 2u);

    free_member(&members[0]);
    free_type(int_type);
}
