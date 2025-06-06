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

    EXPECT_STREQ(x->name, "x");
    EXPECT_EQ(x->bitfield, nullptr);

    //
    // Check field inner
    //
    ASSERT_NE(inner->type, nullptr);
    EXPECT_EQ(inner->type->kind, TYPE_STRUCT);
    EXPECT_STREQ(inner->type->u.struct_t.name, "Inner");

    EXPECT_STREQ(inner->name, "inner");
    EXPECT_EQ(inner->bitfield, nullptr);

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

    EXPECT_STREQ(y->name, "y");
    EXPECT_EQ(y->bitfield, nullptr);

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

    //
    // Check struct Node
    //
    EXPECT_EQ(type->kind, TYPE_STRUCT);
    EXPECT_STREQ(type->u.struct_t.name, "Node");

    Field *data = type->u.struct_t.fields;
    ASSERT_NE(data, nullptr);
    Field *next = data->next;
    EXPECT_EQ(next->next, nullptr);

    //
    // Check field data
    //
    EXPECT_STREQ(data->name, "data");
    EXPECT_EQ(data->bitfield, nullptr);
    ASSERT_NE(data->type, nullptr);
    EXPECT_EQ(data->type->kind, TYPE_INT);

    //
    // Check field next
    //
    EXPECT_STREQ(next->name, "next");
    EXPECT_EQ(next->bitfield, nullptr);
    ASSERT_NE(next->type, nullptr);
    EXPECT_EQ(next->type->kind, TYPE_POINTER);
    EXPECT_EQ(next->type->qualifiers, nullptr);
    EXPECT_EQ(next->type->u.pointer.qualifiers, nullptr);

    Type *target = next->type->u.pointer.target;
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->kind, TYPE_STRUCT);
    EXPECT_STREQ(target->u.struct_t.name, "Node");
    EXPECT_EQ(target->qualifiers, nullptr);

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
