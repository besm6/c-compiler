#pragma once
#include <fcntl.h>
#include <gtest/gtest.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "internal.h"
#include "parser.h"
#include "scanner.h"
#include "semantic.h"
#include "structtab.h"
#include "symtab.h"
#include "test_preprocess.h"
#include "typetab.h"
#include "xalloc.h"

class TypecheckTest : public ::testing::Test {
    const std::string test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    FILE *input_file{};

public:
    Program *program{};

protected:
    void SetUp() override
    {
        auto filename = test_name + ".c";
        input_file    = fopen(filename.c_str(), "w+");
        ASSERT_NE(nullptr, input_file);
        semantic_debug = 1;
    }

    void TearDown() override
    {
        fclose(input_file);
        if (program) {
            free_program(program);
        }
        symtab_print();
        structtab_print();
        nametab_destroy();
        symtab_destroy();
        structtab_destroy();
        typetab_destroy();
        xreport_lost_memory();
        EXPECT_EQ(xtotal_allocated_size(), 0);
        xfree_all();
    }

    FILE *CreateTempFile(const char *content)
    {
        std::string source = preprocess_source(content);
        if (source.empty())
            ADD_FAILURE() << "C preprocessing failed for test source";
        fwrite(source.data(), 1, source.size(), input_file);
        rewind(input_file);
        return input_file;
    }

    void ParseProgram(const char *content)
    {
        program = parse(CreateTempFile(content));
        EXPECT_NE(nullptr, program);
    }

    ExternalDecl *GetExternalDecl(const char *content)
    {
        ParseProgram(content);
        EXPECT_NE(nullptr, program->decls);
        return program->decls;
    }
};

class PipelineTest : public TypecheckTest {
protected:
    void RunPipeline(const char *src)
    {
        ParseProgram(src);
        typecheck_program(program);
    }
};

class LabelLoopsTest : public PipelineTest {
protected:
    void RunLabelLoops(const char *src)
    {
        ParseProgram(src);
        typecheck_program(program);
    }
};
