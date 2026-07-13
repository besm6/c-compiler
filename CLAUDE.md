# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Test

```sh
make              # configure + build the compiler & runtime (RelWithDebInfo) into ./build/
make test         # build all unit tests (incl. textbook chapter tests), do not run
make run          # build + run all unit tests via ctest
make install      # build + install the compiler & runtime (see below)
make debug        # build with Debug flags
make clean        # remove ./build/
```

**`make install`** builds everything, then installs the artifacts via `cmake --install`
to `~/.local` if that directory exists, otherwise `/usr/local`: `parse` → `bin/b6parse`,
`lower` → `bin/b6lower`, `genbesm` → `bin/b6codegen`, `libc.bin` →
`share/besm6/lib/libc.bin`, and the target C11 standard headers
(`libc/besm6/include/*.h`) → `share/besm6/include/`.
The prefix is chosen in the Makefile at install time and passed as `cmake --install build
--prefix`; the driver binaries are renamed (`b6` prefix) only at install time via
`install(PROGRAMS … RENAME)`, so the in-tree build outputs (`build/parse`, `build/lower`,
`build/backend/genbesm`) keep their original names.

**Tests are built by the default build.** A plain `make`/`make all` builds the compiler and
runtime (`parse`, `lower`, `genbesm`, and `libc.bin`) *and* every per-module test executable.
`make test` also builds `all` (so the compiler, runtime, and all nine test executables) but
does not run them; `make run` depends on `make test` and then runs `ctest --test-dir build`
over everything.

**The "Writing a C Compiler" chapter tests are compiled into the regular test binaries.**
The chapter sources (`*/test/chapter*_tests.cpp`, `backend/besm6/test/chapter*_tests.cpp`) are listed
in the same `add_executable(<module>-tests …)` as the unit tests, so e.g. `parser-tests`
and `besm-tests` contain both. There are no separate `*-book-tests` executables and no ctest
`book` label. `fatal_error()` (the libraries call it, but it is defined in the test
executable) is defined exactly once per binary in a regular unit-test source
(`parser/test/simple_tests.cpp`, `semantic/test/typecheck_tests.cpp`, `optimize/test/pipeline_tests.cpp`,
`backend/besm6/test/codegen_tests.cpp`); the chapter sources do **not** redefine it. The scanner
needs none — it uses its own `lex_error()`/`exit()`.

Run a single test binary directly (semantic and translator tests live in subdirectories):
```sh
./build/ast/ast-tests
./build/parser-tests
./build/tac/tac-tests
./build/semantic/semantic-tests
./build/translator/translate-tests
./build/backend/besm6/besm-tests
./build/optimize/optimizer-tests
```

Run a specific GoogleTest case:
```sh
./build/parser-tests --gtest_filter="*ExprTest*"
./build/semantic/semantic-tests --gtest_filter="PipelineTest.*"
```

**Run tests from the build directory** to avoid polluting the source tree with temporary
files that GoogleTest writes during test discovery:
```sh
cd build/semantic && ./semantic-tests
ctest --test-dir build -R "Typecheck|Pipeline"
```

**Runtime library (`libc.bin`).** The runtime routines cover a substantial hosted libc
subset: I/O (`printf`/`sprintf`/`snprintf` over the shared `doprnt` pointer-walk engine,
`puts`, `putchar`, `getch`, `putch`, `putbyte`, `flush`), all of `<string.h>`
(`strlen`/`strcpy`/`strncpy`/`strcat`/`strncat`/`strcmp`/`strncmp`/`strchr`/`strrchr`/`strstr`/`strtok`/`strerror`
and the `mem*` family `memcpy`/`memmove`/`memset`/`memcmp`/`memchr`), the dynamic allocator
(`malloc`/`calloc`/`realloc`/`free`), `atoi`, and the math helpers
(`fabs`/`fmin`/`fmax`/`fma`/`modf`, plus the frameless `frexp`/`ldexp`). The C sources are
split by Dubna dependency: the **portable** routines (everything above the I/O leaves — the
format engine, allocator, string/mem/math) live in `libc/besm6/*.c` and are shared by every
BESM-6 assembler backend, while only the three Dubna-monitor **leaves** — `putbyte` (owns the
KOI7 stdout buffer), `flush` (`b/tout`), `getch` (`moncard_`/`monread_`) — stay in
`libc/besm6/madlen/*.c` alongside the hand-written Madlen helpers; the sibling
`libc/besm6/unix` target reimplements those three over Unix v7 syscalls (`write.s`/`read.s`
extracode leaves, plus a `crt0.s` startup that calls `int main`) and archives everything with
`b6ar`/`b6ranlib` into a `b6as` `libc.a` (+ standalone `crt0.o`) for the `b6as`/`b6ld`/`b6sim`
path. All `.c` are compiled by our own toolchain (`parse → lower → genbesm --madlen` → `.madlen`) and
assembled with the Madlen helpers (`b_*.madlen`, `b_tout`, `exit`, `frexp`, `ldexp`) into
`libc.bin` — see `libc/besm6/CMakeLists.txt` (`LIBC_C_PORTABLE` / `LIBC_C_DUBNA` /
`LIBC_MADLEN`) and `libc/besm6/unix/CMakeLists.txt` for the Unix `libc.a`/`crt0.o`. The
original B sources have been removed; the runtime is now C plus Madlen only. Runtime-helper
names use `$` as their special separator (e.g. `b$tout`, `b$ret`, `b$save`) — this is the
**canonical IR form** carried unchanged from the scanner through TAC into the `Besm_Module`;
each backend renders it per-dialect: the Madlen emitter lowers `$`→`/` (so `b$tout` → `b/tout`,
matching the `.madlen` helper symbols), while the Unix (`b6as`) emitter keeps `$` (`b6as`
accepts `$` in names). An `extern T name[]` array emits no TAC top-level; a later
reference decays the array to its address via `GET_ADDRESS`, which self-declares the
external name (SUBP), so cross-module array indexing needs no special array-ness record.

