#include <gtest/gtest.h>

#include "ast.h"
#include "xalloc.h"

// Internal clone functions not declared in ast.h but with external linkage.
extern "C" {
DeclSpec *clone_decl_spec(const DeclSpec *spec);
FunctionSpec *clone_function_spec(const FunctionSpec *fs);
AlignmentSpec *clone_alignment_spec(const AlignmentSpec *as);
Designator *clone_designator(const Designator *design);
Initializer *clone_initializer(const Initializer *init);
}

class AstCloneTest : public ::testing::Test {
protected:
    void TearDown() override
    {
        xreport_lost_memory();
        EXPECT_EQ(xtotal_allocated_size(), 0);
        xfree_all();
    }
};

// ---- clone_type_qualifier ----

TEST_F(AstCloneTest, CloneTypeQualifier)
{
    TypeQualifier *orig = new_type_qualifier(TYPE_QUALIFIER_CONST);
    orig->next          = new_type_qualifier(TYPE_QUALIFIER_VOLATILE);

    TypeQualifier *copy = clone_type_qualifier(orig);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_type_qualifier(orig, copy));

    free_type_qualifier(orig);
    free_type_qualifier(copy);
}

TEST_F(AstCloneTest, CloneTypeQualifierNull)
{
    EXPECT_EQ(nullptr, clone_type_qualifier(nullptr));
}

// ---- clone_type ----

TEST_F(AstCloneTest, CloneType)
{
    Type *orig = new_type(TYPE_INT, __func__, __FILE__, __LINE__);

    Type *copy = clone_type(orig, __func__, __FILE__, __LINE__);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_type(orig, copy));

    free_type(orig);
    free_type(copy);
}

TEST_F(AstCloneTest, CloneTypePointer)
{
    Type *orig                 = new_type(TYPE_POINTER, __func__, __FILE__, __LINE__);
    orig->u.pointer.target     = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    orig->u.pointer.qualifiers = new_type_qualifier(TYPE_QUALIFIER_CONST);

    Type *copy = clone_type(orig, __func__, __FILE__, __LINE__);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_type(orig, copy));
    EXPECT_NE(orig->u.pointer.target, copy->u.pointer.target);

    free_type(orig);
    free_type(copy);
}

TEST_F(AstCloneTest, CloneTypeStruct)
{
    Type *orig              = new_type(TYPE_STRUCT, __func__, __FILE__, __LINE__);
    orig->u.struct_t.name   = xstrdup("point");
    Field *f                = new_field();
    f->type                 = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    f->name                 = xstrdup("x");
    orig->u.struct_t.fields = f;

    Type *copy = clone_type(orig, __func__, __FILE__, __LINE__);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_type(orig, copy));
    EXPECT_NE(orig->u.struct_t.name, copy->u.struct_t.name);
    EXPECT_STREQ("point", copy->u.struct_t.name);

    free_type(orig);
    free_type(copy);
}

TEST_F(AstCloneTest, CloneTypeEnum)
{
    Type *orig                 = new_type(TYPE_ENUM, __func__, __FILE__, __LINE__);
    orig->u.enum_t.name        = xstrdup("color");
    orig->u.enum_t.enumerators = new_enumerator(xstrdup("RED"), nullptr);

    Type *copy = clone_type(orig, __func__, __FILE__, __LINE__);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_type(orig, copy));
    EXPECT_NE(orig->u.enum_t.name, copy->u.enum_t.name);
    EXPECT_STREQ("color", copy->u.enum_t.name);

    free_type(orig);
    free_type(copy);
}

TEST_F(AstCloneTest, CloneTypeTypedefName)
{
    Type *orig                = new_type(TYPE_TYPEDEF_NAME, __func__, __FILE__, __LINE__);
    orig->u.typedef_name.name = xstrdup("size_t");

    Type *copy = clone_type(orig, __func__, __FILE__, __LINE__);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_type(orig, copy));
    EXPECT_NE(orig->u.typedef_name.name, copy->u.typedef_name.name);
    EXPECT_STREQ("size_t", copy->u.typedef_name.name);

    free_type(orig);
    free_type(copy);
}

