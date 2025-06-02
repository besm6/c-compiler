#include <gtest/gtest.h>

#include "ast.h"
#include "resolve.h"

// Fixture for resolve_statement tests
class ResolveStatementTest : public ::testing::Test {
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

    // Helper to create a DeclSpec
    DeclSpec *make_decl_spec(StorageClass storage)
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

    // Helper to create a DeclOrStmt (DECL)
    DeclOrStmt *make_decl_or_stmt_decl(Declaration *decl)
    {
        DeclOrStmt *ds = (DeclOrStmt *)malloc(sizeof(DeclOrStmt));
        ds->kind       = DECL_OR_STMT_DECL;
        ds->u.decl     = decl;
        ds->next       = NULL;
        return ds;
    }

    // Helper to create a DeclOrStmt (STMT)
    DeclOrStmt *make_decl_or_stmt_stmt(Stmt *stmt)
    {
        DeclOrStmt *ds = (DeclOrStmt *)malloc(sizeof(DeclOrStmt));
        ds->kind       = DECL_OR_STMT_STMT;
        ds->u.stmt     = stmt;
        ds->next       = NULL;
        return ds;
    }

    // Helper to create a STMT_EXPR
    Stmt *make_expr_stmt(Expr *expr)
    {
        Stmt *s   = (Stmt *)malloc(sizeof(Stmt));
        s->kind   = STMT_EXPR;
        s->u.expr = expr;
        return s;
    }

    // Helper to create a STMT_COMPOUND
    Stmt *make_compound_stmt(DeclOrStmt *items)
    {
        Stmt *s       = (Stmt *)malloc(sizeof(Stmt));
        s->kind       = STMT_COMPOUND;
        s->u.compound = items;
        return s;
    }

    // Helper to create a STMT_IF
    Stmt *make_if_stmt(Expr *condition, Stmt *then_stmt, Stmt *else_stmt = NULL)
    {
        Stmt *s                = (Stmt *)malloc(sizeof(Stmt));
        s->kind                = STMT_IF;
        s->u.if_stmt.condition = condition;
        s->u.if_stmt.then_stmt = then_stmt;
        s->u.if_stmt.else_stmt = else_stmt;
        return s;
    }

    // Helper to create a STMT_SWITCH
    Stmt *make_switch_stmt(Expr *expr, Stmt *body)
    {
        Stmt *s               = (Stmt *)malloc(sizeof(Stmt));
        s->kind               = STMT_SWITCH;
        s->u.switch_stmt.expr = expr;
        s->u.switch_stmt.body = body;
        return s;
    }

    // Helper to create a STMT_WHILE
    Stmt *make_while_stmt(Expr *condition, Stmt *body)
    {
        Stmt *s                   = (Stmt *)malloc(sizeof(Stmt));
        s->kind                   = STMT_WHILE;
        s->u.while_stmt.condition = condition;
        s->u.while_stmt.body      = body;
        return s;
    }

    // Helper to create a STMT_DO_WHILE
    Stmt *make_do_while_stmt(Stmt *body, Expr *condition)
    {
        Stmt *s                 = (Stmt *)malloc(sizeof(Stmt));
        s->kind                 = STMT_DO_WHILE;
        s->u.do_while.body      = body;
        s->u.do_while.condition = condition;
        return s;
    }

    // Helper to create a ForInit (DECL)
    ForInit *make_for_init_decl(Declaration *decl)
    {
        ForInit *init = (ForInit *)malloc(sizeof(ForInit));
        init->kind    = FOR_INIT_DECL;
        init->u.decl  = decl;
        return init;
    }

    // Helper to create a STMT_FOR
    Stmt *make_for_stmt(ForInit *init, Expr *condition, Expr *update, Stmt *body)
    {
        Stmt *s                 = (Stmt *)malloc(sizeof(Stmt));
        s->kind                 = STMT_FOR;
        s->u.for_stmt.init      = init;
        s->u.for_stmt.condition = condition;
        s->u.for_stmt.update    = update;
        s->u.for_stmt.body      = body;
        return s;
    }

    // Helper to create a STMT_RETURN
    Stmt *make_return_stmt(Expr *expr)
    {
        Stmt *s   = (Stmt *)malloc(sizeof(Stmt));
        s->kind   = STMT_RETURN;
        s->u.expr = expr;
        return s;
    }

