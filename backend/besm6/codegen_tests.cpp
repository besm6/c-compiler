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
    auto *iname  = add(besm_new_instr(BESM_STMT_NAME));
    iname->name  = xstrdup("scal");

    // ,sti, 14
    auto *sti14  = add(besm_new_instr(BESM_MEM_STI));
    sti14->addr  = 14;

    // ,sti, 12
    auto *sti12  = add(besm_new_instr(BESM_MEM_STI));
    sti12->addr  = 12;

    // ,ati, 11
    auto *ati11  = add(besm_new_instr(BESM_MEM_ATI));
    ati11->addr  = 11;

    // ,ntr, 3
    auto *ntr3   = add(besm_new_instr(BESM_EXP_SETR));
    ntr3->addr   = 3;

    // 14 ,xta,
    auto *xta14  = add(besm_new_instr(BESM_MEM_XTA));
    xta14->reg   = 14;

    // ,utc, =i1
    auto *utc    = add(besm_new_instr(BESM_MOD_UTC));
    utc->name    = xstrdup("=i1");

    // ,x-a,
    add(besm_new_instr(BESM_ARITH_RSUB));

    // ,ati, 14
    auto *ati14  = add(besm_new_instr(BESM_MEM_ATI));
    ati14->addr  = 14;

    // ,ntr, 18
    auto *ntr18  = add(besm_new_instr(BESM_EXP_SETR));
    ntr18->addr  = 18;

    // ,xta,   (load mem[0] = 0, i.e. A ← 0)
    add(besm_new_instr(BESM_MEM_XTA));

    // *1: ,bss,   (loop label)
    auto *lbl1   = add(besm_new_instr(BESM_STMT_LABEL));
    lbl1->name   = xstrdup("*1");

    // 11 ,xts,
    auto *xts11  = add(besm_new_instr(BESM_MEM_XTS));
    xts11->reg   = 11;

    // 12 ,a*x,
    auto *mul12  = add(besm_new_instr(BESM_ARITH_MUL));
    mul12->reg   = 12;

    // 11 ,utm, 1
    auto *utm11  = add(besm_new_instr(BESM_REG_UTM));
    utm11->reg   = 11;
    utm11->addr  = 1;

    // 12 ,utm, 1
    auto *utm12  = add(besm_new_instr(BESM_REG_UTM));
    utm12->reg   = 12;
    utm12->addr  = 1;

    // 15 ,a+x,
    auto *add15  = add(besm_new_instr(BESM_ARITH_ADD));
    add15->reg   = 15;

    // 14 ,vlm, *1
    auto *vlm14  = add(besm_new_instr(BESM_BRANCH_VLM));
    vlm14->reg   = 14;
    vlm14->name  = xstrdup("*1");

    // ,ntr, 6
    auto *ntr6   = add(besm_new_instr(BESM_EXP_SETR));
    ntr6->addr   = 6;

    // 13 ,uj,
    auto *uj13   = add(besm_new_instr(BESM_BRANCH_UJ));
    uj13->reg    = 13;

    // ,end,
    add(besm_new_instr(BESM_STMT_END));

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
          13 ,uj,
             ,end,
)", output);
}

TEST_F(CodegenTest, EmptyFunctionOneParam)
{
    std::string output = CompileToMadlen("void foo(int bar) {}");
    EXPECT_EQ(R"(c Module: foo
      foo:   ,name,
          13 ,uj,
             ,end,
)", output);
}

TEST_F(CodegenTest, EmptyFunctionTwoParams)
{
    std::string output = CompileToMadlen("void foo(int bar, int quz) {}");
    EXPECT_EQ(R"(c Module: foo
      foo:   ,name,
          14 ,utc, 1
          15 ,utm,
          13 ,uj,
             ,end,
)", output);
}

TEST_F(CodegenTest, EmptyFunctionVariadic)
{
    std::string output = CompileToMadlen("void foo(int bar, ...) {}");
    EXPECT_EQ(R"(c Module: foo
      foo:   ,name,
          14 ,utc, 1
          15 ,utm,
          13 ,uj,
             ,end,
)", output);
}

