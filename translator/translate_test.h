#pragma once

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "parser.h"
#include "semantic.h"
#include "structtab.h"
#include "symtab.h"
#include "tac.h"
#include "translate.h"
#include "typetab.h"
#include "xalloc.h"

class TranslateTest : public ::testing::Test {
    FILE *input_file{};

protected:
    Program *program{};

    void SetUp() override
    {
        input_file = tmpfile();
        ASSERT_NE(nullptr, input_file);
    }

    void TearDown() override
    {
        fclose(input_file);
        if (program) {
            free_program(program);
        }
        symtab_destroy();
        structtab_destroy();
        typetab_destroy();
        nametab_destroy();
        xreport_lost_memory();
        EXPECT_EQ(xtotal_allocated_size(), 0);
        xfree_all();
    }

    // Parse C source, run the full tacker pipeline on each declaration, and
    // return the concatenated YAML output for every translated toplevel.
    std::string CompileToYaml(const char *src)
    {
        fwrite(src, 1, strlen(src), input_file);
        rewind(input_file);
        program = parse(input_file);
        EXPECT_NE(nullptr, program);

        std::string result;
        ExternalDecl *decls = program->decls;
        program->decls      = nullptr;
        while (decls) {
            ExternalDecl *next = decls->next;
            decls->next        = nullptr;
            typecheck_global_decl(decls);
            label_loops(decls);
            Tac_TopLevel *tac = translate(decls);
            free_external_decl(decls);
            if (tac) {
                FILE *f = tmpfile();
                EXPECT_NE(nullptr, f);
                for (const Tac_TopLevel *t = tac; t; t = t->next)
                    tac_export_yaml(f, t);
                long len = ftell(f);
                rewind(f);
                std::string yaml(static_cast<size_t>(len), '\0');
                EXPECT_TRUE(fread(&yaml[0], 1, static_cast<size_t>(len), f));
                fclose(f);
                result += yaml;
                tac_free_toplevel(tac);
            }
            decls = next;
        }
        return result;
    }
};
