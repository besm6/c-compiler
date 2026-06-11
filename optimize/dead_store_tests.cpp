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

// Label("fn") → Copy(5, g) → FunCall("bar") → Return(ConstInt(0));  g is a static variable.
// At FunCall, static names are re-livened (callee may read them); Copy(5, g) is not a dead store.
TEST_F(OptimizerTest, DeadStoreStaticBeforeFunCallSurvives)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *cp    = make_copy(make_const_int(5), make_var("g"));
    Tac_Instruction *call  = make_fun_call("bar");
    Tac_Instruction *ret   = make_return(make_const_int(0));
    entry->next = cp;
    cp->next    = call;
    call->next  = ret;

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
        "      value: 5\n"
        "  dst:\n"
        "    kind: var\n"
        "    name: g\n"
        "- instruction:\n"
        "  kind: fun_call\n"
        "  fun_name: bar\n"
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

// Label("fn") → Unary(NEGATE, a, t.0) → Return(ConstInt(0))
// t.0 is never used; Unary is removable → removed.
TEST_F(OptimizerTest, DeadStoreUnaryRemoved)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *un    = make_unary(TAC_UNARY_NEGATE, make_var("a"), make_var("t.0"));
    Tac_Instruction *ret   = make_return(make_const_int(0));
    entry->next = un;
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

// Label("fn") → SignExtend(x, t.0) → Return(ConstInt(0))
// t.0 is never used; conversion instructions are removable → removed.
TEST_F(OptimizerTest, DeadStoreConversionRemoved)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *conv  = make_conversion(TAC_INSTRUCTION_SIGN_EXTEND,
                                             make_var("x"), make_var("t.0"));
    Tac_Instruction *ret   = make_return(make_const_int(0));
    entry->next = conv;
    conv->next  = ret;

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

// Label("fn") → SignExtend(x, t.0) → Return(t.0)
// t.0 is live at Return → SignExtend survives.
TEST_F(OptimizerTest, DeadStoreConversionSurvives)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *conv  = make_conversion(TAC_INSTRUCTION_SIGN_EXTEND,
                                             make_var("x"), make_var("t.0"));
    Tac_Instruction *ret   = make_return(make_var("t.0"));
    entry->next = conv;
    conv->next  = ret;

    OptFlags flags         = opt_flags_default();
    flags.copy_propagation = false;
    Tac_Instruction *result = optimize_function(entry, flags, nullptr);

    EXPECT_EQ(capture_instructions(result),
        "- instruction:\n"
        "  kind: label\n"
        "  name: fn\n"
        "- instruction:\n"
        "  kind: sign_extend\n"
        "  src:\n"
        "    kind: var\n"
        "    name: x\n"
        "  dst:\n"
        "    kind: var\n"
        "    name: t.0\n"
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: var\n"
        "    name: t.0\n");
}

// Label("fn") → Load(ptr, t.0) → Return(ConstInt(0))
// t.0 is never used; LOAD is removable → removed (ptr never added to live).
TEST_F(OptimizerTest, DeadStoreLoadRemoved)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *load  = make_load(make_var("ptr"), make_var("t.0"));
    Tac_Instruction *ret   = make_return(make_const_int(0));
    entry->next = load;
    load->next  = ret;

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

// Label("fn") → Load(ptr, t.0) → Return(t.0)
// t.0 is live at Return → Load survives.
TEST_F(OptimizerTest, DeadStoreLoadSurvives)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *load  = make_load(make_var("ptr"), make_var("t.0"));
    Tac_Instruction *ret   = make_return(make_var("t.0"));
    entry->next = load;
    load->next  = ret;

    OptFlags flags         = opt_flags_default();
    flags.copy_propagation = false;
    Tac_Instruction *result = optimize_function(entry, flags, nullptr);

    EXPECT_EQ(capture_instructions(result),
        "- instruction:\n"
        "  kind: label\n"
        "  name: fn\n"
        "- instruction:\n"
        "  kind: load\n"
        "  src_ptr:\n"
        "    kind: var\n"
        "    name: ptr\n"
        "  dst:\n"
        "    kind: var\n"
        "    name: t.0\n"
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: var\n"
        "    name: t.0\n");
}