**The BESM-6 tests (`besm-tests`) can be run from any directory.** At startup every test
binary `chdir()`s into its own build directory (a GoogleTest global environment compiled in
via `libutil/test/test_chdir.cpp` and the `test_chdir_to_bindir()` CMake helper, keyed off a
per-target `TEST_BINARY_DIR` define), so `besm-tests` always lands in `build/backend/besm6`
— the directory that holds the assembled runtime library `libc.bin`, which the Dubna
simulator job links from the current working directory. (Before this, running from elsewhere
used a stale or missing library and the run tests reported `ERROR`.) The same chdir keeps
manual runs of every other test binary from littering the source tree with their scratch
files (`<TestName>.c`, `.ast`, `.dub`, `.lst`); under `ctest`/`make run` it is a no-op
because ctest already runs each binary in its build directory. Either of these works:
```sh
cd build/backend/besm6 && ./besm-tests
./build/backend/besm6/besm-tests          # also fine — it chdir()s itself
```

**Debugging a failing run with Dubna instruction tracing.** A `CompileAndRun` test leaves
its job file `<TestName>.dub` in `build/backend/besm6`. Re-run it under the simulator with
full instruction/register tracing via `-d c`:

```sh
cd build/backend/besm6
dubna -d c RemainderRun.dub > RemainderRun.trace
```

The trace lists every instruction with the effective address, the memory word read/written,
and the resulting `ACC` / `RAU` (mode register R) / index-register values, e.g.:

```
01065 R: 00 037 0000 ntr
      RAU = 00
01066 L: 16 015 0014 aox 14(16)
      Memory Read [01101] = 6400 0000 0000 0000
      ACC = 6400 0000 0000 0002
      RAU = 04
```

Search the trace for a routine label (e.g. `B/MOD`, `B/DIV`) to follow a runtime helper and
inspect the accumulator/exponent at each step — the fastest way to localize a wrong
mode-bit, exponent, or addressing error in hand-written Madlen. To build a focused job by
hand: assemble a `.mad` with `genbesm --madlen` (Unix is the default dialect, so the flag is
required here), wrap it with the `*name/*disc/*file:libc,40/*assem
… *library:40/*execute/*end file` boilerplate (see `codegen_test.h` `CompileAndRun`), and
run `dubna [-d c] job.dub`.