    // Helper to create a STMT_LABELED
    Stmt *make_labeled_stmt(const char *label, Stmt *stmt)
    {
        Stmt *s            = (Stmt *)malloc(sizeof(Stmt));
        s->kind            = STMT_LABELED;
        s->u.labeled.label = strdup(label);
        s->u.labeled.stmt  = stmt;
        return s;
    }

    // Helper to create a STMT_CASE
    Stmt *make_case_stmt(Expr *expr, Stmt *stmt)
    {
        Stmt *s             = (Stmt *)malloc(sizeof(Stmt));
        s->kind             = STMT_CASE;
        s->u.case_stmt.expr = expr;
        s->u.case_stmt.stmt = stmt;
        return s;
    }

    // Helper to create a STMT_DEFAULT
    Stmt *make_default_stmt(Stmt *stmt)
    {
        Stmt *s           = (Stmt *)malloc(sizeof(Stmt));
        s->kind           = STMT_DEFAULT;
        s->u.default_stmt = stmt;
        return s;
    }

    // Helper to create a STMT_GOTO
    Stmt *make_goto_stmt(const char *label)
    {
        Stmt *s         = (Stmt *)malloc(sizeof(Stmt));
        s->kind         = STMT_GOTO;
        s->u.goto_label = strdup(label);
        return s;
    }

    // Helper to create a STMT_CONTINUE
    Stmt *make_continue_stmt()
    {
        Stmt *s = (Stmt *)malloc(sizeof(Stmt));
        s->kind = STMT_CONTINUE;
        return s;
    }

    // Helper to create a STMT_BREAK
    Stmt *make_break_stmt()
    {
        Stmt *s = (Stmt *)malloc(sizeof(Stmt));
        s->kind = STMT_BREAK;
        return s;
    }

    // Helper to free a Statement
    void free_statement(Stmt *s)
    {
        if (!s)
            return;
        switch (s->kind) {
        case STMT_EXPR:
        case STMT_RETURN:
            free_expr(s->u.expr);
            break;
        case STMT_COMPOUND:
            for (DeclOrStmt *ds = s->u.compound; ds;) {
                DeclOrStmt *next = ds->next;
                if (ds->kind == DECL_OR_STMT_STMT) {
                    free_statement(ds->u.stmt);
                } else {
                    free_declaration(ds->u.decl);
                }
                free(ds);
                ds = next;
            }
            break;
        case STMT_IF:
            free_expr(s->u.if_stmt.condition);
            free_statement(s->u.if_stmt.then_stmt);
            free_statement(s->u.if_stmt.else_stmt);
            break;
        case STMT_SWITCH:
            free_expr(s->u.switch_stmt.expr);
            free_statement(s->u.switch_stmt.body);
            break;
        case STMT_WHILE:
            free_expr(s->u.while_stmt.condition);
            free_statement(s->u.while_stmt.body);
            break;
        case STMT_DO_WHILE:
            free_statement(s->u.do_while.body);
            free_expr(s->u.do_while.condition);
            break;
        case STMT_FOR:
            free_for_init(s->u.for_stmt.init);
            free_expr(s->u.for_stmt.condition);
            free_expr(s->u.for_stmt.update);
            free_statement(s->u.for_stmt.body);
            break;
        case STMT_GOTO:
            free(s->u.goto_label);
            break;
        case STMT_LABELED:
            free(s->u.labeled.label);
            free_statement(s->u.labeled.stmt);
            break;
        case STMT_CASE:
            free_expr(s->u.case_stmt.expr);
            free_statement(s->u.case_stmt.stmt);
            break;
        case STMT_DEFAULT:
            free_statement(s->u.default_stmt);
            break;
        case STMT_CONTINUE:
        case STMT_BREAK:
            break;
        }
        free(s);
    }

    // Helper to free a Declaration
    void free_declaration(Declaration *decl)
    {
        if (!decl)
            return;
        if (decl->kind == DECL_VAR) {
            free_decl_spec(decl->u.var.specifiers);
            for (InitDeclarator *id = decl->u.var.declarators; id;) {
                InitDeclarator *next = id->next;
                free(id->name);
                free_type(id->type);
                free_initializer(id->init);
                free(id);
                id = next;
            }
        }
        free(decl);
    }

    // Helper to free a DeclSpec
    void free_decl_spec(DeclSpec *spec)
    {
        if (!spec)
            return;
        free(spec);
    }

