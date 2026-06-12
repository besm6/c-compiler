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

    EXPECT_EQ(R"(c
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

    EXPECT_EQ(R"(c
      foo:   ,name,
          13 ,uj,
             ,end,
)", output);
}

TEST_F(CodegenTest, EmptyFunctionOneParam)
{
    std::string output = CompileToMadlen("void foo(int bar) {}");
    EXPECT_EQ(R"(c
      foo:   ,name,
          13 ,uj,
             ,end,
)", output);
}

TEST_F(CodegenTest, EmptyFunctionTwoParams)
{
    std::string output = CompileToMadlen("void foo(int bar, int quz) {}");
    EXPECT_EQ(R"(c
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
    EXPECT_EQ(R"(c
      foo:   ,name,
          14 ,utc, 1
          15 ,utm,
          13 ,uj,
             ,end,
)", output);
}

TEST_F(CodegenTest, MainEntryPointInt)
{
    std::string output = CompileToMadlen("int main() { return 0; }");
    EXPECT_EQ(R"(c
     main:   ,name,
    b/ret:   ,subp,
  program:   ,entry,
             ,its, 13
             ,call, b/save0
             ,xta, =0
             ,uj, b/ret
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, MainEntryPointVoid)
{
    std::string output = CompileToMadlen("void main() {}");
    EXPECT_EQ(R"(c
     main:   ,name,
  program:   ,entry,
          13 ,uj,
             ,end,
)", output);
}

TEST_F(CodegenTest, CallOkno)
{
    std::string output = CompileToMadlen("void OKHO(void); void foo() { OKHO(); }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save0
             ,call, OKHO
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, CopyParamToAuto)
{
    // copy a → g, b → h: two params stored to two globals; no autos
    std::string output = CompileToMadlen("int g, h; void foo(int a, int b) { g = a; h = b; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
        h:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
        h:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
             ,utc, g
             ,atx,
           6 ,xta, 1
             ,utc, h
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, GetAddressGlobalInt)
{
    // p is a global pointer — the store p = &g survives DSE (globals are live at
    // exit), which also keeps get_address g → t.0 alive.  Exercises the
    // global-src GET_ADDRESS (UTC/VTM/ITA) and local→global COPY (XTA/UTC/ATX).
    std::string output = CompileToMadlen("int g; int *p; void foo(void) { p = &g; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
        p:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
        p:   ,subp,
             ,its, 13
             ,call, b/save0
          15 ,utm, 1
             ,utc, g
          14 ,vtm, 0
             ,ita, 14
           7 ,atx,
           7 ,xta,
             ,utc, p
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, GetAddressAuto)
{
    // get_address a → t.0 (ITA), copy t.0 → global g (local→global)
    // autos: a@(7,0), t.0@(7,1); num_autos=2
    std::string output = CompileToMadlen("int *g; void foo(void) { int a; g = &a; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save0
          15 ,utm, 2
             ,ita, 7
           7 ,atx, 1
           7 ,xta, 1
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, StoreThroughPtr)
{
    // store src=a → dst_ptr=p: params p@(6,0), a@(6,1); num_autos=0
    std::string output = CompileToMadlen("void foo(int *p, int a) { *p = a; }");
    EXPECT_EQ(R"(c
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
    EXPECT_EQ(R"(c
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
    // binary ADD src1=a(6,0) src2=b(6,1) dst=t.0; copy t.0 → global g
    // frame: a@(6,0), b@(6,1), t.0@(7,0); num_autos=1
    std::string output = CompileToMadlen("int g; void foo(int a, int b) { g = a + b; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
           6 ,a+x, 1
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, SubTwoParams)
{
    // binary SUBTRACT src1=a(6,0) src2=b(6,1) dst=t.0; copy t.0 → global g
    // frame: a@(6,0), b@(6,1), t.0@(7,0); num_autos=1
    std::string output = CompileToMadlen("int g; void foo(int a, int b) { g = a - b; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
           6 ,a-x, 1
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, AddAutoAndParam)
{
    // binary ADD src1=b(param) src2=c(auto) dst=t.0; copy t.0 → global g
    // frame: b@(6,0), c@(7,0), t.0@(7,1); num_autos=2
    std::string output = CompileToMadlen("int g; void foo(int b) { int c; g = b + c; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 2
           6 ,xta,
           7 ,a+x,
           7 ,atx, 1
           7 ,xta, 1
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, AddTwoAutos)
{
    // binary ADD src1=a(auto) src2=b(auto) dst=t.0; copy t.0 → global g
    // frame: a@(7,0), b@(7,1), t.0@(7,2); num_autos=3
    std::string output = CompileToMadlen("int g; void foo(void) { int a; int b; g = a + b; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save0
          15 ,utm, 3
           7 ,xta,
           7 ,a+x, 1
           7 ,atx, 2
           7 ,xta, 2
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, LabelJump)
{
    // goto end; end: g = x — the trivial forward goto is folded away by the optimizer
    // (end: is immediately next), leaving just the param→global copy
    std::string output = CompileToMadlen("int g; void foo(int x) { goto end; end: g = x; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// label_loops() resets label_seq=0 per function; STMT_WHILE allocates .L0 (end)
// then .L1 (continue), so the continue label (loop top) is .L1 and end is .L0.
TEST_F(CodegenTest, WhileLoopJumpIfZero)
{
    std::string output = CompileToMadlen("void foo(int x) { while (x) {} }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
      *L1:   ,bss,
           6 ,xta,
             ,uza, *L0
             ,uj, *L1
      *L0:   ,bss,
             ,uj, b/ret
             ,end,
)", output);
}

// new_temp() allocates "t.0" for the do-while loop-top label;
// label_loops assigns .L0 (end) and .L1 (continue) — both dead (no break/continue),
// so the optimizer removes them; bar() call keeps the loop body non-empty.
TEST_F(CodegenTest, DoWhileJumpIfNotZero)
{
    std::string output = CompileToMadlen(
        "void bar(void); void foo(int x) { do { bar(); } while (x); }");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save
      t*0:   ,bss,
             ,call, bar
           6 ,xta,
             ,u1a, t*0
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, FuncArg1)
{
    std::string output = CompileToMadlen(R"(
        void foo(int bar);
        void quz() {
            foo(42);
        }
    )");
    EXPECT_EQ(R"(c
      quz:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save0
             ,xta, =52
          14 ,vtm, -1
             ,call, foo
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, RunRev)
{
    char in_path[]  = "/tmp/rev_input_XXXXXX";
    int  in_fd      = mkstemp(in_path);
    ASSERT_GE(in_fd, 0);
    const char *input = "hello\nworld\nfoo\n";
    write(in_fd, input, strlen(input));
    close(in_fd);

    char out_path[] = "/tmp/rev_output_XXXXXX";
    int  out_fd     = mkstemp(out_path);
    ASSERT_GE(out_fd, 0);
    close(out_fd);

    RunExternalProgram("rev", {in_path}, out_path);

    FILE *f = fopen(out_path, "r");
    ASSERT_NE(nullptr, f);
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    std::string got(static_cast<size_t>(len), '\0');
    fread(&got[0], 1, static_cast<size_t>(len), f);
    fclose(f);

    EXPECT_EQ(got, "olleh\ndlrow\noof\n");

    unlink(in_path);
    unlink(out_path);
}

TEST_F(CodegenTest, EmptyProgram)
{
    std::string result = CompileAndRun("void program() {}");
    EXPECT_EQ("", result);
}

TEST_F(CodegenTest, PrintChar)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        void program() {
            writeb('Q');
            writeb('\n');
        }
    )");
    EXPECT_EQ("Q\n", result);
}

TEST_F(CodegenTest, PrintDecimal)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        int printd(int num);
        void program() {
            printd(42);
            writeb('\n');
        }
    )");
    EXPECT_EQ("42\n", result);
}

TEST_F(CodegenTest, PrintTwoLines)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("First line.\nSecond line.\n");
        }
    )");
    EXPECT_EQ("FIRST LINE.\nSECOND LINE.\n", result);
}

// Global → local: x = g where x is never used — mark it as volatile.
TEST_F(CodegenTest, CopyGlobalToLocal)
{
    std::string output = CompileToMadlen(R"(
        int g;
        void foo(void) {
            volatile int x;
            x = g;
        }
    )");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save0
          15 ,utm, 1
             ,utc, g
             ,xta,
           7 ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// Local → global: COPY a → g (Case C)
TEST_F(CodegenTest, CopyLocalToGlobal)
{
    std::string output = CompileToMadlen("int g; void foo(int a) { g = a; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
           6 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// Global → global: COPY g → h (Case D)
TEST_F(CodegenTest, CopyGlobalToGlobal)
{
    std::string output = CompileToMadlen("int g, h; void foo(void) { h = g; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
        h:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
        h:   ,subp,
             ,its, 13
             ,call, b/save0
             ,utc, g
             ,xta,
             ,utc, h
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// Runtime: write to global, copy global→global, read back via local.
TEST_F(CodegenTest, GlobalScalarReadWrite)
{
    std::string result = CompileAndRun(R"(
        void writeb(int ch);
        int g, h;
        void copy_to_h(int v) { g = v; h = g; }
        void write_h(void) { int x; x = h; writeb(x); writeb('\n'); }
        void program() { copy_to_h('M'); write_h(); }
    )");
    EXPECT_EQ("M\n", result);
}

// COPY from integer constant to a local variable.
// 42 decimal = 52 octal → literal =52.
TEST_F(CodegenTest, CopyConstToLocal)
{
    std::string output = CompileToMadlen(R"(
        void foo(void) {
            volatile int x;
            x = 42;
        }
    )");
    EXPECT_EQ(R"(c
      foo:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save0
          15 ,utm, 1
             ,xta, =52
           7 ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// COPY from integer constant 0 to a global variable.
TEST_F(CodegenTest, CopyConstToGlobal)
{
    std::string output = CompileToMadlen("int g; void foo(void) { g = 0; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save0
             ,xta, =0
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// COPY from negative integer constant: -1 must be masked to 41 bits
// (0x1FFFFFFFFFF = 37777777777777 octal), not emitted as a 64-bit pattern.
TEST_F(CodegenTest, CopyNegConst)
{
    std::string output = CompileToMadlen("int g; void foo(void) { g = -1; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save0
             ,xta, =37777777777777
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// BINARY with a constant right operand: g = a + 5.
TEST_F(CodegenTest, BinaryConstSrc2)
{
    std::string output = CompileToMadlen("int g; void foo(int a) { g = a + 5; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
           6 ,xta,
             ,a+x, =5
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// BINARY with a constant left operand: g = 5 + a.
TEST_F(CodegenTest, BinaryConstSrc1)
{
    std::string output = CompileToMadlen("int g; void foo(int a) { g = 5 + a; }");
    EXPECT_EQ(R"(c
        g:   ,name,
             ,bss, 1
             ,end,
c
      foo:   ,name,
    b/ret:   ,subp,
        g:   ,subp,
             ,its, 13
             ,call, b/save
          15 ,utm, 1
             ,xta, =5
           6 ,a+x,
           7 ,atx,
           7 ,xta,
             ,utc, g
             ,atx,
             ,uj, b/ret
             ,end,
)", output);
}

// Float constant in a function-call argument.
// 1.5 as double → "=r1.5" literal.
TEST_F(CodegenTest, FuncArgFloat)
{
    std::string output = CompileToMadlen(R"(
        void foo(double x);
        void quz(void) { foo(1.5); }
    )");
    EXPECT_EQ(R"(c
      quz:   ,name,
    b/ret:   ,subp,
             ,its, 13
             ,call, b/save0
             ,xta, =r1.5
          14 ,vtm, -1
             ,call, foo
             ,uj, b/ret
             ,end,
)", output);
}

TEST_F(CodegenTest, PrintFormatDecimal)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("foo = %d, bar = %d\n", 123, -456);
        }
    )");
    EXPECT_EQ("FOO = 123, BAR = -456\n", result);
}

TEST_F(CodegenTest, PrintFormatOctal)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("foo = %o, bar = %o\n", 123, -456);
        }
    )");
    EXPECT_EQ("FOO = 173, BAR = 37777777777070\n", result);
}

TEST_F(CodegenTest, PrintFormatString)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("hello %s\n", "world");
        }
    )");
    EXPECT_EQ("HELLO WORLD\n", result);
}

TEST_F(CodegenTest, PrintFormatChar)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            printf("hello %c%c%c%c%c\n", '(', '-', '_', '-', ')');
        }
    )");
    EXPECT_EQ("HELLO (-_-)\n", result);
}

// Integer comparisons (task #4).  Operands are volatile so the optimizer cannot fold
// the comparisons at compile time; each comparison lowers to a runtime relational
// helper call (b/lt, b/le, b/gt, b/ge, b/eq, b/ne) that leaves a raw 0/1 in A.

// All six operators with a > b (5 vs 3): <, <=, >, >=, ==, != → 0 0 1 1 0 1.
TEST_F(CodegenTest, CompareSignedGreater)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = 5, b = 3;
            printf("%d%d%d%d%d%d\n", a < b, a <= b, a > b, a >= b, a == b, a != b);
        }
    )");
    EXPECT_EQ("001101\n", result);
}

// All six operators with a < b (3 vs 5): <, <=, >, >=, ==, != → 1 1 0 0 0 1.
TEST_F(CodegenTest, CompareSignedLess)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = 3, b = 5;
            printf("%d%d%d%d%d%d\n", a < b, a <= b, a > b, a >= b, a == b, a != b);
        }
    )");
    EXPECT_EQ("110001\n", result);
}

// All six operators with a == b (4 vs 4): <, <=, >, >=, ==, != → 0 1 0 1 1 0.
TEST_F(CodegenTest, CompareSignedEqual)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = 4, b = 4;
            printf("%d%d%d%d%d%d\n", a < b, a <= b, a > b, a >= b, a == b, a != b);
        }
    )");
    EXPECT_EQ("010110\n", result);
}