**Running a program on the Unix (`b6as`) path.** Alongside the Madlen/`dubna` `CompileAndRun`,
`codegen_test.h` provides `CompileAndRunUnix` — the Unix-dialect run harness (`besm-tests`
`CodegenTest.UnixRun*`, `test/unix_run_tests.cpp`). It compiles in-process via `CompileToUnix`
(`genbesm --unix`), assembles with `b6as`, links with `b6ld` — **`crt0.o` first**, then the
program object, then `libc.a` (`b6ld` takes the entry point from the first object's first text
word) — and runs the linked `b.out` under the `b6sim` simulator via `RunExternalProgram`,
returning the captured stdout. `b6sim` traps the Unix v7 syscalls onto the host, so a program's
`write(1,…)` lands straight on stdout — no `.lst`/`≠` scraping (unlike the `dubna` path). The
Unix `crt0` calls `int main(void)` (not the Madlen libc's `void program()`), so these test
programs define `main()`. All external tools (`b6as`/`b6ld`/`b6sim`, in the sibling `v7besm`
tree) resolve by bare name on `PATH`; `crt0.o` and `libc.a` are staged next to `besm-tests` in
`build/backend/besm6/`. Tests using it guard with `SKIP_IF_NO_UNIX_RUN_TOOLS()` so `make run`
stays green where the toolchain is absent. To reproduce by hand:
`b6sim build/backend/besm6/<TestName>.b6` (add `-d irm` / `--trace=FILE` for tracing).

**Running a program on the Bemsh (`*bemsh`) path.** `codegen_test.h` also provides
`CompileAndRunBemsh` — the Bemsh-dialect `dubna` run harness (`besm-tests` `CodegenTest.Bemsh*Run`,
`test/bemsh_run_tests.cpp`). It compiles in-process via `CompileToBemsh` (`genbesm --bemsh`), wraps
the output in a `*bemsh` job linking the Bemsh runtime `libbem.bin` on library 40 (model:
`backend/besm6/tmp/bemsh.dub`, entry `*main progra`), runs `dubna`, and scrapes the same `.lst`
`≠`/`----` framing as the Madlen path — both share the `ExtractDubnaOutput` helper. genbesm
`--bemsh` wraps each module in its own `ввд$$$…кнц$$$` Macro-Bemsh deck (the translator processes
one module per deck). Unlike Madlen's auto-declaring `,call,`, Bemsh's `пв` needs an explicit
`внешн` per call target (spliced in by `codegen.c` for the Bemsh dialect) and must carry the return
register — `пв name(13)`; code labels are emitted as a labeled `ноп` (not `экв *`, which captures
the wrong half-cell address). `libbem.bin` holds the hand-written helpers (task B4) plus the full
compiled portable C libc (task B5): `printf`/`doprnt`, `sprintf`/`snprintf`, the `str*`/`mem*`
families, the `fabs`/`fmax`/`fmin`/`fma`/`modf` math, `atoi`, and the stdout chain
`putbyte`/`flush`/`putchar`/`putch`/`puts` — the same `LIBC_C_PORTABLE` set as the Madlen `libc.bin`
minus `malloc` (needs the Unix heap map) and `getch` (input, deferred). So these tests use `void
program()` with the same hosted-libc surface as the Madlen path; the printf/string/float run tests
are in `test/bemsh_printf_tests.cpp`. Bringing up that surface surfaced (and fixed) B4 helper bugs —
a missing base-register load (Madlen `,base,` loads the register, Bemsh `УПОТР` only declares it, so
each based helper needs an explicit `уиа _NAME(14)`) and `экв *` code labels (now labeled `ноп`) —
plus emitter literal bugs (Madlen-form `=377`/`=:64` literals → Bemsh `=в'377'`/`=в'6400000000000000'`,
and a type-Е mantissa overflow on 2^40 → octal bit-pattern fallback). To reproduce by hand:
`dubna [-d rime] build/backend/besm6/<TestName>.dub`.

**Target standard headers (`libc/besm6/include/`).** C11 standard-library headers for
programs compiled for the BESM-6 (the freestanding subset is complete; the hosted subset
declares the few implemented libc routines plus future ones — see the dir's `README.md`).
The compiler has no preprocessor, so these are consumed by an external preprocessor first.
Use the C compiler's preprocessor (`cc -E`), not a standalone `cpp`: a traditional `cpp`
(e.g. Apple's `/usr/bin/cpp`) only recognizes a `#` directive in column 1, so indented
`#include` lines silently fail to expand. No `-P` is needed — `parse`'s scanner consumes
`# line` markers and keeping them preserves original line numbers in diagnostics:
`cc -E -nostdinc -Ilibc/besm6/include prog.c | parse -`. The `besm-headers` CTest
(`scripts/check_headers.sh`, run under `make run`) preprocesses and parses every header to
catch syntax errors. The unit-test fixtures preprocess their C snippets automatically via
`libutil/test/test_preprocess.h` (using the CMake `BESM6_CPP`/`BESM6_INCLUDE_DIR` defines), so
tests `#include <stdio.h>` instead of hand-declaring libc routines. `<stdarg.h>` is
functional (word-pointer `va_list`); its runtime behaviour is covered by `stdarg_tests.cpp`.

Static analysis (requires cppcheck):
```sh
ctest --test-dir build -R cppcheck
```

Try the compiler tools:
```sh
./build/parse input.c               # parse → binary AST to input.ast (NOT stdout)
./build/parse input.c --yaml        # parse → YAML AST
./build/parse input.c --dot         # parse → Graphviz DOT
./build/parse -D input.c            # debug: parser trace + AST dump + leak report

# IMPORTANT: parse and lower do NOT write binary output to stdout — they create a new
# file with an .ast / .tac suffix next to the input.  To emit to stdout (e.g. to pipe or
# redirect), pass "-" as the output argument:
./build/parse input.c -          > /tmp/input.ast   # "-" = binary AST to stdout
./build/parse input.c               # → input.ast (default file output)
./build/lower /tmp/input.ast              # → binary TAC to /tmp/input.tac (default target: besm6)
./build/lower --yaml /tmp/input.ast -     # → YAML TAC to stdout ("-" = stdout)
./build/lower -t x86_64 /tmp/input.ast -  # → TAC with x86_64 type sizes/offsets
./build/lower -D /tmp/input.ast           # debug: translator trace

# genbesm defaults to the Unix (b6as) dialect; the output file's extension follows the
# dialect (.s for Unix, .mad for Madlen, .bem for Bemsh) when no output name is given.
./build/backend/genbesm /tmp/input.tac              # → Unix b6as assembly to /tmp/input.s
./build/backend/genbesm --madlen /tmp/input.tac     # → Madlen assembly to /tmp/input.mad
./build/backend/genbesm /tmp/input.tac out.s        # explicit output filename
```

Compiler flags in use: `-Wall -Werror -Wshadow` — all warnings are errors.

## Architecture

This is a multi-platform C11 compiler. The shared frontend emits TAC; machine backends under `backend/` consume TAC and emit target assembly. The pipeline:

```
Source (.c)
  → [parse]      Scanner → Parser → AST (binary/YAML/DOT)
  → [lower]      Typecheck → Translate → Optimize → TAC (binary/YAML/DOT)
  → [genx86]     Frame alloc → Instruction select → GNU AT&T assembly (.s)
  → [genbesm]    Frame alloc → Instruction select → Unix b6as assembly (.s, default)
                                                  → Madlen assembly (.mad, --madlen)
```

**`parse`** (`parser/main.c`): Lexes and parses a C source file, outputs a binary AST stream (via `wio`) to stdout, or `--yaml`/`--dot` for human-readable forms.

**`lower`** (`translator/main.c`): Reads the binary AST, runs semantic analysis and TAC lowering, then runs the TAC optimizer, and outputs TAC. Lowering is complete. The optimizer runs four passes in a fixed-point loop: constant folding, unreachable code elimination, copy propagation, and dead store elimination. Optimizer flags: `--no-unreachable`, `--no-copy-prop`, `--no-dead-store`, `--opt-debug`. The TAC YAML format is documented in [docs/Technical_Reference.md](docs/Technical_Reference.md).

### Compiler phases

| Phase | Location | Status |
|---|---|---|
| Lexer | `scanner/` | Complete |
| Parser | `parser/` | Complete |
| AST | `ast/` | Complete (alloc/free/export/import/yaml/graphviz/clone/compare) |
| Type checking | `semantic/typecheck.c` | Complete |
| Loop labeling | `semantic/label_loops.c` | Complete |
| Const conversion | `semantic/const_convert.c` | Complete |
| AST → TAC lowering | `translator/translate.c`, `expr.c`, `stmt.c` | Complete |
| TAC optimizer | `optimize/` | Complete (const fold, unreachable elim, copy prop, dead store elim) |
| x86_64 code gen | `backend/x86/` | Planned |
| BESM-6 code gen | `backend/besm6/` | Complete (frame alloc, static data, block-scope static locals (captured per function at typecheck via `static_locals_add`, carried on `Tac_TopLevel.function.static_locals`, and emitted by `besm_emit_static_locals` as a module-local labeled datum inside the owning function's own `,name,`/`,end,` module after the code — no SUBP; same-named statics stay distinct by a TU-wide `name$N` suffix on later occurrences (`$`→`/` in Madlen, kept in Unix) so the flat Unix object has no duplicate label; address-initialized pointer static locals (`static char *p = "ABC";`, `static int *q = &g;`) label their first `BESM_DATA_Z00` address word through the dedicated `Besm_Instr.label` field — its `name` already holds the referenced symbol — and a string literal used only by a static-local initializer is emitted as a top-level static constant via `emit_referenced_string_constants` in `translator/translate.c`; multi-dimensional char arrays are a flat byte blob packed 6/word with rows contiguous (no per-row padding): the static path's `char_array_log_items` flattens the init list (NULL-safe for empty-string rows, which round-trip through `.tac` as a NULL `val`), the automatic path's `gen_char_array_string_init` (`translator/stmt.c`) emits per-byte stores with zero-fill for each string row, indexing decays a char-innermost array of any rank to a fat byte pointer (`is_byte_pointer`/`is_fat_pointer` look through array element types) and the subscript/`+`/`++` paths multiply the index by the row's C size so it byte-addresses at ADD_PTR scale 1, and static/extern-local arrays are recorded for the decay too — char data keeps its source (ASCII) encoding while the static path repacks to KOI-7, so tests use uppercase Latin to keep both equal, see [docs/KOI7_Encoding.md](docs/KOI7_Encoding.md)), UTF-8→KOI7, main entry, global variable access, COPY/GET_ADDRESS/LOAD/STORE (GET_ADDRESS of a global emits a lone `14 ,vtm, name`: VTM is a Format-2 instruction like UTC, so its own 15-bit address field takes the relocatable label directly and `M[14] = offset + C` needs no `utc name` ahead of it — a local at a nonzero frame offset still needs `utc reg,off` + `14 ,vtm, 0`, since its index register can only ride in the UTC's register field, VTM's own being the destination; `BESM_SHAPE_IMMR` in both emitters therefore renders `instr->name` when set; word deref via WTC: `wtc` loads the pointer word's address into the C address-modifier register, then a bare `xta`/`atx` reads/writes `mem[C]` — no index register r1; STORE loads the source first since C resets after the next instruction)/BINARY (incl. shifts, unsigned add via b/uadd, unsigned sub via b/usub, multiply via b/mul, unsigned multiply via b/umul, divide via b/div, unsigned divide via b/udiv, remainder via b/mod, unsigned remainder via b/umod, constant strength reduction for power-of-two operands (multiply → `asn` left shift, signed masked back to 41 bits via `aax`; unsigned divide → `asn` right shift; unsigned remainder → `aax` low-bit mask; signed divide/remainder stay on b/div/b/mod), FP add/sub/mul/div inline via a+x/a-x/a*x/a/x with NTR-bracketed normalization, FP comparisons via b/flt/b/fle/b/fgt/b/fge ordering helpers with FP ==/!= reusing b/eq/b/ne)/UNARY negate (int/unsigned/FP)/UNARY complement/UNARY not/FUN_CALL (direct call by label via `,call,`; indirect call through a function-pointer frame slot via `wtc` of the slot then a bare `13 ,vjm, 0` — VJM jumps to offset+C, so C carries the target address; a function name used as a value decays to its label address through GET_ADDRESS, and `(*fp)(…)` strips the function-pointer deref to call through the pointer directly; a direct call to a `_Noreturn` callee is a tail `,uj,` (FUN_CALL_NORETURN) with the callee declared SUBP — unlike `,call,`, a `,uj,` to an undefined name is an assembler error — and a parameterless `_Noreturn` function definition itself drops the `b/save0` prologue and the dead `b/ret` epilogue, keeping only `,ntr, 7` plus `15 ,mtj, 7` when it has autos/temps; the definition's `_Noreturn`-ness rides on the serialized `Tac_TopLevel.function.noret` flag)/RETURN/LABEL/JUMP/JUMP_IF_ZERO/JUMP_IF_NOT_ZERO/integer width conversions (TRUNCATE/ZERO_EXTEND/SIGN_EXTEND)/int↔FP conversions (signed INT_TO_DOUBLE/INT_TO_FLOAT inline via INT-format+normalize, UINT_TO_DOUBLE/UINT_TO_FLOAT via b/utod, DOUBLE_TO_INT/FLOAT_TO_INT via b/dtoi, DOUBLE_TO_UINT/FLOAT_TO_UINT via b/dtou, FLOAT_TO_DOUBLE/DOUBLE_TO_FLOAT copies)/ADD_PTR (word pointer & array index scaling; the base is always a pointer value — the translator decays an array rvalue to its address via GET_ADDRESS before the ADD_PTR, so no array-vs-pointer disambiguation is needed in the backend; char*/void* byte arithmetic via scale=1 — constant ±1 through b/pinc/b/pdec, other byte deltas through b/padd's floored divide by 6)/COPY_TO_OFFSET/COPY_FROM_OFFSET (aggregate member access — word-aligned members, plus packed char members via the COPY_BYTE_TO_OFFSET/COPY_BYTE_FROM_OFFSET kinds: byte extract for reads, b/stb RMW for writes)/ALLOCATE_LOCAL (contiguous multi-word frame slots for local arrays & structs)/fat-pointer char access (char*/void* byte LOAD inline via WTC/XTA/ASX/AAX, byte STORE via b/stb RMW helper, GET_ADDRESS_BYTE of a char sets the fat marker, int*↔char* casts via PTR_TO_CHAR_PTR/CHAR_PTR_TO_PTR; byte access uses the dedicated LOAD_BYTE/STORE_BYTE/GET_ADDRESS_BYTE kinds; char-array indexing & string/array decay to a fat pointer at offset_enc 5 via GET_ADDRESS_DECAY + static FAT_POINTER init; frontend routes char* +/-/+=/-=/++/-- to ADD_PTR scale=1)/PTR_DIFF (char*−char* difference → ptrdiff_t byte count via b/pdiff: decode both fat pointers to absolute byte positions word*6+(5−offset_enc) and subtract) done; peephole pass (`peephole.c`, run in `codegen_function` before emission) with rule #27 redundant-reload elimination — matched on a `Loc` (frame slot / global + word offset / dereference through a named pointer), not a raw `(reg,off)` pair, because a `utc`/`wtc` and the instruction after it form an atomic **C group** whose consumer addresses `mem[C]` and whose `(reg,addr)` fields name no slot; the sweep steps cursor-by-group, so a match splices out setter and consumer together (2 nodes, or 3 for `utc gp`+`wtc`+`xta` through a global pointer) and it is structurally impossible to strand a setter; this catches `g.x = 7; return g.x;` and `*p = x; return *p;`, neither of which TAC copy propagation can reach; no memory-clobber analysis is needed because memory is only ever written from A, so a store cannot falsify "A mirrors L" — see [docs/Peephole_Rewrites.md](docs/Peephole_Rewrites.md) §5.9 — rule #28 dead temp-store elimination — drops an `atx` to a `%`-temporary slot whose value is never re-read before overwrite/block end; restricted to never-aliased temporaries via `frame_slot_is_temp`, with a single-basic-block guard for temporaries live across an edge — and rule #29 NTR mode coalescing — tracks the mode register R (seeded to 7 after `b/save`), deletes any `ntr n` whose operand equals the current R, and drops a dead `ntr` overwritten by a later `ntr` before any R-dependent use, so consecutive FP ops keep R=0 and restore to 7 once at the end, and rule #30 compare→branch fusion — a relational helper's 0/1 result feeding a `JUMP_IF_ZERO`/`JUMP_IF_NOT_ZERO` is consumed directly by the `uza`/`u1a`; needs no dedicated rule, it is the emergent product of #27+#28 given the runtime helpers' logical-ω exit contract — and rule #31 branch/label cleanup — drops a `uj` whose target is the immediately following label, deletes instructions between an unconditional `uj` and the next label as unreachable (which also collapses the duplicate `uj b/ret` the RETURN+epilogue emit; a `stop` is deliberately *not* such a terminator — the halt is resumable from the console, so the code after it is live), and inverts a conditional that only skips an unconditional jump (`uza L`/`uj M`/`L:` ⇒ `u1a M`); these three need list look-ahead/mutation so they run directly in the sweep, not via the `rule_table`); post-peephole frame shrink (`used_auto_words` / `remove_instr` in [codegen.c](codegen.c), run in `codegen_function` after `besm_peephole`) reclaims auto slots the peephole pass left unreferenced — it shrinks the prologue `utm 15` stack extension to the auto words still in use (dropping the `utm` entirely when none remain), reclaiming only `%`+digit temporaries whose slot is no longer referenced while always keeping named locals and aggregates reserved (their address may be taken, and `&x` of a slot-0 local emits `ita 7` with the register number in the address field, which a plain reg==REG_AUTO scan would miss); the nine `<besm6.h>` **intrinsics** ([docs/Besm6_Intrinsics.md](docs/Besm6_Intrinsics.md)) — an intrinsic *is* a call in the IR (declared as an ordinary prototype, so the front end checks arity and coerces arguments; `tac/`/`optimize/`/`ast/`/`translator/` are untouched, and `dead_store` already treats a FUN_CALL as never-dead, which is the "never eliminable" contract Tier 1 needs), and `codegen_intrinsic` ([intrinsics.c](backend/besm6/intrinsics.c)) intercepts every `__besm6_`-prefixed FUN_CALL at the top of `instr.c`'s FUN_CALL case and emits the machine instruction inline instead of a `,call,` — **every** one of them must be intercepted, since all nine collide under Madlen's 8-char truncation and one left to fall through would silently alias another rather than fail to link (hence the trailing `fatal_error`, not a `return false`); the five Tier-2 bit ops (`apx`/`aux`/`acx`/`anx`/`arx`) take the inline A-op-X binop shape, with a no-op `,aox,` after `arx` alone to restore logical ω (it is the one of the five leaving *multiplicative* ω, and peephole #27+#28 put a branch directly on its result); Tier 1 (`ext` 033, `mod` 002 — kinds `BESM_IO_EXT`/`BESM_IO_MOD`, *not* the `BESM_MOD_*` C-register group) and Tier 3 (the extracode, `BESM_IO_EXTRACODE`, `BESM_SHAPE_SPECIAL` since its mnemonic *is* its opcode) share their addressing: a constant address ≤ `07777` becomes the Format-1 offset field, anything else is materialized into `REG_SCRATCH`=r14 (`EA = M[14]+0`), and the accumulator is loaded *last* because materializing the address clobbers A (the intervening `ati` also leaves A unknown to peephole #27, which is what makes `__besm6_ext(a, a)` work); an extracode sets `M[016]`, so it clobbers r14; all three kinds are `is_block_boundary` (a read address switches R to logical; the monitor's handler runs arbitrary code) while `BESM_BRANCH_STOP` is deliberately *not* a rule-#31(b) terminator — the halt is resumable, so `__besm6_stop` is not `_Noreturn` and the code after it is live; `semantic/expressions.c` constant-folds and range-checks the one argument that is an opcode rather than a value (`__besm6_extracode`'s `op`, 050..077); per-dialect spelling diverges (Madlen `,ext,`/`,mod,`/`,33,` raw octal — Madlen names no halt — /`,*74,`; Bemsh `увв`/`рег`/`стоп`/`э74`; b6as `ext`/`mod`/`stop`/`$74`), and every numeric address field is decimal in all three, so `__besm6_ext(04031, …)` emits `,ext, 2073` |
| AArch64 / RISC-V / ARM32 code gen | — | Planned |

### Key data structures

- **`Program`** (`ast/ast.h`): Root node; linked list of `ExternalDecl` (function definition or declaration).
- **`ExternalDecl`**, **`Declaration`**, **`Stmt`**, **`Expr`**, **`Type`**: Core AST nodes. Defined in `ast/ast.h`, spec in `ast/ast.asdl`.
- **`Tac_Instruction`**, **`Tac_Value`**, **`Tac_Type`**: TAC IR nodes. Defined in `tac/tac.h`, spec in `tac/tacky.asdl`.
- **`Besm_Module`**, **`Besm_Func`**, **`Besm_Block`**, **`Besm_Instr`**: BESM-6 backend IR nodes. Defined in `backend/besm6/besm.h`, spec in `backend/besm6/besm6.asdl`.
- **`symtab`** (`semantic/symtab.c/h`): Scoped identifier → `Symbol` map; `symtab_purge(level)` removes block-scope entries on block exit.
- **`structtab`** (`semantic/structtab.c/h`): Struct/union/enum tag → `StructDef` map; scoped, purged on block exit.
- **`typetab`** (`semantic/typetab.c/h`): Typedef name → `TypeDef` map; `typetab_resolve(name)` returns the underlying `Type*`.

### Important design decisions

- **No identifier shadowing**: Inner blocks may not redeclare a name that already exists in any enclosing scope. `symtab` / `structtab` / `typetab` reject duplicates with `fatal_error`. This is a permanent design decision — do not add shadowing support.
- **TAC name convention — frame-resident names start with `%`**: A TAC `var` name encodes its storage class by its first character. `%`+digit is a compiler temporary (`new_temp`); `%`+letter/`_` is a parameter or automatic local; a leading letter/`_`/`$` is a module-level global, static, string constant, or function. Loop and branch-target labels are also `%`-prefixed (`%L`+digit from `label_loops`, or a `%`+digit temporary). `percent_locals_in_function` in `translator/translate.c` (run just before the optimizer) prefixes parameter and automatic-local names — in the body and in the `params`/`locals` lists — with `%`. This lets a backend classify a name without the non-serialized `locals` list: `backend/besm6/frame.c` gives a stack slot to any `%`-prefixed name and treats every other referenced name as an external global. The no-shadowing rule makes the renaming unambiguous. Keep the body and the `params`/`locals` lists consistent (the optimizer's alias analysis matches names against both).
- **`.asdl` files are canonical specs, not code generators.** `ast/ast.asdl` and `tac/tacky.asdl` document the IR; `ast/ast.h` and `tac/tac.h` are maintained manually and must stay in sync.
- **Word I/O (`libutil/wio`)**: AST and TAC binary streams use `size_t`-wide words for portability. Use `wio` for all IR serialization.
- **TAC binary tags (`tac/tags.h`)**: Each TAC node header uses one `size_t`-wide word encoding `TAG_BASE + kind`. Tag constants are readable 4-letter ASCII (e.g. `cnst`, `insr`, `tval`). Stream magic is `TAC2`. See `tac/tags.h`, `tac_export.c`, `tac_import.c`.
- **`xalloc` (`libutil/xalloc`)**: All allocations go through `xalloc`/`xfree`. In debug builds, `xalloc_report()` prints leak totals.
- **Single-pass semantics**: `typecheck_global_decl()` binds names and type-checks in a single pass.
- **Scope tracking**: `scope_level` is incremented on block entry; `scope_decrement()` decrements it and calls `symtab_purge`, `structtab_purge`, and `typetab_purge` — all backed by `map_remove_level_free`.
- **`typedef` handling**: `STORAGE_CLASS_TYPEDEF` declarations are registered in `typetab`. `validate_type` resolves `TYPE_TYPEDEF_NAME` recursively; all type-utility helpers (`get_size`, `get_alignment`, `is_integer`, etc.) resolve typedef names transparently.
- **`switch` semantic validation**: Integer controlling expression with `int` promotion via `convert_to_kind`; constant integer case values evaluated by `try_eval_const_int`; duplicates detected via a `SwitchCtx` stack; multiple defaults and stray case/default labels rejected.
- **TAC lowering coverage**: `translate.c` calls `fatal_error()` on unimplemented constructs. All C11 constructs are now lowered.

### AST quirks

- **Function prototypes vs. definitions**: Only function *definitions* (with a body) parse as `EXTERNAL_DECL_FUNCTION`. A bare prototype such as `int f(int);` parses as `EXTERNAL_DECL_DECLARATION` / `DECL_VAR` with a `TYPE_FUNCTION` declarator type. The typecheck pass (`typecheck_file_scope_var_decl`) detects `TYPE_FUNCTION` and registers it via `symtab_add_fun()`.
- **`f(void)` sentinel**: The parser represents a `(void)` parameter list as a single `Param` node with `TYPE_VOID` and a NULL name (not as an empty list). `typecheck_fn_decl()` strips this sentinel before param processing. Params with a NULL name are skipped when adding to symtab.
- **`_Static_assert` in struct/union bodies**: The parser accepts `_Static_assert` as a struct/union member. The AST uses a `FieldKind` discriminator to distinguish it from regular member declarations.

## Tests

Tests are GoogleTest (C++17). Source lives alongside the module it tests:

- `ast/test/clone_tests.cpp` → `ast-tests`
- `scanner/test/tests.cpp` → `scanner-tests`
- `parser/test/simple_tests.cpp`, `statement_tests.cpp`, … (9 files, including `negative_tests.cpp`) → `parser-tests`
- `tac/test/yaml_tests.cpp`, `graphviz_tests.cpp`, `binary_tests.cpp` → `tac-tests`
- `semantic/test/symtab_tests.cpp`, `structtab_tests.cpp`, `typetab_tests.cpp`, `typecheck_tests.cpp`, `real_tests.cpp`, `pipeline_tests.cpp`, `label_loops_tests.cpp`, `const_convert_tests.cpp`, `coercion_tests.cpp` → `semantic-tests`
- `backend/besm6/test/codegen_tests.cpp`, `arith_tests.cpp`, `copy_tests.cpp`, `flow_tests.cpp`, `run_tests.cpp`, `unary_tests.cpp`, `convert_tests.cpp`, `frame_tests.cpp`, `init_tests.cpp`, `label_tests.cpp`, `ptr_tests.cpp`, `struct_tests.cpp`, `char_tests.cpp`, `peephole_tests.cpp`, `printf_tests.cpp`, `funcptr_tests.cpp`, `stdarg_tests.cpp`, `mem_tests.cpp`, the Bemsh dialect tests `bemsh_tests.cpp` (golden `.bemsh`) and `bemsh_run_tests.cpp` (`CompileAndRunBemsh`: `genbesm --bemsh`+`besmc`+`dubna`), and the Unix (`b6as`) dialect tests `unix_tests.cpp` (golden `.s`), `unix_link_tests.cpp` (`CompileAndAssembleUnix`: `b6as`+`b6ld`), `unix_run_tests.cpp` (`CompileAndRunUnix`: `b6as`+`b6ld`+`b6sim`) → `besm-tests`
- `translator/test/decl_tests.cpp`, `expr_tests.cpp`, `stmt_tests.cpp`, `cast_tests.cpp`, `incdec_tests.cpp`, `switch_tests.cpp`, `ptr_tests.cpp`, `struct_tests.cpp` → `translate-tests`
- `optimize/test/const_fold_tests.cpp`, `jump_unreachable_tests.cpp`, `copy_prop_tests.cpp`, `dead_store_tests.cpp`, `type_conv_tests.cpp`, `pipeline_tests.cpp` → `optimizer-tests`
- `libutil/test/string_map_tests.cpp`, `wio_tests.cpp`, `xalloc_tests.cpp` → `libutil-tests`

The `chapter*_tests.cpp` files in `parser/test/`, `scanner/test/`, `semantic/test/`,
`optimize/test/`, and `backend/besm6/test/` are the "Writing a C Compiler" book tests; they are compiled into the same
per-module test executables as the unit tests above (e.g. `parser-tests`, `besm-tests`) and
run by `make run` (see **Build & Test** above).

## Documentation

- [README.md](README.md) — goals, getting started, component overview
- [docs/Technical_Reference.md](docs/Technical_Reference.md) — detailed reference: repo layout, components, build system, TAC YAML format, development notes
- [docs/Memory_Allocation.md](docs/Memory_Allocation.md) — memory allocator (`xalloc`) design and usage
- [docs/String_Map.md](docs/String_Map.md) — `libutil/string_map` key-value store
- [docs/Word_Oriented_IO.md](docs/Word_Oriented_IO.md) — word-oriented I/O (`wio`) for binary IR streams
- [backend/x86/TODO.md](backend/x86/TODO.md) — x86_64 backend work plan with effort estimates
- [backend/besm6/TODO.md](backend/besm6/TODO.md) — BESM-6 backend work plan with effort estimates
- [docs/Besm6_Data_Representation.md](docs/Besm6_Data_Representation.md) — BESM-6 data representation: bit layouts, ranges, and sizeof for every C scalar type
- [docs/Besm6_Calling_Conventions.md](docs/Besm6_Calling_Conventions.md) — BESM-6 C calling convention (registers, b/save, b/ret)
- [docs/Besm6_Instruction_Set.md](docs/Besm6_Instruction_Set.md) — BESM-6 instruction set reference
- [docs/Besm6_Runtime_Library.md](docs/Besm6_Runtime_Library.md) — BESM-6 runtime helper library specifications (`b/save`, `b/mul`, `b/div`, comparisons, etc.)
- [docs/Besm6_Intrinsics.md](docs/Besm6_Intrinsics.md) — BESM-6 compiler intrinsics (`libc/besm6/include/besm6.h`): the user manual for the nine `__besm6_*` intrinsics — the two supervisor instructions `ext`/`mod` that reach every peripheral, the bit-manipulation instructions with no C equivalent (`apx`/`aux`/`acx`/`anx`/`arx`), the halt `stop`, and the extracode trap. All nine are lowered inline (never a call); the manual covers usage, the per-dialect assembly, the diagnostics, and how the lowering works
- [docs/Frexp_Ldexp.md](docs/Frexp_Ldexp.md) — the `frexp`/`ldexp` C11 math pair: meaning, usage, and a proposed frameless Madlen implementation via the BESM-6 exponent-field instructions (`E+X`, `ASN`, `STI`)
- [docs/Standard_Include_Files.md](docs/Standard_Include_Files.md) — C11 standard headers (`libc/besm6/include/`): role of each header, declared functions, inter-header relationships, and BESM-6 specifics (freestanding vs hosted, no complex/atomics/threads)
- [docs/KOI7_Encoding.md](docs/KOI7_Encoding.md) — KOI-7 character encoding: the BESM-6 code page (code→glyph), the ASCII→KOI7 conversion the codegen performs (`utf8_to_koi7.c`), and how the glyph data was collected on Dubna
- [docs/Madlen.md](docs/Madlen.md) — Madlen assembler syntax for the Dubna monitor (the assembler this backend emits; one of three BESM-6 assemblers documented here — see also Bemsh and `b6as`)
- [docs/Bemsh.md](docs/Bemsh.md) — Bemsh, the BESM-6 autocode (Shtarkman, 1967): the Cyrillic-mnemonic assembly language, its statement/column form, and how it differs from Madlen
- [docs/Besm6_Unix_Assembler.md](docs/Besm6_Unix_Assembler.md) — the BESM-6 Unix assembler (`b6as` in `cmd/as/`): AT&T-style syntax with Madlen mnemonics — tokenization, directives, operand forms, and expression evaluation
- [docs/Type_Coercion.md](docs/Type_Coercion.md) — C11 type coercion and arithmetic conversion rules
- [docs/Type_Sizes_Alignment.md](docs/Type_Sizes_Alignment.md) — type sizes and alignment per target architecture
- [docs/TAC_Optimization.md](docs/TAC_Optimization.md) — machine-independent TAC optimization: constant folding, unreachable code elimination, copy propagation, dead store elimination
- [docs/Peephole_Rewrites.md](docs/Peephole_Rewrites.md) — peephole optimization in the BESM-6 backend: concept, the `besm_peephole` pass, and the catalogue of store/reload, NTR, compare/branch, and strength-reduction rewrites (Phase M)
- [docs/C_Grammar.md](docs/C_Grammar.md) — C grammar article: scanner (`c11.l`), parser (`c11.y`), ASDL (`c11.asdl`), and how they relate to the hand-written implementation
- [docs/Tests_From_The_Book.md](docs/Tests_From_The_Book.md) — textbook-style intro for newcomers: test-driven development, how the "Writing a C Compiler" tests are organized, and how each test maps to a compiler phase
- [grammar/README.md](grammar/README.md) — C11 grammar coverage notes
- [grammar/c11.y](grammar/c11.y), [grammar/c11.l](grammar/c11.l), [grammar/c11.asdl](grammar/c11.asdl) — reference grammar and abstract syntax (not used for code generation)