TEST_F(AstCloneTest, CloneTypeFunction)
{
    Type *orig                   = new_type(TYPE_FUNCTION, __func__, __FILE__, __LINE__);
    orig->u.function.return_type = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    Param *p                     = new_param();
    p->name                      = xstrdup("n");
    p->type                      = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    orig->u.function.params      = p;

    Type *copy = clone_type(orig, __func__, __FILE__, __LINE__);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_type(orig, copy));
    EXPECT_NE(orig->u.function.return_type, copy->u.function.return_type);
    EXPECT_NE(orig->u.function.params, copy->u.function.params);

    free_type(orig);
    free_type(copy);
}

// ---- clone_literal ----

TEST_F(AstCloneTest, CloneLiteralInt)
{
    Literal *orig   = new_literal(LITERAL_INT);
    orig->u.int_val = 42;

    Literal *copy = clone_literal(orig);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_literal(orig, copy));
    EXPECT_EQ(42, copy->u.int_val);

    free_literal(orig);
    free_literal(copy);
}

TEST_F(AstCloneTest, CloneLiteralFloat)
{
    Literal *orig    = new_literal(LITERAL_FLOAT);
    orig->u.real_val = 3.14;

    Literal *copy = clone_literal(orig);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_literal(orig, copy));
    EXPECT_EQ(3.14, copy->u.real_val);

    free_literal(orig);
    free_literal(copy);
}

TEST_F(AstCloneTest, CloneLiteralChar)
{
    Literal *orig    = new_literal(LITERAL_CHAR);
    orig->u.char_val = 'A';

    Literal *copy = clone_literal(orig);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_literal(orig, copy));
    EXPECT_EQ('A', copy->u.char_val);

    free_literal(orig);
    free_literal(copy);
}

TEST_F(AstCloneTest, CloneLiteralString)
{
    Literal *orig      = new_literal(LITERAL_STRING);
    orig->u.string_val = xstrdup("hello");

    Literal *copy = clone_literal(orig);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_literal(orig, copy));
    EXPECT_NE(orig->u.string_val, copy->u.string_val);
    EXPECT_STREQ("hello", copy->u.string_val);

    free_literal(orig);
    free_literal(copy);
}

TEST_F(AstCloneTest, CloneLiteralEnum)
{
    Literal *orig      = new_literal(LITERAL_ENUM);
    orig->u.enum_const = xstrdup("RED");

    Literal *copy = clone_literal(orig);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_literal(orig, copy));
    EXPECT_NE(orig->u.enum_const, copy->u.enum_const);
    EXPECT_STREQ("RED", copy->u.enum_const);

    free_literal(orig);
    free_literal(copy);
}

// ---- clone_expression ----

TEST_F(AstCloneTest, CloneExpression)
{
    Expr *orig  = new_expression(EXPR_VAR);
    orig->u.var = xstrdup("x");

    Expr *copy = clone_expression(orig);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_expr(orig, copy));
    EXPECT_NE(orig->u.var, copy->u.var);
    EXPECT_STREQ("x", copy->u.var);

    free_expression(orig);
    free_expression(copy);
}

TEST_F(AstCloneTest, CloneExpressionNull)
{
    EXPECT_EQ(nullptr, clone_expression(nullptr));
}

// ---- clone_unary_op ----

TEST_F(AstCloneTest, CloneUnaryOp)
{
    Expr *orig                   = new_expression(EXPR_UNARY_OP);
    orig->u.unary_op.op          = UNARY_NEG;
    orig->u.unary_op.expr        = new_expression(EXPR_VAR);
    orig->u.unary_op.expr->u.var = xstrdup("x");

    Expr *copy = clone_expression(orig);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_expr(orig, copy));
    EXPECT_EQ(UNARY_NEG, copy->u.unary_op.op);
    EXPECT_NE(orig->u.unary_op.expr, copy->u.unary_op.expr);

    free_expression(orig);
    free_expression(copy);
}

// ---- clone_binary_op ----

TEST_F(AstCloneTest, CloneBinaryOp)
{
    Expr *orig                     = new_expression(EXPR_BINARY_OP);
    orig->u.binary_op.op           = BINARY_ADD;
    orig->u.binary_op.left         = new_expression(EXPR_VAR);
    orig->u.binary_op.left->u.var  = xstrdup("a");
    orig->u.binary_op.right        = new_expression(EXPR_VAR);
    orig->u.binary_op.right->u.var = xstrdup("b");

    Expr *copy = clone_expression(orig);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_expr(orig, copy));
    EXPECT_EQ(BINARY_ADD, copy->u.binary_op.op);
    EXPECT_NE(orig->u.binary_op.left, copy->u.binary_op.left);
    EXPECT_NE(orig->u.binary_op.right, copy->u.binary_op.right);

    free_expression(orig);
    free_expression(copy);
}

