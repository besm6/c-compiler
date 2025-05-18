#include "fixture.h"

TEST_F(ParserTest, ParseSimpleDeclaration)
{
    Declaration *decl = GetDeclaration("int x;");

    EXPECT_EQ(DECL_VAR, decl->kind);
    ASSERT_NE(nullptr, decl->u.var.specifiers);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    ASSERT_NE(nullptr, decl->u.var.declarators);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
    EXPECT_EQ(nullptr, decl->u.var.declarators->init);
}

TEST_F(ParserTest, ParseInitializedDeclaration)
{
    Declaration *decl = GetDeclaration("int x = 42;");

    EXPECT_EQ(DECL_VAR, decl->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
    ASSERT_NE(nullptr, decl->u.var.declarators->init);
    EXPECT_EQ(INITIALIZER_SINGLE, decl->u.var.declarators->init->kind);
    EXPECT_EQ(EXPR_LITERAL, decl->u.var.declarators->init->u.expr->kind);
    EXPECT_EQ(LITERAL_INT, decl->u.var.declarators->init->u.expr->u.literal->kind);
    EXPECT_EQ(42, decl->u.var.declarators->init->u.expr->u.literal->u.int_val);
}

TEST_F(ParserTest, ParseMultipleDeclarators)
{
    Declaration *decl = GetDeclaration("int x, y;");

    EXPECT_EQ(DECL_VAR, decl->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
    ASSERT_NE(nullptr, decl->u.var.declarators->next);
    EXPECT_STREQ("y", decl->u.var.declarators->next->declarator->name);
    EXPECT_EQ(nullptr, decl->u.var.declarators->next->next);
}

TEST_F(ParserTest, ParseEmptyDeclaration)
{
    Declaration *decl = GetDeclaration("int;");

    EXPECT_EQ(DECL_EMPTY, decl->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_EQ(nullptr, decl->u.var.declarators);
}

TEST_F(ParserTest, ParseStaticAssertDeclaration)
{
    Declaration *decl = GetDeclaration("_Static_assert(1, \"msg\");");

    EXPECT_EQ(DECL_STATIC_ASSERT, decl->kind);
    EXPECT_EQ(EXPR_LITERAL, decl->u.static_assrt.condition->kind);
    EXPECT_EQ(LITERAL_INT, decl->u.static_assrt.condition->u.literal->kind);
    EXPECT_EQ(1, decl->u.static_assrt.condition->u.literal->u.int_val);
    EXPECT_STREQ("\"msg\"", decl->u.static_assrt.message); // TODO: strip quotes
}

TEST_F(ParserTest, ParseFunctionDefinitionNoParams)
{
    ExternalDecl *ext = GetExternalDecl("int f() {}");

    EXPECT_EQ(EXTERNAL_DECL_FUNCTION, ext->kind);
    EXPECT_EQ(TYPE_INT, ext->u.function.specifiers->base_type->kind);
    EXPECT_STREQ("f", ext->u.function.declarator->name);
    ASSERT_NE(nullptr, ext->u.function.declarator->suffixes);
    EXPECT_EQ(SUFFIX_FUNCTION, ext->u.function.declarator->suffixes->kind);
    EXPECT_TRUE(ext->u.function.declarator->suffixes->u.function.params->is_empty);
    EXPECT_EQ(STMT_COMPOUND, ext->u.function.body->kind);
    EXPECT_EQ(nullptr, ext->u.function.body->u.compound);
}

TEST_F(ParserTest, ParseFunctionDefinitionWithParams)
{
    ExternalDecl *ext = GetExternalDecl("int f(int x) { return x; }");

    EXPECT_EQ(EXTERNAL_DECL_FUNCTION, ext->kind);
    EXPECT_EQ(TYPE_INT, ext->u.function.specifiers->base_type->kind);
    EXPECT_STREQ("f", ext->u.function.declarator->name);
    EXPECT_EQ(SUFFIX_FUNCTION, ext->u.function.declarator->suffixes->kind);
    ParamList *params = ext->u.function.declarator->suffixes->u.function.params;
    EXPECT_FALSE(params->is_empty);
    ASSERT_NE(nullptr, params->u.params);
    EXPECT_EQ(TYPE_INT, params->u.params->type->kind);
    EXPECT_STREQ("x", params->u.params->name);
    EXPECT_EQ(STMT_COMPOUND, ext->u.function.body->kind);
    ASSERT_NE(nullptr, ext->u.function.body->u.compound);
    EXPECT_EQ(STMT_RETURN, ext->u.function.body->u.compound->u.stmt->kind);
    EXPECT_STREQ("x", ext->u.function.body->u.compound->u.stmt->u.expr->u.var);
}

TEST_F(ParserTest, ParseDeclarationList)
{
    ExternalDecl *ext = GetExternalDecl("int f() { int x; int y; }");
    EXPECT_EQ(EXTERNAL_DECL_FUNCTION, ext->kind);
    EXPECT_EQ(STMT_COMPOUND, ext->u.function.body->kind);

    DeclOrStmt *items = ext->u.function.body->u.compound;
    ASSERT_NE(nullptr, items);
    EXPECT_EQ(DECL_OR_STMT_DECL, items->kind);
    EXPECT_EQ(DECL_VAR, items->u.decl->kind);
    EXPECT_EQ(TYPE_INT, items->u.decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", items->u.decl->u.var.declarators->declarator->name);
    ASSERT_NE(nullptr, items->next);
    EXPECT_EQ(DECL_OR_STMT_DECL, items->next->kind);
    EXPECT_EQ(DECL_VAR, items->next->u.decl->kind);
    EXPECT_STREQ("y", items->next->u.decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseTypeVoid)
{
    Declaration *decl = GetDeclaration("void x;");

    EXPECT_EQ(TYPE_VOID, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseTypeChar)
{
    Declaration *decl = GetDeclaration("char x;");

    EXPECT_EQ(TYPE_CHAR, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseTypeShort)
{
    Declaration *decl = GetDeclaration("short x;");

    EXPECT_EQ(TYPE_SHORT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseTypeInt)
{
    Declaration *decl = GetDeclaration("int x;");

    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseTypeLong)
{
    Declaration *decl = GetDeclaration("long x;");

    EXPECT_EQ(TYPE_LONG, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseTypeFloat)
{
    Declaration *decl = GetDeclaration("float x;");

    EXPECT_EQ(TYPE_FLOAT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseTypeDouble)
{
    Declaration *decl = GetDeclaration("double x;");

    EXPECT_EQ(TYPE_DOUBLE, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseTypeSigned)
{
    Declaration *decl = GetDeclaration("signed x;");

    EXPECT_EQ(decl->kind, DECL_VAR);
    EXPECT_EQ(decl->next, nullptr);

    ASSERT_NE(nullptr, decl->u.var.specifiers);
    Type *base_type = decl->u.var.specifiers->base_type;
    ASSERT_NE(nullptr, base_type);
    EXPECT_EQ(TYPE_INT, base_type->kind);
    EXPECT_EQ(SIGNED_SIGNED, base_type->u.integer.signedness);

    ASSERT_NE(nullptr, decl->u.var.declarators);
    Declarator *declarator = decl->u.var.declarators->declarator;
    ASSERT_NE(nullptr, declarator);
    EXPECT_STREQ("x", declarator->name);
}

TEST_F(ParserTest, ParseTypeUnsigned)
{
    Declaration *decl = GetDeclaration("unsigned x;");

    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_EQ(SIGNED_UNSIGNED, decl->u.var.specifiers->base_type->u.integer.signedness);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseTypeBool)
{
    Declaration *decl = GetDeclaration("_Bool x;");

    EXPECT_EQ(TYPE_BOOL, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseTypeComplex)
{
    Declaration *decl = GetDeclaration("_Complex x;");

    EXPECT_EQ(TYPE_COMPLEX, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseTypeImaginary)
{
    Declaration *decl = GetDeclaration("_Imaginary x;");

    EXPECT_EQ(TYPE_IMAGINARY, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseTypeStruct)
{
    Declaration *decl = GetDeclaration("struct S { int x; } s;");

    EXPECT_EQ(decl->kind, DECL_VAR);
    EXPECT_EQ(decl->next, nullptr);
    ASSERT_NE(decl->u.var.specifiers, nullptr);

    Type *type = decl->u.var.specifiers->base_type;
    EXPECT_EQ(type->kind, TYPE_STRUCT);
    EXPECT_STREQ(type->u.struct_t.name, "S");

    Field *field = type->u.struct_t.fields;
    ASSERT_NE(field, nullptr);
    EXPECT_EQ(field->next, nullptr);

    ASSERT_NE(field->type, nullptr);
    EXPECT_EQ(field->type->kind, TYPE_INT);
    EXPECT_EQ(field->type->u.integer.signedness, SIGNED_SIGNED);
    EXPECT_STREQ(field->name, "x");

    InitDeclarator *init_decl = decl->u.var.declarators;
    ASSERT_NE(init_decl, nullptr);
    EXPECT_EQ(init_decl->next, nullptr);
    EXPECT_EQ(init_decl->init, nullptr);

    Declarator *declarator = init_decl->declarator;
    ASSERT_NE(declarator, nullptr);
    EXPECT_EQ(declarator->next, nullptr);
    EXPECT_STREQ(declarator->name, "s");
    EXPECT_EQ(declarator->pointers, nullptr);
    EXPECT_EQ(declarator->suffixes, nullptr);
}

TEST_F(ParserTest, ParseTypeAnonymousStruct)
{
    Declaration *decl = GetDeclaration("struct { int x; } s;");

    EXPECT_EQ(decl->kind, DECL_VAR);
    EXPECT_EQ(decl->next, nullptr);
    ASSERT_NE(decl->u.var.specifiers, nullptr);

    Type *type = decl->u.var.specifiers->base_type;
    EXPECT_EQ(type->kind, TYPE_STRUCT);
    EXPECT_EQ(type->u.struct_t.name, nullptr);

    Field *field = type->u.struct_t.fields;
    ASSERT_NE(field, nullptr);
    EXPECT_EQ(field->next, nullptr);

    ASSERT_NE(field->type, nullptr);
    EXPECT_EQ(field->type->kind, TYPE_INT);
    EXPECT_EQ(field->type->u.integer.signedness, SIGNED_SIGNED);
    EXPECT_STREQ(field->name, "x");

    InitDeclarator *init_decl = decl->u.var.declarators;
    ASSERT_NE(init_decl, nullptr);
    EXPECT_EQ(init_decl->next, nullptr);
    EXPECT_EQ(init_decl->init, nullptr);

    Declarator *declarator = init_decl->declarator;
    ASSERT_NE(declarator, nullptr);
    EXPECT_EQ(declarator->next, nullptr);
    EXPECT_STREQ(declarator->name, "s");
    EXPECT_EQ(declarator->pointers, nullptr);
    EXPECT_EQ(declarator->suffixes, nullptr);
}

TEST_F(ParserTest, ParseTypeUnion)
{
    Declaration *decl = GetDeclaration("union U { int x; } u;");

    EXPECT_EQ(decl->kind, DECL_VAR);
    EXPECT_EQ(decl->next, nullptr);
    ASSERT_NE(decl->u.var.specifiers, nullptr);

    Type *type = decl->u.var.specifiers->base_type;
    EXPECT_EQ(type->kind, TYPE_UNION);
    EXPECT_STREQ(type->u.struct_t.name, "U");

    Field *field = type->u.struct_t.fields;
    ASSERT_NE(field, nullptr);
    EXPECT_EQ(field->next, nullptr);

    ASSERT_NE(field->type, nullptr);
    EXPECT_EQ(field->type->kind, TYPE_INT);
    EXPECT_EQ(field->type->u.integer.signedness, SIGNED_SIGNED);
    EXPECT_STREQ(field->name, "x");

    InitDeclarator *init_decl = decl->u.var.declarators;
    ASSERT_NE(init_decl, nullptr);
    EXPECT_EQ(init_decl->next, nullptr);
    EXPECT_EQ(init_decl->init, nullptr);

    Declarator *declarator = init_decl->declarator;
    ASSERT_NE(declarator, nullptr);
    EXPECT_EQ(declarator->next, nullptr);
    EXPECT_STREQ(declarator->name, "u");
    EXPECT_EQ(declarator->pointers, nullptr);
    EXPECT_EQ(declarator->suffixes, nullptr);
}

TEST_F(ParserTest, ParseTypeEnum)
{
    Declaration *decl = GetDeclaration("enum E { A };");

    Type *type = decl->u.var.specifiers->base_type;
    EXPECT_EQ(TYPE_ENUM, type->kind);
    EXPECT_STREQ("E", type->u.enum_t.name);
    ASSERT_NE(nullptr, type->u.enum_t.enumerators);
    EXPECT_STREQ("A", type->u.enum_t.enumerators->name);
    EXPECT_EQ(nullptr, type->u.enum_t.enumerators->value);
}

TEST_F(ParserTest, ParseTypeAnonymousEnum)
{
    Declaration *decl = GetDeclaration("enum { A };");

    Type *type = decl->u.var.specifiers->base_type;
    EXPECT_EQ(TYPE_ENUM, type->kind);
    EXPECT_EQ(nullptr, type->u.enum_t.name);
    ASSERT_NE(nullptr, type->u.enum_t.enumerators);
    EXPECT_STREQ("A", type->u.enum_t.enumerators->name);
}

//TODO: Add typedef MyType to the symbol table.
TEST_F(ParserTest, DISABLED_ParseTypeTypedef)
{
    ExternalDecl *ext = GetExternalDecl("typedef int T; T x;");
    EXPECT_EQ(EXTERNAL_DECL_DECLARATION, ext->kind);

    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(STORAGE_CLASS_TYPEDEF, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("T", decl->u.var.declarators->declarator->name);

    decl = ext->next->u.declaration;
    //TODO: check typedef node
    EXPECT_EQ(nullptr, decl->next);
}

// _Atomic() is not supported in this parser.
TEST_F(ParserTest, DISABLED_ParseTypeAtomic)
{
    Declaration *decl = GetDeclaration("_Atomic(int) x;");
    //TODO: check _Atomic(int) node
    ASSERT_NE(decl, nullptr);
}

TEST_F(ParserTest, ParseTypeQualifierConst)
{
    Declaration *decl = GetDeclaration("const int x;");

    ASSERT_NE(nullptr, decl->u.var.specifiers->qualifiers);
    EXPECT_EQ(TYPE_QUALIFIER_CONST, decl->u.var.specifiers->qualifiers->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseTypeQualifierRestrict)
{
    Declaration *decl = GetDeclaration("restrict int x;");

    ASSERT_NE(nullptr, decl->u.var.specifiers->qualifiers);
    EXPECT_EQ(TYPE_QUALIFIER_RESTRICT, decl->u.var.specifiers->qualifiers->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseTypeQualifierVolatile)
{
    Declaration *decl = GetDeclaration("volatile int x;");

    ASSERT_NE(nullptr, decl->u.var.specifiers->qualifiers);
    EXPECT_EQ(TYPE_QUALIFIER_VOLATILE, decl->u.var.specifiers->qualifiers->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseTypeQualifierAtomic)
{
    Declaration *decl = GetDeclaration("_Atomic int x;");

    ASSERT_NE(nullptr, decl->u.var.specifiers->qualifiers);
    EXPECT_EQ(TYPE_QUALIFIER_ATOMIC, decl->u.var.specifiers->qualifiers->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseStorageClassTypedef)
{
    Declaration *decl = GetDeclaration("typedef int x;");

    ASSERT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_TYPEDEF, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseStorageClassExtern)
{
    Declaration *decl = GetDeclaration("extern int x;");

    ASSERT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_EXTERN, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseStorageClassStatic)
{
    Declaration *decl = GetDeclaration("static int x;");

    ASSERT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_STATIC, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseStorageClassThreadLocal)
{
    Declaration *decl = GetDeclaration("_Thread_local int x;");

    ASSERT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_THREAD_LOCAL, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseStorageClassAuto)
{
    Declaration *decl = GetDeclaration("auto int x;");

    ASSERT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_AUTO, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseStorageClassRegister)
{
    Declaration *decl = GetDeclaration("register int x;");

    ASSERT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_REGISTER, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseFunctionSpecifierInline)
{
    ExternalDecl *ext = GetExternalDecl("inline int f() {}");
    EXPECT_EQ(EXTERNAL_DECL_FUNCTION, ext->kind);

    ASSERT_NE(nullptr, ext->u.function.specifiers->func_specs);
    EXPECT_EQ(FUNC_SPEC_INLINE, ext->u.function.specifiers->func_specs->kind);
    EXPECT_EQ(TYPE_INT, ext->u.function.specifiers->base_type->kind);
    EXPECT_STREQ("f", ext->u.function.declarator->name);
}

TEST_F(ParserTest, ParseFunctionSpecifierNoreturn)
{
    ExternalDecl *ext = GetExternalDecl("_Noreturn void f() {}");
    EXPECT_EQ(EXTERNAL_DECL_FUNCTION, ext->kind);

    ASSERT_NE(nullptr, ext->u.function.specifiers->func_specs);
    EXPECT_EQ(FUNC_SPEC_NORETURN, ext->u.function.specifiers->func_specs->kind);
    EXPECT_EQ(TYPE_VOID, ext->u.function.specifiers->base_type->kind);
    EXPECT_STREQ("f", ext->u.function.declarator->name);
}

TEST_F(ParserTest, ParseAlignmentSpecifierType)
{
    Declaration *decl = GetDeclaration("_Alignas(int) int x;");

    ASSERT_NE(nullptr, decl->u.var.specifiers->align_spec);
    EXPECT_EQ(ALIGN_SPEC_TYPE, decl->u.var.specifiers->align_spec->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->align_spec->u.type->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseAlignmentSpecifierExpr)
{
    Declaration *decl = GetDeclaration("_Alignas(8) int x;");

    ASSERT_NE(nullptr, decl->u.var.specifiers->align_spec);
    EXPECT_EQ(ALIGN_SPEC_EXPR, decl->u.var.specifiers->align_spec->kind);
    EXPECT_EQ(EXPR_LITERAL, decl->u.var.specifiers->align_spec->u.expr->kind);
    EXPECT_EQ(8, decl->u.var.specifiers->align_spec->u.expr->u.literal->u.int_val);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->name);
}

TEST_F(ParserTest, ParseFunctionDeclaration)
{
    Declaration *decl = GetDeclaration("int f();");

    EXPECT_EQ(DECL_VAR, decl->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_STREQ("f", decl->u.var.declarators->declarator->name);
    ASSERT_NE(nullptr, decl->u.var.declarators->declarator->suffixes);
    EXPECT_EQ(SUFFIX_FUNCTION, decl->u.var.declarators->declarator->suffixes->kind);
    EXPECT_TRUE(decl->u.var.declarators->declarator->suffixes->u.function.params->is_empty);
}
