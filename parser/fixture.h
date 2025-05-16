#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ast.h"
#include "scanner.h"

// Test fixture
class ParserTest : public ::testing::Test {
    const std::string test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    FILE *input_file;

public:
    Program *program{};

protected:
    void SetUp() override
    {
        auto filename = test_name + ".c";
        input_file = fopen(filename.c_str(), "w+");
        ASSERT_NE(nullptr, input_file);
    }

    void TearDown() override
    {
        fclose(input_file);
        if (program) {
            free_program(program);
        }
    }

    // Helper to create a temporary file with content
    FILE *CreateTempFile(const char *content)
    {
        fwrite(content, 1, strlen(content), input_file);
        rewind(input_file);
        return input_file;
    }

    // Helper to get function body from program
    DeclOrStmt *GetFunctionBody(const char *content)
    {
        program = parse(CreateTempFile(content));
        EXPECT_NE(nullptr, program);
        print_program(stdout, program);

        // Find function.
        ExternalDecl *decl = program->decls;
        EXPECT_NE(nullptr, decl);
        while (decl->kind != EXTERNAL_DECL_FUNCTION) {
            decl = decl->next;
            EXPECT_NE(nullptr, decl);
        }
        EXPECT_NE(nullptr, decl->u.function.body);
        EXPECT_EQ(STMT_COMPOUND, decl->u.function.body->kind);

        return decl->u.function.body->u.compound;
    }

    // Helper to get function body from program
    Type *TestType(const char *content)
    {
        init_scanner(CreateTempFile(content));
        advance_token();
        Type *type = parse_type_name();
        EXPECT_NE(type, nullptr);
        print_type(stdout, type, 0);
        return type;
    }

    // Helper to get external declaration from program
    ExternalDecl *GetExternalDecl(const char *content)
    {
        program = parse(CreateTempFile(content));
        EXPECT_NE(nullptr, program);
        print_program(stdout, program);

        EXPECT_NE(nullptr, program->decls);
        return program->decls;
    }

    // Helper to get declaration from program
    Declaration *GetDeclaration(const char *content)
    {
        program = parse(CreateTempFile(content));
        EXPECT_NE(nullptr, program);
        print_program(stdout, program);

        // Expect one external declaration.
        EXPECT_NE(nullptr, program->decls);
        EXPECT_EQ(EXTERNAL_DECL_DECLARATION, program->decls->kind);
        EXPECT_EQ(nullptr, program->decls->next);
        return program->decls->u.declaration;
    }
};
