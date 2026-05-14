#include <gtest/gtest.h>

#include "ast.h"
#include "resolve.h"

// Fixture for resolve_expr tests
class ResolveExprTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        type_table   = create_hash_table();
        symbol_table = create_hash_table();
        // Add a known struct tag
        StructEntry *struct_entry               = (StructEntry *)malloc(sizeof(StructEntry));
        struct_entry->unique_tag                = strdup("struct_123");
        struct_entry->struct_from_current_scope = 1;
        hash_table_insert(type_table, "my_struct", struct_entry);
        // Add a known variable
        VarEntry *var_entry           = (VarEntry *)malloc(sizeof(VarEntry));
        var_entry->unique_name        = strdup("var_123");
        var_entry->from_current_scope = 1;
        var_entry->has_linkage        = 0;
        hash_table_insert(symbol_table, "my_var", var_entry);
        // Add a known function
        VarEntry *func_entry           = (VarEntry *)malloc(sizeof(VarEntry));
        func_entry->unique_name        = strdup("func_123");
        func_entry->from_current_scope = 1;
        func_entry->has_linkage        = 1;
        hash_table_insert(symbol_table, "my_func", func_entry);
    }

    void TearDown() override
    {
        hash_table_free(type_table);
        hash_table_free(symbol_table);
        type_table   = NULL;
        symbol_table = NULL;
    }

    // Helper to create a TYPE_STRUCT
    Type *make_struct_type(const char *name)
    {
        Type *t              = (Type *)malloc(sizeof(Type));
        t->kind              = TYPE_STRUCT;
        t->u.struct_t.name   = strdup(name);
        t->u.struct_t.fields = NULL;
        t->qualifiers        = NULL;
        return t;
    }

    // Helper to create an EXPR_VAR
    Expr *make_var_expr(const char *name)
    {
        Expr *e  = (Expr *)malloc(sizeof(Expr));
        e->kind  = EXPR_VAR;
        e->u.var = strdup(name);
        e->next  = NULL;
        e->type  = NULL;
        return e;
    }

    // Helper to create an EXPR_UNARY_OP
    Expr *make_unary_expr(UnaryOp op, Expr *expr)
    {
        Expr *e            = (Expr *)malloc(sizeof(Expr));
        e->kind            = EXPR_UNARY_OP;
        e->u.unary_op.op   = op;
        e->u.unary_op.expr = expr;
        e->next            = NULL;
        e->type            = NULL;
        return e;
    }

    // Helper to create an EXPR_BINARY_OP
    Expr *make_binary_expr(BinaryOp op, Expr *left, Expr *right)
    {
        Expr *e              = (Expr *)malloc(sizeof(Expr));
        e->kind              = EXPR_BINARY_OP;
        e->u.binary_op.op    = op;
        e->u.binary_op.left  = left;
        e->u.binary_op.right = right;
        e->next              = NULL;
        e->type              = NULL;
        return e;
    }

    // Helper to create an EXPR_ASSIGN
    Expr *make_assign_expr(AssignOp op, Expr *target, Expr *value)
    {
        Expr *e            = (Expr *)malloc(sizeof(Expr));
        e->kind            = EXPR_ASSIGN;
        e->u.assign.op     = op;
        e->u.assign.target = target;
        e->u.assign.value  = value;
        e->next            = NULL;
        e->type            = NULL;
        return e;
    }

    // Helper to create an EXPR_COND
    Expr *make_cond_expr(Expr *condition, Expr *then_expr, Expr *else_expr)
    {
        Expr *e             = (Expr *)malloc(sizeof(Expr));
        e->kind             = EXPR_COND;
        e->u.cond.condition = condition;
        e->u.cond.then_expr = then_expr;
        e->u.cond.else_expr = else_expr;
        e->next             = NULL;
        e->type             = NULL;
        return e;
    }

    // Helper to create an EXPR_CAST
    Expr *make_cast_expr(Type *type, Expr *expr)
    {
        Expr *e        = (Expr *)malloc(sizeof(Expr));
        e->kind        = EXPR_CAST;
        e->u.cast.type = type;
        e->u.cast.expr = expr;
        e->next        = NULL;
        e->type        = NULL;
        return e;
    }

    // Helper to create an EXPR_CALL
    Expr *make_call_expr(Expr *func, Expr *args)
    {
        Expr *e        = (Expr *)malloc(sizeof(Expr));
        e->kind        = EXPR_CALL;
        e->u.call.func = func;
        e->u.call.args = args;
        e->next        = NULL;
        e->type        = NULL;
        return e;
    }

    // Helper to create an EXPR_COMPOUND
    Expr *make_compound_expr(Type *type, InitItem *init)
    {
        Expr *e                    = (Expr *)malloc(sizeof(Expr));
        e->kind                    = EXPR_COMPOUND;
        e->u.compound_literal.type = type;
        e->u.compound_literal.init = init;
        e->next                    = NULL;
        e->type                    = NULL;
        return e;
    }

    // Helper to create an InitItem
    InitItem *make_init_item(Initializer *init)
    {
        InitItem *item    = (InitItem *)malloc(sizeof(InitItem));
        item->designators = NULL;
        item->init        = init;
        item->next        = NULL;
        return item;
    }

    // Helper to create an Initializer
    Initializer *make_single_initializer(Expr *expr)
    {
        Initializer *init = (Initializer *)malloc(sizeof(Initializer));
        init->kind        = INITIALIZER_SINGLE;
        init->u.expr      = expr;
        return init;
    }

    // Helper to create an EXPR_FIELD_ACCESS
    Expr *make_field_access_expr(Expr *expr, const char *field)
    {
        Expr *e                 = (Expr *)malloc(sizeof(Expr));
        e->kind                 = EXPR_FIELD_ACCESS;
        e->u.field_access.expr  = expr;
        e->u.field_access.field = strdup(field);
        e->next                 = NULL;
        e->type                 = NULL;
        return e;
    }

    // Helper to create an EXPR_PTR_ACCESS
    Expr *make_ptr_access_expr(Expr *expr, const char *field)
    {
        Expr *e               = (Expr *)malloc(sizeof(Expr));
        e->kind               = EXPR_PTR_ACCESS;
        e->u.ptr_access.expr  = expr;
        e->u.ptr_access.field = strdup(field);
        e->next               = NULL;
        e->type               = NULL;
        return e;
    }

    // Helper to create an EXPR_POST_INC
    Expr *make_post_inc_expr(Expr *expr)
    {
        Expr *e       = (Expr *)malloc(sizeof(Expr));
        e->kind       = EXPR_POST_INC;
        e->u.post_inc = expr;
        e->next       = NULL;
        e->type       = NULL;
        return e;
    }

    // Helper to create an EXPR_POST_DEC
    Expr *make_post_dec_expr(Expr *expr)
    {
        Expr *e       = (Expr *)malloc(sizeof(Expr));
        e->kind       = EXPR_POST_DEC;
        e->u.post_dec = expr;
        e->next       = NULL;
        e->type       = NULL;
        return e;
    }

    // Helper to create an EXPR_SIZEOF_EXPR
    Expr *make_sizeof_expr_expr(Expr *expr)
    {
        Expr *e          = (Expr *)malloc(sizeof(Expr));
        e->kind          = EXPR_SIZEOF_EXPR;
        e->u.sizeof_expr = expr;
        e->next          = NULL;
        e->type          = NULL;
        return e;
    }

    // Helper to create an EXPR_SIZEOF_TYPE
    Expr *make_sizeof_type_expr(Type *type)
    {
        Expr *e          = (Expr *)malloc(sizeof(Expr));
        e->kind          = EXPR_SIZEOF_TYPE;
        e->u.sizeof_type = type;
        e->next          = NULL;
        e->type          = NULL;
        return e;
    }

    // Helper to create an EXPR_ALIGNOF
    Expr *make_alignof_expr(Type *type)
    {
        Expr *e       = (Expr *)malloc(sizeof(Expr));
        e->kind       = EXPR_ALIGNOF;
        e->u.align_of = type;
        e->next       = NULL;
        e->type       = NULL;
        return e;
    }

    // Helper to create an EXPR_GENERIC
    Expr *make_generic_expr(Expr *controlling_expr, GenericAssoc *associations)
    {
        Expr *e                       = (Expr *)malloc(sizeof(Expr));
        e->kind                       = EXPR_GENERIC;
        e->u.generic.controlling_expr = controlling_expr;
        e->u.generic.associations     = associations;
        e->next                       = NULL;
        e->type                       = NULL;
        return e;
    }

    // Helper to create a GenericAssoc
    GenericAssoc *make_type_assoc(Type *type, Expr *expr)
    {
        GenericAssoc *assoc      = (GenericAssoc *)malloc(sizeof(GenericAssoc));
        assoc->kind              = GENERIC_ASSOC_TYPE;
        assoc->u.type_assoc.type = type;
        assoc->u.type_assoc.expr = expr;
        assoc->next              = NULL;
        return assoc;
    }

    // Helper to create an EXPR_LITERAL
    Expr *make_literal_expr(int value)
    {
        Expr *e        = (Expr *)malloc(sizeof(Expr));
        e->kind        = EXPR_LITERAL;
        Literal *lit   = (Literal *)malloc(sizeof(Literal));
        lit->kind      = LITERAL_INT;
        lit->u.int_val = value;
        e->u.literal   = lit;
        e->next        = NULL;
        e->type        = NULL;
        return e;
    }

    // Helper to free an Expr (simplified)
    void free_expr(Expr *e)
    {
        if (!e)
            return;
        switch (e->kind) {
        case EXPR_LITERAL:
            free(e->u.literal);
            break;
        case EXPR_VAR:
            free(e->u.var);
            break;
        case EXPR_UNARY_OP:
            free_expr(e->u.unary_op.expr);
            break;
        case EXPR_BINARY_OP:
            free_expr(e->u.binary_op.left);
            free_expr(e->u.binary_op.right);
            break;
        case EXPR_ASSIGN:
            free_expr(e->u.assign.target);
            free_expr(e->u.assign.value);
            break;
        case EXPR_COND:
            free_expr(e->u.cond.condition);
            free_expr(e->u.cond.then_expr);
            free_expr(e->u.cond.else_expr);
            break;
        case EXPR_CAST:
            free_type(e->u.cast.type);
            free_expr(e->u.cast.expr);
            break;
        case EXPR_CALL:
            free_expr(e->u.call.func);
            for (Expr *arg = e->u.call.args; arg;) {
                Expr *next = arg->next;
                free_expr(arg);
                arg = next;
            }
            break;
        case EXPR_COMPOUND:
            free_type(e->u.compound_literal.type);
            for (InitItem *item = e->u.compound_literal.init; item;) {
                InitItem *next = item->next;
                free_initializer(item->init);
                free(item);
                item = next;
            }
            break;
        case EXPR_FIELD_ACCESS:
            free_expr(e->u.field_access.expr);
            free(e->u.field_access.field);
            break;
        case EXPR_PTR_ACCESS:
            free_expr(e->u.ptr_access.expr);
            free(e->u.ptr_access.field);
            break;
        case EXPR_POST_INC:
            free_expr(e->u.post_inc);
            break;
        case EXPR_POST_DEC:
            free_expr(e->u.post_dec);
            break;
        case EXPR_SIZEOF_EXPR:
            free_expr(e->u.sizeof_expr);
            break;
        case EXPR_SIZEOF_TYPE:
            free_type(e->u.sizeof_type);
            break;
        case EXPR_ALIGNOF:
            free_type(e->u.align_of);
            break;
        case EXPR_GENERIC:
            free_expr(e->u.generic.controlling_expr);
            for (GenericAssoc *assoc = e->u.generic.associations; assoc;) {
                GenericAssoc *next = assoc->next;
                if (assoc->kind == GENERIC_ASSOC_TYPE) {
                    free_type(assoc->u.type_assoc.type);
                    free_expr(assoc->u.type_assoc.expr);
                } else {
                    free_expr(assoc->u.default_assoc);
                }
                free(assoc);
                assoc = next;
            }
            break;
        }
        free(e);
    }

    // Helper to free a Type (simplified)
    void free_type(Type *t)
    {
        if (!t)
            return;
        if (t->kind == TYPE_STRUCT) {
            free(t->u.struct_t.name);
        } else if (t->kind == TYPE_POINTER) {
            free_type(t->u.pointer.target);
        } else if (t->kind == TYPE_ARRAY) {
            free_type(t->u.array.element);
        } else if (t->kind == TYPE_FUNCTION) {
            free_type(t->u.function.return_type);
            for (Param *p = t->u.function.params; p;) {
                Param *next = p->next;
                free(p->name);
                free_type(p->type);
                free(p);
                p = next;
            }
        }
        free(t);
    }

    // Helper to free an Initializer
    void free_initializer(Initializer *init)
    {
        if (!init)
            return;
        if (init->kind == INITIALIZER_SINGLE) {
            free_expr(init->u.expr);
        } else {
            for (InitItem *item = init->u.items; item;) {
                InitItem *next = item->next;
                free_initializer(item->init);
                free(item);
                item = next;
            }
        }
        free(init);
    }
};

