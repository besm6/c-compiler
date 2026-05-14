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

// Fixture for resolve_structure_declaration tests
class ResolveStructureDeclarationTest : public ::testing::Test {
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
        hash_table_insert(type_table, "known_struct", struct_entry);
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

    // Helper to create a TYPE_POINTER
    Type *make_pointer_type(Type *target)
    {
        Type *t                 = (Type *)malloc(sizeof(Type));
        t->kind                 = TYPE_POINTER;
        t->u.pointer.target     = target;
        t->u.pointer.qualifiers = NULL;
        t->qualifiers           = NULL;
        return t;
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

    // Helper to create a DeclSpec
    DeclSpec *make_decl_spec()
    {
        DeclSpec *spec   = (DeclSpec *)malloc(sizeof(DeclSpec));
        spec->qualifiers = NULL;
        spec->storage    = STORAGE_CLASS_NONE;
        spec->func_specs = NULL;
        spec->align_spec = NULL;
        return spec;
    }

    // Helper to create a Declaration (DECL_VAR for struct)
    Declaration *make_struct_declaration(const char *tag, Field *fields)
    {
        Declaration *d                               = (Declaration *)malloc(sizeof(Declaration));
        d->kind                                      = DECL_VAR;
        d->u.var.specifiers                          = make_decl_spec();
        d->u.var.declarators                         = NULL; // No declarators for struct decl
        d->u.var.specifiers->type                    = make_struct_type(tag);
        d->u.var.specifiers->type->u.struct_t.fields = fields;
        d->next                                      = NULL;
        return d;
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
                free(id);
                id = next;
            }
        }
        free(d);
    }

    // Helper to free a DeclSpec
    void free_decl_spec(DeclSpec *spec)
    {
        if (!spec)
            return;
        free_type(spec->type);
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
        case TYPE_POINTER:
            free_type(t->u.pointer.target);
            break;
        default:
            break;
        }
        free(t);
    }
};

TEST_F(ResolveStructureDeclarationTest, ResolveStructDecl_NewTag)
{
    Declaration *d = make_struct_declaration("my_struct", NULL);
    resolve_structure_declaration(d);
    StructEntry *entry = (StructEntry *)hash_table_find(type_table, "my_struct");
    EXPECT_STREQ(d->u.var.specifiers->type->u.struct_t.name, "my_struct_123");
    EXPECT_STREQ(entry->unique_tag, "my_struct_123");
    EXPECT_EQ(entry->struct_from_current_scope, 1);
    free_declaration(d);
}

TEST_F(ResolveStructureDeclarationTest, ResolveStructDecl_ExistingTag)
{
    // Add existing tag
    StructEntry *existing               = (StructEntry *)malloc(sizeof(StructEntry));
    existing->unique_tag                = strdup("struct_existing");
    existing->struct_from_current_scope = 1;
    hash_table_insert(type_table, "my_struct", existing);
    Declaration *d = make_struct_declaration("my_struct", NULL);
    resolve_structure_declaration(d);
    EXPECT_STREQ(d->u.var.specifiers->type->u.struct_t.name, "struct_existing");
    free_declaration(d);
}

TEST_F(ResolveStructureDeclarationTest, ResolveStructDecl_KnownMemberType)
{
    Field *field   = make_field("f", make_struct_type("known_struct"));
    Declaration *d = make_struct_declaration("my_struct", field);
    resolve_structure_declaration(d);
    EXPECT_STREQ(d->u.var.specifiers->type->u.struct_t.name, "my_struct_123");
    EXPECT_STREQ(d->u.var.specifiers->type->u.struct_t.fields->type->u.struct_t.name, "struct_123");
    free_declaration(d);
}

TEST_F(ResolveStructureDeclarationTest, ResolveStructDecl_UnknownMemberType)
{
    Field *field   = make_field("f", make_struct_type("unknown_struct"));
    Declaration *d = make_struct_declaration("my_struct", field);
    EXPECT_EXIT(
        {
            resolve_structure_declaration(d);
            exit(0);
        },
        ::testing::ExitedWithCode(1), "Undeclared structure type unknown_struct");
    free_declaration(d);
}

TEST_F(ResolveStructureDeclarationTest, ResolveStructDecl_MultipleMembers)
{
    Field *field1  = make_field("f1", make_struct_type("known_struct"));
    Field *field2  = make_field("f2", make_int_type());
    field1->next   = field2;
    Declaration *d = make_struct_declaration("my_struct", field1);
    resolve_structure_declaration(d);
    EXPECT_STREQ(d->u.var.specifiers->type->u.struct_t.name, "my_struct_123");
    EXPECT_STREQ(d->u.var.specifiers->type->u.struct_t.fields->type->u.struct_t.name, "struct_123");
    EXPECT_EQ(d->u.var.specifiers->type->u.struct_t.fields->next->type->kind, TYPE_INT);
    free_declaration(d);
}

TEST_F(ResolveStructureDeclarationTest, ResolveStructDecl_SelfReferential)
{
    Declaration *d                               = make_struct_declaration("my_struct", NULL);
    Type *self_type                              = make_struct_type("my_struct");
    Field *field                                 = make_field("self", make_pointer_type(self_type));
    d->u.var.specifiers->type->u.struct_t.fields = field;
    resolve_structure_declaration(d);
    EXPECT_STREQ(d->u.var.specifiers->type->u.struct_t.name, "my_struct_123");
    EXPECT_STREQ(
        d->u.var.specifiers->type->u.struct_t.fields->type->u.pointer.target->u.struct_t.name,
        "my_struct_123");
    free_declaration(d);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    testing::GTEST_FLAG(death_test_style) = "threadsafe";
    return RUN_ALL_TESTS();
}
