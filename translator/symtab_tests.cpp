#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "translator.h"
#include "symtab.h"
#include "internal.h"
#include "xalloc.h"

// Helper functions for type creation
Type *new_array_type(Type *element, size_t size)
{
    Type *array            = new_type(TYPE_ARRAY);
    array->u.array.element = element;
    array->u.array.size    = new_expression(EXPR_LITERAL);

    array->u.array.size->u.literal            = new_literal(LITERAL_INT);
    array->u.array.size->u.literal->u.int_val = size;
    return array;
}

Type *new_function_type(Type *return_type, Param *params)
{
    Type *func                   = new_type(TYPE_FUNCTION);
    func->u.function.return_type = return_type;
    func->u.function.params      = params;
    return func;
}

// Helper to create a parameter list
Param *create_param(const char *name, Type *type)
{
    Param *p = new_param();
    p->name  = xstrdup(name);
    p->type  = type;
    return p;
}

// Helper to compare StaticInitializer lists
bool compare_static_initializer(const StaticInitializer *a, const StaticInitializer *b)
{
    while (a && b) {
        if (a->kind != b->kind)
            return false;
        switch (a->kind) {
        case INIT_CHAR:
            if (a->u.char_val != b->u.char_val)
                return false;
            break;
        case INIT_INT:
            if (a->u.int_val != b->u.int_val)
                return false;
            break;
        case INIT_LONG:
            if (a->u.long_val != b->u.long_val)
                return false;
            break;
        case INIT_UCHAR:
            if (a->u.uchar_val != b->u.uchar_val)
                return false;
            break;
        case INIT_UINT:
            if (a->u.uint_val != b->u.uint_val)
                return false;
            break;
        case INIT_ULONG:
            if (a->u.ulong_val != b->u.ulong_val)
                return false;
            break;
        case INIT_DOUBLE:
            if (a->u.double_val != b->u.double_val)
                return false;
            break;
        case INIT_STRING:
            if (strcmp(a->u.string_val.str, b->u.string_val.str) != 0 ||
                a->u.string_val.null_terminated != b->u.string_val.null_terminated)
                return false;
            break;
        case INIT_ZERO:
            if (a->u.zero_bytes != b->u.zero_bytes)
                return false;
            break;
        case INIT_POINTER:
            if (strcmp(a->u.ptr_id, b->u.ptr_id) != 0)
                return false;
            break;
        }
        a = a->next;
        b = b->next;
    }
    return a == NULL && b == NULL;
}

// Test fixture for symtab tests
class SymtabTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        xalloc_debug = 1;
        symtab_init();
    }
    void TearDown() override
    {
        symtab_destroy();
        xreport_lost_memory();
        EXPECT_EQ(xtotal_allocated_size(), 0);
        xfree_all();
    }
};

// Test symtab_init
TEST_F(SymtabTest, InitEmptyTable)
{
    ASSERT_EQ(symtab_get_opt("x"), nullptr);
}

// Test symtab_destroy
TEST_F(SymtabTest, DestroyEmptyTable)
{
    symtab_destroy();
    symtab_init(); // Re-init to ensure no corruption
}

// Test symtab_add_automatic_var
TEST_F(SymtabTest, AddAutomaticVar1)
{
    Type *int_type = new_type(TYPE_INT);
    symtab_add_automatic_var("x", int_type);

    Symbol *sym = symtab_get("x");
    ASSERT_STREQ(sym->name, "x");
    ASSERT_TRUE(compare_type(sym->type, int_type));
    ASSERT_EQ(sym->kind, SYM_LOCAL);

    free_type(int_type);
}

// Test symtab_add_automatic_var overwrite
TEST_F(SymtabTest, AddAutomaticVarOverwrite)
{
    Type *int_type  = new_type(TYPE_INT);
    Type *char_type = new_type(TYPE_CHAR);

    symtab_add_automatic_var("x", int_type);
    symtab_add_automatic_var("x", char_type);

    Symbol *sym = symtab_get("x");
    ASSERT_STREQ(sym->name, "x");
    ASSERT_TRUE(compare_type(sym->type, char_type));
    ASSERT_EQ(sym->kind, SYM_LOCAL);

    free_type(int_type);
    free_type(char_type);
}