TEST_F(ResolveExprTest, ResolveVar_KnownIdentifier)
{
    Expr *e = make_var_expr("my_var");
    resolve_expr(e);
    EXPECT_STREQ(e->u.var, "var_123");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolveVar_UnknownIdentifier)
{
    Expr *e = make_var_expr("unknown_var");
    EXPECT_EXIT(
        {
            resolve_expr(e);
            exit(0);
        },
        ::testing::ExitedWithCode(1), "Undeclared variable unknown_var");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolveUnaryOp)
{
    Expr *inner = make_var_expr("my_var");
    Expr *e     = make_unary_expr(UNARY_DEREF, inner);
    resolve_expr(e);
    EXPECT_STREQ(e->u.unary_op.expr->u.var, "var_123");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolveBinaryOp)
{
    Expr *left  = make_var_expr("my_var");
    Expr *right = make_var_expr("my_var");
    Expr *e     = make_binary_expr(BINARY_ADD, left, right);
    resolve_expr(e);
    EXPECT_STREQ(e->u.binary_op.left->u.var, "var_123");
    EXPECT_STREQ(e->u.binary_op.right->u.var, "var_123");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolveAssign)
{
    Expr *target = make_var_expr("my_var");
    Expr *value  = make_var_expr("my_var");
    Expr *e      = make_assign_expr(ASSIGN_SIMPLE, target, value);
    resolve_expr(e);
    EXPECT_STREQ(e->u.assign.target->u.var, "var_123");
    EXPECT_STREQ(e->u.assign.value->u.var, "var_123");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolveCond)
{
    Expr *condition = make_var_expr("my_var");
    Expr *then_expr = make_var_expr("my_var");
    Expr *else_expr = make_var_expr("my_var");
    Expr *e         = make_cond_expr(condition, then_expr, else_expr);
    resolve_expr(e);
    EXPECT_STREQ(e->u.cond.condition->u.var, "var_123");
    EXPECT_STREQ(e->u.cond.then_expr->u.var, "var_123");
    EXPECT_STREQ(e->u.cond.else_expr->u.var, "var_123");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolveCast)
{
    Type *type  = make_struct_type("my_struct");
    Expr *inner = make_var_expr("my_var");
    Expr *e     = make_cast_expr(type, inner);
    resolve_expr(e);
    EXPECT_STREQ(e->u.cast.type->u.struct_t.name, "struct_123");
    EXPECT_STREQ(e->u.cast.expr->u.var, "var_123");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolveCall_KnownFunction)
{
    Expr *func = make_var_expr("my_func");
    Expr *arg1 = make_var_expr("my_var");
    Expr *arg2 = make_var_expr("my_var");
    arg1->next = arg2;
    Expr *e    = make_call_expr(func, arg1);
    resolve_expr(e);
    EXPECT_STREQ(e->u.call.func->u.var, "func_123");
    EXPECT_STREQ(e->u.call.args->u.var, "var_123");
    EXPECT_STREQ(e->u.call.args->next->u.var, "var_123");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolveCall_UnknownFunction)
{
    Expr *func = make_var_expr("unknown_func");
    Expr *e    = make_call_expr(func, NULL);
    EXPECT_EXIT(
        {
            resolve_expr(e);
            exit(0);
        },
        ::testing::ExitedWithCode(1), "Undeclared function unknown_func");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolveCompound)
{
    Type *type        = make_struct_type("my_struct");
    Expr *inner       = make_var_expr("my_var");
    Initializer *init = make_single_initializer(inner);
    InitItem *item    = make_init_item(init);
    Expr *e           = make_compound_expr(type, item);
    resolve_expr(e);
    EXPECT_STREQ(e->u.compound_literal.type->u.struct_t.name, "struct_123");
    EXPECT_STREQ(e->u.compound_literal.init->init->u.expr->u.var, "var_123");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolveFieldAccess)
{
    Expr *inner = make_var_expr("my_var");
    Expr *e     = make_field_access_expr(inner, "field");
    resolve_expr(e);
    EXPECT_STREQ(e->u.field_access.expr->u.var, "var_123");
    EXPECT_STREQ(e->u.field_access.field, "field");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolvePtrAccess)
{
    Expr *inner = make_var_expr("my_var");
    Expr *e     = make_ptr_access_expr(inner, "field");
    resolve_expr(e);
    EXPECT_STREQ(e->u.ptr_access.expr->u.var, "var_123");
    EXPECT_STREQ(e->u.ptr_access.field, "field");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolvePostInc)
{
    Expr *inner = make_var_expr("my_var");
    Expr *e     = make_post_inc_expr(inner);
    resolve_expr(e);
    EXPECT_STREQ(e->u.post_inc->u.var, "var_123");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolvePostDec)
{
    Expr *inner = make_var_expr("my_var");
    Expr *e     = make_post_dec_expr(inner);
    resolve_expr(e);
    EXPECT_STREQ(e->u.post_dec->u.var, "var_123");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolveSizeofExpr)
{
    Expr *inner = make_var_expr("my_var");
    Expr *e     = make_sizeof_expr_expr(inner);
    resolve_expr(e);
    EXPECT_STREQ(e->u.sizeof_expr->u.var, "var_123");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolveSizeofType)
{
    Type *type = make_struct_type("my_struct");
    Expr *e    = make_sizeof_type_expr(type);
    resolve_expr(e);
    EXPECT_STREQ(e->u.sizeof_type->u.struct_t.name, "struct_123");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolveAlignof)
{
    Type *type = make_struct_type("my_struct");
    Expr *e    = make_alignof_expr(type);
    resolve_expr(e);
    EXPECT_STREQ(e->u.align_of->u.struct_t.name, "struct_123");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolveGeneric)
{
    Expr *control       = make_var_expr("my_var");
    Type *type          = make_struct_type("my_struct");
    Expr *assoc_expr    = make_var_expr("my_var");
    GenericAssoc *assoc = make_type_assoc(type, assoc_expr);
    Expr *e             = make_generic_expr(control, assoc);
    resolve_expr(e);
    EXPECT_STREQ(e->u.generic.controlling_expr->u.var, "var_123");
    EXPECT_STREQ(e->u.generic.associations->u.type_assoc.type->u.struct_t.name, "struct_123");
    EXPECT_STREQ(e->u.generic.associations->u.type_assoc.expr->u.var, "var_123");
    free_expr(e);
}

TEST_F(ResolveExprTest, ResolveLiteral)
{
    Expr *e    = make_literal_expr(42);
    Expr *orig = (Expr *)malloc(sizeof(Expr));
    *orig      = *e;
    resolve_expr(e);
    EXPECT_EQ(e->kind, EXPR_LITERAL);
    EXPECT_EQ(e->u.literal->kind, orig->u.literal->kind);
    EXPECT_EQ(e->u.literal->u.int_val, orig->u.literal->u.int_val);
    free_expr(e);
    free_expr(orig);
}

TEST_F(ResolveExprTest, ResolveNullExpr)
{
    Expr *e = NULL;
    resolve_expr(e);
    EXPECT_EQ(e, nullptr);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    testing::GTEST_FLAG(death_test_style) = "threadsafe";
    return RUN_ALL_TESTS();
}
