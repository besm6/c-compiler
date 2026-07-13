//
// The BESM-6 compiler intrinsics declared in <besm6.h> (docs/Besm6_Intrinsics.md).
//
// The back end does not lower them yet (tasks I2-I5 in backend/besm6/TODO.md),
// so these tests stop at typecheck: they pin the contract the header
// establishes — the names, the signatures, the argument types the prototype
// checks against, and the _Noreturn-ness of __besm6_stop.
//
#include "typecheck_fixture.h"

// Every intrinsic and every convenience macro, called once.
TEST_F(PipelineTest, Besm6IntrinsicsCallAll)
{
    RunPipeline(R"(#include <besm6.h>
unsigned poke(unsigned a, unsigned m)
{
    unsigned ready = __besm6_ext(04031, 0);
    unsigned grp   = __besm6_mod(0237, 0);
    unsigned bits  = __besm6_apx(a, m);
    unsigned back  = __besm6_aux(bits, m);
    unsigned ones  = __besm6_acx(a, 0);
    unsigned top   = __besm6_anx(a, 0);
    unsigned sum   = __besm6_arx(ready, grp);
    unsigned code  = __besm6_extracode(077, 1, 0);

    unsigned n = b6_popcount(back) + b6_highbit(sum) + b6_grp_read();
    b6_grp_mask(m);
    b6_grp_clear(1);

    if (n + ones + top + code == 0)
        __besm6_stop(5);
    return n;
})");

    const Symbol *ext = symtab_get("__besm6_ext");
    ASSERT_NE(ext, nullptr);
    EXPECT_EQ(ext->kind, SYM_FUNC);
    ASSERT_NE(ext->type, nullptr);
    EXPECT_EQ(ext->type->kind, TYPE_FUNCTION);
    EXPECT_FALSE(ext->u.func.defined);
    EXPECT_FALSE(ext->u.func.noret);

    const Symbol *stop = symtab_get("__besm6_stop");
    ASSERT_NE(stop, nullptr);
    EXPECT_EQ(stop->kind, SYM_FUNC);
    EXPECT_FALSE(stop->u.func.noret);
}

// A machine word is carried as unsigned, never as int: a signed int on this
// target holds only 41 of the 48 bits, so ГРП bit 48 would not survive it.
TEST_F(PipelineTest, Besm6IntrinsicWordIsUnsigned)
{
    RunPipeline(R"(#include <besm6.h>
unsigned grp(void)
{
    return b6_grp_read();
})");

    const Symbol *mod = symtab_get("__besm6_mod");
    ASSERT_NE(mod, nullptr);
    ASSERT_NE(mod->type, nullptr);
    ASSERT_EQ(mod->type->kind, TYPE_FUNCTION);

    const Type *ret = mod->type->u.function.return_type;
    ASSERT_NE(ret, nullptr);
    EXPECT_EQ(ret->kind, TYPE_UINT);

    const Param *acc = mod->type->u.function.params;
    ASSERT_NE(acc, nullptr);
    ASSERT_NE(acc->next, nullptr);
    ASSERT_NE(acc->next->type, nullptr);
    EXPECT_EQ(acc->next->type->kind, TYPE_UINT);
}

// The halt is RESUMABLE: the operator presses continue on the console and the
// machine goes on at the next instruction.  So __besm6_stop is deliberately NOT
// _Noreturn — it is an ordinary void call, the code after it is reachable, and a
// non-void function containing one still has to return a value.
TEST_F(PipelineTest, Besm6StopIsResumable)
{
    RunPipeline(R"(#include <besm6.h>
int panic(int code)
{
    __besm6_stop(1);
    return code;
})");
    EXPECT_NE(program, nullptr);

    const Symbol *stop = symtab_get("__besm6_stop");
    ASSERT_NE(stop, nullptr);
    EXPECT_FALSE(stop->u.func.noret);
}

// The prototype is enforced: the front end checks arity against it.
TEST_F(PipelineTest, Besm6IntrinsicWrongArity_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(#include <besm6.h>
unsigned f(unsigned a)
{
    return __besm6_acx(a);
})"),
                 "wrong number of arguments");
}

// __besm6_extracode's opcode *is* the instruction's opcode, so it must be a compile-time
// constant — the one intrinsic argument the front end has to look at.  A non-constant is
// diagnosed here rather than left to miscompile in the back end.
TEST_F(PipelineTest, Besm6ExtracodeOpNotConstant_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(#include <besm6.h>
unsigned trap(int op, unsigned ea)
{
    return __besm6_extracode(op, ea, 0);
})"),
                 "compile-time constant");
}

// Only 050..077 are extracodes; anything else names a different instruction entirely.
TEST_F(PipelineTest, Besm6ExtracodeOpOutOfRange_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(#include <besm6.h>
unsigned trap(unsigned ea)
{
    return __besm6_extracode(0100, ea, 0);
})"),
                 "not an extracode");
}

// A constant *expression* is fine, and is folded at typecheck: the argument reaches the back
// end as a literal whatever the optimizer does with it.  (The v7 write syscall is $77 4.)
TEST_F(PipelineTest, Besm6ExtracodeOpConstantExpr)
{
    RunPipeline(R"(#include <besm6.h>
enum { SYSCALL = 070 };
unsigned wr(unsigned n)
{
    return __besm6_extracode(SYSCALL + 7, 4, n);
})");
    EXPECT_NE(program, nullptr);
}