// ---- clone_assign_op ----

TEST_F(AstCloneTest, CloneAssignOp)
{
    Expr *orig                                 = new_expression(EXPR_ASSIGN);
    orig->u.assign.op                          = ASSIGN_SIMPLE;
    orig->u.assign.target                      = new_expression(EXPR_VAR);
    orig->u.assign.target->u.var               = xstrdup("x");
    orig->u.assign.value                       = new_expression(EXPR_LITERAL);
    orig->u.assign.value->u.literal            = new_literal(LITERAL_INT);
    orig->u.assign.value->u.literal->u.int_val = 1;

    Expr *copy = clone_expression(orig);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_expr(orig, copy));
    EXPECT_EQ(ASSIGN_SIMPLE, copy->u.assign.op);
    EXPECT_NE(orig->u.assign.target, copy->u.assign.target);
    EXPECT_NE(orig->u.assign.value, copy->u.assign.value);

    free_expression(orig);
    free_expression(copy);
}

// ---- clone_field ----

TEST_F(AstCloneTest, CloneField)
{
    Field *orig = new_field();
    orig->type  = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    orig->name  = xstrdup("x");

    Field *copy = clone_field(orig);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_field(orig, copy));
    EXPECT_NE(orig->name, copy->name);
    EXPECT_STREQ("x", copy->name);

    free_field(orig);
    free_field(copy);
}

TEST_F(AstCloneTest, CloneFieldNull)
{
    EXPECT_EQ(nullptr, clone_field(nullptr));
}

// ---- clone_enumerator ----

TEST_F(AstCloneTest, CloneEnumerator)
{
    Expr *val                 = new_expression(EXPR_LITERAL);
    val->u.literal            = new_literal(LITERAL_INT);
    val->u.literal->u.int_val = 0;

    Enumerator *orig = new_enumerator(xstrdup("RED"), val);
    orig->next       = new_enumerator(xstrdup("GREEN"), nullptr);

    Enumerator *copy = clone_enumerator(orig);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_enumerator(orig, copy));
    EXPECT_NE(orig->name, copy->name);
    EXPECT_STREQ("RED", copy->name);
    ASSERT_NE(nullptr, copy->next);
    EXPECT_STREQ("GREEN", copy->next->name);

    free_enumerator(orig);
    free_enumerator(copy);
}

TEST_F(AstCloneTest, CloneEnumeratorNull)
{
    EXPECT_EQ(nullptr, clone_enumerator(nullptr));
}

// ---- clone_param ----

TEST_F(AstCloneTest, CloneParam)
{
    Param *orig = new_param();
    orig->name  = xstrdup("n");
    orig->type  = new_type(TYPE_INT, __func__, __FILE__, __LINE__);

    Param *copy = clone_param(orig);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_param(orig, copy));
    EXPECT_NE(orig->name, copy->name);
    EXPECT_STREQ("n", copy->name);

    free_param(orig);
    free_param(copy);
}

TEST_F(AstCloneTest, CloneParamNull)
{
    EXPECT_EQ(nullptr, clone_param(nullptr));
}

// ---- clone_function_spec ----

TEST_F(AstCloneTest, CloneFunctionSpec)
{
    FunctionSpec *orig = new_function_spec(FUNC_SPEC_INLINE);
    orig->next         = new_function_spec(FUNC_SPEC_NORETURN);

    FunctionSpec *copy = clone_function_spec(orig);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_function_spec(orig, copy));
    EXPECT_EQ(FUNC_SPEC_INLINE, copy->kind);
    ASSERT_NE(nullptr, copy->next);
    EXPECT_EQ(FUNC_SPEC_NORETURN, copy->next->kind);

    free_function_spec(orig);
    free_function_spec(copy);
}

TEST_F(AstCloneTest, CloneFunctionSpecNull)
{
    EXPECT_EQ(nullptr, clone_function_spec(nullptr));
}

// ---- clone_alignment_spec ----

