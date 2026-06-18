# Tests from the book

This article explains, from the ground up, how we test our C compiler using the example
programs from Nora Sandler's book *Writing a C Compiler* (No Starch Press, 2024). It is
written for a reader who is new to compilers **and** new to automated testing. By the end
you should understand what test-driven development is, how the book's tests are organized,
and — most importantly — *why each test belongs to a particular part of our compiler*.

If you have not yet read the project overview, skim the [README](../README.md) and
[Technical Reference](Technical_Reference.md) first. A friendly companion to this article
is [Learn From This Project](Learn_From_This_Project.md).

## 1. Where the tests come from

The book builds a C compiler one feature at a time over 20 chapters, and it ships with a
large suite of small C programs used to check each step. That suite is published as the
book's companion repository,
[writing-a-c-compiler-tests](https://github.com/nlsandler/writing-a-c-compiler-tests), from
which we draw the programs. It contains roughly **1,600 C programs**, grouped into one
directory per chapter:

```
tests/
├── chapter_1/    A minimal compiler (just `int main(void) { return N; }`)
├── chapter_2/    Unary operators (-, ~)
├── chapter_3/    Binary operators (+, -, *, /, %)
├── ...
├── chapter_18/   Structures and unions
├── chapter_19/   Optimizing the intermediate code
└── chapter_20/   Register allocation
```

Each chapter adds a new slice of the C language. Chapter 1 only knows how to return a
constant; chapter 14 understands pointers; chapter 18 understands `struct`. This gradual
growth is the key to the whole approach: **we never try to test everything at once.** We
take one chapter, turn its programs into tests, make them pass, and move on. Our running
checklist lives in [TODO.md](../TODO.md).

These programs are *source material*. We do not run them straight from the upstream
sources; instead we translate each one into a proper unit test in our own test framework,
which is what the rest of this article is about.

## 2. What is test-driven development?

A **test** is a small, automated check that answers a yes/no question about your program:
"given this input, does the code do the right thing?" Instead of compiling a program by
hand and squinting at the output, you write the question down once, as code, and let the
computer ask it for you — every time, forever.

**Test-driven development** (TDD) is the discipline of letting those tests lead the work.
The classic loop has three steps, often called *red → green → refactor*:

1. **Red.** Write a test for behavior you want, before (or alongside) the code. Run it; it
   fails (red), because the feature isn't there yet — or it reveals a bug.
2. **Green.** Write the smallest amount of code needed to make the test pass (green).
3. **Refactor.** Now that the test guards the behavior, clean up the code without fear: if
   you break something, a test goes red and tells you immediately.

Two ideas make this powerful for a compiler:

- **A test is executable documentation.** `EXPECT_EQ("2\n", run("int main(void){return 2;}"))`
  says, unambiguously, "this program must produce the output `2`." Nobody has to remember
  it; the test remembers.
- **Tests catch regressions.** A *regression* is when a change quietly breaks something
  that used to work. With hundreds of tests, the moment you break chapter 3 while editing
  chapter 9, a red test points at the damage. We saw this for real while writing chapter 1:
  improving the scanner's error messages touched shared code, and re-running the whole
  suite (1,327 tests) confirmed nothing else broke.

We are not writing the book's compiler from scratch — ours already exists — so we are not
doing pure "test first" TDD. But we are doing the part that matters most: building a dense
safety net of tests, organized so that a failure tells you *exactly where* to look.

## 3. Two kinds of tests: positive and negative

The book's programs come in two flavors, and so do our tests.

A **positive test** uses a *valid* C program and checks that the compiler accepts it and
produces the right answer. Example: `int main(void) { return 2; }` must compile, run, and
yield `2`.

A **negative test** uses a *broken* C program and checks that the compiler **rejects** it —
and, crucially, that it complains in a way a human can understand. Example: `return 1foo;`
is not legal C (a number cannot run straight into letters), so the compiler must stop and
say so clearly.

Negative tests are easy to undervalue, so let us be explicit about why they matter. A
compiler that accepts a broken program is dangerous (it produces nonsense). But a compiler
that rejects a broken program with a *baffling* message is merely annoying — and our
compiler used to do exactly that. Before this work, feeding it a stray `@` produced:

```
Parse error: Expected token 73, got 1 (lexeme: @)
```

"Token 73" means nothing to a human. Writing a negative test forces you to decide what a
*good* message would be, and then improve the compiler until it says it. After the change,
the same input produces:

```
<input>:0: lexical error: invalid character '@'
```

This is why **negative tests are a forcing function for good diagnostics.** In our tree we
mark them with a `_Neg` suffix in the test name, so positive and negative tests are easy to
tell apart at a glance.

## 4. A quick tour of our compiler

To know *which* part of the compiler should catch a given error, you need a mental map of
the parts. Our compiler is a **pipeline**: source code flows through a series of phases,
each transforming it a little, until machine code comes out the far end.

```
Source (.c)
  → Scanner      groups characters into tokens         (e.g. `return`, `0`, `;`)
  → Parser       groups tokens into a tree (the AST)    (e.g. "a return statement")
  → AST          the program as a structured tree
  → Semantic     checks meaning: types, declarations    (e.g. "is `x` declared? is this assignment legal?")
  → Translator   lowers the tree to simple 3-address code (TAC)
  → Optimizer    simplifies the TAC                      (e.g. folds `2+3` into `5`)
  → BESM-6 backend  turns TAC into machine instructions for the BESM-6 computer
```

Each phase has a single, well-defined job:

- **Scanner** (`scanner/`, also called the *lexer*): reads raw characters and emits
  *tokens* — the words of the language. It rejects characters that cannot begin any token.
- **Parser** (`parser/`): reads tokens and checks they form *grammatically* valid C,
  building an Abstract Syntax Tree (AST). It rejects things like `return int;`.
- **AST** (`ast/`): the tree data structure the parser produces.
- **Semantic analysis** (`semantic/`): walks the tree and checks that it *means* something
  — every name is declared, types match, you don't take the address of a constant, and so
  on. It rejects things like `int *p = 1;`.
- **Translator** (`translator/`): lowers the validated tree into **TAC** (Three-Address
  Code), a simple intermediate language that is easy to optimize and to translate.
- **Optimizer** (`optimize/`): rewrites the TAC to be smaller or faster without changing
  what it computes.
- **BESM-6 backend** (`backend/besm6/`): emits real assembly for the BESM-6, a historic
  Soviet mainframe, which we run on the **Dubna** simulator.

The full diagram and phase status table live in [CLAUDE.md](../CLAUDE.md) and the
[Technical Reference](Technical_Reference.md).

## 5. How the book organizes its tests

Inside each chapter, the book sorts programs into subdirectories *by what is wrong with
them* (or, for valid programs, that nothing is wrong). Chapter 1 looks like this:

```
tests/chapter_1/
├── valid/            programs that must compile and run
│   ├── return_2.c
│   ├── multi_digit.c
│   └── ...
├── invalid_lex/      programs with a bad *token*  (a scanner error)
│   ├── at_sign.c
│   └── invalid_identifier.c
└── invalid_parse/    programs with bad *grammar*  (a parser error)
    ├── no_semicolon.c
    └── ...
```

Later chapters add more categories as the language grows richer:

- `invalid_lex` — the program contains a character or token that cannot be lexed.
- `invalid_parse` — the tokens are fine, but they don't form a legal sentence.
- `invalid_semantics`, `invalid_types`, `invalid_declarations`, `invalid_labels`,
  `invalid_struct_tags` — the sentence is grammatical but meaningless or ill-typed.
- `valid` — the program is correct; it must compile and produce a specific result.
- `extra_credit` — optional features (compound assignment, `goto`, `switch`, unions). We
  import these right alongside the core tests.

This sorting is not just tidiness. **The directory name is a hypothesis about which phase
should reject the program.** That observation is the heart of the next section.

## 6. Matching tests to compiler phases

Here is the central idea of this whole effort:

> The *kind* of mistake in a program tells you *which phase* of the compiler is responsible
> for catching it.

A broken program fails at the earliest phase that can notice the problem. Walk through
three broken programs and watch where each one stops:

- `return 1foo;` — The very first phase, the **scanner**, tries to read `1foo` as a token.
  A number may not be glued to letters, so the scanner gives up immediately. This is a
  *lexical* error. → **scanner**
- `return int;` — The scanner is happy: `return`, `int`, and `;` are all perfectly good
  tokens. But the **parser** expects an *expression* after `return`, and `int` is a
  keyword, not an expression. The grammar is violated. → **parser**
- `int *p = 1;` — The scanner and parser are both happy; this is a grammatically correct
  declaration. Only when the **semantic** phase checks *types* does it notice you are
  trying to put the integer `1` into a pointer variable. → **semantic**

So each test directory maps onto a component of our compiler:

| Book directory | Compiler phase | Our test file | What the test asserts |
|---|---|---|---|
| `invalid_lex` | scanner | `scanner/chapterNN_tests.cpp` | aborts with a lexical-error message |
| `invalid_parse` | parser | `parser/chapterNN_tests.cpp` | aborts with a parse-error message |
| `invalid_semantics`, `invalid_types`, `invalid_declarations`, `invalid_labels`, `invalid_struct_tags` | semantic | `semantic/chapterNN_tests.cpp` | aborts with a semantic-error message |
| `valid` (and `extra_credit`, `libraries`) | BESM-6 backend | `backend/besm6/chapterNN_tests.cpp` | compiles, runs, and prints the expected result |
| chapter 19 (`constant_folding`, `copy_propagation`, …) | optimizer | `optimize/chapter19_tests.cpp` | the TAC is simplified as expected |

This is a beautiful correspondence: **our source tree already has one directory per phase**
(`scanner/`, `parser/`, `semantic/`, `translator/`, `optimize/`, `backend/besm6/`), and the
book's tests already sort themselves into those same buckets. Importing a chapter is mostly
a matter of carrying each program to its rightful home.

## 7. How we run the positive tests

Negative tests are about *rejecting* bad programs, which any phase can do on its own. But a
positive test must *run* a valid program and check its answer — and to run a program you
need a complete compiler with a real backend. We have one: the **BESM-6 backend** plus the
**Dubna** simulator. So our positive tests genuinely compile each program to BESM-6 machine
code and execute it.

There is one wrinkle. On our BESM-6 runtime, execution starts at a function called
`program`, not `main`, and we observe a program by what it *prints*, not by an exit code.
The book's valid programs, however, are written as `int main(void)` and are judged by the
value `main` returns. To bridge the two, we wrap each program with a tiny `program` that
calls `main` and prints its return value. That wrapper lives in
[backend/besm6/book_run.h](../backend/besm6/book_run.h):

```c
// Wrap a book program so program() prints `main()`'s return value as "%d\n".
inline std::string WrapMain(const std::string &program)
{
    return "int printf(const char *format, ...);\n" + program +
           "\nvoid program(void) { printf(\"%d\\n\", main()); }\n";
}
```

So a book program that ends `return 2;` becomes a program that *prints* `2`, and our test
simply checks that the output is `"2\n"`. This one trick works for the whole corpus: simple
programs whose return value is the point, and the book's "self-checking" programs (which
return `0` on success and a nonzero code on the first failed check) alike.