// Label("fn") → GetAddress(x, ptr) → Return(ConstInt(0))
// ptr (the dst) is dead; GET_ADDRESS is removable → removed.
TEST_F(OptimizerTest, DeadStoreGetAddressRemoved)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *ga    = make_get_address(make_var("x"), make_var("ptr"));
    Tac_Instruction *ret   = make_return(make_const_int(0));
    entry->next = ga;
    ga->next    = ret;

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

// Label("fn") → CopyToOffset(val, "s", 0) → Return(ConstInt(0))
// COPY_TO_OFFSET is a memory mutation and is not in is_removable → always survives.
TEST_F(OptimizerTest, DeadStoreCopyToOffsetNotRemoved)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *cto   = make_copy_to_offset(make_var("val"), "s", 0);
    Tac_Instruction *ret   = make_return(make_const_int(0));
    entry->next = cto;
    cto->next   = ret;

    OptFlags flags         = opt_flags_default();
    flags.copy_propagation = false;
    Tac_Instruction *result = optimize_function(entry, flags, nullptr);

    EXPECT_EQ(capture_instructions(result),
        "- instruction:\n"
        "  kind: label\n"
        "  name: fn\n"
        "- instruction:\n"
        "  kind: copy_to_offset\n"
        "  src:\n"
        "    kind: var\n"
        "    name: val\n"
        "  dst: s\n"
        "  offset: 0\n"
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 0\n");
}

// Label("fn") → GetAddress(x, ptr) → Copy(5, x) → Return(ConstInt(0))
// Alias analysis marks x as address-taken; x is seeded live at function exit.
// Copy(5, x) survives because x IS live; GetAddress is removed because ptr is dead.
TEST_F(OptimizerTest, DeadStoreAddressTakenSurvives)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *ga    = make_get_address(make_var("x"), make_var("ptr"));
    Tac_Instruction *cp    = make_copy(make_const_int(5), make_var("x"));
    Tac_Instruction *ret   = make_return(make_const_int(0));
    entry->next = ga;
    ga->next    = cp;
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
        "      value: 5\n"
        "  dst:\n"
        "    kind: var\n"
        "    name: x\n"
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 0\n");
}

// Label("fn") → Copy(1, t.0) → Copy(2, t.0) → Return(t.0)
// Two consecutive assignments to t.0; only the last one is live at Return.
// Backward pass: Copy(2, t.0) survives; Copy(1, t.0) becomes dead → removed.
TEST_F(OptimizerTest, DeadStoreDoubleOverwrite)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *cp1   = make_copy(make_const_int(1), make_var("t.0"));
    Tac_Instruction *cp2   = make_copy(make_const_int(2), make_var("t.0"));
    Tac_Instruction *ret   = make_return(make_var("t.0"));
    entry->next = cp1;
    cp1->next   = cp2;
    cp2->next   = ret;

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

