# Memory Allocation in the C Compiler

The compiler allocates hundreds of small objects as it parses and analyses a source file — AST nodes, type descriptors, symbol-table entries, TAC instructions. All of them go through a thin wrapper around the standard C allocator called **xalloc**. This article explains why that wrapper exists and how every piece of it works.

## The problem with plain `malloc`

The standard library's `malloc` has three rough edges that compound each other in a complex program:

1. **It can return NULL.** When the system runs out of memory, `malloc` returns a null pointer instead of a valid address. Every call site must check for this and propagate the error up the call chain. In practice this adds noise and is often forgotten.

2. **The memory is not zeroed.** `malloc` gives you whatever bytes were sitting in that region previously. Reading an un-initialised field is undefined behaviour and a common source of hard-to-reproduce bugs.

3. **There is no record of what was allocated.** If a block is never freed, you have a *memory leak*. With plain `malloc` you have no way to ask "which allocations are still alive and where were they made?" without an external tool like Valgrind.

## What xalloc adds

`xalloc` is a wrapper that fixes all three issues:

- **Never returns NULL.** If `calloc` fails, `xalloc` prints the size and the exact source location of the failing call and calls `exit(1)`. The rest of the compiler never has to handle allocation failure.
- **Memory is always zeroed.** `xalloc` uses `calloc` internally, so every new object starts life with all bits set to zero.
- **Every live block is tracked in a linked list.** At any moment you can ask for a full report of what has not been freed yet, including which function and source line allocated it.

## The BlockHeader — a hidden label on every block

When you call `xalloc(size, ...)`, the function does not allocate just `size` bytes. It allocates `sizeof(BlockHeader) + size` bytes and reserves the front portion for its own bookkeeping:

```
low address                                   high address
┌─────────────────────────────────────────────────────────┐
│           BlockHeader (internal)            │ user data │
│  next · prev · requested_size · func · file │ ◄─── ptr  │
└─────────────────────────────────────────────────────────┘
```

The pointer returned to the caller (`ptr`) points at the *user data* region, just past the header. The header is invisible to the rest of the program.

`BlockHeader` contains:

| Field | Type | Purpose |
|---|---|---|
| `next` | `BlockHeader *` | Next block in the tracking list |
| `prev` | `BlockHeader *` | Previous block in the tracking list |
| `requested_size` | `size_t` | How many bytes the caller asked for |
| `funcname` | `const char *` | Name of the C function that called xalloc |
| `filename` | `const char *` | Source file that called xalloc |
| `lineno` | `unsigned` | Line number that called xalloc |

When `xfree` is called with a user pointer, it recovers the header by subtracting `sizeof(BlockHeader)` from the address — the reverse of what `xalloc` did.

## The doubly linked list

Every allocated block is linked into a global doubly linked list so that the tracker can find all live blocks without scanning the entire heap.

```
head
 │
 ▼
┌────────────────────┐     ┌────────────────────┐     ┌────────────────────┐
│ B3 (newest)        │     │ B2                 │     │ B1 (oldest)        │
│  prev = NULL       │◄───►│  prev = &B3        │◄───►│  prev = &B2        │
│  next = &B2        │     │  next = &B1        │     │  next = NULL       │
└────────────────────┘     └────────────────────┘     └────────────────────┘
```

New blocks are inserted at the *head* — the cheapest possible insertion. Removal on `xfree` is O(1) because both neighbours are known through `prev` and `next`; no list scan is needed.

`xfree` also validates the list before unlinking: it checks that `h->prev->next == h` and `h->next->prev == h`. If either check fails it prints `"Damaged memory list"` and aborts, which usually means a buffer overrun elsewhere corrupted the header.

## Function-by-function walkthrough

### `xalloc`

```c
void *xalloc(size_t size, const char *funcname, const char *filename, unsigned lineno)
```

1. Calls `calloc(1, sizeof(BlockHeader) + size)` — one zeroed block.
2. Fills the `BlockHeader` fields (size, caller location).
3. Links the header at the front of the global list.
4. Returns `(char *)rawptr + sizeof(BlockHeader)` — the user data region.

If `calloc` returns NULL the function prints a diagnostic and calls `exit(1)`.

### `xfree`

```c
void xfree(void *ptr)
```

1. Does nothing if `ptr` is NULL (mirrors `free` behaviour).
2. Recovers the header: `h = (BlockHeader *)((char *)ptr - sizeof(BlockHeader))`.
3. Validates list integrity, then unlinks `h` by updating its neighbours' `next`/`prev`.
4. Zeroes `h->next` and `h->prev` to catch use-after-free bugs more reliably.
5. Calls `free(h)` to release the entire raw block.

### `xstrdup`

```c
char *xstrdup(const char *str)
```

A convenience wrapper that allocates `strlen(str) + 1` bytes via `xalloc` and copies the string into them. The resulting allocation is tracked like any other.

### `xreport_lost_memory`

```c
void xreport_lost_memory(void)
```

Walks the linked list from `head` and prints one line per surviving block:

```
Lost memory:
48 bytes allocated by alloc_expr() at line 125 of file ast_alloc.c
```

The filename is trimmed to just the base name (everything after the last `/`) to keep output readable.

### `xtotal_allocated_size`

```c
size_t xtotal_allocated_size(void)
```

Sums `requested_size` across all live blocks. Useful in tests that want to assert "no memory is outstanding after cleanup."

### `xfree_all`

```c
void xfree_all(void)
```

Frees every raw block in one sweep and resets `head` to NULL. Called at program exit after `xreport_lost_memory` so that operating-system memory maps are clean even if some blocks were legitimately not freed through the normal free path.

### `xalloc_debug`

A global `int` flag. When set to `1`, every call to `xalloc` and `xfree` prints a line to stdout:

```
--- xalloc 48 bytes, 0x600003a08030
--- xfree 48 bytes, 0x600003a08030
```

Enable it in code with:

```c
extern int xalloc_debug;
xalloc_debug = 1;
```

The compiler's `parse -D` and `lower -D` flags set `xalloc_debug` automatically.

## How callers use xalloc

The four extra arguments (`funcname`, `filename`, `lineno`) are always filled with the standard C preprocessor macros `__func__`, `__FILE__`, `__LINE__`, which the compiler expands to the *call site* — not the location inside xalloc itself. Every allocator in `ast/ast_alloc.c` follows this pattern:

```c
Expr *alloc_expr(void)
{
    Expr *e = xalloc(sizeof(Expr), __func__, __FILE__, __LINE__);
    return e;
}
```

At program exit the two main executables call:

```c
if (args->debug) {
    xreport_lost_memory();   // print surviving blocks
}
xfree_all();                 // release everything unconditionally
```

This means debug mode (`-D`) shows you exactly which allocations were not freed and where they came from, without needing an external profiler.

## What xalloc does NOT do

- **Thread safety.** The global `head` pointer is not protected by a mutex. The compiler is single-threaded, so this is fine.
- **Bounds checking.** There are no guard bytes before or after the user data region. A buffer overrun will silently corrupt neighbouring memory (and possibly the `BlockHeader`, which `xfree` will detect as a "damaged list").
- **Replace external tools.** `xreport_lost_memory` tells you *that* memory leaked and *where it was allocated*. It does not tell you *why* it was not freed. Valgrind or AddressSanitizer are better for that deeper analysis. `xalloc` complements those tools, not replaces them.
