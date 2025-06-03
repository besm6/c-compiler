#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "symtab.h"

// Helper functions for type creation (assumed available)
extern Type *new_int_type();
extern Type *new_char_type();
extern Type *new_array_type(Type *element, size_t size);
extern Type *new_pointer_type(Type *target);
extern Type *new_function_type(Type *return_type, Param *params);
extern bool compare_type(const Type *a, const Type *b);
extern void free_type(Type *t);

// Helper to create a parameter list
Param *create_param(const char *name, Type *type)
{
    Param *p      = (Param *)malloc(sizeof(Param));
    p->name       = strdup(name);
    p->type       = type;
    p->specifiers = NULL;
    p->next       = NULL;
    return p;
}

// Helper to free a parameter list
void free_param(Param *p)
{
    while (p) {
        Param *next = p->next;
        free(p->name);
        // Type freed elsewhere
        free(p);
        p = next;
    }
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
    void SetUp() override { symtab_init(); }
    void TearDown() override { symtab_destroy(); }
};

// Test symtab_init
TEST_F(SymtabTest, InitEmptyTable)
{
    ASSERT_EQ(symtab_size(), 0u);
    ASSERT_EQ(symtab_get_opt("x"), nullptr);
}

// Test symtab_destroy
TEST_F(SymtabTest, DestroyEmptyTable)
{
    symtab_destroy();
    symtab_init(); // Re-init to ensure no corruption
    ASSERT_EQ(symtab_size(), 0u);
}

// Test symtab_add_automatic_var
TEST_F(SymtabTest, AddAutomaticVar)
{
    Type *int_type = new_int_type();
    symtab_add_automatic_var("x", int_type);

    Symbol *sym = symtab_get("x");
    ASSERT_STREQ(sym->name, "x");
    ASSERT_TRUE(compare_type(sym->type, int_type));
    ASSERT_EQ(sym->kind, SYM_LOCAL);

    ASSERT_EQ(symtab_size(), 1u);

    free_type(int_type);
}

// Test symtab_add_automatic_var overwrite
TEST_F(SymtabTest, AddAutomaticVarOverwrite)
{
    Type *int_type  = new_int_type();
    Type *char_type = new_char_type();

    symtab_add_automatic_var("x", int_type);
    symtab_add_automatic_var("x", char_type);

    Symbol *sym = symtab_get("x");
    ASSERT_STREQ(sym->name, "x");
    ASSERT_TRUE(compare_type(sym->type, char_type));
    ASSERT_EQ(sym->kind, SYM_LOCAL);

    ASSERT_EQ(symtab_size(), 1u);

    free_type(int_type);
    free_type(char_type);
}

// Test symtab_add_static_var
TEST_F(SymtabTest, AddStaticVarWithInitializer)
{
    Type *int_type          = new_int_type();
    StaticInitializer *init = (StaticInitializer *)malloc(sizeof(StaticInitializer));
    init->kind              = INIT_INT;
    init->u.int_val         = 42;
    init->next              = NULL;

    symtab_add_static_var("x", int_type, true, INIT_INITIALIZED, init);

    Symbol *sym = symtab_get("x");
    ASSERT_STREQ(sym->name, "x");
    ASSERT_TRUE(compare_type(sym->type, int_type));
    ASSERT_EQ(sym->kind, SYM_STATIC);
    ASSERT_TRUE(sym->u.static_var.global);
    ASSERT_EQ(sym->u.static_var.init_kind, INIT_INITIALIZED);
    ASSERT_TRUE(compare_static_initializer(sym->u.static_var.init_list, init));

    ASSERT_EQ(symtab_size(), 1u);

    free_type(int_type);
    free(init); // Assumes symtab makes a copy
}

// Test symtab_add_static_var with no initializer
TEST_F(SymtabTest, AddStaticVarNoInitializer)
{
    Type *int_type = new_int_type();

    symtab_add_static_var("y", int_type, false, INIT_TENTATIVE, NULL);

    Symbol *sym = symtab_get("y");
    ASSERT_STREQ(sym->name, "y");
    ASSERT_TRUE(compare_type(sym->type, int_type));
    ASSERT_EQ(sym->kind, SYM_STATIC);
    ASSERT_FALSE(sym->u.static_var.global);
    ASSERT_EQ(sym->u.static_var.init_kind, INIT_TENTATIVE);
    ASSERT_EQ(sym->u.static_var.init_list, nullptr);

    ASSERT_EQ(symtab_size(), 1u);

    free_type(int_type);
}

