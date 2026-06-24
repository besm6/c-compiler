# TODO

Work plan for the BESM-6 backend, in no partucular order.

| #  | Task | Description |
|----|------|-------------|
| 1 | Assembler for Unix | Port assembler from Elbrous-B |
| 2 | Linker for Unix | Port linker from Elbrous-B |
| 3 | Switch jump-table optimization | For dense case ranges, replace the linear compare chain with an index-scaled `utc` / `uj` dispatch through a table of `,log, label` words. |
| 4 | Register allocator (word-pointer promotion) | Promote frequently-dereferenced **word** pointers into the callee-saved, currently-unused index registers r1–r5. Since `EA = M[reg]+offset+C`, a pointer in `rN` dereferences in a single index-modified `xta off(rN)`/`atx off(rN)` — replacing the 2–3-instr `wtc/xta` (or `utc/wtc/xta` for globals) sequence, folding constant member/element offsets for free, and letting consecutive derefs reuse `rN` (C resets after one instruction, so it can't be shared). Only pointers benefit (BESM-6 has no reg-reg ops, so scalars must pass through A/memory regardless). New backend pass `regalloc.c`, run in `codegen_function` after `frame_build` and before instruction selection: pick ≤5 profitable word-pointer locals by deref/use count (weight loop bodies), excluding fat `char*`/`void*` (byte-offset-encoded — keep them on the WTC/byte path) and address-taken pointers; record on the `Frame` so a promoted name resolves to *ixreg-resident* instead of an auto slot. Plumb a storage-kind through the value helpers (`emit_store_a`/`emit_xta_val`): def → `ati rN`, plain-value use → `ita rN`, deref sites (`LOAD`/`STORE`/`ADD_PTR`) emit the index-modified form. Save/restore each used `rN` in the prologue/epilogue (`c/save` preserves only r6/r7/r13; r1–r5 being callee-saved means a promoted pointer also survives calls with no spill — that fixed save cost is the profitability threshold). Adjust the post-peephole frame shrink for freed auto slots / added save slots. Follow-ups: redundant `ita`/`ati` peephole, `main` skips the save, r8–r12 with spill-around-call. |
| 7 | CSE (common subexpression elimination) | Machine-independent TAC pass. Reuse the forward-dataflow framework (`optimize/copy_prop.c`), CFG (`optimize/cfg.c`), and alias pre-analysis (`optimize/alias.c`); add expression hashing/value numbering (handle commutativity; invalidate on Load/Store/FunCall). Register in the `optimize_function` fixed-point loop (`optimize/optimize.c`) plus an `OptFlags`/CLI flag. |
| 8 | LICM (loop-invariant code motion) | Machine-independent TAC pass. Prerequisite: a new dominator-tree and natural-loop analysis (none exists; the loop info from `semantic/label_loops.c` is lost by TAC time). Then hoist loop-invariant computations into a preheader, reusing the liveness framework from `optimize/dead_store.c`. |
| 9 | Generalized strength reduction | Today only power-of-two mul/div is reduced, and only in the backend (`backend/besm6/instr.c`). Add TAC-level reduction for non-power-of-two constants (`a*3 → (a<<1)+a`, magic-number division) so all targets benefit; extend `constant_fold` pattern matching. Needs a per-target cost model. |
| 10 | Function inlining | Machine-independent, interprocedural. Build a call graph over `TAC_INSTRUCTION_FUN_CALL` (none exists; TAC is intraprocedural), add a size/recursion heuristic, substitute bodies (rename `%`-locals, map params→args, return→assign+jump), and run before the fixed-point loop so the existing passes clean up the inlined code. |

## DISABLED book-test triage

The "Writing a C Compiler" chapter tests (imported for an x86-64 / IEEE-754 target) carry
**128 real `DISABLED_` test cases** across the parser, semantic, optimizer, and BESM-6
backend modules. The tasks below adapt each test so it is sensible for the BESM-6 target and
re-enable it, removing only tests with no BESM-6 analogue. One task per adaptation category;
all are tracked here regardless of module. Locate the named tests with
`grep -n DISABLED_ <file>`. Principles: (1) drop `static` on a global where it only blocks
the test; (2) replace literals that make no sense on BESM-6; (3) implement a missing libc
function; (4) remove NaN/inf parts; (5) shorten names that collide in 8 chars; (6) for
no-analogue tests, adapt where possible and remove the rest.

**Stale headers (no action needed):** six files mention `DISABLED_` only in comments and
have **zero** real disabled tests — their leftover headers can be tidied opportunistically:
`semantic/chapter16_tests.cpp`, `semantic/chapter17_tests.cpp`,
`backend/besm6/chapter11_tests.cpp`, `backend/besm6/chapter13_tests.cpp`,
`backend/besm6/chapter14_tests.cpp`, `backend/besm6/chapter19_tests.cpp` (the last four were
re-enabled when block-scope statics landed).

### Backend BESM-6 — dynamic allocation

| #  | Task | Description |
|----|------|-------------|
| 23 | Wire up the heap & re-enable dynamic-allocation tests | `malloc`/`calloc`/`realloc`/`free` already link into `libc.bin` but the run harness never calls `heap_setup`, so allocations fail. Initialize a heap region at program start, then re-enable ch17 VoidPointerSimple, ArrayOfPointersToVoid, CommonPointerType, ConversionByAssignment, VoidPointerExplicitCast, SizeofExpressions, PassAllocedMemory. For ch17 MemoryManagementFunctions, implement `aligned_alloc` or drop that sub-call. Also unblocks the ch18 malloc/calloc tests: ScalarMemberAccessArrow, ScalarMemberAccessLinkedList, ScalarMemberAccessNestedStruct, StructCopyWithArrowOperator, AccessRetvalMembers, OpaqueStruct, ReturnStructPointer, IncompleteStructs, IncompleteUnionTypes, MemberOffsets (with #49). |

### Optimizer (`optimize/`)

| #  | Task | Description |
|----|------|-------------|
| 39 | Fix NaN constant-fold infinite loop | Folding `0.0/0.0` never converges in the fixed-point loop. Re-enables ch19_tests1 CastNanNotExecuted, FoldNan, ReturnNan and ch19_tests3 RedundantNanCopy (4). |
| 40 | Adapt static-local name-collision DSE test | A static local `arr` collides with `main`'s `arr` under the no-shadowing / static-naming scheme. Rename to re-enable ch19_tests4 DSE_AllTypes_DontElim_RecognizeAllUses (1). |

### Backend BESM-6 — codegen & libc

| #  | Task | Description |
|----|------|-------------|
| 41 | libc string/memory routines | Implement `strcmp`, `memcmp`, `memcpy`, `memset`, `puts`, `putchar` in `libc.bin`. Unblocks the large ch18 cohort blocked only on these (often paired with block-scope statics, now supported): StructCopyCopyStruct, StructCopyThroughPointer, StructCopyStackClobber, ParametersStackClobber, ParamsAndReturnsStackClobber, ClassifyParams, ParamCallingConventions, ReturnCallingConventions, AutoStructInitializers, NestedAutoStructInitializers, NestedStaticStructInitializers, StaticStructInitializers, ScalarMemberAccessStaticStructs (~13+, several also need #23/#47/#48). |
| 42 | Packed char member at non-zero byte offset (codegen bug) | A char or char-array member at a non-zero byte offset reads wrong through a struct/struct-pointer. Re-enables ch18 GlobalStruct, ArrayOfStructs, ParamStructPointer; also StructSizes, RetvalStructSizes (packed sub-word layout, via memcmp from #41) (5). |
| 43 | Sub-word char-array row pointer / multi-dim char arrays | Indexing a row of a packed char array yields a pointer into the middle of a word; the pointer model only supports word-aligned word pointers. Re-enables ch16 LiteralsAndCompoundInitializers, TransferByEightbyte (2). |
| 44 | char-signedness in static initializers | Plain `char` signedness mismatch in static char data. Re-enables ch16 StaticInitializers (1). |
| 45 | Discarded multi-word (sret) struct return | A discarded multi-word struct return value is mishandled. Re-enables ch18 IgnoreRetval (1). |
| 46 | gen_lval for function-call (temporary) results | `&f().arr[i]` — `gen_lval` has no case for a function-call (temporary) result. Re-enables ch18 TemporaryLifetime, UnionTempLifetime (2). |
| 47 | Union member access / punning under BESM-6 integer representation | Reading an integer back through a char/other union member yields a BESM-6-specific value (41-bit + tag bits), not the x86 result. Adapt expected values or restrict to representable cases. Re-enables ch18 UnionInitAndMemberAccess, UnionsInConditionals, NestedUnionAccess, StaticUnionAccess, ClassifyUnions, UnionInits, UnionRetvals, StaticUnionInits, UnionNamespace, MemberComparisons, ParamPassing, CopyThruPointer, CopyNonScalarMembers (~13, several also need #41/#23). |
| 48 | Adapt 64-bit-constant struct-member tests to 41-bit range | Constants exceed the BESM-6 41-bit integer range. Adapt the literals: ch18 BitwiseOpsStructMembers, CompoundAssignStructMembers, IncrStructMembers (overlaps #41 for the calling-convention tests) (3). |
| 49 | word/byte pointer-punning arithmetic | Pointer-to-integer byte-address arithmetic / word↔byte pointer comparison. Re-enables ch18 MemberOffsets (also needs #23), CompareUnionPointers (also #47) (2). |
| 50 | REMOVE x86-only ABI / page-boundary tests | No BESM-6 analogue (hand-written x86 `.s` helpers, RAX return-pointer ABI). Delete: ch10 PushArgOnPageBoundary; ch18 PassArgsOnPageBoundary, ReturnBigStructOnPageBoundary, ReturnPointerInRax, ReturnSpaceOverlap, ReturnStructOnPageBoundary (6). |
| 51 | REMOVE/adapt no-analogue overflow & oversized tests | Depend on x86 widths/core size. Adapt or delete: ch12 ArithmeticWraparound (wraps at 2^64), Logical (uses 2^60); ch17 SizeofExtern (12M-word array exceeds BESM-6 core) (3). |
| 52 | REMOVE backend tag-shadowing tests (no-shadowing design) | Require nested tag shadowing or a parameter shadowing a file-scope static — impossible under the permanent no-shadowing rule. Adapt where possible, otherwise delete: ch18 ResolveTags, StructDeclInSwitchStatement, DeclShadowsDecl, StructShadowsUnion, UnionShadowsStruct, Namespaces, LabelTagMemberNamespace, RedeclareUnion, ScalarMemberAccessDot (9). |
| 53 | Re-test block-scope-static-only tests | These were disabled for "no block-scope static storage", which is now supported; confirm they pass (or surface the residual blocker) and re-enable. ch18 StaticVsAuto, StructCopyWithDotOperator, StructCopyWithArrowOperator (the last also needs the heap, #23) (3). |

**Cross-references (no new task):** besm6 ch15 EquivalentDeclarators re-enables once the
existing **task #19** (tentative/extern-after-definition clobber) lands. The ch18
malloc/calloc tests are unblocked by **task #23** above.