// Negative operands exercise the 41-bit signed sign test (-7 < 2).
TEST_F(CodegenTest, CompareSignedNegative)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = -7, b = 2;
            printf("%d%d%d%d%d%d\n", a < b, a <= b, a > b, a >= b, a == b, a != b);
        }
    )");
    EXPECT_EQ("110001\n", result);
}

// The four unsigned ordering ops on small values (5 vs 3): <, <=, >, >= → 0 0 1 1.
// (Small operands fit the signed range, so the temporary signed-helper mapping holds.)
TEST_F(CodegenTest, CompareUnsigned)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile unsigned x = 5, y = 3;
            printf("%d%d%d%d\n", x < y, x <= y, x > y, x >= y);
        }
    )");
    EXPECT_EQ("0011\n", result);
}

// A comparison feeding an if: the 0/1 result drives the already-working
// JUMP_IF_ZERO control flow.
TEST_F(CodegenTest, CompareInBranch)
{
    std::string result = CompileAndRun(R"(
        int printf(const char *format, ...);
        void program() {
            volatile int a = 7, b = 4;
            if (a > b)
                printf("yes\n");
            else
                printf("no\n");
        }
    )");
    EXPECT_EQ("YES\n", result);
}

// Madlen-level shape check: a comparison emits xta / xts / ,call, b/lt / atx, with no
// caller-side stack pop (the helper pops its own operand).
TEST_F(CodegenTest, CompareMadlenShape)
{
    std::string out = CompileToMadlen(R"(
        int cmp(int a, int b) {
            return a < b;
        }
    )");
    EXPECT_NE(out.find(",xts,"), std::string::npos);
    EXPECT_NE(out.find(",call, b/lt"), std::string::npos);
    // No UTM stack adjustment is emitted around the helper call.
    EXPECT_EQ(out.find("15 ,utm, -1"), std::string::npos);
}