// Test symtab_add_fun
TEST_F(SymtabTest, AddFunction)
{
    Type *int_type = new_int_type();
    Param *param   = create_param("a", int_type);
    Type *fun_type = new_function_type(int_type, param);

    symtab_add_fun("f", fun_type, true, true);

    Symbol *sym = symtab_get("f");
    ASSERT_STREQ(sym->name, "f");
    ASSERT_TRUE(compare_type(sym->type, fun_type));
    ASSERT_EQ(sym->kind, SYM_FUNC);
    ASSERT_TRUE(sym->u.func.global);
    ASSERT_TRUE(sym->u.func.defined);

    ASSERT_EQ(symtab_size(), 1u);

    free_param(param);
    free_type(int_type);
    free_type(fun_type);
}

// Test symtab_add_string
TEST_F(SymtabTest, AddStringLiteral)
{
    const char *str = "hello";
    char *str_id    = symtab_add_string((char *)str);

    Symbol *sym = symtab_get(str_id);
    ASSERT_STREQ(sym->name, str_id);
    Type *expected_type = new_array_type(new_char_type(), strlen(str) + 1);
    ASSERT_TRUE(compare_type(sym->type, expected_type));
    ASSERT_EQ(sym->kind, SYM_CONST);
    StaticInitializer expected_init = { .kind         = INIT_STRING,
                                        .u.string_val = { (char *)str, true },
                                        .next         = NULL };
    ASSERT_TRUE(compare_static_initializer(sym->u.const_init, &expected_init));

    ASSERT_EQ(symtab_size(), 1u);

    free_type(expected_type->u.array.element);
    free_type(expected_type);
    // str_id owned by symtab
}

// Test symtab_get
TEST_F(SymtabTest, GetNonExistentSymbol)
{
    ASSERT_EXIT(symtab_get("x"), ::testing::ExitedWithCode(1), "Symbol x not found");
}

// Test symtab_get_opt
TEST_F(SymtabTest, GetOptExistentAndNonExistent)
{
    Type *int_type = new_int_type();
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
    Type *int_type = new_int_type();

    symtab_add_static_var("x", int_type, true, INIT_NONE, NULL);
    symtab_add_static_var("y", int_type, false, INIT_NONE, NULL);
    symtab_add_automatic_var("z", int_type);

    ASSERT_TRUE(symtab_is_global("x"));
    ASSERT_FALSE(symtab_is_global("y"));
    ASSERT_FALSE(symtab_is_global("z"));

    free_type(int_type);
}

// Test symtab_iter
TEST_F(SymtabTest, IterateSymbols)
{
    Type *int_type = new_int_type();

    symtab_add_automatic_var("x", int_type);
    symtab_add_static_var("y", int_type, true, INIT_NONE, NULL);

    std::map<std::string, Symbol> collected;
    SymtabIterator callback = [](char *name, Symbol *symbol, void *user_data) {
        auto *map = static_cast<std::map<std::string, Symbol> *>(user_data);
        map->emplace(name, *symbol);
    };
    symtab_iter(callback, &collected);

    ASSERT_EQ(collected.size(), 2u);
    ASSERT_TRUE(collected.find("x") != collected.end());
    ASSERT_TRUE(collected.find("y") != collected.end());
    ASSERT_EQ(collected["x"].kind, SYM_LOCAL);
    ASSERT_EQ(collected["y"].kind, SYM_STATIC);

    free_type(int_type);
}

// Test symtab_size
TEST_F(SymtabTest, SizeMultipleSymbols)
{
    Type *int_type = new_int_type();

    symtab_add_automatic_var("x", int_type);
    symtab_add_static_var("y", int_type, true, INIT_NONE, NULL);
    symtab_add_fun("f", new_function_type(int_type, NULL), true, false);

    ASSERT_EQ(symtab_size(), 3u);

    free_type(int_type);
    free_type(new_function_type(int_type, NULL));
}

// Test symtab_add_string unique IDs
TEST_F(SymtabTest, AddStringUniqueIDs)
{
    char *id1 = symtab_add_string("str1");
    char *id2 = symtab_add_string("str2");

    ASSERT_STRNE(id1, id2);
    ASSERT_TRUE(symtab_get_opt(id1) != nullptr);
    ASSERT_TRUE(symtab_get_opt(id2) != nullptr);

    ASSERT_EQ(symtab_size(), 2u);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
