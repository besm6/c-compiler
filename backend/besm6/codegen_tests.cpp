#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include "codegen.h"
#include "tac.h"
#include "xalloc.h"

extern "C" int xalloc_debug;

class CodegenTest : public ::testing::Test {
protected:
    void TearDown() override
    {
        xreport_lost_memory();
        EXPECT_EQ(xtotal_allocated_size(), 0);
        xfree_all();
    }

    static std::string capture(const Tac_TopLevel *tl)
    {
        FILE *f = tmpfile();
        EXPECT_NE(f, nullptr);
        codegen_program(tl, f);
        long len = ftell(f);
        rewind(f);
        std::string result(static_cast<size_t>(len), '\0');
        EXPECT_TRUE(fread(&result[0], 1, static_cast<size_t>(len), f));
        fclose(f);
        return result;
    }
};

// Verify that codegen_program() emits the correct Madlen prologue/epilogue
// for a trivial void function with no parameters and no body instructions.
TEST_F(CodegenTest, VoidFooFunction)
{
    Tac_TopLevel *tl      = tac_new_toplevel(TAC_TOPLEVEL_FUNCTION);
    tl->u.function.name   = xstrdup("foo");
    tl->u.function.global = false;
    tl->u.function.params = nullptr;
    tl->u.function.body   = nullptr;

    std::string output = capture(tl);

    tac_free_toplevel(tl);

    EXPECT_EQ(R"(c Module: foo
      foo:   ,name,
             ,its, 13
          13 ,vjm, c/save
             ,uj, c/ret
             ,end,
)", output);
}
