#include "optimizer_test_fixture.h"

// ---------------------------------------------------------------------------
// Dead store elimination tests
// ---------------------------------------------------------------------------

// Label("fn") → Binary(ADD, a, b, t.0) → Copy(2, t.0) → Return(t.0)
// t.0 is overwritten by Copy before any use; Binary is a dead store and removed.
TEST_F(OptimizerTest, DeadStoreBinaryRemoved)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *bin   = make_binary(TAC_BINARY_ADD,
                                         make_var("a"), make_var("b"), make_var("t.0"));
    Tac_Instruction *cp    = make_copy(make_const_int(2), make_var("t.0"));
    Tac_Instruction *ret   = make_return(make_var("t.0"));
    entry->next = bin;
    bin->next   = cp;
    cp->next    = ret;

    OptFlags flags         = opt_flags_default();
    flags.copy_propagation = false;
    Tac_Instruction *result = optimize_function(entry, flags, nullptr);

    EXPECT_EQ(capture_instructions(result),
        "- instruction:\n"
        "  kind: label\n"
        "  name: fn\n"
        "- instruction:\n"
        "  kind: copy\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 2\n"
        "  dst:\n"
        "    kind: var\n"
        "    name: t.0\n"
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: var\n"
        "    name: t.0\n");
}

// Label("fn") → Copy(3, t.0) → Return(t.0)
// copy_prop rewrites Return to Return(3); dead_store_elim then removes Copy(3, t.0).
TEST_F(OptimizerTest, DeadStoreCopyRemovedAfterCopyProp)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *cp    = make_copy(make_const_int(3), make_var("t.0"));
    Tac_Instruction *ret   = make_return(make_var("t.0"));
    entry->next = cp;
    cp->next    = ret;

    OptFlags flags = opt_flags_default();
    Tac_Instruction *result = optimize_function(entry, flags, nullptr);

    EXPECT_EQ(capture_instructions(result),
        "- instruction:\n"
        "  kind: label\n"
        "  name: fn\n"
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 3\n");
}

// Label("fn") → Copy(3, t.0) → Return(t.0)  (copy_prop disabled)
// t.0 is live at Return → Copy is not a dead store.
TEST_F(OptimizerTest, DeadStoreUsedSurvives)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *cp    = make_copy(make_const_int(3), make_var("t.0"));
    Tac_Instruction *ret   = make_return(make_var("t.0"));
    entry->next = cp;
    cp->next    = ret;

    OptFlags flags         = opt_flags_default();

    flags.copy_propagation = false;
    Tac_Instruction *result = optimize_function(entry, flags, nullptr);

    EXPECT_EQ(capture_instructions(result),
        "- instruction:\n"
        "  kind: label\n"
        "  name: fn\n"
        "- instruction:\n"
        "  kind: copy\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 3\n"
        "  dst:\n"
        "    kind: var\n"
        "    name: t.0\n"
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: var\n"
        "    name: t.0\n");
}

// Label("fn") → Store(v, ptr) → Return(ConstInt(0))
// STORE is not removable regardless of liveness.
TEST_F(OptimizerTest, DeadStoreNotStore)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *store = make_store(make_var("v"), make_var("ptr"));
    Tac_Instruction *ret   = make_return(make_const_int(0));
    entry->next = store;
    store->next = ret;

    OptFlags flags         = opt_flags_default();

    flags.copy_propagation = false;
    Tac_Instruction *result = optimize_function(entry, flags, nullptr);

    EXPECT_EQ(capture_instructions(result),
        "- instruction:\n"
        "  kind: label\n"
        "  name: fn\n"
        "- instruction:\n"
        "  kind: store\n"
        "  src:\n"
        "    kind: var\n"
        "    name: v\n"
        "  dst_ptr:\n"
        "    kind: var\n"
        "    name: ptr\n"
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 0\n");
}

