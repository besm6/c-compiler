#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "codegen_test.h"

// Required by parser and semantic libraries.
extern "C" _Noreturn void fatal_error(const char *message, ...)
{
    fprintf(stderr, "Fatal error: ");
    va_list ap;
    va_start(ap, message);
    vfprintf(stderr, message, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

// Build the SCAL (scalar product) function from the Madlen documentation and
// verify the emitted Madlen text matches the expected listing exactly.
TEST_F(CodegenTest, ScalFunction)
{
    Besm_Module *module = besm_new_module("test");

    Besm_Func *func = besm_new_func("scal", BESM_CC_BESM6_C);
    module->funcs   = func;

    Besm_Block *block = besm_new_block();
    func->blocks      = block;

    // Helper: append a new instruction to the block body and return it.
    Besm_Instr *tail = nullptr;
    auto add         = [&](Besm_Instr *i) -> Besm_Instr * {
        if (!block->body)
            block->body = i;
        else
            tail->next = i;
        tail = i;
        return i;
    };

    // scal: ,name,
    auto *iname      = add(besm_new_instr(BESM_INSTR_NAME));
    iname->u.name    = xstrdup("scal");

    // ,sti, 14
    auto *sti14            = add(besm_new_instr(BESM_INSTR_MEM));
    sti14->u.mem.kind      = BESM_MEM_STI;
    sti14->u.mem.u.ireg    = 14;

    // ,sti, 12
    auto *sti12            = add(besm_new_instr(BESM_INSTR_MEM));
    sti12->u.mem.kind      = BESM_MEM_STI;
    sti12->u.mem.u.ireg    = 12;

    // ,ati, 11
    auto *ati11            = add(besm_new_instr(BESM_INSTR_MEM));
    ati11->u.mem.kind      = BESM_MEM_ATI;
    ati11->u.mem.u.ireg    = 11;

    // ,ntr, 3
    auto *ntr3             = add(besm_new_instr(BESM_INSTR_EXP));
    ntr3->u.exp.kind       = BESM_EXP_SETR;
    ntr3->u.exp.u.imm      = 3;

    // 14 ,xta,
    auto *xta14                       = add(besm_new_instr(BESM_INSTR_MEM));
    xta14->u.mem.kind                 = BESM_MEM_XTA;
    xta14->u.mem.u.addr.kind          = BESM_MEM_ADDR_REG;
    xta14->u.mem.u.addr.reg.num       = 14;
    xta14->u.mem.u.addr.u.offset      = 0;

    // ,utc, =i1
    auto *utc                    = add(besm_new_instr(BESM_INSTR_MOD));
    utc->u.mod.kind              = BESM_MOD_UTC;
    utc->u.mod.addr.kind         = BESM_MEM_ADDR_LABEL;
    utc->u.mod.addr.reg.num      = 0;
    utc->u.mod.addr.u.name       = xstrdup("=i1");

    // ,x-a,
    auto *rsub                      = add(besm_new_instr(BESM_INSTR_ARITH));
    rsub->u.arith.kind              = BESM_ARITH_RSUB;
    rsub->u.arith.addr.kind         = BESM_MEM_ADDR_REG;
    rsub->u.arith.addr.reg.num      = 0;
    rsub->u.arith.addr.u.offset     = 0;

    // ,ati, 14
    auto *ati14            = add(besm_new_instr(BESM_INSTR_MEM));
    ati14->u.mem.kind      = BESM_MEM_ATI;
    ati14->u.mem.u.ireg    = 14;

    // ,ntr, 18
    auto *ntr18            = add(besm_new_instr(BESM_INSTR_EXP));
    ntr18->u.exp.kind      = BESM_EXP_SETR;
    ntr18->u.exp.u.imm     = 18;

    // ,xta,   (load mem[0] = 0, i.e. A ← 0)
    auto *xta0                       = add(besm_new_instr(BESM_INSTR_MEM));
    xta0->u.mem.kind                 = BESM_MEM_XTA;
    xta0->u.mem.u.addr.kind          = BESM_MEM_ADDR_REG;
    xta0->u.mem.u.addr.reg.num       = 0;
    xta0->u.mem.u.addr.u.offset      = 0;

    // *1: ,bss,   (loop label)
    auto *lbl1      = add(besm_new_instr(BESM_INSTR_LABEL));
    lbl1->u.name    = xstrdup("*1");

    // 11 ,xts,
    auto *xts11                       = add(besm_new_instr(BESM_INSTR_MEM));
    xts11->u.mem.kind                 = BESM_MEM_XTS;
    xts11->u.mem.u.addr.kind          = BESM_MEM_ADDR_REG;
    xts11->u.mem.u.addr.reg.num       = 11;
    xts11->u.mem.u.addr.u.offset      = 0;

    // 12 ,a*x,
    auto *mul12                       = add(besm_new_instr(BESM_INSTR_ARITH));
    mul12->u.arith.kind               = BESM_ARITH_MUL;
    mul12->u.arith.addr.kind          = BESM_MEM_ADDR_REG;
    mul12->u.arith.addr.reg.num       = 12;
    mul12->u.arith.addr.u.offset      = 0;

    // 11 ,utm, 1
    auto *utm11                  = add(besm_new_instr(BESM_INSTR_REG));
    utm11->u.reg.kind            = BESM_REG_UTM;
    utm11->u.reg.u.vtm.dst.num   = 11;
    utm11->u.reg.u.vtm.value     = 1;

    // 12 ,utm, 1
    auto *utm12                  = add(besm_new_instr(BESM_INSTR_REG));
    utm12->u.reg.kind            = BESM_REG_UTM;
    utm12->u.reg.u.vtm.dst.num   = 12;
    utm12->u.reg.u.vtm.value     = 1;

    // 15 ,a+x,
    auto *add15                       = add(besm_new_instr(BESM_INSTR_ARITH));
    add15->u.arith.kind               = BESM_ARITH_ADD;
    add15->u.arith.addr.kind          = BESM_MEM_ADDR_REG;
    add15->u.arith.addr.reg.num       = 15;
    add15->u.arith.addr.u.offset      = 0;

    // 14 ,vlm, *1
    auto *vlm14                            = add(besm_new_instr(BESM_INSTR_BRANCH));
    vlm14->u.branch.kind                   = BESM_BRANCH_VLM;
    vlm14->u.branch.u.jump.reg.num         = 14;
    vlm14->u.branch.u.jump.tgt.kind        = BESM_TARGET_LABEL;
    vlm14->u.branch.u.jump.tgt.u.name      = xstrdup("*1");

    // ,ntr, 6
    auto *ntr6             = add(besm_new_instr(BESM_INSTR_EXP));
    ntr6->u.exp.kind       = BESM_EXP_SETR;
    ntr6->u.exp.u.imm      = 6;

    // 13 ,uj,
    auto *uj13                         = add(besm_new_instr(BESM_INSTR_BRANCH));
    uj13->u.branch.kind                = BESM_BRANCH_UJ;
    uj13->u.branch.u.addr.kind         = BESM_MEM_ADDR_REG;
    uj13->u.branch.u.addr.reg.num      = 13;
    uj13->u.branch.u.addr.u.offset     = 0;

    // ,end,
    add(besm_new_instr(BESM_INSTR_END));

    std::string out = capture(module);
    besm_free_module(module);

    EXPECT_EQ(R"(c Module: test
     scal:   ,name,
             ,sti, 14
             ,sti, 12
             ,ati, 11
             ,ntr, 3
          14 ,xta,
             ,utc, =i1
             ,x-a,
             ,ati, 14
             ,ntr, 18
             ,xta,
       *1:   ,bss,
          11 ,xts,
          12 ,a*x,
          11 ,utm, 1
          12 ,utm, 1
          15 ,a+x,
          14 ,vlm, *1
             ,ntr, 6
          13 ,uj,
             ,end,
)", out);
}

// Verify that codegen_program() emits the correct Madlen prologue/epilogue
// for a trivial void function with no parameters and no body instructions.
TEST_F(CodegenTest, EmptyFunctionNoArgs)
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

TEST_F(CodegenTest, EmptyFunctionWithParam)
{
    std::string output = CompileToMadlen("void foo(int bar) {}");
    EXPECT_EQ(R"(c Module: foo
      foo:   ,name,
             ,its, 13
          13 ,vjm, c/save
             ,uj, c/ret
             ,end,
)", output);
}