// Test symtab_add_static_var
TEST_F(SymtabTest, AddStaticVarWithInitializer)
{
    Type *int_type          = new_type(TYPE_INT);
    StaticInitializer *init = new_static_initializer(INIT_INT);
    init->u.int_val         = 42;

    symtab_add_static_var("x", int_type, true, INIT_INITIALIZED, init);

    Symbol *sym = symtab_get("x");
    ASSERT_STREQ(sym->name, "x");
    ASSERT_TRUE(compare_type(sym->type, int_type));
    ASSERT_EQ(sym->kind, SYM_STATIC);
    ASSERT_TRUE(sym->u.static_var.global);
    ASSERT_EQ(sym->u.static_var.init_kind, INIT_INITIALIZED);
    ASSERT_TRUE(compare_static_initializer(sym->u.static_var.init_list, init));

    free_type(int_type);
}

// Test symtab_add_static_var with no initializer
TEST_F(SymtabTest, AddStaticVarNoInitializer)
{
    Type *int_type = new_type(TYPE_INT);

    symtab_add_static_var("y", int_type, false, INIT_TENTATIVE, NULL);

    Symbol *sym = symtab_get("y");
    ASSERT_STREQ(sym->name, "y");
    ASSERT_TRUE(compare_type(sym->type, int_type));
    ASSERT_EQ(sym->kind, SYM_STATIC);
    ASSERT_FALSE(sym->u.static_var.global);
    ASSERT_EQ(sym->u.static_var.init_kind, INIT_TENTATIVE);
    ASSERT_EQ(sym->u.static_var.init_list, nullptr);

    free_type(int_type);
}

// Test symtab_add_fun
TEST_F(SymtabTest, AddFunction)
{
    Type *int_type = new_type(TYPE_INT);
    Param *param   = create_param("a", clone_type(int_type));
    const Type *fun_type = new_function_type(clone_type(int_type), param);

    symtab_add_fun("f", fun_type, true, true);

    Symbol *sym = symtab_get("f");
    ASSERT_STREQ(sym->name, "f");
    ASSERT_TRUE(compare_type(sym->type, fun_type));
    ASSERT_EQ(sym->kind, SYM_FUNC);
    ASSERT_TRUE(sym->u.func.global);
    ASSERT_TRUE(sym->u.func.defined);

    free_type(int_type);
}

// Test symtab_add_string
TEST_F(SymtabTest, AddStringLiteral)
{
    const char *str    = "hello";
    const char *str_id = symtab_add_string(str);

    Symbol *sym = symtab_get(str_id);
    ASSERT_STREQ(sym->name, str_id);
    Type *expected_type = new_array_type(new_type(TYPE_CHAR), strlen(str) + 1);
    ASSERT_TRUE(compare_type(sym->type, expected_type));
    ASSERT_EQ(sym->kind, SYM_CONST);
    StaticInitializer expected_init = { .kind         = INIT_STRING,
                                        .u.string_val = { xstrdup(str), true },
                                        .next         = NULL };
    ASSERT_TRUE(compare_static_initializer(sym->u.const_init, &expected_init));

    free_type(expected_type->u.array.element);
    free_type(expected_type);
    xfree(expected_init.u.string_val.str);
    // str_id owned by symtab
}

// Test symtab_get
TEST_F(SymtabTest, GetNonExistentSymbol)
{
    ASSERT_EXIT(symtab_get("x"), ::testing::ExitedWithCode(1), "Symbol 'x' not found");
}

// Test symtab_get_opt
TEST_F(SymtabTest, GetOptExistentAndNonExistent)
{
    Type *int_type = new_type(TYPE_INT);
    symtab_add_automatic_var("x", int_type);

    Symbol *sym = symtab_get_opt("x");
    ASSERT_NE(sym, nullptr);
    ASSERT_STREQ(sym->name, "x");
    ASSERT_TRUE(compare_type(sym->type, int_type));

    ASSERT_EQ(symtab_get_opt("y"), nullptr);

    free_type(int_type);
}

// Test symtab_is_global
TEST_F(SymtabTest, IsGlobal)
{
    Type *int_type = new_type(TYPE_INT);

    symtab_add_static_var("x", int_type, true, INIT_NONE, NULL);
    symtab_add_static_var("y", int_type, false, INIT_NONE, NULL);
    symtab_add_automatic_var("z", int_type);

    ASSERT_TRUE(symtab_is_global("x"));
    ASSERT_FALSE(symtab_is_global("y"));
    ASSERT_FALSE(symtab_is_global("z"));

    free_type(int_type);
}

// Test symtab_add_string unique IDs
TEST_F(SymtabTest, AddStringUniqueIDs)
{
    const char *id1 = symtab_add_string("str1");
    const char *id2 = symtab_add_string("str2");

    ASSERT_STRNE(id1, id2);
    ASSERT_TRUE(symtab_get_opt(id1) != nullptr);
    ASSERT_TRUE(symtab_get_opt(id2) != nullptr);
}