TEST_F(AstCloneTest, CloneAlignmentSpec)
{
    AlignmentSpec *orig_t = new_alignment_spec(ALIGN_SPEC_TYPE);
    orig_t->u.type        = new_type(TYPE_INT, __func__, __FILE__, __LINE__);

    AlignmentSpec *copy_t = clone_alignment_spec(orig_t);
    ASSERT_NE(nullptr, copy_t);
    EXPECT_NE(orig_t, copy_t);
    EXPECT_TRUE(compare_alignment_spec(orig_t, copy_t));
    EXPECT_NE(orig_t->u.type, copy_t->u.type);

    free_alignment_spec(orig_t);
    free_alignment_spec(copy_t);

    AlignmentSpec *orig_e                = new_alignment_spec(ALIGN_SPEC_EXPR);
    orig_e->u.expr                       = new_expression(EXPR_LITERAL);
    orig_e->u.expr->u.literal            = new_literal(LITERAL_INT);
    orig_e->u.expr->u.literal->u.int_val = 8;

    AlignmentSpec *copy_e = clone_alignment_spec(orig_e);
    ASSERT_NE(nullptr, copy_e);
    EXPECT_NE(orig_e, copy_e);
    EXPECT_TRUE(compare_alignment_spec(orig_e, copy_e));
    EXPECT_NE(orig_e->u.expr, copy_e->u.expr);

    free_alignment_spec(orig_e);
    free_alignment_spec(copy_e);
}

TEST_F(AstCloneTest, CloneAlignmentSpecNull)
{
    EXPECT_EQ(nullptr, clone_alignment_spec(nullptr));
}

// ---- clone_decl_spec ----

TEST_F(AstCloneTest, CloneDeclSpec)
{
    DeclSpec *orig   = new_decl_spec();
    orig->qualifiers = new_type_qualifier(TYPE_QUALIFIER_CONST);
    orig->func_specs = new_function_spec(FUNC_SPEC_INLINE);

    DeclSpec *copy = clone_decl_spec(orig);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_decl_spec(orig, copy));

    free_decl_spec(orig);
    free_decl_spec(copy);
}

TEST_F(AstCloneTest, CloneDeclSpecNull)
{
    EXPECT_EQ(nullptr, clone_decl_spec(nullptr));
}

// ---- clone_storage_class ----

TEST_F(AstCloneTest, CloneStorageClass)
{
    DeclSpec *orig = new_decl_spec();
    orig->storage  = STORAGE_CLASS_STATIC;

    DeclSpec *copy = clone_decl_spec(orig);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_EQ(STORAGE_CLASS_STATIC, copy->storage);
    EXPECT_TRUE(compare_decl_spec(orig, copy));

    free_decl_spec(orig);
    free_decl_spec(copy);
}

// ---- clone_designator ----

TEST_F(AstCloneTest, CloneDesignator)
{
    Designator *orig_a                   = new_designator(DESIGNATOR_ARRAY);
    orig_a->u.expr                       = new_expression(EXPR_LITERAL);
    orig_a->u.expr->u.literal            = new_literal(LITERAL_INT);
    orig_a->u.expr->u.literal->u.int_val = 3;

    Designator *copy_a = clone_designator(orig_a);
    ASSERT_NE(nullptr, copy_a);
    EXPECT_NE(orig_a, copy_a);
    EXPECT_TRUE(compare_designator(orig_a, copy_a));
    EXPECT_NE(orig_a->u.expr, copy_a->u.expr);

    free_designator(orig_a);
    free_designator(copy_a);

    Designator *orig_f = new_designator(DESIGNATOR_FIELD);
    orig_f->u.name     = xstrdup("x");

    Designator *copy_f = clone_designator(orig_f);
    ASSERT_NE(nullptr, copy_f);
    EXPECT_NE(orig_f, copy_f);
    EXPECT_TRUE(compare_designator(orig_f, copy_f));
    EXPECT_NE(orig_f->u.name, copy_f->u.name);
    EXPECT_STREQ("x", copy_f->u.name);

    free_designator(orig_f);
    free_designator(copy_f);
}

TEST_F(AstCloneTest, CloneDesignatorNull)
{
    EXPECT_EQ(nullptr, clone_designator(nullptr));
}

// ---- clone_initializer ----

