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

// Fixture for resolve_global_declaration tests
class ResolveGlobalDeclarationTest : public ::testing::Test {
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

    // Helper to create an INITIALIZER_SINGLE
    Initializer *make_single_initializer(Expr *expr)
    {
        Initializer *init = (Initializer *)malloc(sizeof(Initializer));
        init->kind        = INITIALIZER_SINGLE;
        init->u.expr      = expr;
        return init;
    }

    // Helper to create a DeclSpec
    DeclSpec *make_decl_spec(StorageClass storage = STORAGE_CLASS_NONE)
    {
        DeclSpec *spec   = (DeclSpec *)malloc(sizeof(DeclSpec));
        spec->qualifiers = NULL;
        spec->storage    = storage;
        spec->func_specs = NULL;
        spec->align_spec = NULL;
        return spec;
    }

    // Helper to create an InitDeclarator
    InitDeclarator *make_init_declarator(const char *name, Type *type, Initializer *init = NULL)
    {
        InitDeclarator *id = (InitDeclarator *)malloc(sizeof(InitDeclarator));
        id->name           = strdup(name);
        id->type           = type;
        id->init           = init;
        id->next           = NULL;
        return id;
    }

    // Helper to create a Declaration (DECL_VAR)
    Declaration *make_var_declaration(DeclSpec *spec, InitDeclarator *declarators)
    {
        Declaration *decl       = (Declaration *)malloc(sizeof(Declaration));
        decl->kind              = DECL_VAR;
        decl->u.var.specifiers  = spec;
        decl->u.var.declarators = declarators;
        decl->next              = NULL;
        return decl;
    }

    // Helper to create a Declaration (DECL_EMPTY for struct)
    Declaration *make_struct_declaration(const char *tag, Field *fields)
    {
        Declaration *d                     = (Declaration *)malloc(sizeof(Declaration));
        d->kind                            = DECL_EMPTY;
        d->u.empty.specifiers              = make_decl_spec();
        d->u.empty.type                    = make_struct_type(tag);
        d->u.empty.type->u.struct_t.fields = fields;
        d->next                            = NULL;
        return d;
    }

    // Helper to create a Field
    Field *make_field(const char *name, Type *type)
    {
        Field *f    = (Field *)malloc(sizeof(Field));
        f->name     = name ? strdup(name) : NULL;
        f->type     = type;
        f->bitfield = NULL;
        f->next     = NULL;
        return f;
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
        fd->u.function.specifiers  = make_decl_spec();
        fd->u.function.param_decls = NULL;
        fd->u.function.body        = body;
        fd->next                   = NULL;
        if (type->kind == TYPE_FUNCTION) {
            type->u.function.params = params;
        }
        return fd;
    }

    // Helper to create an ExternalDecl (DECLARATION)
    ExternalDecl *make_declaration(Declaration *decl)
    {
        ExternalDecl *ed  = (ExternalDecl *)malloc(sizeof(ExternalDecl));
        ed->kind          = EXTERNAL_DECL_DECLARATION;
        ed->u.declaration = decl;
        ed->next          = NULL;
        return ed;
    }

    // Helper to free an ExternalDecl
    void free_external_decl(ExternalDecl *ed)
    {
        if (!ed)
            return;
        if (ed->kind == EXTERNAL_DECL_FUNCTION) {
            free(ed->u.function.name);
            free_type(ed->u.function.type);
            free_decl_spec(ed->u.function.specifiers);
            free_statement(ed->u.function.body);
        } else {
            free_declaration(ed->u.declaration);
        }
        free(ed);
    }

    // Helper to free a Declaration
    void free_declaration(Declaration *d)
    {
        if (!d)
            return;
        if (d->kind == DECL_VAR) {
            free_decl_spec(d->u.var.specifiers);
            for (InitDeclarator *id = d->u.var.declarators; id;) {
                InitDeclarator *next = id->next;
                free(id->name);
                free_type(id->type);
                free_initializer(id->init);
                free(id);
                id = next;
            }
        } else if (d->kind == DECL_EMPTY) {
            free_decl_spec(d->u.empty.specifiers);
            free_type(d->u.empty.type);
        }
        free(d);
    }

