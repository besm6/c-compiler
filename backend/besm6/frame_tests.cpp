#include <gtest/gtest.h>

#include "abi.h"
#include "frame.h"
#include "tac.h"
#include "xalloc.h"

// Helpers to build minimal TAC nodes without going through the full importer.

static Tac_Val *make_var(const char *name)
{
    auto *v       = static_cast<Tac_Val *>(xalloc(sizeof(Tac_Val), __func__, __FILE__, __LINE__));
    v->next       = nullptr;
    v->kind       = TAC_VAL_VAR;
    v->u.var_name = xstrdup(name);
    return v;
}

static Tac_Instruction *make_copy(const char *src, const char *dst)
{
    auto *i   = static_cast<Tac_Instruction *>(
        xalloc(sizeof(Tac_Instruction), __func__, __FILE__, __LINE__));
    i->next        = nullptr;
    i->kind        = TAC_INSTRUCTION_COPY;
    i->u.copy.src  = make_var(src);
    i->u.copy.dst  = make_var(dst);
    return i;
}

static Tac_Instruction *make_return(const char *name)
{
    auto *i          = static_cast<Tac_Instruction *>(
        xalloc(sizeof(Tac_Instruction), __func__, __FILE__, __LINE__));
    i->next           = nullptr;
    i->kind           = TAC_INSTRUCTION_RETURN;
    i->u.return_.src  = make_var(name);
    return i;
}

static Tac_Param *make_param(const char *name)
{
    auto *p  = static_cast<Tac_Param *>(xalloc(sizeof(Tac_Param), __func__, __FILE__, __LINE__));
    p->next  = nullptr;
    p->name  = xstrdup(name);
    return p;
}

// Build a Function toplevel node with given params and body.
static Tac_TopLevel *make_fn(Tac_Param *params, Tac_Instruction *body)
{
    auto *tl = static_cast<Tac_TopLevel *>(
        xalloc(sizeof(Tac_TopLevel), __func__, __FILE__, __LINE__));
    tl->next                   = nullptr;
    tl->kind                   = TAC_TOPLEVEL_FUNCTION;
    tl->u.function.name        = xstrdup("test_fn");
    tl->u.function.global      = false;
    tl->u.function.params      = params;
    tl->u.function.body        = body;
    return tl;
}

// Free helpers.
static void free_val(Tac_Val *v)
{
    while (v) {
        Tac_Val *next = v->next;
        if (v->kind == TAC_VAL_VAR)
            xfree(v->u.var_name);
        xfree(v);
        v = next;
    }
}

static void free_instr(Tac_Instruction *i)
{
    while (i) {
        Tac_Instruction *next = i->next;
        switch (i->kind) {
        case TAC_INSTRUCTION_COPY:
            free_val(i->u.copy.src);
            free_val(i->u.copy.dst);
            break;
        case TAC_INSTRUCTION_RETURN:
            free_val(i->u.return_.src);
            break;
        default:
            break;
        }
        xfree(i);
        i = next;
    }
}

static void free_params(Tac_Param *p)
{
    while (p) {
        Tac_Param *next = p->next;
        xfree(p->name);
        xfree(p);
        p = next;
    }
}

static void free_fn(Tac_TopLevel *tl)
{
    free_params(tl->u.function.params);
    free_instr(tl->u.function.body);
    xfree(tl->u.function.name);
    xfree(tl);
}

// ---- Tests ---------------------------------------------------------------

// f(x, y) { t = x; return t; }  — x,y are params; t is auto
TEST(FrameTest, ParamsBeforeAutos)
{
    Tac_Param *px  = make_param("x");
    Tac_Param *py  = make_param("y");
    px->next       = py;

    Tac_Instruction *copy = make_copy("x", "t");
    Tac_Instruction *ret  = make_return("t");
    copy->next            = ret;

    Tac_TopLevel *fn = make_fn(px, copy);
    Frame *f = frame_build(fn);

    int reg, off;

    ASSERT_TRUE(frame_lookup(f, "x", &reg, &off));
    EXPECT_EQ(reg, REG_PAR);
    EXPECT_EQ(off, 0);

    ASSERT_TRUE(frame_lookup(f, "y", &reg, &off));
    EXPECT_EQ(reg, REG_PAR);
    EXPECT_EQ(off, 1);

    ASSERT_TRUE(frame_lookup(f, "t", &reg, &off));
    EXPECT_EQ(reg, REG_AUTO);
    EXPECT_EQ(off, 0);

    EXPECT_EQ(frame_num_autos(f), 1);

    frame_free(f);
    free_fn(fn);
}

// No params — every VAR becomes an auto slot.
TEST(FrameTest, NoParams)
{
    Tac_Instruction *copy = make_copy("a", "b");
    Tac_Instruction *ret  = make_return("b");
    copy->next            = ret;

    Tac_TopLevel *fn = make_fn(nullptr, copy);
    Frame *f = frame_build(fn);

    int reg, off;

    ASSERT_TRUE(frame_lookup(f, "a", &reg, &off));
    EXPECT_EQ(reg, REG_AUTO);
    EXPECT_EQ(off, 0);

    ASSERT_TRUE(frame_lookup(f, "b", &reg, &off));
    EXPECT_EQ(reg, REG_AUTO);
    EXPECT_EQ(off, 1);

    EXPECT_EQ(frame_num_autos(f), 2);

    frame_free(f);
    free_fn(fn);
}

// Looking up an unknown name returns false.
TEST(FrameTest, MissReturnsFalse)
{
    Tac_TopLevel *fn = make_fn(nullptr, nullptr);
    Frame *f = frame_build(fn);

    int reg = -1, off = -1;
    EXPECT_FALSE(frame_lookup(f, "nonexistent", &reg, &off));
    EXPECT_EQ(reg, -1); // untouched on miss
    EXPECT_EQ(off, -1);

    frame_free(f);
    free_fn(fn);
}

// A var that appears multiple times should get only one slot.
TEST(FrameTest, NoDuplicateSlots)
{
    Tac_Instruction *i1 = make_copy("x", "y");
    Tac_Instruction *i2 = make_copy("x", "z"); // x seen again
    i1->next            = i2;

    Tac_TopLevel *fn = make_fn(nullptr, i1);
    Frame *f = frame_build(fn);

    int rx, ox, ry, oy, rz, oz;
    ASSERT_TRUE(frame_lookup(f, "x", &rx, &ox));
    ASSERT_TRUE(frame_lookup(f, "y", &ry, &oy));
    ASSERT_TRUE(frame_lookup(f, "z", &rz, &oz));

    // x, y, z each get a distinct offset; x must not be duplicated.
    EXPECT_NE(ox, oy);
    EXPECT_NE(ox, oz);
    EXPECT_EQ(frame_num_autos(f), 3); // x, y, z — no duplicates

    frame_free(f);
    free_fn(fn);
}