TEST_F(CodegenTest, CallOkno)
{
    std::string output = CompileToMadlen("void OKHO(void); void foo() { OKHO(); }");
    EXPECT_EQ(R"(c Module: foo
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
             ,call, OKHO
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, CopyParamToAuto)
{
    // copy a → b: src=param a@(6,0), dst=auto b@(7,0); num_autos=1
    std::string output = CompileToMadlen("void foo(int a) { int b; b = a; }");
    EXPECT_EQ(R"(c Module: foo
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
           7 ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, GetAddressAuto)
{
    // get_address a → t.0, copy t.0 → p
    // autos: a@(7,0), t.0@(7,1), p@(7,2); num_autos=3
    std::string output = CompileToMadlen("void foo(void) { int a; int *p = &a; }");
    EXPECT_EQ(R"(c Module: foo
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 3
             ,ita, 7
           7 ,atx, 1
           7 ,xta, 1
           7 ,atx, 2
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, StoreThroughPtr)
{
    // store src=a → dst_ptr=p: params p@(6,0), a@(6,1); num_autos=0
    std::string output = CompileToMadlen("void foo(int *p, int a) { *p = a; }");
    EXPECT_EQ(R"(c Module: foo
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
             ,ati, 1
           6 ,xta, 1
           1 ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, LoadAndStoreThroughPtr)
{
    // load *p → t.0, store t.0 → *q
    // params: p@(6,0), q@(6,1); auto: t.0@(7,0); num_autos=1
    std::string output = CompileToMadlen("void foo(int *p, int *q) { *q = *p; }");
    EXPECT_EQ(R"(c Module: foo
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
             ,ati, 1
           1 ,xta,
           7 ,atx,
           6 ,xta, 1
             ,ati, 1
           7 ,xta,
           1 ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, AddTwoParams)
{
    // binary ADD src1=a(6,0) src2=b(6,1) dst=t.0 then copy t.0→c
    // frame: a@(6,0), b@(6,1), t.0@(7,0), c@(7,1); num_autos=2
    std::string output = CompileToMadlen("void foo(int a, int b) { int c; c = a + b; }");
    EXPECT_EQ(R"(c Module: foo
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 2
           6 ,xta,
           6 ,a+x, 1
           7 ,atx,
           7 ,xta,
           7 ,atx, 1
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, SubTwoParams)
{
    // binary SUBTRACT src1=a(6,0) src2=b(6,1) dst=t.0 then copy t.0→c
    // frame: a@(6,0), b@(6,1), t.0@(7,0), c@(7,1); num_autos=2
    std::string output = CompileToMadlen("void foo(int a, int b) { int c; c = a - b; }");
    EXPECT_EQ(R"(c Module: foo
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 2
           6 ,xta,
           6 ,a-x, 1
           7 ,atx,
           7 ,xta,
           7 ,atx, 1
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, AddAutoAndParam)
{
    // c = b + c: binary ADD src1=b(param) src2=c(auto) dst=t.0; copy t.0→c
    // frame scan: b@(6,0), c first seen as src2 → c@(7,0), t.0@(7,1); num_autos=2
    std::string output = CompileToMadlen("void foo(int b) { int c; c = b + c; }");
    EXPECT_EQ(R"(c Module: foo
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 2
           6 ,xta,
           7 ,a+x,
           7 ,atx, 1
           7 ,xta, 1
           7 ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, AddTwoAutos)
{
    // c = a + b: all locals; binary ADD src1=a src2=b dst=t.0; copy t.0→c
    // frame scan: a@(7,0), b@(7,1), t.0@(7,2), c@(7,3); num_autos=4
    std::string output = CompileToMadlen("void foo(void) { int a; int b; int c; c = a + b; }");
    EXPECT_EQ(R"(c Module: foo
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 4
           7 ,xta,
           7 ,a+x, 1
           7 ,atx, 2
           7 ,xta, 2
           7 ,atx, 3
             ,uj, b/ret
             ,end,
)", output);
}
