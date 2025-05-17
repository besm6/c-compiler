//
// Nested types
//
#include "fixture.h"

//
// Nested Struct with Simple Field
//      struct Outer {
//          int x;
//          struct Inner {
//              int y;
//          } inner;
//      }
//
TEST_F(ParserTest, NestedStructWithSimpleField)
{
    Type *type = TestType("struct Outer { int x; struct Inner { int y; } inner; };");

    //
    // Check struct Outer
    //
    EXPECT_EQ(type->kind, TYPE_STRUCT);
    EXPECT_STREQ(type->u.struct_t.name, "Outer");

    Field *x = type->u.struct_t.fields;
    ASSERT_NE(x, nullptr);
    Field *inner = x->next;
    EXPECT_EQ(inner->next, nullptr);

    //
    // Check field x
    //
    ASSERT_NE(x->type, nullptr);
    EXPECT_EQ(x->type->kind, TYPE_INT);
    EXPECT_EQ(x->type->u.integer.signedness, SIGNED_SIGNED);

    ASSERT_NE(x->declarator, nullptr);
    EXPECT_EQ(x->declarator->next, nullptr);
    EXPECT_EQ(x->declarator->kind, DECLARATOR_NAMED);

    ASSERT_NE(x->declarator->u.named.name, nullptr);
    EXPECT_STREQ(x->declarator->u.named.name, "x");
    EXPECT_EQ(x->declarator->u.named.pointers, nullptr);
    EXPECT_EQ(x->declarator->u.named.suffixes, nullptr);

    //
    // Check field inner
    //
    ASSERT_NE(inner->type, nullptr);
    EXPECT_EQ(inner->type->kind, TYPE_STRUCT);
    EXPECT_STREQ(inner->type->u.struct_t.name, "Inner");

    ASSERT_NE(inner->declarator, nullptr);
    EXPECT_EQ(inner->declarator->next, nullptr);
    EXPECT_EQ(inner->declarator->kind, DECLARATOR_NAMED);

    ASSERT_NE(inner->declarator->u.named.name, nullptr);
    EXPECT_STREQ(inner->declarator->u.named.name, "inner");
    EXPECT_EQ(inner->declarator->u.named.pointers, nullptr);
    EXPECT_EQ(inner->declarator->u.named.suffixes, nullptr);

    //
    // Check struct Inner
    //
    Field *y = inner->type->u.struct_t.fields;
    EXPECT_EQ(y->next, nullptr);

    //
    // Check field y
    //
    ASSERT_NE(y->type, nullptr);
    EXPECT_EQ(y->type->kind, TYPE_INT);
    EXPECT_EQ(y->type->u.integer.signedness, SIGNED_SIGNED);

    ASSERT_NE(y->declarator, nullptr);
    EXPECT_EQ(y->declarator->next, nullptr);
    EXPECT_EQ(y->declarator->kind, DECLARATOR_NAMED);

    ASSERT_NE(y->declarator->u.named.name, nullptr);
    EXPECT_STREQ(y->declarator->u.named.name, "y");
    EXPECT_EQ(y->declarator->u.named.pointers, nullptr);
    EXPECT_EQ(y->declarator->u.named.suffixes, nullptr);

    free_type(type);
}

//
// Struct with Pointer to Itself
//      struct Node {
//          int data;
//          struct Node *next;
//      }
//
TEST_F(ParserTest, StructWithPointerToItself)
{
    Type *type = TestType("struct Node { int data; struct Node *next; };");

    EXPECT_EQ(type->kind, TYPE_STRUCT);
    //TODO
    free_type(type);
}

//
// Function Pointer with Struct Parameter
//      void (*)(struct Pair {
//                  int x;
//                  int y;
//              })
//
TEST_F(ParserTest, FunctionPointerWithStructParameter)
{
    Type *type = TestType("void (*)(struct Pair { int x; int y; });");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    //TODO
    free_type(type);
}

//
// Nested Struct with Array Field
//      struct Container {
//          struct Item {
//              int value;
//          } items[10];
//      }
//
TEST_F(ParserTest, NestedStructWithArrayField)
{
    Type *type = TestType("struct Container { struct Item { int value; } items[10]; };");

    EXPECT_EQ(type->kind, TYPE_STRUCT);
    //TODO
    free_type(type);
}

//
// Union with Nested Struct and Anonymous Struct
//      union Variant {
//          struct {
//              int a;
//              int b;
//          };
//          struct Named {
//              float x;
//          } named;
//      }
//
TEST_F(ParserTest, UnionWithNestedStructAndAnonymousStruct)
{
    Type *type = TestType("union Variant { struct { int a; int b; }; struct Named { float x; } named; };");

    EXPECT_EQ(type->kind, TYPE_UNION);
    //TODO
    free_type(type);
}
