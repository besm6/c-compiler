# C Compiler for BESM-6

A C compiler project aimed at the [BESM-6](https://en.wikipedia.org/wiki/BESM-6) mainframe. The long-term idea is a self-hosting toolchain that can help build systems such as the [Unix v7 port for BESM-6](https://github.com/besm6/v7besm) and work with the [Dubna monitor](https://github.com/besm6/dubna).

**This repository is unfinished.** The frontend (lexing, parsing, AST) is in active use; semantic analysis libraries exist; intermediate code and a machine backend are still being brought up. For file-by-file detail, build options, and tests, see [docs/TECHNICAL.md](docs/TECHNICAL.md).

## Goals

* **Self-hosting**: The compiler should eventually compile itself.
* **Unix v7 kernel**: Target use case includes building the [v7besm](https://github.com/besm6/v7besm) kernel.
* **Dubna integration**: Run naturally under the [Dubna](https://github.com/besm6/dubna) environment.

## Current status (what works today)

| Area | Notes |
|------|--------|
| **Build** | CMake-based build; optional `Makefile` wrapper. Unit tests via GoogleTest. |
| **`cast`** | Reads C source and writes an abstract syntax tree (AST): binary `.ast`, or `--yaml` / `--dot` for inspection and graphs. |
| **Semantic passes in `tacker`** | For each top-level declaration read from a binary AST, the translator runs name resolution and type checking (and will run further passes as they are implemented). |
| **TAC as a data model** | Types and helpers for three-address code exist under `tac/` (including pretty-printing), but **AST→TAC translation and TAC export are not wired up yet.** |
| **Preprocessor, assembler, BESM-6 code generation** | **Not in this repo** at this stage. |

If you only want to try the project: build it, run `cast` on a small `.c` file, and open the YAML or DOT output. See [Getting started](#getting-started) below.

## How the pieces fit together (architecture)

A compiler is usually described as a pipeline. You can think of it like an assembly line: each stage turns the program into a richer or lower-level representation until it matches the real machine.

1. **Scanner (lexer)** splits the source text into *tokens* (keywords, identifiers, numbers, punctuation).
2. **Parser** builds a *syntax tree* (AST) that matches the grammar of the language.
3. **Semantic analysis** checks meaning: types, scopes, and whether names refer to the right declarations.
4. **Intermediate code** (here, *three-address code*, TAC) is a machine-neutral form that is easier to optimize and translate than raw C syntax.
5. **Backend** (not present yet) would turn TAC into BESM-6 assembly or object code.

Stages 1–3 are largely in place for parsing and checking; stage 4 is partially designed (data structures, reference grammar) but not yet connected end-to-end; stage 5 is future work.

```mermaid
flowchart LR
    subgraph done [Implemented or in progress]
        SourceCode[Source text]
        Scanner[Scanner]
        Parser[Parser]
        AST[AST]
        Semantic[Semantic passes]
        SourceCode --> Scanner --> Parser --> AST --> Semantic
    end
    subgraph planned [Planned]
        TAC[TAC generation and I/O]
        Backend[BESM-6 backend]
        Semantic --> TAC --> Backend
    end
```

The repository ships two programs: **`cast`** (C → AST) and **`tacker`** (reads binary AST, runs analysis; TAC output is still incomplete). Details and command lines are in [docs/TECHNICAL.md](docs/TECHNICAL.md#executables-cast-and-tacker).

## Getting started

**You need:** CMake 3.10 or newer, a C11 compiler, and a C++17 compiler (tests only). Make is optional. Network access the first time you configure the project so CMake can fetch GoogleTest.

**Build and test:**

```bash
make          # creates build/, runs cmake, builds
make test     # runs ctest in build/
```

Or with CMake directly:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build
```

**Parse a file to YAML** (human-readable tree):

```bash
./build/cast --yaml hello.c hello.yaml
```

**Parse to Graphviz DOT** (for a diagram if you have [Graphviz](https://graphviz.org/) installed):

```bash
./build/cast --dot hello.c hello.dot
dot -Tpng hello.dot -o hello.png
```

For debug logging, verbose mode, and what `tacker` does today, see [docs/TECHNICAL.md](docs/TECHNICAL.md).

## Documentation

| Document | Purpose |
|----------|---------|
| [docs/TECHNICAL.md](docs/TECHNICAL.md) | Repository layout, components, build system, tests, ASDL vs C code, development notes, references |
| [grammar/README.md](grammar/README.md) | Notes on the C11 grammar artifacts in `grammar/` |

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file.

Copyright (c) 2025 besm6