// Label("fn") → FunCall("f") → Return(ConstInt(0))
// FUN_CALL has observable side effects and is never removed.
TEST_F(OptimizerTest, DeadStoreNotFunCall)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *call  = make_fun_call("f");
    Tac_Instruction *ret   = make_return(make_const_int(0));
    entry->next = call;
    call->next  = ret;

    OptFlags flags         = opt_flags_default();

    flags.copy_propagation = false;
    Tac_Instruction *result = optimize_function(entry, flags, nullptr);

    EXPECT_EQ(capture_instructions(result),
        "- instruction:\n"
        "  kind: label\n"
        "  name: fn\n"
        "- instruction:\n"
        "  kind: fun_call\n"
        "  fun_name: f\n"
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 0\n");
}

// Label("fn") → Copy(3, g) → Return(ConstInt(0));  g is a static variable.
// Static names are seeded as live at function exit; Copy is not a dead store.
TEST_F(OptimizerTest, DeadStoreStaticSurvives)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *cp    = make_copy(make_const_int(3), make_var("g"));
    Tac_Instruction *ret   = make_return(make_const_int(0));
    entry->next = cp;
    cp->next    = ret;

    const Tac_TopLevel *tl = make_static_tl("g");

    OptFlags flags         = opt_flags_default();

    flags.copy_propagation = false;
    Tac_Instruction *result = optimize_function(entry, flags, tl);

    EXPECT_EQ(capture_instructions(result),
        "- instruction:\n"
        "  kind: label\n"
        "  name: fn\n"
        "- instruction:\n"
        "  kind: copy\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 3\n"
        "  dst:\n"
        "    kind: var\n"
        "    name: g\n"
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 0\n");
}

// Label("fn") → Binary(ADD, a, b, t.0) → Unary(NEGATE, t.0, t.1) → Return(ConstInt(0))
// t.1 is dead → Unary removed; that leaves t.0 dead → Binary removed in the same pass.
TEST_F(OptimizerTest, DeadStoreCascade)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *bin   = make_binary(TAC_BINARY_ADD,
                                         make_var("a"), make_var("b"), make_var("t.0"));
    Tac_Instruction *un    = make_unary(TAC_UNARY_NEGATE, make_var("t.0"), make_var("t.1"));
    Tac_Instruction *ret   = make_return(make_const_int(0));
    entry->next = bin;
    bin->next   = un;
    un->next    = ret;

    OptFlags flags         = opt_flags_default();

    flags.copy_propagation = false;
    Tac_Instruction *result = optimize_function(entry, flags, nullptr);

    EXPECT_EQ(capture_instructions(result),
        "- instruction:\n"
        "  kind: label\n"
        "  name: fn\n"
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 0\n");
}

// Block 0: Label("fn") → Copy(3, t.0) → Jump("use")
// Block 1: Label("use") → Return(t.0)
// t.0 is live in the successor block; Copy in block 0 must not be removed.
TEST_F(OptimizerTest, DeadStoreCrossBlockNotRemoved)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *cp    = make_copy(make_const_int(3), make_var("t.0"));
    Tac_Instruction *jmp   = make_jump("use");
    Tac_Instruction *lbl   = make_label("use");
    Tac_Instruction *ret   = make_return(make_var("t.0"));
    entry->next = cp;
    cp->next    = jmp;
    jmp->next   = lbl;
    lbl->next   = ret;

    OptFlags flags         = opt_flags_default();

    flags.copy_propagation = false;
    Tac_Instruction *result = optimize_function(entry, flags, nullptr);

    // unreachable_elim folds the trivial jump+label into a straight sequence;
    // the important check is that Copy(3, t.0) was NOT removed.
    EXPECT_EQ(capture_instructions(result),
        "- instruction:\n"
        "  kind: label\n"
        "  name: fn\n"
        "- instruction:\n"
        "  kind: copy\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 3\n"
        "  dst:\n"
        "    kind: var\n"
        "    name: t.0\n"
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: var\n"
        "    name: t.0\n");
}
