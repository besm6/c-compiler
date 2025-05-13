#include "fixture.h"

// Test declaration: int x;
TEST_F(ParserTest, ParseSimpleDeclaration)
{
    FILE *f          = CreateTempFile("int x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    EXPECT_EQ(EXTERNAL_DECL_DECLARATION, ext->kind);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(DECL_VAR, decl->kind);
    EXPECT_NE(nullptr, decl->u.var.specifiers);
    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_NE(nullptr, decl->u.var.declarators);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
    EXPECT_EQ(nullptr, decl->u.var.declarators->init);
}

// Test declaration: int x = 42;
TEST_F(ParserTest, ParseInitializedDeclaration)
{
    FILE *f          = CreateTempFile("int x = 42;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    EXPECT_EQ(EXTERNAL_DECL_DECLARATION, ext->kind);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(DECL_VAR, decl->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
    EXPECT_NE(nullptr, decl->u.var.declarators->init);
    EXPECT_EQ(INITIALIZER_SINGLE, decl->u.var.declarators->init->kind);
    EXPECT_EQ(EXPR_LITERAL, decl->u.var.declarators->init->u.expr->kind);
    EXPECT_EQ(LITERAL_INT, decl->u.var.declarators->init->u.expr->u.literal->kind);
    EXPECT_EQ(42, decl->u.var.declarators->init->u.expr->u.literal->u.int_val);
}

// Test declaration: int x, y;
TEST_F(ParserTest, ParseMultipleDeclarators)
{
    FILE *f          = CreateTempFile("int x, y;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    EXPECT_EQ(EXTERNAL_DECL_DECLARATION, ext->kind);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(DECL_VAR, decl->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
    EXPECT_NE(nullptr, decl->u.var.declarators->next);
    EXPECT_STREQ("y", decl->u.var.declarators->next->declarator->u.named.name);
    EXPECT_EQ(nullptr, decl->u.var.declarators->next->next);
}

// Test declaration: int;
TEST_F(ParserTest, ParseEmptyDeclaration)
{
    FILE *f          = CreateTempFile("int;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    EXPECT_EQ(EXTERNAL_DECL_DECLARATION, ext->kind);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(DECL_EMPTY, decl->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_EQ(nullptr, decl->u.var.declarators);
}

// Test static assert: _Static_assert(1, "msg");
TEST_F(ParserTest, ParseStaticAssertDeclaration)
{
    FILE *f          = CreateTempFile("_Static_assert(1, \"msg\");");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    EXPECT_EQ(EXTERNAL_DECL_DECLARATION, ext->kind);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(DECL_STATIC_ASSERT, decl->kind);
    EXPECT_EQ(EXPR_LITERAL, decl->u.static_assrt.condition->kind);
    EXPECT_EQ(LITERAL_INT, decl->u.static_assrt.condition->u.literal->kind);
    EXPECT_EQ(1, decl->u.static_assrt.condition->u.literal->u.int_val);
    EXPECT_STREQ("msg", decl->u.static_assrt.message);
}

// Test function definition: int f() {}
TEST_F(ParserTest, ParseFunctionDefinitionNoParams)
{
    FILE *f          = CreateTempFile("int f() {}");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    EXPECT_EQ(EXTERNAL_DECL_FUNCTION, ext->kind);
    EXPECT_EQ(TYPE_INT, ext->u.function.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("f", ext->u.function.declarator->u.named.name);
    EXPECT_NE(nullptr, ext->u.function.declarator->u.named.suffixes);
    EXPECT_EQ(SUFFIX_FUNCTION, ext->u.function.declarator->u.named.suffixes->kind);
    EXPECT_TRUE(ext->u.function.declarator->u.named.suffixes->u.function.params->is_empty);
    EXPECT_EQ(STMT_COMPOUND, ext->u.function.body->kind);
    EXPECT_EQ(nullptr, ext->u.function.body->u.compound);
}

// Test function definition: int f(int x) { return x; }
TEST_F(ParserTest, ParseFunctionDefinitionWithParams)
{
    FILE *f          = CreateTempFile("int f(int x) { return x; }");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    EXPECT_EQ(EXTERNAL_DECL_FUNCTION, ext->kind);
    EXPECT_EQ(TYPE_INT, ext->u.function.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("f", ext->u.function.declarator->u.named.name);
    EXPECT_EQ(SUFFIX_FUNCTION, ext->u.function.declarator->u.named.suffixes->kind);
    ParamList *params = ext->u.function.declarator->u.named.suffixes->u.function.params;
    EXPECT_FALSE(params->is_empty);
    EXPECT_NE(nullptr, params->u.params);
    EXPECT_EQ(TYPE_INT, params->u.params->type->kind);
    EXPECT_STREQ("x", params->u.params->name);
    EXPECT_EQ(STMT_COMPOUND, ext->u.function.body->kind);
    EXPECT_NE(nullptr, ext->u.function.body->u.compound);
    EXPECT_EQ(STMT_RETURN, ext->u.function.body->u.compound->u.stmt->kind);
    EXPECT_STREQ("x", ext->u.function.body->u.compound->u.stmt->u.expr->u.var);
}

// Test declaration list: int x; int y;
TEST_F(ParserTest, ParseDeclarationList)
{
    FILE *f          = CreateTempFile("int f() { int x; int y; }");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    EXPECT_EQ(EXTERNAL_DECL_FUNCTION, ext->kind);
    EXPECT_EQ(STMT_COMPOUND, ext->u.function.body->kind);
    DeclOrStmt *items = ext->u.function.body->u.compound;
    EXPECT_NE(nullptr, items);
    EXPECT_EQ(DECL_OR_STMT_DECL, items->kind);
    EXPECT_EQ(DECL_VAR, items->u.decl->kind);
    EXPECT_EQ(TYPE_INT, items->u.decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", items->u.decl->u.var.declarators->declarator->u.named.name);
    EXPECT_NE(nullptr, items->next);
    EXPECT_EQ(DECL_OR_STMT_DECL, items->next->kind);
    EXPECT_EQ(DECL_VAR, items->next->u.decl->kind);
    EXPECT_STREQ("y", items->next->u.decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: void x;
TEST_F(ParserTest, ParseTypeVoid)
{
    FILE *f          = CreateTempFile("void x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_VOID, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: char x;
TEST_F(ParserTest, ParseTypeChar)
{
    FILE *f          = CreateTempFile("char x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_CHAR, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: short x;
TEST_F(ParserTest, ParseTypeShort)
{
    FILE *f          = CreateTempFile("short x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_SHORT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: int x;
TEST_F(ParserTest, ParseTypeInt)
{
    FILE *f          = CreateTempFile("int x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: long x;
TEST_F(ParserTest, ParseTypeLong)
{
    FILE *f          = CreateTempFile("long x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_LONG, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: float x;
TEST_F(ParserTest, ParseTypeFloat)
{
    FILE *f          = CreateTempFile("float x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_FLOAT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: double x;
TEST_F(ParserTest, ParseTypeDouble)
{
    FILE *f          = CreateTempFile("double x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_DOUBLE, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: signed x;
TEST_F(ParserTest, ParseTypeSigned)
{
    FILE *f          = CreateTempFile("signed x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_EQ(SIGNED_SIGNED, decl->u.var.specifiers->type_specs->u.basic->u.char_t.signedness);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: unsigned x;
TEST_F(ParserTest, ParseTypeUnsigned)
{
    FILE *f          = CreateTempFile("unsigned x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_EQ(SIGNED_UNSIGNED, decl->u.var.specifiers->type_specs->u.basic->u.char_t.signedness);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: _Bool x;
TEST_F(ParserTest, ParseTypeBool)
{
    FILE *f          = CreateTempFile("_Bool x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_BOOL, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: _Complex x;
TEST_F(ParserTest, ParseTypeComplex)
{
    FILE *f          = CreateTempFile("_Complex x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_COMPLEX, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: _Imaginary x;
TEST_F(ParserTest, ParseTypeImaginary)
{
    FILE *f          = CreateTempFile("_Imaginary x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_IMAGINARY, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: struct S { int x; } s;
TEST_F(ParserTest, ParseTypeStruct)
{
    FILE *f          = CreateTempFile("struct S { int x; } s;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_STRUCT, decl->u.var.specifiers->type_specs->kind);
    EXPECT_STREQ("S", decl->u.var.specifiers->type_specs->u.struct_spec.name);
    EXPECT_NE(nullptr, decl->u.var.specifiers->type_specs->u.struct_spec.fields);
    EXPECT_FALSE(decl->u.var.specifiers->type_specs->u.struct_spec.fields->is_anonymous);
    EXPECT_STREQ("x", decl->u.var.specifiers->type_specs->u.struct_spec.fields->u.named.name);
    EXPECT_EQ(TYPE_INT,
              decl->u.var.specifiers->type_specs->u.struct_spec.fields->u.named.type->kind);
    EXPECT_STREQ("s", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: struct { int x; } s;
TEST_F(ParserTest, ParseTypeAnonymousStruct)
{
    FILE *f          = CreateTempFile("struct { int x; } s;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_STRUCT, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(nullptr, decl->u.var.specifiers->type_specs->u.struct_spec.name);
    EXPECT_NE(nullptr, decl->u.var.specifiers->type_specs->u.struct_spec.fields);
    EXPECT_FALSE(decl->u.var.specifiers->type_specs->u.struct_spec.fields->is_anonymous);
    EXPECT_STREQ("x", decl->u.var.specifiers->type_specs->u.struct_spec.fields->u.named.name);
    EXPECT_EQ(TYPE_INT,
              decl->u.var.specifiers->type_specs->u.struct_spec.fields->u.named.type->kind);
    EXPECT_STREQ("s", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: union U { int x; } u;
TEST_F(ParserTest, ParseTypeUnion)
{
    FILE *f          = CreateTempFile("union U { int x; } u;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_UNION, decl->u.var.specifiers->type_specs->kind);
    EXPECT_STREQ("U", decl->u.var.specifiers->type_specs->u.struct_spec.name);
    EXPECT_NE(nullptr, decl->u.var.specifiers->type_specs->u.struct_spec.fields);
    EXPECT_FALSE(decl->u.var.specifiers->type_specs->u.struct_spec.fields->is_anonymous);
    EXPECT_STREQ("x", decl->u.var.specifiers->type_specs->u.struct_spec.fields->u.named.name);
    EXPECT_EQ(TYPE_INT,
              decl->u.var.specifiers->type_specs->u.struct_spec.fields->u.named.type->kind);
    EXPECT_STREQ("u", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: enum E { A };
TEST_F(ParserTest, ParseTypeEnum)
{
    FILE *f          = CreateTempFile("enum E { A };");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_ENUM, decl->u.var.specifiers->type_specs->kind);
    EXPECT_STREQ("E", decl->u.var.specifiers->type_specs->u.enum_spec.name);
    EXPECT_NE(nullptr, decl->u.var.specifiers->type_specs->u.enum_spec.enumerators);
    EXPECT_STREQ("A", decl->u.var.specifiers->type_specs->u.enum_spec.enumerators->name);
    EXPECT_EQ(nullptr, decl->u.var.specifiers->type_specs->u.enum_spec.enumerators->value);
}

// Test type specifier: enum { A };
TEST_F(ParserTest, ParseTypeAnonymousEnum)
{
    FILE *f          = CreateTempFile("enum { A };");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_ENUM, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(nullptr, decl->u.var.specifiers->type_specs->u.enum_spec.name);
    EXPECT_NE(nullptr, decl->u.var.specifiers->type_specs->u.enum_spec.enumerators);
    EXPECT_STREQ("A", decl->u.var.specifiers->type_specs->u.enum_spec.enumerators->name);
}

// Test type specifier: typedef int T; T x;
TEST_F(ParserTest, ParseTypeTypedef)
{
    FILE *f          = CreateTempFile("typedef int T; T x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(STORAGE_CLASS_TYPEDEF, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("T", decl->u.var.declarators->declarator->u.named.name);
    EXPECT_NE(nullptr, ext->next);
    decl = ext->next->u.declaration;
    EXPECT_EQ(TYPE_SPEC_TYPEDEF_NAME, decl->u.var.specifiers->type_specs->kind);
    EXPECT_STREQ("T", decl->u.var.specifiers->type_specs->u.typedef_name);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: _Atomic(int) x;
TEST_F(ParserTest, ParseTypeAtomic)
{
    FILE *f          = CreateTempFile("_Atomic(int) x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(TYPE_SPEC_ATOMIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_ATOMIC, decl->u.var.specifiers->type_specs->u.atomic->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.atomic->u.atomic.base->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type qualifier: const int x;
TEST_F(ParserTest, ParseTypeQualifierConst)
{
    FILE *f          = CreateTempFile("const int x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_NE(nullptr, decl->u.var.specifiers->qualifiers);
    EXPECT_EQ(TYPE_QUALIFIER_CONST, decl->u.var.specifiers->qualifiers->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type qualifier: restrict int x;
TEST_F(ParserTest, ParseTypeQualifierRestrict)
{
    FILE *f          = CreateTempFile("restrict int x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_NE(nullptr, decl->u.var.specifiers->qualifiers);
    EXPECT_EQ(TYPE_QUALIFIER_RESTRICT, decl->u.var.specifiers->qualifiers->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type qualifier: volatile int x;
TEST_F(ParserTest, ParseTypeQualifierVolatile)
{
    FILE *f          = CreateTempFile("volatile int x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_NE(nullptr, decl->u.var.specifiers->qualifiers);
    EXPECT_EQ(TYPE_QUALIFIER_VOLATILE, decl->u.var.specifiers->qualifiers->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type qualifier: _Atomic int x;
TEST_F(ParserTest, ParseTypeQualifierAtomic)
{
    FILE *f          = CreateTempFile("_Atomic int x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_NE(nullptr, decl->u.var.specifiers->qualifiers);
    EXPECT_EQ(TYPE_QUALIFIER_ATOMIC, decl->u.var.specifiers->qualifiers->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test storage class: typedef int x;
TEST_F(ParserTest, ParseStorageClassTypedef)
{
    FILE *f          = CreateTempFile("typedef int x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_TYPEDEF, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test storage class: extern int x;
TEST_F(ParserTest, ParseStorageClassExtern)
{
    FILE *f          = CreateTempFile("extern int x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_EXTERN, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test storage class: static int x;
TEST_F(ParserTest, ParseStorageClassStatic)
{
    FILE *f          = CreateTempFile("static int x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_STATIC, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test storage class: _Thread_local int x;
TEST_F(ParserTest, ParseStorageClassThreadLocal)
{
    FILE *f          = CreateTempFile("_Thread_local int x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_THREAD_LOCAL, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test storage class: auto int x;
TEST_F(ParserTest, ParseStorageClassAuto)
{
    FILE *f          = CreateTempFile("auto int x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_AUTO, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test storage class: register int x;
TEST_F(ParserTest, ParseStorageClassRegister)
{
    FILE *f          = CreateTempFile("register int x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_REGISTER, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test function specifier: inline int f() {}
TEST_F(ParserTest, ParseFunctionSpecifierInline)
{
    FILE *f          = CreateTempFile("inline int f() {}");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    EXPECT_EQ(EXTERNAL_DECL_FUNCTION, ext->kind);
    EXPECT_NE(nullptr, ext->u.function.specifiers->func_specs);
    EXPECT_EQ(FUNC_SPEC_INLINE, ext->u.function.specifiers->func_specs->kind);
    EXPECT_EQ(TYPE_INT, ext->u.function.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("f", ext->u.function.declarator->u.named.name);
}

// Test function specifier: _Noreturn void f() {}
TEST_F(ParserTest, ParseFunctionSpecifierNoreturn)
{
    FILE *f          = CreateTempFile("_Noreturn void f() {}");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    EXPECT_EQ(EXTERNAL_DECL_FUNCTION, ext->kind);
    EXPECT_NE(nullptr, ext->u.function.specifiers->func_specs);
    EXPECT_EQ(FUNC_SPEC_NORETURN, ext->u.function.specifiers->func_specs->kind);
    EXPECT_EQ(TYPE_VOID, ext->u.function.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("f", ext->u.function.declarator->u.named.name);
}

// Test alignment specifier: _Alignas(int) int x;
TEST_F(ParserTest, ParseAlignmentSpecifierType)
{
    FILE *f          = CreateTempFile("_Alignas(int) int x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_NE(nullptr, decl->u.var.specifiers->align_spec);
    EXPECT_EQ(ALIGN_SPEC_TYPE, decl->u.var.specifiers->align_spec->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->align_spec->u.type->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test alignment specifier: _Alignas(8) int x;
TEST_F(ParserTest, ParseAlignmentSpecifierExpr)
{
    FILE *f          = CreateTempFile("_Alignas(8) int x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_NE(nullptr, decl->u.var.specifiers->align_spec);
    EXPECT_EQ(ALIGN_SPEC_EXPR, decl->u.var.specifiers->align_spec->kind);
    EXPECT_EQ(EXPR_LITERAL, decl->u.var.specifiers->align_spec->u.expr->kind);
    EXPECT_EQ(8, decl->u.var.specifiers->align_spec->u.expr->u.literal->u.int_val);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type name: (int) x
TEST_F(ParserTest, ParseTypeNameSimple)
{
    FILE *f          = CreateTempFile("(int) x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(EXPR_CAST, decl->u.var.declarators->init->u.expr->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.declarators->init->u.expr->u.cast.type->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->init->u.expr->u.cast.expr->u.var);
}

// Test type name: (const int*) x
TEST_F(ParserTest, ParseTypeNameQualified)
{
    FILE *f          = CreateTempFile("(const int*) x;");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(EXPR_CAST, decl->u.var.declarators->init->u.expr->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.declarators->init->u.expr->u.cast.type->kind);
    EXPECT_NE(nullptr, decl->u.var.declarators->init->u.expr->u.cast.type->qualifiers);
    EXPECT_EQ(TYPE_QUALIFIER_CONST,
              decl->u.var.declarators->init->u.expr->u.cast.type->qualifiers->kind);
    EXPECT_NE(nullptr,
              decl->u.var.declarators->init->u.expr->u.cast.type->qualifiers->next); // Pointer
    EXPECT_STREQ("x", decl->u.var.declarators->init->u.expr->u.cast.expr->u.var);
}

TEST_F(ParserTest, ParseFunctionDeclaration)
{
    FILE *f          = CreateTempFile("int f();");
    Program *program = parse(f);
    fclose(f);

    ExternalDecl *ext = GetExternalDecl(program);
    EXPECT_EQ(EXTERNAL_DECL_DECLARATION, ext->kind);
    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(DECL_VAR, decl->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("f", decl->u.var.declarators->declarator->u.named.name);
    EXPECT_NE(nullptr, decl->u.var.declarators->declarator->u.named.suffixes);
    EXPECT_EQ(SUFFIX_FUNCTION, decl->u.var.declarators->declarator->u.named.suffixes->kind);
    EXPECT_TRUE(decl->u.var.declarators->declarator->u.named.suffixes->u.function.params->is_empty);
}
