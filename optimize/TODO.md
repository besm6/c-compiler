# TAC Optimizer — Work Plan

Reference: [docs/TAC_Optimization.md](../docs/TAC_Optimization.md)

Create unit tests which compile C code to TAC, then apply `optimize_function()`
and check YAML result:

1. End-to-end: a C function `int f(void) { return 6/2; }` lowers to `Return(3)`
   with all passes enabled (constant folding folds division; copy prop removes the
   temp; dead store elimination removes the store to the temp).
2. `int f(void) { return 6/2; }` → single `Return(ConstInt(3))`.
3. `int g(int x) { int t = x; return t; }` → `Return(Var("x"))` (copy propagated,
   temp store eliminated).
4. `int h(int x) { if (0) return 1; return 2; }` → single `Return(ConstInt(2))`
   (dead branch removed).

The above tests are just a beginning. Create a comprehensive set of similar tests
with good coverage.

## Done

Tests 1–3 and a comprehensive set (12 active) are implemented in
`optimize/pipeline_tests.cpp` (`PipelineTest.*`).

Test 4 (`DeadBranchIfZero` / `DeadBranchIfOne`) is disabled: `lower` itself crashes
with "Damaged memory list in xfree()" on `if`-statement inputs, indicating a
pre-existing memory bug that must be fixed before these tests can be enabled.
