#pragma once

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "besm.h"
#include "codegen.h"
#include "parser.h"
#include "semantic.h"
#include "structtab.h"
#include "symtab.h"
#include "tac.h"
#include "translate.h"
#include "typetab.h"
#include "xalloc.h"

extern "C" int xalloc_debug;

class CodegenTest : public ::testing::Test {
    FILE    *input_file{};
    Program *program{};

protected:
    void SetUp() override
    {
        input_file = tmpfile();
        ASSERT_NE(nullptr, input_file);
    }

    void TearDown() override
    {
        fclose(input_file);
        if (program)
            free_program(program);
        symtab_destroy();
        structtab_destroy();
        typetab_destroy();
        nametab_destroy();
        xreport_lost_memory();
        EXPECT_EQ(xtotal_allocated_size(), 0);
        xfree_all();
    }

    // Capture Madlen output from a pre-built Besm_Module (used by Madlen-level tests).
    static std::string capture(const Besm_Module *module)
    {
        FILE *f = tmpfile();
        EXPECT_NE(nullptr, f);
        emit_madlen_module(f, module);
        long len = ftell(f);
        rewind(f);
        std::string result(static_cast<size_t>(len), '\0');
        EXPECT_TRUE(fread(&result[0], 1, static_cast<size_t>(len), f));
        fclose(f);
        return result;
    }

    // Capture Madlen output from a pre-built TAC toplevel (used by codegen-level tests).
    static std::string capture(const Tac_TopLevel *tl)
    {
        FILE *f = tmpfile();
        EXPECT_NE(nullptr, f);
        codegen_program(tl, f);
        long len = ftell(f);
        rewind(f);
        std::string result(static_cast<size_t>(len), '\0');
        EXPECT_TRUE(fread(&result[0], 1, static_cast<size_t>(len), f));
        fclose(f);
        return result;
    }

    // Parse C source, run full typecheck+translate+codegen pipeline, and
    // return the concatenated Madlen assembly for every translated toplevel.
    std::string CompileToMadlen(const char *src)
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
            typecheck_decl(decls);
            Tac_TopLevel *tac = translate(decls);
            free_external_decl(decls);
            if (tac) {
                for (const Tac_TopLevel *t = tac; t; t = t->next)
                    result += capture(t);
                tac_free_toplevel(tac);
            }
            decls = next;
        }
        return result;
    }
};