// Block 0: Label("fn") → Copy(5, t.0) → JIZ(cond, "B")
// Block 1: Return(ConstInt(1))
// Block 2: Label("B") → Return(ConstInt(0))
// t.0 is dead in both successors; liveness analysis computes out[block0]=∅ → Copy removed.
TEST_F(OptimizerTest, DeadStoreDeadInAllBranches)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *cp    = make_copy(make_const_int(5), make_var("t.0"));
    Tac_Instruction *jiz   = make_jump_if_zero(make_var("cond"), "B");
    Tac_Instruction *ret1  = make_return(make_const_int(1));
    Tac_Instruction *lbl   = make_label("B");
    Tac_Instruction *ret0  = make_return(make_const_int(0));
    entry->next = cp;
    cp->next    = jiz;
    jiz->next   = ret1;
    ret1->next  = lbl;
    lbl->next   = ret0;

    OptFlags flags         = opt_flags_default();
    flags.copy_propagation = false;
    Tac_Instruction *result = optimize_function(entry, flags, nullptr);

    EXPECT_EQ(capture_instructions(result),
        "- instruction:\n"
        "  kind: label\n"
        "  name: fn\n"
        "- instruction:\n"
        "  kind: jump_if_zero\n"
        "  condition:\n"
        "    kind: var\n"
        "    name: cond\n"
        "  target: B\n"
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 1\n"
        "- instruction:\n"
        "  kind: label\n"
        "  name: B\n"
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

// ---------------------------------------------------------------------------
// Volatile accesses must never be removed, even when their destination is dead.
// ---------------------------------------------------------------------------

// Label("fn") → Load(p, t.0) [volatile] → Return(0)
// t.0 is never read. A plain Load would be removed (it is removable), but a
// volatile load has an observable side effect and must survive.
TEST_F(OptimizerTest, DeadStoreVolatileLoadSurvives)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *ld    = as_volatile(make_load(make_var("p"), make_var("t.0")));
    Tac_Instruction *ret   = make_return(make_const_int(0));
    entry->next = ld;
    ld->next    = ret;

    OptFlags flags         = opt_flags_default();
    flags.copy_propagation = false;
    Tac_Instruction *result = optimize_function(entry, flags, nullptr);

    EXPECT_EQ(capture_instructions(result),
        "- instruction:\n"
        "  kind: label\n"
        "  name: fn\n"
        "- instruction:\n"
        "  kind: load\n"
        "  volatile: true\n"
        "  src_ptr:\n"
        "    kind: var\n"
        "    name: p\n"
        "  dst:\n"
        "    kind: var\n"
        "    name: t.0\n"
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 0\n");
}

// Control for the test above: the same Load WITHOUT the volatile flag is a dead
// store (t.0 never read) and is removed.
TEST_F(OptimizerTest, DeadStoreNonVolatileLoadRemoved)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *ld    = make_load(make_var("p"), make_var("t.0"));
    Tac_Instruction *ret   = make_return(make_const_int(0));
    entry->next = ld;
    ld->next    = ret;

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

// Label("fn") → Copy(5, x) [volatile] → Return(0)
// x is never read, so a plain Copy would be a dead store and removed. A volatile
// copy models a write to a volatile object and must survive.
TEST_F(OptimizerTest, DeadStoreVolatileCopySurvives)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *cp    = as_volatile(make_copy(make_const_int(5), make_var("x")));
    Tac_Instruction *ret   = make_return(make_const_int(0));
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
        "  volatile: true\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 5\n"
        "  dst:\n"
        "    kind: var\n"
        "    name: x\n"
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 0\n");
}

// Two volatile writes to the same object: neither may be coalesced away. Without
// the volatile flag, Copy(1, x) would be a dead store (overwritten by Copy(2, x)).
TEST_F(OptimizerTest, DeadStoreVolatileDoubleWriteBothSurvive)
{
    Tac_Instruction *entry = make_label("fn");
    Tac_Instruction *cp1   = as_volatile(make_copy(make_const_int(1), make_var("x")));
    Tac_Instruction *cp2   = as_volatile(make_copy(make_const_int(2), make_var("x")));
    Tac_Instruction *ret   = make_return(make_const_int(0));
    entry->next = cp1;
    cp1->next   = cp2;
    cp2->next   = ret;

    OptFlags flags         = opt_flags_default();
    flags.copy_propagation = false;
    Tac_Instruction *result = optimize_function(entry, flags, nullptr);

    EXPECT_EQ(capture_instructions(result),
        "- instruction:\n"
        "  kind: label\n"
        "  name: fn\n"
        "- instruction:\n"
        "  kind: copy\n"
        "  volatile: true\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 1\n"
        "  dst:\n"
        "    kind: var\n"
        "    name: x\n"
        "- instruction:\n"
        "  kind: copy\n"
        "  volatile: true\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 2\n"
        "  dst:\n"
        "    kind: var\n"
        "    name: x\n"
        "- instruction:\n"
        "  kind: return\n"
        "  src:\n"
        "    kind: constant\n"
        "    const:\n"
        "      kind: int\n"
        "      value: 0\n");
}
