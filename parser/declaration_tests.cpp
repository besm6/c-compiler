#include "fixture.h"

// Test declaration: int x;
TEST_F(ParserTest, ParseSimpleDeclaration)
{
    Declaration *decl = GetDeclaration("int x;");

    EXPECT_EQ(DECL_VAR, decl->kind);
    EXPECT_NE(nullptr, decl->u.var.specifiers);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->base_type->kind);
    EXPECT_NE(nullptr, decl->u.var.declarators);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
    EXPECT_EQ(nullptr, decl->u.var.declarators->init);
}

#if 0
// Test declaration: int x = 42;
TEST_F(ParserTest, ParseInitializedDeclaration)
{
    Declaration *decl = GetDeclaration("int x = 42;");

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
    Declaration *decl = GetDeclaration("int x, y;");

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
    Declaration *decl = GetDeclaration("int;");

    EXPECT_EQ(DECL_EMPTY, decl->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_EQ(nullptr, decl->u.var.declarators);
}

// Test static assert: _Static_assert(1, "msg");
TEST_F(ParserTest, DISABLED_ParseStaticAssertDeclaration) // TODO
{
    Declaration *decl = GetDeclaration("_Static_assert(1, \"msg\");");

    EXPECT_EQ(DECL_STATIC_ASSERT, decl->kind);
    EXPECT_EQ(EXPR_LITERAL, decl->u.static_assrt.condition->kind);
    EXPECT_EQ(LITERAL_INT, decl->u.static_assrt.condition->u.literal->kind);
    EXPECT_EQ(1, decl->u.static_assrt.condition->u.literal->u.int_val);
    EXPECT_STREQ("msg", decl->u.static_assrt.message);
}

// Test function definition: int f() {}
TEST_F(ParserTest, ParseFunctionDefinitionNoParams)
{
    ExternalDecl *ext = GetExternalDecl("int f() {}");

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
    ExternalDecl *ext = GetExternalDecl("int f(int x) { return x; }");

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
    ExternalDecl *ext = GetExternalDecl("int f() { int x; int y; }");
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
    Declaration *decl = GetDeclaration("void x;");

    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_VOID, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: char x;
TEST_F(ParserTest, ParseTypeChar)
{
    Declaration *decl = GetDeclaration("char x;");

    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_CHAR, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: short x;
TEST_F(ParserTest, ParseTypeShort)
{
    Declaration *decl = GetDeclaration("short x;");

    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_SHORT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: int x;
TEST_F(ParserTest, ParseTypeInt)
{
    Declaration *decl = GetDeclaration("int x;");

    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: long x;
TEST_F(ParserTest, ParseTypeLong)
{
    Declaration *decl = GetDeclaration("long x;");

    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_LONG, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: float x;
TEST_F(ParserTest, ParseTypeFloat)
{
    Declaration *decl = GetDeclaration("float x;");

    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_FLOAT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: double x;
TEST_F(ParserTest, ParseTypeDouble)
{
    Declaration *decl = GetDeclaration("double x;");

    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_DOUBLE, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: signed x;
TEST_F(ParserTest, ParseTypeSigned)
{
    Declaration *decl = GetDeclaration("signed x;");

    EXPECT_EQ(decl->kind, DECL_VAR);
    EXPECT_EQ(decl->next, nullptr);

    EXPECT_NE(nullptr, decl->u.var.specifiers);
    TypeSpec *type_specs = decl->u.var.specifiers->type_specs;
    EXPECT_NE(nullptr, type_specs);
    EXPECT_EQ(TYPE_SPEC_BASIC, type_specs->kind);
    EXPECT_EQ(TYPE_INT, type_specs->u.basic->kind);
    EXPECT_EQ(SIGNED_SIGNED, type_specs->u.basic->u.integer.signedness);

    EXPECT_NE(nullptr, decl->u.var.declarators);
    Declarator *declarator = decl->u.var.declarators->declarator;
    EXPECT_NE(nullptr, declarator);
    EXPECT_STREQ("x", declarator->u.named.name);
}
//   ExternalDecl: Declaration
//     Declaration: Variable
//       DeclSpec:
//         TypeSpec: signed
//       InitDeclarator:
//         Declarator:
//           Name: "x"

// Test type specifier: unsigned x;
TEST_F(ParserTest, ParseTypeUnsigned)
{
    Declaration *decl = GetDeclaration("unsigned x;");

    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_EQ(SIGNED_UNSIGNED, decl->u.var.specifiers->type_specs->u.basic->u.integer.signedness);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: _Bool x;
TEST_F(ParserTest, ParseTypeBool)
{
    Declaration *decl = GetDeclaration("_Bool x;");

    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_BOOL, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: _Complex x;
TEST_F(ParserTest, ParseTypeComplex)
{
    Declaration *decl = GetDeclaration("_Complex x;");

    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_COMPLEX, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: _Imaginary x;
TEST_F(ParserTest, ParseTypeImaginary)
{
    Declaration *decl = GetDeclaration("_Imaginary x;");

    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_IMAGINARY, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type specifier: struct S { int x; } s;
TEST_F(ParserTest, DISABLED_ParseTypeStruct) // TODO
{
    Declaration *decl = GetDeclaration("struct S { int x; } s;");

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
TEST_F(ParserTest, DISABLED_ParseTypeAnonymousStruct) // TODO
{
    Declaration *decl = GetDeclaration("struct { int x; } s;");

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
TEST_F(ParserTest, DISABLED_ParseTypeUnion) // TODO
{
    Declaration *decl = GetDeclaration("union U { int x; } u;");

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
    Declaration *decl = GetDeclaration("enum E { A };");

    EXPECT_EQ(TYPE_SPEC_ENUM, decl->u.var.specifiers->type_specs->kind);
    EXPECT_STREQ("E", decl->u.var.specifiers->type_specs->u.enum_spec.name);
    EXPECT_NE(nullptr, decl->u.var.specifiers->type_specs->u.enum_spec.enumerators);
    EXPECT_STREQ("A", decl->u.var.specifiers->type_specs->u.enum_spec.enumerators->name);
    EXPECT_EQ(nullptr, decl->u.var.specifiers->type_specs->u.enum_spec.enumerators->value);
}

// Test type specifier: enum { A };
TEST_F(ParserTest, ParseTypeAnonymousEnum)
{
    Declaration *decl = GetDeclaration("enum { A };");

    EXPECT_EQ(TYPE_SPEC_ENUM, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(nullptr, decl->u.var.specifiers->type_specs->u.enum_spec.name);
    EXPECT_NE(nullptr, decl->u.var.specifiers->type_specs->u.enum_spec.enumerators);
    EXPECT_STREQ("A", decl->u.var.specifiers->type_specs->u.enum_spec.enumerators->name);
}

#if 0
// Test type specifier: typedef int T; T x;
TEST_F(ParserTest, ParseTypeTypedef)
{
    ExternalDecl *ext = GetExternalDecl("typedef int T; T x;");
    EXPECT_EQ(EXTERNAL_DECL_DECLARATION, ext->kind);

    Declaration *decl = ext->u.declaration;
    EXPECT_EQ(STORAGE_CLASS_TYPEDEF, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("T", decl->u.var.declarators->declarator->u.named.name);

    decl = ext->next->u.declaration;
    EXPECT_EQ(TYPE_SPEC_TYPEDEF_NAME, decl->u.var.specifiers->type_specs->kind);
//  EXPECT_STREQ("T", decl->u.var.specifiers->type_specs->u.typedef_name); // TODO
//  EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name); // TODO
    EXPECT_EQ(nullptr, decl->next);
}

// Test type specifier: _Atomic(int) x;
TEST_F(ParserTest, ParseTypeAtomic)
{
    Declaration *decl = GetDeclaration("_Atomic(int) x;");

    EXPECT_EQ(TYPE_SPEC_ATOMIC, decl->u.var.specifiers->type_specs->kind);
//  EXPECT_EQ(TYPE_ATOMIC, decl->u.var.specifiers->type_specs->u.atomic->kind); // TODO
//  EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.atomic->u.atomic.base->kind); //
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type qualifier: const int x;
TEST_F(ParserTest, ParseTypeQualifierConst)
{
    Declaration *decl = GetDeclaration("const int x;");

    EXPECT_NE(nullptr, decl->u.var.specifiers->qualifiers);
    EXPECT_EQ(TYPE_QUALIFIER_CONST, decl->u.var.specifiers->qualifiers->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type qualifier: restrict int x;
TEST_F(ParserTest, ParseTypeQualifierRestrict)
{
    Declaration *decl = GetDeclaration("restrict int x;");

    EXPECT_NE(nullptr, decl->u.var.specifiers->qualifiers);
    EXPECT_EQ(TYPE_QUALIFIER_RESTRICT, decl->u.var.specifiers->qualifiers->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type qualifier: volatile int x;
TEST_F(ParserTest, ParseTypeQualifierVolatile)
{
    Declaration *decl = GetDeclaration("volatile int x;");

    EXPECT_NE(nullptr, decl->u.var.specifiers->qualifiers);
    EXPECT_EQ(TYPE_QUALIFIER_VOLATILE, decl->u.var.specifiers->qualifiers->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test type qualifier: _Atomic int x;
TEST_F(ParserTest, ParseTypeQualifierAtomic)
{
    Declaration *decl = GetDeclaration("_Atomic int x;");

    EXPECT_NE(nullptr, decl->u.var.specifiers->qualifiers);
    EXPECT_EQ(TYPE_QUALIFIER_ATOMIC, decl->u.var.specifiers->qualifiers->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test storage class: typedef int x;
TEST_F(ParserTest, ParseStorageClassTypedef)
{
    Declaration *decl = GetDeclaration("typedef int x;");

    EXPECT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_TYPEDEF, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test storage class: extern int x;
TEST_F(ParserTest, ParseStorageClassExtern)
{
    Declaration *decl = GetDeclaration("extern int x;");

    EXPECT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_EXTERN, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test storage class: static int x;
TEST_F(ParserTest, ParseStorageClassStatic)
{
    Declaration *decl = GetDeclaration("static int x;");

    EXPECT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_STATIC, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test storage class: _Thread_local int x;
TEST_F(ParserTest, ParseStorageClassThreadLocal)
{
    Declaration *decl = GetDeclaration("_Thread_local int x;");

    EXPECT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_THREAD_LOCAL, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test storage class: auto int x;
TEST_F(ParserTest, ParseStorageClassAuto)
{
    Declaration *decl = GetDeclaration("auto int x;");

    EXPECT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_AUTO, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test storage class: register int x;
TEST_F(ParserTest, ParseStorageClassRegister)
{
    Declaration *decl = GetDeclaration("register int x;");

    EXPECT_NE(nullptr, decl->u.var.specifiers->storage);
    EXPECT_EQ(STORAGE_CLASS_REGISTER, decl->u.var.specifiers->storage->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test function specifier: inline int f() {}
TEST_F(ParserTest, ParseFunctionSpecifierInline)
{
    ExternalDecl *ext = GetExternalDecl("inline int f() {}");
    EXPECT_EQ(EXTERNAL_DECL_FUNCTION, ext->kind);

    EXPECT_NE(nullptr, ext->u.function.specifiers->func_specs);
    EXPECT_EQ(FUNC_SPEC_INLINE, ext->u.function.specifiers->func_specs->kind);
    EXPECT_EQ(TYPE_INT, ext->u.function.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("f", ext->u.function.declarator->u.named.name);
}

// Test function specifier: _Noreturn void f() {}
TEST_F(ParserTest, ParseFunctionSpecifierNoreturn)
{
    Declaration *decl = GetDeclaration("_Noreturn void f() {}");

    EXPECT_EQ(EXTERNAL_DECL_FUNCTION, ext->kind);
    EXPECT_NE(nullptr, ext->u.function.specifiers->func_specs);
    EXPECT_EQ(FUNC_SPEC_NORETURN, ext->u.function.specifiers->func_specs->kind);
    EXPECT_EQ(TYPE_VOID, ext->u.function.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("f", ext->u.function.declarator->u.named.name);
}

// Test alignment specifier: _Alignas(int) int x;
TEST_F(ParserTest, ParseAlignmentSpecifierType)
{
    Declaration *decl = GetDeclaration("_Alignas(int) int x;");

    EXPECT_NE(nullptr, decl->u.var.specifiers->align_spec);
    EXPECT_EQ(ALIGN_SPEC_TYPE, decl->u.var.specifiers->align_spec->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->align_spec->u.type->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
}

// Test alignment specifier: _Alignas(8) int x;
TEST_F(ParserTest, ParseAlignmentSpecifierExpr)
{
    Declaration *decl = GetDeclaration("_Alignas(8) int x;");

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
    Declaration *decl = GetDeclaration("(int) x;");

    EXPECT_EQ(DECL_VAR, decl->kind);
    EXPECT_NE(nullptr, decl->u.var.specifiers);
    EXPECT_EQ(TYPE_SPEC_BASIC, decl->u.var.specifiers->type_specs->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_NE(nullptr, decl->u.var.declarators);
    EXPECT_STREQ("x", decl->u.var.declarators->declarator->u.named.name);
    EXPECT_EQ(nullptr, decl->u.var.declarators->init);
}

// Test type name: (const int*) x
TEST_F(ParserTest, ParseTypeNameQualified)
{
    Declaration *decl = GetDeclaration("(const int*) x;");

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
    Declaration *decl = GetDeclaration("int f();");

    EXPECT_EQ(DECL_VAR, decl->kind);
    EXPECT_EQ(TYPE_INT, decl->u.var.specifiers->type_specs->u.basic->kind);
    EXPECT_STREQ("f", decl->u.var.declarators->declarator->u.named.name);
    EXPECT_NE(nullptr, decl->u.var.declarators->declarator->u.named.suffixes);
    EXPECT_EQ(SUFFIX_FUNCTION, decl->u.var.declarators->declarator->u.named.suffixes->kind);
    EXPECT_TRUE(decl->u.var.declarators->declarator->u.named.suffixes->u.function.params->is_empty);
}
#endif
#endif