## 8. Anatomy of a test in our framework

We write tests with [GoogleTest](https://github.com/google/googletest), a widely used C++
testing library. You only need to recognize a handful of pieces:

- `TEST(SuiteName, TestName) { ... }` defines a test. `TEST_F(FixtureName, TestName)` does
  the same but gives the test access to a shared *fixture* (setup/teardown helpers).
- `EXPECT_EQ(expected, actual)` checks two values are equal; if not, the test fails but
  keeps going.
- `EXPECT_DEATH(statement, "regex")` checks that running `statement` makes the process
  **abort**, and that its error output matches the regular expression. This is exactly what
  we need for negative tests: our compiler reports a fatal error and exits, so "did it die
  with the right message?" is the question to ask.

Let us read one real test of each kind, straight from the committed chapter-1 files.

**A scanner negative test** (from [scanner/chapter1_tests.cpp](../scanner/chapter1_tests.cpp)).
The helper `LexToEnd` runs the scanner over a string until end-of-input; on a bad token the
scanner aborts, and `EXPECT_DEATH` catches it:

```cpp
// return 0@1; — '@' is not part of any C token outside a literal.
TEST(ScannerChapter1, AtSign_Neg)
{
    EXPECT_DEATH(LexToEnd("int main(void) {\n    return 0@1;\n}\n"),
                 "invalid character '@'");
}
```

`LexToEnd` itself is a small shared helper in
[scanner/scan_fixture.h](../scanner/scan_fixture.h) — it writes the source to a temporary
file, points the scanner at it, and pulls tokens until the end. We factor such helpers into
a header so every chapter's scanner tests can reuse them.

**A parser negative test** (from [parser/chapter1_tests.cpp](../parser/chapter1_tests.cpp)).
Here the `ParserTest` fixture provides `parse(CreateTempFile(...))`:

```cpp
// Missing semicolon after the return value.
TEST_F(ParserTest, Chapter1_NoSemicolon_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile("int main (void) {\n    return 0\n}\n")),
                 "expected ';', got '\\}'");
}
```

Notice the assertion is the *improved* message we designed while writing the test:
`expected ';', got '}'`, not `Expected token 73`. The test and the diagnostic were built
together — that is the forcing function from §3 in action. (The `\\}` is just the regex
escape for a literal `}`.)

**A positive run test** (from [backend/besm6/chapter1_tests.cpp](../backend/besm6/chapter1_tests.cpp)).
The `CodegenTest` fixture's `CompileAndRun` compiles the source, runs it on Dubna, and
returns whatever it printed:

```cpp
// return 2;
TEST_F(CodegenTest, Chapter1_Return2)
{
    EXPECT_EQ("2\n", CompileAndRun(WrapMain("int main(void) { return 2; }")));
}
```

Read it aloud: "wrap `int main(void){return 2;}` so it prints `main()`'s result, compile
and run it, and expect the output `2`." That is a complete, end-to-end test of the entire
compiler in three lines.

## 9. Naming and file organization

A few simple conventions keep the growing suite navigable:

- **One file per chapter, per component.** Chapter 5's parser tests go in
  `parser/chapter5_tests.cpp`; its semantic tests in `semantic/chapter5_tests.cpp`; its
  runnable programs in `backend/besm6/chapter5_tests.cpp`. The file's directory tells you
  the phase; the filename tells you the chapter.
- **`_Neg` marks negative tests.** A name ending in `_Neg` is a "this must be rejected"
  test; everything else is a positive test.
- **Tests live next to the code they exercise.** This is a long-standing rule in this
  project (see the Tests section of the [Technical Reference](Technical_Reference.md)): the
  scanner's tests are in `scanner/`, the parser's in `parser/`, and so on. The book's
  directory layout maps onto ours almost perfectly, which is why the import is mechanical.

Each new test file is added to its component's build list in the relevant `CMakeLists.txt`
(for example `parser/CMakeLists.txt` lists the sources of the `parser-tests` binary), so it
is picked up automatically when you build.

## 10. The incremental workflow

We import the corpus **one chapter at a time**, in order, because each chapter depends only
on features from earlier chapters. The plan is a checklist in [TODO.md](../TODO.md). A task
(one chapter) is considered *done* when:

1. its new test files build, and
2. the **entire** test suite still passes (`make test`).

Sometimes the book exercises a feature our BESM-6 backend cannot handle *yet* (some
floating-point cases, some `struct`-by-value cases). We do not let that block a whole
chapter. GoogleTest lets you prefix a test name with `DISABLED_` to register it but skip it:

```cpp
TEST_F(CodegenTest, DISABLED_Chapter13_LongDoubleSum) { /* backend gap: FP */ }
```

The test is visible (and reported as skipped) so we remember to come back to it, but it does
not fail the build. This keeps progress flowing: the passing subset of a chapter lands now,
and the gaps are tracked rather than hidden.

## 11. Running the tests

Build everything and run the whole suite from the top of the repository:

```sh
make test                 # configure, build, and run every test via ctest
```

To run a single component or a single test while developing, **run from inside the `build/`
directory**, not from the repository root:

```sh
ctest --test-dir build -R Chapter1            # every chapter-1 test, anywhere
cd build/parser   && ./parser-tests           # just the parser tests
cd build/scanner  && ./scanner-tests
cd build/backend/besm6 && ./besm-tests        # the run tests (need libc.bin here)
```

Why the `cd`? Some fixtures write small temporary files into the *current directory* while
they run. If you launch a test binary from the repository root, those scratch files litter
your source tree; launched from inside `build/`, they stay out of the way. The BESM-6 run
tests have an extra reason: they link the runtime library `libc.bin`, which is built in
`build/backend/besm6`, so they must run from there. (Also: don't run two `besm-tests`
processes at once — they share scratch filenames.)

To run just one test by name:

```sh
cd build/backend/besm6 && ./besm-tests --gtest_filter='CodegenTest.Chapter1_Return2'
```

## 12. Takeaways

If you remember three things from this article, make them these:

1. **Tests are how you make change safe.** A dense, automated suite turns "I hope I didn't
   break anything" into "the computer just confirmed I didn't." That is what lets a compiler
   grow without rotting.
2. **An error is a classifier.** The kind of mistake in a program tells you which phase
   should catch it: bad token → scanner, bad grammar → parser, bad meaning → semantic. Our
   tests are sorted by phase precisely because the errors sort themselves that way.
3. **Negative tests buy you good error messages.** Asking "what should the compiler *say*
   here?" — and writing the test that demands it — is the cheapest way to make a compiler
   pleasant to use.

From here, the natural next steps are to read the running plan in [TODO.md](../TODO.md),
browse the committed chapter-1 tests named throughout this article, and consult the
[Technical Reference](Technical_Reference.md) for the full phase-by-phase design.
