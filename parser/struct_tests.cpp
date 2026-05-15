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
    ASSERT_NE(x->u.member.type, nullptr);
    EXPECT_EQ(x->u.member.type->kind, TYPE_INT);

    EXPECT_STREQ(x->u.member.name, "x");
    EXPECT_EQ(x->u.member.bitfield, nullptr);

    //
    // Check field inner
    //
    ASSERT_NE(inner->u.member.type, nullptr);
    EXPECT_EQ(inner->u.member.type->kind, TYPE_STRUCT);
    EXPECT_STREQ(inner->u.member.type->u.struct_t.name, "Inner");

    EXPECT_STREQ(inner->u.member.name, "inner");
    EXPECT_EQ(inner->u.member.bitfield, nullptr);

    //
    // Check struct Inner
    //
    Field *y = inner->u.member.type->u.struct_t.fields;
    EXPECT_EQ(y->next, nullptr);

    //
    // Check field y
    //
    ASSERT_NE(y->u.member.type, nullptr);
    EXPECT_EQ(y->u.member.type->kind, TYPE_INT);

    EXPECT_STREQ(y->u.member.name, "y");
    EXPECT_EQ(y->u.member.bitfield, nullptr);

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
    EXPECT_STREQ(data->u.member.name, "data");
    EXPECT_EQ(data->u.member.bitfield, nullptr);
    ASSERT_NE(data->u.member.type, nullptr);
    EXPECT_EQ(data->u.member.type->kind, TYPE_INT);

    //
    // Check field next
    //
    EXPECT_STREQ(next->u.member.name, "next");
    EXPECT_EQ(next->u.member.bitfield, nullptr);
    ASSERT_NE(next->u.member.type, nullptr);
    EXPECT_EQ(next->u.member.type->kind, TYPE_POINTER);
    EXPECT_EQ(next->u.member.type->qualifiers, nullptr);
    EXPECT_EQ(next->u.member.type->u.pointer.qualifiers, nullptr);

    Type *target = next->u.member.type->u.pointer.target;
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

    EXPECT_STREQ(fx->u.member.name, "x");
    EXPECT_EQ(fx->u.member.bitfield, nullptr);
    ASSERT_NE(fx->u.member.type, nullptr);
    EXPECT_EQ(fx->u.member.type->kind, TYPE_INT);
    EXPECT_EQ(fx->u.member.type->qualifiers, nullptr);

    EXPECT_STREQ(fy->u.member.name, "y");
    EXPECT_EQ(fy->u.member.bitfield, nullptr);
    ASSERT_NE(fy->u.member.type, nullptr);
    EXPECT_EQ(fy->u.member.type->kind, TYPE_INT);
    EXPECT_EQ(fy->u.member.type->qualifiers, nullptr);

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
    EXPECT_STREQ(items->u.member.name, "items");
    EXPECT_EQ(items->u.member.bitfield, nullptr);

    ASSERT_NE(items->u.member.type, nullptr);
    EXPECT_EQ(items->u.member.type->kind, TYPE_ARRAY);
    EXPECT_EQ(items->u.member.type->qualifiers, nullptr);
    EXPECT_FALSE(items->u.member.type->u.array.is_static);
    EXPECT_EQ(items->u.member.type->u.array.qualifiers, nullptr);

    Expr *size = items->u.member.type->u.array.size;
    ASSERT_NE(size, nullptr);
    EXPECT_EQ(size->kind, EXPR_LITERAL);
    EXPECT_EQ(size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(size->u.literal->u.int_val, 10);

    Type *elem = items->u.member.type->u.array.element;
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

    EXPECT_STREQ(value->u.member.name, "value");
    EXPECT_EQ(value->u.member.bitfield, nullptr);
    ASSERT_NE(value->u.member.type, nullptr);
    EXPECT_EQ(value->u.member.type->kind, TYPE_INT);
    EXPECT_EQ(value->u.member.type->qualifiers, nullptr);

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
    Type *type =
        TestType("union Variant { struct { int a; int b; }; struct Named { float x; } named; };");

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
    EXPECT_EQ(anon_member->u.member.name, nullptr);
    EXPECT_EQ(anon_member->u.member.bitfield, nullptr);
    Type *anon_struct = anon_member->u.member.type;
    ASSERT_NE(anon_struct, nullptr);
    EXPECT_EQ(anon_struct->kind, TYPE_STRUCT);
    EXPECT_EQ(anon_struct->u.struct_t.name, nullptr);
    EXPECT_EQ(anon_struct->qualifiers, nullptr);

    Field *fa = anon_struct->u.struct_t.fields;
    ASSERT_NE(fa, nullptr);
    Field *fb = fa->next;
    ASSERT_NE(fb, nullptr);
    EXPECT_EQ(fb->next, nullptr);

    EXPECT_STREQ(fa->u.member.name, "a");
    EXPECT_EQ(fa->u.member.bitfield, nullptr);
    ASSERT_NE(fa->u.member.type, nullptr);
    EXPECT_EQ(fa->u.member.type->kind, TYPE_INT);
    EXPECT_EQ(fa->u.member.type->qualifiers, nullptr);

    EXPECT_STREQ(fb->u.member.name, "b");
    EXPECT_EQ(fb->u.member.bitfield, nullptr);
    ASSERT_NE(fb->u.member.type, nullptr);
    EXPECT_EQ(fb->u.member.type->kind, TYPE_INT);
    EXPECT_EQ(fb->u.member.type->qualifiers, nullptr);

    //
    // struct Named { float x; } named;
    //
    EXPECT_STREQ(named_member->u.member.name, "named");
    EXPECT_EQ(named_member->u.member.bitfield, nullptr);
    Type *named_struct = named_member->u.member.type;
    ASSERT_NE(named_struct, nullptr);
    EXPECT_EQ(named_struct->kind, TYPE_STRUCT);
    EXPECT_STREQ(named_struct->u.struct_t.name, "Named");
    EXPECT_EQ(named_struct->qualifiers, nullptr);

    Field *fx = named_struct->u.struct_t.fields;
    ASSERT_NE(fx, nullptr);
    EXPECT_EQ(fx->next, nullptr);

    EXPECT_STREQ(fx->u.member.name, "x");
    EXPECT_EQ(fx->u.member.bitfield, nullptr);
    ASSERT_NE(fx->u.member.type, nullptr);
    EXPECT_EQ(fx->u.member.type->kind, TYPE_FLOAT);
    EXPECT_EQ(fx->u.member.type->qualifiers, nullptr);

    free_type(type);
}