TEST_F(AstCloneTest, CloneInitializer)
{
    Initializer *orig_s                  = new_initializer(INITIALIZER_SINGLE);
    orig_s->u.expr                       = new_expression(EXPR_LITERAL);
    orig_s->u.expr->u.literal            = new_literal(LITERAL_INT);
    orig_s->u.expr->u.literal->u.int_val = 7;

    Initializer *copy_s = clone_initializer(orig_s);
    ASSERT_NE(nullptr, copy_s);
    EXPECT_NE(orig_s, copy_s);
    EXPECT_TRUE(compare_initializer(orig_s, copy_s));
    EXPECT_NE(orig_s->u.expr, copy_s->u.expr);

    free_initializer(orig_s);
    free_initializer(copy_s);

    Initializer *inner                  = new_initializer(INITIALIZER_SINGLE);
    inner->u.expr                       = new_expression(EXPR_LITERAL);
    inner->u.expr->u.literal            = new_literal(LITERAL_INT);
    inner->u.expr->u.literal->u.int_val = 1;

    Initializer *orig_c = new_initializer(INITIALIZER_COMPOUND);
    orig_c->u.items     = new_init_item(nullptr, inner);

    Initializer *copy_c = clone_initializer(orig_c);
    ASSERT_NE(nullptr, copy_c);
    EXPECT_NE(orig_c, copy_c);
    EXPECT_TRUE(compare_initializer(orig_c, copy_c));
    EXPECT_NE(orig_c->u.items, copy_c->u.items);

    free_initializer(orig_c);
    free_initializer(copy_c);
}

TEST_F(AstCloneTest, CloneInitializerNull)
{
    EXPECT_EQ(nullptr, clone_initializer(nullptr));
}

// ---- clone_init_item ----

TEST_F(AstCloneTest, CloneInitItem)
{
    Designator *des = new_designator(DESIGNATOR_FIELD);
    des->u.name     = xstrdup("x");

    Initializer *init                  = new_initializer(INITIALIZER_SINGLE);
    init->u.expr                       = new_expression(EXPR_LITERAL);
    init->u.expr->u.literal            = new_literal(LITERAL_INT);
    init->u.expr->u.literal->u.int_val = 5;

    InitItem *orig = new_init_item(des, init);

    InitItem *copy = clone_init_item(orig);
    ASSERT_NE(nullptr, copy);
    EXPECT_NE(orig, copy);
    EXPECT_TRUE(compare_init_item(orig, copy));
    EXPECT_NE(orig->designators, copy->designators);
    EXPECT_NE(orig->init, copy->init);

    free_init_item(orig);
    free_init_item(copy);
}

TEST_F(AstCloneTest, CloneInitItemNull)
{
    EXPECT_EQ(nullptr, clone_init_item(nullptr));
}

// ---- clone_generic_assoc ----

TEST_F(AstCloneTest, CloneGenericAssoc)
{
    GenericAssoc *orig_t                 = new_generic_assoc(GENERIC_ASSOC_TYPE);
    orig_t->u.type_assoc.type            = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    orig_t->u.type_assoc.expr            = new_expression(EXPR_LITERAL);
    orig_t->u.type_assoc.expr->u.literal = new_literal(LITERAL_INT);
    orig_t->u.type_assoc.expr->u.literal->u.int_val = 1;

    GenericAssoc *copy_t = clone_generic_assoc(orig_t);
    ASSERT_NE(nullptr, copy_t);
    EXPECT_NE(orig_t, copy_t);
    EXPECT_TRUE(compare_generic_assoc(orig_t, copy_t));
    EXPECT_NE(orig_t->u.type_assoc.type, copy_t->u.type_assoc.type);
    EXPECT_NE(orig_t->u.type_assoc.expr, copy_t->u.type_assoc.expr);

    free_generic_assoc(orig_t);
    free_generic_assoc(copy_t);

    GenericAssoc *orig_d           = new_generic_assoc(GENERIC_ASSOC_DEFAULT);
    orig_d->u.default_assoc        = new_expression(EXPR_VAR);
    orig_d->u.default_assoc->u.var = xstrdup("x");

    GenericAssoc *copy_d = clone_generic_assoc(orig_d);
    ASSERT_NE(nullptr, copy_d);
    EXPECT_NE(orig_d, copy_d);
    EXPECT_TRUE(compare_generic_assoc(orig_d, copy_d));
    EXPECT_NE(orig_d->u.default_assoc, copy_d->u.default_assoc);

    free_generic_assoc(orig_d);
    free_generic_assoc(copy_d);
}

TEST_F(AstCloneTest, CloneGenericAssocNull)
{
    EXPECT_EQ(nullptr, clone_generic_assoc(nullptr));
}