    // Helper to free a ForInit
    void free_for_init(ForInit *init)
    {
        if (!init)
            return;
        if (init->kind == FOR_INIT_DECL) {
            free_declaration(init->u.decl);
        } else {
            free_expr(init->u.expr);
        }
        free(init);
    }

    // Helper to free an Expr
    void free_expr(Expr *e)
    {
        if (!e)
            return;
        switch (e->kind) {
        case EXPR_VAR:
            free(e->u.var);
            break;
        case EXPR_LITERAL:
            free(e->u.literal);
            break;
        default:
            break;
        }
        free(e);
    }

    // Helper to free a Type
    void free_type(Type *t)
    {
        if (!t)
            return;
        if (t->kind == TYPE_INT) {
            // No dynamic fields
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
};

TEST_F(ResolveStatementTest, ResolveStmtExpr)
{
    Expr *expr = make_var_expr("my_var");
    Stmt *s    = make_expr_stmt(expr);
    resolve_statement(s);
    EXPECT_STREQ(s->u.expr->u.var, "var_123");
    free_statement(s);
}

TEST_F(ResolveStatementTest, ResolveStmtCompound_VarDecl)
{
    Type *type         = make_int_type();
    InitDeclarator *id = make_init_declarator("x", type);
    DeclSpec *spec     = make_decl_spec(STORAGE_CLASS_NONE);
    Declaration *decl  = make_var_declaration(spec, id);
    DeclOrStmt *ds     = make_decl_or_stmt_decl(decl);
    Stmt *s            = make_compound_stmt(ds);
    resolve_statement(s);
    EXPECT_TRUE(strstr(s->u.compound->u.decl->u.var.declarators->name, "x_") != NULL);
    free_statement(s);
}

TEST_F(ResolveStatementTest, ResolveStmtCompound_DuplicateDecl)
{
    Type *type          = make_int_type();
    InitDeclarator *id1 = make_init_declarator("x", type);
    DeclSpec *spec1     = make_decl_spec(STORAGE_CLASS_NONE);
    Declaration *decl1  = make_var_declaration(spec1, id1);
    DeclOrStmt *ds1     = make_decl_or_stmt_decl(decl1);
    Type *type2         = make_int_type();
    InitDeclarator *id2 = make_init_declarator("x", type2);
    DeclSpec *spec2     = make_decl_spec(STORAGE_CLASS_NONE);
    Declaration *decl2  = make_var_declaration(spec2, id2);
    DeclOrStmt *ds2     = make_decl_or_stmt_decl(decl2);
    ds1->next           = ds2;
    Stmt *s             = make_compound_stmt(ds1);
    EXPECT_EXIT(
        {
            resolve_statement(s);
            exit(0);
        },
        ::testing::ExitedWithCode(1), "Duplicate variable declaration x");
    free_statement(s);
}

TEST_F(ResolveStatementTest, ResolveStmtIf)
{
    Expr *condition = make_var_expr("my_var");
    Stmt *then_stmt = make_expr_stmt(make_var_expr("my_var"));
    Stmt *else_stmt = make_expr_stmt(make_var_expr("my_var"));
    Stmt *s         = make_if_stmt(condition, then_stmt, else_stmt);
    resolve_statement(s);
    EXPECT_STREQ(s->u.if_stmt.condition->u.var, "var_123");
    EXPECT_STREQ(s->u.if_stmt.then_stmt->u.expr->u.var, "var_123");
    EXPECT_STREQ(s->u.if_stmt.else_stmt->u.expr->u.var, "var_123");
    free_statement(s);
}

TEST_F(ResolveStatementTest, ResolveStmtSwitch)
{
    Expr *expr = make_var_expr("my_var");
    Stmt *body = make_expr_stmt(make_var_expr("my_var"));
    Stmt *s    = make_switch_stmt(expr, body);
    resolve_statement(s);
    EXPECT_STREQ(s->u.switch_stmt.expr->u.var, "var_123");
    EXPECT_STREQ(s->u.switch_stmt.body->u.expr->u.var, "var_123");
    free_statement(s);
}

TEST_F(ResolveStatementTest, ResolveStmtWhile)
{
    Expr *condition = make_var_expr("my_var");
    Stmt *body      = make_expr_stmt(make_var_expr("my_var"));
    Stmt *s         = make_while_stmt(condition, body);
    resolve_statement(s);
    EXPECT_STREQ(s->u.while_stmt.condition->u.var, "var_123");
    EXPECT_STREQ(s->u.while_stmt.body->u.expr->u.var, "var_123");
    free_statement(s);
}

TEST_F(ResolveStatementTest, ResolveStmtDoWhile)
{
    Stmt *body      = make_expr_stmt(make_var_expr("my_var"));
    Expr *condition = make_var_expr("my_var");
    Stmt *s         = make_do_while_stmt(body, condition);
    resolve_statement(s);
    EXPECT_STREQ(s->u.do_while.body->u.expr->u.var, "var_123");
    EXPECT_STREQ(s->u.do_while.condition->u.var, "var_123");
    free_statement(s);
}

TEST_F(ResolveStatementTest, ResolveStmtFor_DeclInit)
{
    Type *type         = make_int_type();
    InitDeclarator *id = make_init_declarator("x", type);
    DeclSpec *spec     = make_decl_spec(STORAGE_CLASS_NONE);
    Declaration *decl  = make_var_declaration(spec, id);
    ForInit *init      = make_for_init_decl(decl);
    Expr *condition    = make_var_expr("my_var");
    Expr *update       = make_var_expr("my_var");
    Stmt *body         = make_expr_stmt(make_var_expr("my_var"));
    Stmt *s            = make_for_stmt(init, condition, update, body);
    resolve_statement(s);
    EXPECT_TRUE(strstr(s->u.for_stmt.init->u.decl->u.var.declarators->name, "x_") != NULL);
    EXPECT_STREQ(s->u.for_stmt.condition->u.var, "var_123");
    EXPECT_STREQ(s->u.for_stmt.update->u.var, "var_123");
    EXPECT_STREQ(s->u.for_stmt.body->u.expr->u.var, "var_123");
    free_statement(s);
}

TEST_F(ResolveStatementTest, ResolveStmtReturn)
{
    Expr *expr = make_var_expr("my_var");
    Stmt *s    = make_return_stmt(expr);
    resolve_statement(s);
    EXPECT_STREQ(s->u.expr->u.var, "var_123");
    free_statement(s);
}

TEST_F(ResolveStatementTest, ResolveStmtLabeled)
{
    Stmt *inner = make_expr_stmt(make_var_expr("my_var"));
    Stmt *s     = make_labeled_stmt("label", inner);
    resolve_statement(s);
    EXPECT_STREQ(s->u.labeled.stmt->u.expr->u.var, "var_123");
    free_statement(s);
}

TEST_F(ResolveStatementTest, ResolveStmtCase)
{
    Expr *expr  = make_var_expr("my_var");
    Stmt *inner = make_expr_stmt(make_var_expr("my_var"));
    Stmt *s     = make_case_stmt(expr, inner);
    resolve_statement(s);
    EXPECT_STREQ(s->u.case_stmt.expr->u.var, "var_123");
    EXPECT_STREQ(s->u.case_stmt.stmt->u.expr->u.var, "var_123");
    free_statement(s);
}

TEST_F(ResolveStatementTest, ResolveStmtDefault)
{
    Stmt *inner = make_expr_stmt(make_var_expr("my_var"));
    Stmt *s     = make_default_stmt(inner);
    resolve_statement(s);
    EXPECT_STREQ(s->u.default_stmt->u.expr->u.var, "var_123");
    free_statement(s);
}

TEST_F(ResolveStatementTest, ResolveStmtGoto)
{
    Stmt *s    = make_goto_stmt("label");
    Stmt *orig = (Stmt *)malloc(sizeof(Stmt));
    *orig      = *s;
    resolve_statement(s);
    EXPECT_STREQ(s->u.goto_label, orig->u.goto_label);
    free_statement(s);
    free(orig);
}

TEST_F(ResolveStatementTest, ResolveStmtContinue)
{
    Stmt *s = make_continue_stmt();
    resolve_statement(s);
    EXPECT_EQ(s->kind, STMT_CONTINUE);
    free_statement(s);
}

TEST_F(ResolveStatementTest, ResolveStmtBreak)
{
    Stmt *s = make_break_stmt();
    resolve_statement(s);
    EXPECT_EQ(s->kind, STMT_BREAK);
    free_statement(s);
}

TEST_F(ResolveStatementTest, ResolveNullStmt)
{
    Stmt *s = NULL;
    resolve_statement(s);
    EXPECT_EQ(s, nullptr);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    testing::GTEST_FLAG(death_test_style) = "threadsafe";
    return RUN_ALL_TESTS();
}
