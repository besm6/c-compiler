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

    //
    // void (*)(struct Pair { int x; int y; })
    //
    EXPECT_EQ(type->kind, TYPE_POINTER);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers, nullptr);

    Type *fn = type->u.pointer.target;
    ASSERT_NE(fn, nullptr);
    EXPECT_EQ(fn->kind, TYPE_FUNCTION);
    EXPECT_EQ(fn->qualifiers, nullptr);
    EXPECT_FALSE(fn->u.function.variadic);

    Type *ret = fn->u.function.return_type;
    ASSERT_NE(ret, nullptr);
    EXPECT_EQ(ret->kind, TYPE_VOID);
    EXPECT_EQ(ret->qualifiers, nullptr);

    Param *param = fn->u.function.params;
    ASSERT_NE(param, nullptr);
    EXPECT_EQ(param->next, nullptr);
    EXPECT_EQ(param->name, nullptr);

    Type *pair_ty = param->type;
    ASSERT_NE(pair_ty, nullptr);
    EXPECT_EQ(pair_ty->kind, TYPE_STRUCT);
    EXPECT_STREQ(pair_ty->u.struct_t.name, "Pair");
    EXPECT_EQ(pair_ty->qualifiers, nullptr);

    Field *fx = pair_ty->u.struct_t.fields;
    ASSERT_NE(fx, nullptr);
    Field *fy = fx->next;
    ASSERT_NE(fy, nullptr);
    EXPECT_EQ(fy->next, nullptr);

    EXPECT_STREQ(fx->name, "x");
    EXPECT_EQ(fx->bitfield, nullptr);
    ASSERT_NE(fx->type, nullptr);
    EXPECT_EQ(fx->type->kind, TYPE_INT);
    EXPECT_EQ(fx->type->qualifiers, nullptr);

    EXPECT_STREQ(fy->name, "y");
    EXPECT_EQ(fy->bitfield, nullptr);
    ASSERT_NE(fy->type, nullptr);
    EXPECT_EQ(fy->type->kind, TYPE_INT);
    EXPECT_EQ(fy->type->qualifiers, nullptr);

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

    //
    // struct Container
    //
    EXPECT_EQ(type->kind, TYPE_STRUCT);
    EXPECT_STREQ(type->u.struct_t.name, "Container");
    EXPECT_EQ(type->qualifiers, nullptr);

    Field *items = type->u.struct_t.fields;
    ASSERT_NE(items, nullptr);
    EXPECT_EQ(items->next, nullptr);

    //
    // Field items: struct Item[10]
    //
    EXPECT_STREQ(items->name, "items");
    EXPECT_EQ(items->bitfield, nullptr);

    ASSERT_NE(items->type, nullptr);
    EXPECT_EQ(items->type->kind, TYPE_ARRAY);
    EXPECT_EQ(items->type->qualifiers, nullptr);
    EXPECT_FALSE(items->type->u.array.is_static);
    EXPECT_EQ(items->type->u.array.qualifiers, nullptr);

    Expr *size = items->type->u.array.size;
    ASSERT_NE(size, nullptr);
    EXPECT_EQ(size->kind, EXPR_LITERAL);
    EXPECT_EQ(size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(size->u.literal->u.int_val, 10);

    Type *elem = items->type->u.array.element;
    ASSERT_NE(elem, nullptr);
    EXPECT_EQ(elem->kind, TYPE_STRUCT);
    EXPECT_STREQ(elem->u.struct_t.name, "Item");
    EXPECT_EQ(elem->qualifiers, nullptr);

    //
    // struct Item
    //
    Field *value = elem->u.struct_t.fields;
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->next, nullptr);

    EXPECT_STREQ(value->name, "value");
    EXPECT_EQ(value->bitfield, nullptr);
    ASSERT_NE(value->type, nullptr);
    EXPECT_EQ(value->type->kind, TYPE_INT);
    EXPECT_EQ(value->type->qualifiers, nullptr);

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

    //
    // union Variant
    //
    EXPECT_EQ(type->kind, TYPE_UNION);
    EXPECT_STREQ(type->u.struct_t.name, "Variant");
    EXPECT_EQ(type->qualifiers, nullptr);

    Field *anon_member = type->u.struct_t.fields;
    ASSERT_NE(anon_member, nullptr);
    Field *named_member = anon_member->next;
    ASSERT_NE(named_member, nullptr);
    EXPECT_EQ(named_member->next, nullptr);

    //
    // Anonymous struct { int a; int b; };
    //
    EXPECT_EQ(anon_member->name, nullptr);
    EXPECT_EQ(anon_member->bitfield, nullptr);
    Type *anon_struct = anon_member->type;
    ASSERT_NE(anon_struct, nullptr);
    EXPECT_EQ(anon_struct->kind, TYPE_STRUCT);
    EXPECT_EQ(anon_struct->u.struct_t.name, nullptr);
    EXPECT_EQ(anon_struct->qualifiers, nullptr);

    Field *fa = anon_struct->u.struct_t.fields;
    ASSERT_NE(fa, nullptr);
    Field *fb = fa->next;
    ASSERT_NE(fb, nullptr);
    EXPECT_EQ(fb->next, nullptr);

    EXPECT_STREQ(fa->name, "a");
    EXPECT_EQ(fa->bitfield, nullptr);
    ASSERT_NE(fa->type, nullptr);
    EXPECT_EQ(fa->type->kind, TYPE_INT);
    EXPECT_EQ(fa->type->qualifiers, nullptr);

    EXPECT_STREQ(fb->name, "b");
    EXPECT_EQ(fb->bitfield, nullptr);
    ASSERT_NE(fb->type, nullptr);
    EXPECT_EQ(fb->type->kind, TYPE_INT);
    EXPECT_EQ(fb->type->qualifiers, nullptr);

    //
    // struct Named { float x; } named;
    //
    EXPECT_STREQ(named_member->name, "named");
    EXPECT_EQ(named_member->bitfield, nullptr);
    Type *named_struct = named_member->type;
    ASSERT_NE(named_struct, nullptr);
    EXPECT_EQ(named_struct->kind, TYPE_STRUCT);
    EXPECT_STREQ(named_struct->u.struct_t.name, "Named");
    EXPECT_EQ(named_struct->qualifiers, nullptr);

    Field *fx = named_struct->u.struct_t.fields;
    ASSERT_NE(fx, nullptr);
    EXPECT_EQ(fx->next, nullptr);

    EXPECT_STREQ(fx->name, "x");
    EXPECT_EQ(fx->bitfield, nullptr);
    ASSERT_NE(fx->type, nullptr);
    EXPECT_EQ(fx->type->kind, TYPE_FLOAT);
    EXPECT_EQ(fx->type->qualifiers, nullptr);

    free_type(type);
}