    // Helper to free a DeclSpec
    void free_decl_spec(DeclSpec *spec)
    {
        if (!spec)
            return;
        free(spec);
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
            for (Field *f = t->u.struct_t.fields; f;) {
                Field *next = f->next;
                free(f->name);
                free_type(f->type);
                free(f);
                f = next;
            }
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

    // Helper to free an Initializer
    void free_initializer(Initializer *init)
    {
        if (!init)
            return;
        if (init->kind == INITIALIZER_SINGLE) {
            free_expr(init->u.expr);
        }
        free(init);
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

    // Helper to free a Statement
    void free_statement(Stmt *s)
    {
        if (!s)
            return;
        if (s->kind == STMT_EXPR) {
            free_expr(s->u.expr);
        }
        free(s);
    }
};

TEST_F(ResolveGlobalDeclarationTest, ResolveGlobalDecl_FunctionPrototype)
{
    Type *return_type = make_int_type();
    Type *func_type   = make_function_type(return_type, NULL);
    ExternalDecl *ed  = make_function_decl("my_func", func_type, NULL, NULL);
    resolve_global_declaration(ed);
    VarEntry *entry = (VarEntry *)hash_table_find(symbol_table, "my_func");
    EXPECT_STREQ(entry->unique_name, "my_func");
    EXPECT_EQ(entry->from_current_scope, 1);
    EXPECT_EQ(entry->has_linkage, 1);
    free_external_decl(ed);
}

TEST_F(ResolveGlobalDeclarationTest, ResolveGlobalDecl_FunctionWithBody)
{
    Type *return_type = make_int_type();
    Param *param      = make_param("p", make_int_type());
    Type *func_type   = make_function_type(return_type, param);
    Stmt *body        = make_expr_stmt(make_var_expr("my_var"));
    ExternalDecl *ed  = make_function_decl("my_func", func_type, param, body);
    resolve_global_declaration(ed);
    VarEntry *entry = (VarEntry *)hash_table_find(symbol_table, "my_func");
    EXPECT_STREQ(entry->unique_name, "my_func");
    EXPECT_STREQ(ed->u.function.type->u.function.params->name, "p_123");
    EXPECT_STREQ(ed->u.function.body->u.expr->u.var, "var_123");
    free_external_decl(ed);
}

TEST_F(ResolveGlobalDeclarationTest, ResolveGlobalDecl_Variable)
{
    Type *type         = make_int_type();
    InitDeclarator *id = make_init_declarator("x", type);
    DeclSpec *spec     = make_decl_spec();
    Declaration *decl  = make_var_declaration(spec, id);
    ExternalDecl *ed   = make_declaration(decl);
    resolve_global_declaration(ed);
    VarEntry *entry = (VarEntry *)hash_table_find(symbol_table, "x");
    EXPECT_STREQ(entry->unique_name, "x");
    EXPECT_EQ(entry->from_current_scope, 1);
    EXPECT_EQ(entry->has_linkage, 1);
    EXPECT_STREQ(ed->u.declaration->u.var.declarators->name, "x");
    free_external_decl(ed);
}

TEST_F(ResolveGlobalDeclarationTest, ResolveGlobalDecl_VariableWithInitializer)
{
    Expr *expr         = make_var_expr("my_var");
    Initializer *init  = make_single_initializer(expr);
    Type *type         = make_int_type();
    InitDeclarator *id = make_init_declarator("x", type, init);
    DeclSpec *spec     = make_decl_spec();
    Declaration *decl  = make_var_declaration(spec, id);
    ExternalDecl *ed   = make_declaration(decl);
    resolve_global_declaration(ed);
    VarEntry *entry = (VarEntry *)hash_table_find(symbol_table, "x");
    EXPECT_STREQ(entry->unique_name, "x");
    EXPECT_STREQ(ed->u.declaration->u.var.declarators->init->u.expr->u.var, "var_123");
    free_external_decl(ed);
}

TEST_F(ResolveGlobalDeclarationTest, ResolveGlobalDecl_Structure)
{
    Field *field      = make_field("f", make_int_type());
    Declaration *decl = make_struct_declaration("my_struct", field);
    ExternalDecl *ed  = make_declaration(decl);
    resolve_global_declaration(ed);
    StructEntry *entry = (StructEntry *)hash_table_find(type_table, "my_struct");
    EXPECT_STREQ(entry->unique_tag, "my_struct_123");
    EXPECT_STREQ(ed->u.declaration->u.empty.type->u.struct_t.name, "my_struct_123");
    free_external_decl(ed);
}

TEST_F(ResolveGlobalDeclarationTest, ResolveGlobalDecl_DuplicateVariable)
{
    // Add existing variable
    VarEntry *existing           = (VarEntry *)malloc(sizeof(VarEntry));
    existing->unique_name        = strdup("x");
    existing->from_current_scope = 1;
    existing->has_linkage        = 1;
    hash_table_insert(symbol_table, "x", existing);
    Type *type         = make_int_type();
    InitDeclarator *id = make_init_declarator("x", type);
    DeclSpec *spec     = make_decl_spec();
    Declaration *decl  = make_var_declaration(spec, id);
    ExternalDecl *ed   = make_declaration(decl);
    resolve_global_declaration(ed);
    // No error expected, as resolve_global_declaration allows redeclarations with linkage
    VarEntry *entry = (VarEntry *)hash_table_find(symbol_table, "x");
    EXPECT_STREQ(entry->unique_name, "x");
    free_external_decl(ed);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    testing::GTEST_FLAG(death_test_style) = "threadsafe";
    return RUN_ALL_TESTS();
}
