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
};
