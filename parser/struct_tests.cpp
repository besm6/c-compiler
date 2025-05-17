#include "fixture.h"

//
// Nested types
//
// Variant 1: Nested Struct with Simple Field
// struct Outer { int x; struct Inner { int y; } inner; }
//
// Variant 2: Struct with Pointer to Itself
// struct Node { int data; struct Node *next; }
//
// Variant 3: Function Pointer with Struct Parameter
// void (*)(struct Pair { int x; int y; })
//
// Variant 4: Nested Struct with Array Field
// struct Container { struct Item { int value; } items[10]; }
//
// Variant 5: Union with Nested Struct and Anonymous Struct
// union Variant { struct { int a; int b; }; struct Named { float x; } named; }
//
TEST_F(ParserTest, NestedStructWithSimpleField)
{
    Type *type = TestType("struct Outer { int x; struct Inner { int y; } inner; };");

    EXPECT_EQ(type->kind, TYPE_STRUCT);
    //TODO
    free_type(type);
}

TEST_F(ParserTest, StructWithPointerToItself)
{
    Type *type = TestType("struct Node { int data; struct Node *next; };");

    EXPECT_EQ(type->kind, TYPE_STRUCT);
    //TODO
    free_type(type);
}

TEST_F(ParserTest, FunctionPointerWithStructParameter)
{
    Type *type = TestType("void (*)(struct Pair { int x; int y; });");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    //TODO
    free_type(type);
}

TEST_F(ParserTest, NestedStructWithArrayField)
{
    Type *type = TestType("struct Container { struct Item { int value; } items[10]; };");

    EXPECT_EQ(type->kind, TYPE_STRUCT);
    //TODO
    free_type(type);
}

TEST_F(ParserTest, UnionWithNestedStructAndAnonymousStruct)
{
    Type *type = TestType("union Variant { struct { int a; int b; }; struct Named { float x; } named; };");

    EXPECT_EQ(type->kind, TYPE_UNION);
    //TODO
    free_type(type);
}
