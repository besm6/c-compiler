# C Compiler for BESM-6

A C compiler targeting the BESM-6 mainframe, designed to be self-hosting and capable of building the Unix v7 kernel.

## Goals

The primary objectives of this project are:

* **Self-hosting**: The compiler should be able to compile itself.
* **Unix v7 Kernel**: The compiler should be able to build the [Unix v7](https://github.com/besm6/v7besm) kernel.
* **Dubna Integration**: The compiler should integrate natively into the [Dubna monitor system](https://github.com/besm6/dubna).

## Architecture

The compiler follows a traditional multi-stage architecture, split into frontend and backend components:

```
C Source Code
    |
    v
[Scanner]     Lexical Analysis
    |
    v
[Parser]      Syntactic Analysis (based on C11 Yacc grammar)
    |
    v
[AST]         Abstract Syntax Tree (ASDL representation)
    |
    v
[Translator]  Semantic Analysis & Code Generation
    |         - Identifier Resolution
    |         - Type Checking
    |         - Loop Labeling
    |
    v
[TAC]         Three Address Code (ASDL representation)
    |
    v
Assembly/Object Code (target: BESM-6)
```

### Compilation Pipeline

The compiler is implemented as two separate executables:

1. **`cast`** (C AST): Parses C source code and produces an Abstract Syntax Tree
   - Input: C source file
   - Output: AST in binary format (`.ast`), YAML (`.yaml`), or Graphviz DOT (`.dot`)

2. **`tacker`** (TAC maker): Translates AST to Three Address Code
   - Input: AST file (binary format from `cast`)
   - Output: TAC in binary format (`.tac`), YAML (`.yaml`), or Graphviz DOT (`.dot`)

## Components

### Scanner (`scanner/`)

Hand-written lexical analyzer that tokenizes C source code. Implements a complete C11 token set including:
- Keywords (C11 standard keywords and reserved identifiers)
- Identifiers
- Literals (integer, floating-point, character, string)
- Operators and punctuators
- Preprocessor-related tokens (assuming preprocessed input)

**Key Files:**
- `scanner.h` - Token definitions and scanner interface
- `scanner.c` - Lexer implementation
- `tests.cpp` - Unit tests

### Parser (`parser/`)

Recursive descent parser that constructs an Abstract Syntax Tree from tokens. The parser is based on the C11 Yacc grammar specification but implemented as hand-written C code rather than generated from Yacc.

**Features:**
- Full C11 grammar support
- Name table management for identifiers
- AST construction

**Key Files:**
- `parser.h` - Parser interface
- `parser.c` - Parser implementation
- `nametab.c` - Name table for identifier management
- `main.c` - `cast` executable entry point
- Multiple test suites: `simple_tests.cpp`, `statement_tests.cpp`, `type_tests.cpp`, etc.

### AST (`ast/`)

Abstract Syntax Tree representation using ASDL (Abstract Syntax Description Language). Provides a complete representation of C programs including types, declarations, expressions, statements, and program structure.

**Features:**
- Serialization/deserialization (binary format)
- Export to YAML and Graphviz DOT formats
- AST manipulation utilities (clone, compare, free)
- Graph visualization support

**Key Files:**
- `ast.asdl` - ASDL grammar definition
- `ast.h` - AST type definitions
- `ast_alloc.c` - Memory allocation
- `ast_export.c`, `ast_import.c` - Binary serialization
- `ast_yaml.c` - YAML export
- `ast_graphviz.c` - DOT export
- `ast_print.c` - Human-readable printing
- `ast_clone.c`, `ast_compare.c`, `ast_free.c` - Utility functions

### Translator (`translator/`)

Semantic analysis and code generation module that converts AST to Three Address Code (TAC).

**Phases:**
1. **Identifier Resolution** (`resolve.c`): Resolves identifiers to their declarations
2. **Type Checking** (`typecheck.c`): Performs static type checking
3. **Loop Labeling** (`translator.c`): Annotates loops for break/continue handling
4. **Code Generation** (`translator.c`): Translates AST nodes to TAC instructions

**Key Components:**
- `symtab.c` - Symbol table for variables and functions
- `typetab.c` - Type table for type definitions
- `type_utils.c` - Type system utilities (sizes, alignments, compatibility)
- `const_convert.c` - Constant value conversions
- `main.c` - `tacker` executable entry point

**Key Files:**
- `translator.h` - Translator interface
- `translator.c` - Main translation logic
- `resolve.c` - Identifier resolution
- `typecheck.c` - Type checking
- Test suites: `symtab_tests.cpp`, `typetab_tests.cpp`, `typecheck_tests.cpp`

### TAC (`tac/`)

Three Address Code intermediate representation. TAC is a low-level intermediate form that represents computation in terms of operations with at most three operands.

**TAC Structure:**
- **Top-Level**: Functions, static variables, static constants
- **Instructions**: Arithmetic operations, memory operations (load/store), control flow (jumps, labels), type conversions
- **Values**: Constants and variables
- **Types**: Simplified type system (Char, Int, Long, Double, Void, etc.)

**Key Files:**
- `tacky.asdl` - ASDL grammar definition for TAC
- `tac.h` - TAC type definitions
- `tac_alloc.c`, `tac_free.c` - Memory management
- `tac_print.c` - Human-readable printing
- `tac_compare.c` - Comparison utilities

### Grammar (`grammar/`)

C11 grammar definitions and documentation:
- `c11.y` - Yacc grammar specification (C11 standard)
- `c11.l` - Lex specification (reference, scanner is hand-written)
- `c11.asdl` - ASDL grammar definition (alternative representation)
- `README.md` - Detailed explanation of the ASDL definition

### Utilities (`libutil/`)

Common utility libraries used across the compiler:

- **`xalloc`**: Memory allocation with leak detection and reporting
  - `xalloc.c`, `xalloc.h` - Memory management utilities
  - Provides `xalloc()`, `xfree()`, `xfree_all()`, `xstrdup()`
  - Memory tracking and leak reporting in debug mode

- **`wio`**: Binary I/O utilities for serialization
  - `wio.c`, `wio.h` - Wide I/O functions for reading/writing binary data
  - Used for AST and TAC serialization

- **`string_map`**: String-to-value map data structure
  - `string_map.c`, `string_map.h` - O(log n) string map implementation
  - Used in symbol tables and type tables

## Project Structure

```
c-compiler/
├── ast/              # Abstract Syntax Tree implementation
├── grammar/          # C11 grammar definitions (Yacc/Lex/ASDL)
├── libutil/         # Utility libraries (xalloc, wio, string_map)
├── parser/           # Parser implementation
├── scanner/          # Lexical analyzer
├── tac/              # Three Address Code implementation
├── translator/       # Semantic analysis and code generation
├── scripts/          # Build and validation scripts
├── CMakeLists.txt    # Main CMake build configuration
├── Makefile          # Convenience Makefile wrapper
└── LICENSE           # MIT License
```

## Building

### Requirements

* **CMake** 3.10 or later
* **C Compiler** supporting C11 standard
* **C++ Compiler** supporting C++17 standard (for tests)
* **Make** (optional, for convenience wrapper)
* **cppcheck** (optional, for static analysis)

GoogleTest is automatically downloaded by CMake during the build process.

### Build with Makefile

The simplest way to build:

```bash
# Build everything
make

# Run tests
make test

# Clean build files
make clean

# Debug build
make clean
make debug
make
```

### Build with CMake

For more control:

```bash
# Create build directory and configure
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Build
cmake --build .

# Run tests
ctest
```

Or using CMake 3.13+:

```bash
# Configure and build in one step
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build

# Run tests
ctest --test-dir build
```

### Build Options

* `CMAKE_BUILD_TYPE`: `Debug`, `RelWithDebInfo`, or `Release`
* Static analysis with cppcheck is enabled automatically if found

## Usage

### `cast` - C Parser

The `cast` executable parses C source files and outputs AST representations.

**Basic Usage:**
```bash
# Parse a C file and output binary AST
cast input.c output.ast

# Output YAML format
cast --yaml input.c output.yaml

# Output Graphviz DOT format (for visualization)
cast --dot input.c output.dot

# Output to stdout
cast input.c -
```

**Options:**
- `--ast` - Emit AST in binary format (default)
- `--yaml` - Emit YAML format
- `--dot` - Emit Graphviz DOT script
- `-v, --verbose` - Enable verbose mode
- `-D, --debug` - Print debug information
- `-h, --help` - Show help message

**Example:**
```bash
cast -v input.c -  # Parse input.c and print AST to stdout
cast --dot test.c test.dot && dot -Tpng test.dot -o test.png  # Visualize AST
```

### `tacker` - AST to TAC Translator

The `tacker` executable translates AST files to Three Address Code.

**Basic Usage:**
```bash
# Translate AST to TAC (binary format)
cast input.c input.ast  # First, parse to AST
tacker input.ast output.tac

# Output YAML format
tacker --yaml input.ast output.yaml

# Output Graphviz DOT format
tacker --dot input.ast output.dot
```

**Options:**
- `--tac` - Emit TAC in binary format (default)
- `--yaml` - Emit YAML format
- `--dot` - Emit Graphviz DOT script
- `-v, --verbose` - Enable verbose mode
- `-D, --debug` - Print debug information (includes AST and TAC printing)
- `-h, --help` - Show help message

**Example:**
```bash
# Complete pipeline: C source -> AST -> TAC
cast input.c input.ast
tacker input.ast input.tac

# With debugging output
tacker -D input.ast output.tac
```

## Testing

The project uses GoogleTest for unit testing. Tests are automatically discovered and can be run via CMake's test infrastructure.

**Run all tests:**
```bash
make test
# or
ctest --test-dir build
```

**Run specific test suites:**
```bash
# From build directory
./scanner-tests
./parser-tests
./symtab-tests
./typetab-tests
./typecheck-tests
```

**Test Coverage:**

* **Scanner tests** (`scanner/tests.cpp`): Token recognition, literals, keywords, operators
* **Parser tests** (`parser/*_tests.cpp`):
  - `simple_tests.cpp` - Basic parsing
  - `statement_tests.cpp` - Control flow statements
  - `operator_tests.cpp` - Expression operators
  - `type_tests.cpp` - Type parsing
  - `struct_tests.cpp` - Structure and union parsing
  - `declaration_tests.cpp` - Declaration parsing
  - `constant_tests.cpp` - Literal parsing
  - `serialize_tests.cpp` - AST serialization/deserialization
* **Translator tests** (`translator/*_tests.cpp`):
  - `symtab_tests.cpp` - Symbol table operations
  - `typetab_tests.cpp` - Type table operations
  - `typecheck_tests.cpp` - Type checking logic

## Development

### Code Style

* C code follows C11 standard
* C++ test code follows C++17 standard
* Compiler flags: `-Wall -Werror -Wshadow`
* Static analysis with cppcheck (when available)

### Memory Management

The compiler uses a custom memory allocation system (`libutil/xalloc`) that:
* Tracks all allocations for leak detection
* Provides `xfree_all()` for bulk deallocation
* Reports memory leaks in debug mode

### AST and TAC Representation

Both AST and TAC use ASDL (Abstract Syntax Description Language) for their definitions:
* `ast/ast.asdl` - AST structure definition
* `tac/tacky.asdl` - TAC structure definition

The ASDL files are processed to generate C type definitions. See the `ast/` and `tac/` directories for generated headers.

### Debugging

Enable debug output with the `-D` flag for both executables:

```bash
cast -D input.c output.ast
tacker -D input.ast output.tac
```

Debug mode enables:
* Parser debug output
* AST/TAC pretty-printing
* Memory leak reporting (via `xalloc`)
* Import/export debug output

### Visualization

Generate Graphviz visualizations of AST or TAC:

```bash
# Generate AST visualization
cast --dot input.c ast.dot
dot -Tpng ast.dot -o ast.png

# Generate TAC visualization
tacker --dot input.ast tac.dot
dot -Tpng tac.dot -o tac.png
```

## Dependencies

### Build Dependencies

* **CMake** 3.10+ - Build system
* **C Compiler** (GCC, Clang) with C11 support
* **C++ Compiler** (GCC, Clang) with C++17 support (for tests)
* **Make** - Convenience wrapper (optional)

### Runtime Dependencies

None required - the executables are standalone.

### Optional Dependencies

* **cppcheck** - Static analysis (enabled automatically if found)
* **Graphviz** - For visualizing DOT output (not required for compilation)

### Auto-downloaded Dependencies

* **GoogleTest** v1.15.2 - Testing framework (downloaded during CMake configuration)

## References

* **Primary Reference**: ["Writing a C Compiler"](https://nostarch.com/writing-c-compiler) by Nora Sandler
* **C11 Grammar**: Based on ANSI C Yacc grammar by Jeff Lee (updated for C11 standard)
* **Unix v7 for BESM-6**: [v7besm](https://github.com/besm6/v7besm)
* **Dubna Monitor System**: [dubna](https://github.com/besm6/dubna)

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

Copyright (c) 2025 besm6
