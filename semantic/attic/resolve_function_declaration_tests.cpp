#include <gtest/gtest.h>

#include <string>

#include "ast.h"
#include "resolve.h"

// Mock make_named_temporary for predictable unique names
static int unique_id_counter = 0;
char *make_named_temporary(const char *name)
{
    char *result = (char *)malloc(strlen(name) + 10);
    sprintf(result, "%s_%d", name, unique_id_counter++);
    return result;
}

// Fixture for resolve_function_declaration tests
class ResolveFunctionDeclarationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        unique_id_counter = 123; // Reset counter for predictable names
        type_table        = create_hash_table();
        symbol_table      = create_hash_table();
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
    }

    void TearDown() override
    {
        hash_table_free(type_table);
        hash_table_free(symbol_table);
        type_table   = NULL;
        symbol_table = NULL;
    }

    // Helper to create a TYPE_INT
    Type *make_int_type()
    {
        Type *t                 = (Type *)malloc(sizeof(Type));
        t->kind                 = TYPE_INT;
        t->u.integer.signedness = SIGNED_SIGNED;
        t->qualifiers           = NULL;
        return t;
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

    // Helper to create a TYPE_FUNCTION
    Type *make_function_type(Type *return_type, Param *params)
    {
        Type *t                   = (Type *)malloc(sizeof(Type));
        t->kind                   = TYPE_FUNCTION;
        t->u.function.return_type = return_type;
        t->u.function.params      = params;
        t->u.function.variadic    = false;
        t->qualifiers             = NULL;
        return t;
    }

    // Helper to create a Param
    Param *make_param(const char *name, Type *type)
    {
        Param *p      = (Param *)malloc(sizeof(Param));
        p->name       = strdup(name);
        p->type       = type;
        p->specifiers = NULL;
        p->next       = NULL;
        return p;
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

    // Helper to create a STMT_EXPR
    Stmt *make_expr_stmt(Expr *expr)
    {
        Stmt *s   = (Stmt *)malloc(sizeof(Stmt));
        s->kind   = STMT_EXPR;
        s->u.expr = expr;
        return s;
    }

    // Helper to create an ExternalDecl (FUNCTION)
    ExternalDecl *make_function_decl(const char *name, Type *type, Param *params, Stmt *body)
    {
        ExternalDecl *fd           = (ExternalDecl *)malloc(sizeof(ExternalDecl));
        fd->kind                   = EXTERNAL_DECL_FUNCTION;
        fd->u.function.name        = strdup(name);
        fd->u.function.type        = type;
        fd->u.function.specifiers  = NULL;
        fd->u.function.param_decls = NULL;
        fd->u.function.body        = body;
        fd->next                   = NULL;
        // Set params in type for consistency
        if (type->kind == TYPE_FUNCTION) {
            type->u.function.params = params;
        }
        return fd;
    }

    // Helper to free an ExternalDecl
    void free_external_decl(ExternalDecl *fd)
    {
        if (!fd)
            return;
        if (fd->kind == EXTERNAL_DECL_FUNCTION) {
            free(fd->u.function.name);
            free_type(fd->u.function.type);
            free_statement(fd->u.function.body);
        }
        free(fd);
    }

    // Helper to free a Type
    void free_type(Type *t)
    {
        if (!t)
            return;
        switch (t->kind) {
        case TYPE_INT:
            break;
        case TYPE_STRUCT:
            free(t->u.struct_t.name);
            break;
        case TYPE_FUNCTION:
            free_type(t->u.function.return_type);
            for (Param *p = t->u.function.params; p;) {
                Param *next = p->next;
                free(p->name);
                free_type(p->type);
                free(p);
                p = next;
            }
            break;
        default:
            break;
        }
        free(t);
    }

    // Helper to free a Statement
    void free_statement(Stmt *s)
    {
        if (!s)
            return;
        switch (s->kind) {
        case STMT_EXPR:
            free_expr(s->u.expr);
            break;
        default:
            break;
        }
        free(s);
    }

    // Helper to free an Expr
    void free_expr(Expr *e)
    {
        if (!e)
            return;
        if (e->kind == EXPR_VAR) {
            free(e->u.var);
        }
        free(e);
    }
};

TEST_F(ResolveFunctionDeclarationTest, ResolveFunctionDecl_Prototype)
{
    Type *return_type = make_int_type();
    Type *func_type   = make_function_type(return_type, NULL);
    ExternalDecl *fd  = make_function_decl("my_func", func_type, NULL, NULL);
    resolve_function_declaration(fd);
    VarEntry *entry = (VarEntry *)hash_table_find(symbol_table, "my_func");
    EXPECT_STREQ(entry->unique_name, "my_func");
    EXPECT_EQ(entry->from_current_scope, 1);
    EXPECT_EQ(entry->has_linkage, 1);
    free_external_decl(fd);
}

TEST_F(ResolveFunctionDeclarationTest, ResolveFunctionDecl_WithBody)
{
    Type *return_type = make_int_type();
    Param *param      = make_param("p", make_int_type());
    Type *func_type   = make_function_type(return_type, param);
    Stmt *body        = make_expr_stmt(make_var_expr("my_var"));
    ExternalDecl *fd  = make_function_decl("my_func", func_type, param, body);
    resolve_function_declaration(fd);
    VarEntry *entry = (VarEntry *)hash_table_find(symbol_table, "my_func");
    EXPECT_STREQ(entry->unique_name, "my_func");
    EXPECT_STREQ(fd->u.function.type->u.function.params->name, "p_123");
    EXPECT_STREQ(fd->u.function.body->u.expr->u.var, "var_123");
    free_external_decl(fd);
}

TEST_F(ResolveFunctionDeclarationTest, ResolveFunctionDecl_MultipleParams)
{
    Type *return_type = make_int_type();
    Param *param1     = make_param("p1", make_int_type());
    Param *param2     = make_param("p2", make_int_type());
    param1->next      = param2;
    Type *func_type   = make_function_type(return_type, param1);
    ExternalDecl *fd  = make_function_decl("my_func", func_type, param1, NULL);
    resolve_function_declaration(fd);
    EXPECT_STREQ(fd->u.function.type->u.function.params->name, "p1_123");
    EXPECT_STREQ(fd->u.function.type->u.function.params->next->name, "p2_124");
    free_external_decl(fd);
}

TEST_F(ResolveFunctionDeclarationTest, ResolveFunctionDecl_DuplicateDecl)
{
    // Add existing non-linkage function
    VarEntry *existing           = (VarEntry *)malloc(sizeof(VarEntry));
    existing->unique_name        = strdup("my_func");
    existing->from_current_scope = 1;
    existing->has_linkage        = 0;
    hash_table_insert(symbol_table, "my_func", existing);
    Type *return_type = make_int_type();
    Type *func_type   = make_function_type(return_type, NULL);
    ExternalDecl *fd  = make_function_decl("my_func", func_type, NULL, NULL);
    EXPECT_EXIT(
        {
            resolve_function_declaration(fd);
            exit(0);
        },
        ::testing::ExitedWithCode(1), "Duplicate declaration my_func");
    free_external_decl(fd);
}

TEST_F(ResolveFunctionDeclarationTest, ResolveFunctionDecl_StructReturn)
{
    Type *return_type = make_struct_type("my_struct");
    Type *func_type   = make_function_type(return_type, NULL);
    ExternalDecl *fd  = make_function_decl("my_func", func_type, NULL, NULL);
    resolve_function_declaration(fd);
    EXPECT_STREQ(fd->u.function.type->u.function.return_type->u.struct_t.name, "struct_123");
    free_external_decl(fd);
}

TEST_F(ResolveFunctionDeclarationTest, ResolveFunctionDecl_StructParam)
{
    Type *return_type = make_int_type();
    Param *param      = make_param("p", make_struct_type("my_struct"));
    Type *func_type   = make_function_type(return_type, param);
    ExternalDecl *fd  = make_function_decl("my_func", func_type, param, NULL);
    resolve_function_declaration(fd);
    EXPECT_STREQ(fd->u.function.type->u.function.params->type->u.struct_t.name, "struct_123");
    EXPECT_STREQ(fd->u.function.type->u.function.params->name, "p_123");
    free_external_decl(fd);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    testing::GTEST_FLAG(death_test_style) = "threadsafe";
    return RUN_ALL_TESTS();
}
