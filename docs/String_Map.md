# String Map

`libutil/string_map` is a key-value store where every key is a string and every value is
either an integer or a pointer.  The compiler uses it as the backing store for its three
symbol tables and for duplicate-detection checks.  This article explains why it exists,
how the data structure works, and what the less-obvious parts of the implementation do.

---

## Why a string map?

A compiler reads source code and builds up knowledge about every name it sees: variable
names, function names, type aliases, struct tags.  At any point during compilation it may
need to answer questions like "has the name `count` been declared yet?" or "what type does
`my_struct` refer to?".

The naive approach — keep a list of names and scan from the beginning each time — works
fine for tiny programs but slows to a crawl as programs grow.  Scanning a list of *n*
names requires looking at *n* entries on average; computer scientists write this O(n).

A hash table is faster on average, but its worst-case behaviour is poor, and it can be
tricky to implement correctly.  More importantly, its traversal order is unpredictable,
which makes debugging harder.

A **self-balancing binary search tree** (BST) gives O(log n) time for every operation —
insert, lookup, and delete — regardless of what data is stored or in what order it
arrives.  For 10,000 names, log₂(10,000) ≈ 13.  That is at most 13 comparisons to find
any name.  And the tree is always sorted, so iterating it visits names in alphabetical
order, which makes diagnostics reproducible.

`string_map` uses the AVL tree variant of a self-balancing BST, invented in 1962 by
Adelson-Velsky and Landis (hence "AVL").

---

## Binary search trees — the basics

Imagine each entry is a card with a name written on it.  You arrange the cards so that
for every card, all cards to its left come alphabetically before it, and all cards to its
right come after it.

```
                    "main"
                   /      \
             "char"        "printf"
            /      \       /      \
         "alpha"  "int"  "output"  "size"
```

To look up `"int"`:

1. Compare `"int"` with the root `"main"`.  `"int"` < `"main"`, go left.
2. Compare `"int"` with `"char"`.  `"int"` > `"char"`, go right.
3. Compare `"int"` with `"int"`.  Match — found.

Three comparisons for a tree of seven nodes.  If the tree were a sorted list, you would
need up to four comparisons on average.  The advantage grows with size.

---

## The AVL twist — keeping the tree balanced

There is a catch.  Suppose you insert names in alphabetical order: `"alpha"`, `"beta"`,
`"char"`, `"delta"`, …  Each new name goes to the right of the previous one, and the tree
degenerates into a linked list:

```
"alpha"
     \
    "beta"
         \
        "char"
              \
             "delta"
                   \
                  ...
```

Now every lookup must traverse the entire chain — we are back to O(n).

The AVL tree fixes this by rebalancing after every insert or delete.  Rebalancing means
rotating nodes around each other to keep the tree's shape roughly symmetrical.

### Height and balance factor

Every node tracks the **height** of its subtree: the number of edges on the longest path
down to a leaf.  A leaf node has height 1; a missing child counts as height 0.

The **balance factor** of a node is `height(left child) − height(right child)`.  AVL
trees maintain the invariant that every node's balance factor stays within {−1, 0, +1}.
A balance factor of +2 means the left subtree is two levels taller than the right — the
tree leans left and needs to be rotated.

`update_height` recomputes height bottom-up after every structural change.
`balance_factor` computes the difference.  `rebalance` checks the factor and chooses one
of four rotations.

### The four rotation cases

#### Case 1: Left-Left (right rotation)

The tree leans left, and the left child also leans left (or is balanced).

```
Before:              After:
    z (+2)               y
   / \                  / \
  y   D                x    z
 / \               →  / \  / \
x   C                A   B C   D
```

Fix: rotate `z` to the right — `y` takes `z`'s place, and `z` becomes `y`'s right child,
carrying `C` (the subtree that was between them) as its new left child.

#### Case 2: Right-Right (left rotation)

Mirror image of Case 1: the tree leans right, right child also leans right.

```
Before:              After:
   z (−2)               y
  / \                  / \
 A   y                z    x
    / \           →  / \  / \
   B   x            A   B C   D
      / \
     C   D
```

Fix: rotate `z` to the left.

#### Case 3: Left-Right (two rotations)

The tree leans left but the left child leans right.  A single right rotation would not
help because `y`'s right subtree `x` would still be too tall.

```
Step 1 — left-rotate y:      Step 2 — right-rotate z:
    z (+2)                       z (+2)                  x
   / \                          / \                     / \
  y   D                        x   D                  y    z
 / \          →               / \          →         / \  / \
A   x                        y   C                  A   B C   D
   / \                      / \
  B   C                    A   B
```

#### Case 4: Right-Left (two rotations)

Mirror of Case 3: tree leans right, right child leans left.

```
Step 1 — right-rotate y:     Step 2 — left-rotate z:
   z (−2)                       z (−2)                    x
  / \                          / \                        / \
 A   y                        A   x                     z    y
    / \          →               / \           →        / \  / \
   x   D                        B   y                  A   B C   D
  / \                               / \
 B   C                             C   D
```

These four cases cover every possible imbalance.  The `rebalance` function in
`string_map.c` checks the balance factor, then the balance factor of the heavy child, and
dispatches to the right combination of `rotate_left` / `rotate_right` calls.

---

## The StringNode structure

```c
typedef struct StringNode {
    struct StringNode *left;
    struct StringNode *right;
    int height;
    intptr_t value;
    int level;
    char key[1];
} StringNode;
```

**`left`, `right`** — pointers to the two children.  `NULL` means no child in that
direction.

**`height`** — the cached height of the subtree rooted at this node, updated after every
rotation.  Caching avoids recomputing heights by traversing both subtrees every time the
balance factor is needed.

**`value`** — the payload.  The type `intptr_t` is an integer wide enough to hold a
pointer on any platform, so this single field can store either a plain integer (e.g., a
case value in a `switch`) or a pointer cast to an integer (e.g., `(intptr_t)my_symbol`).

**`level`** — the scope nesting depth at the time this entry was inserted.  This is the
key to scope-aware deletion, explained in the next section.

**`key[1]` — the struct hack** — this is the most unusual part of the structure.  The
key string does not live in a separate heap allocation.  Instead it lives *inside the
same allocation as the node itself*.  When `create_node` allocates memory it requests
`sizeof(StringNode) + strlen(key)` bytes:

```c
StringNode *node = xalloc(sizeof(StringNode) + strlen(key), …);
strcpy(node->key, key);
```

`sizeof(StringNode)` includes one byte for `key[0]`.  The extra `strlen(key)` bytes sit
immediately after that in memory, giving the array `strlen(key) + 1` bytes total — just
enough for the string and its null terminator.

The benefit: one allocation per node instead of two.  Fewer allocations means less
fragmentation and one fewer pointer to track.

---

## Core operations

### Initialization

```c
void map_init(StringMap *map)   // sets map->root = NULL
```

`StringMap` contains only a single `root` pointer.  Initialization just zeroes it.  You
can declare a `StringMap` on the stack and call `map_init`; no heap allocation is needed
for the map itself.

### Insertion — `map_insert`

```c
void map_insert(StringMap *map, const char *key, intptr_t value, int level);
```

`insert_node` descends the tree recursively, comparing `key` with each node's key using
`strcmp`.  When it reaches a `NULL` child pointer it creates a new node there.  If it
finds an existing node with the same key it updates the value and level in place.

On the way *back up* the recursion stack (after each recursive call returns), `rebalance`
is called on every ancestor.  This ensures the AVL invariant is restored in one pass —
no separate rebalancing loop is needed.

`map_insert_free` is identical but accepts a `dealloc` callback.  If the key already
exists the old value is passed to `dealloc` before being overwritten, so the caller does
not leak the previously stored object.

### Lookup — `map_get`

```c
bool map_get(const StringMap *map, const char *key, intptr_t *value);
```

Lookup is iterative (no recursion, no stack frames).  Starting at the root, it compares
the target key with the current node and steps left or right until it finds a match or
falls off the tree.  The function returns `true` if found and writes the value through
the `value` pointer.  Passing `NULL` for `value` is valid if you only want to know
whether the key exists.

### Removal by key — `map_remove_key`

```c
void map_remove_key(StringMap *map, const char *key);
```

Deletion has three cases:

- **Leaf node** (no children): simply free it.
- **One child**: replace the node with its only child.
- **Two children**: find the *in-order successor* — the node with the smallest key in
  the right subtree.  Copy its key/value into the node being deleted, then remove the
  successor (which has at most one child, making it a simpler case).

After removing the node, `rebalance` is called on each ancestor on the way back up, just
as in insertion.

### Destruction — `map_destroy`

```c
void map_destroy(StringMap *map);
```

Post-order traversal (left subtree, right subtree, then the node itself) frees every
node.  `map_destroy_free` calls a `dealloc` callback on each value before freeing the
node.

---

## Scope-aware removal — the `level` field

This is the feature that makes `string_map` particularly well-suited for a compiler.

In C, every pair of curly braces `{ }` creates a new scope.  Variables declared inside
the braces are visible until the closing `}`, then they disappear.

```c
int x = 1;       // scope level 0 (file scope)
{
    int y = 2;   // scope level 1
    {
        int z = 3;  // scope level 2
    }
    // z is gone here
}
// y is gone here
```

When a scope is entered, `scope_level` is incremented.  Every name inserted at that
depth records the current `scope_level` in its `level` field.  When the scope closes,
`symtab_purge(level)` is called, which calls:

```c
void map_remove_level(StringMap *map, int level);
```

This removes every node whose `level` *exceeds* the threshold — in other words, every
name that was inserted inside the scope that just closed.

**How it works internally:**  `remove_node_level` does a post-order traversal.  For each
node whose `level > threshold` it calls `remove_single_node`, the same helper used by
`map_remove_key`.  Because many nodes may be removed in one pass, the BST property can
be violated at multiple points simultaneously.  Rather than rebalancing after each
individual removal — which would be complex — the code does all the removals first, then
makes a second pass (`rebalance_tree`) to restore the AVL invariant everywhere.

`map_remove_level_free` does the same but calls a `dealloc` callback on each removed
value.  One subtlety: in the two-children case, `remove_single_node` promotes the
successor *in-place*, replacing the current node's key and value with the successor's
data before freeing the successor struct.  If `dealloc` were called after that swap it
would free the wrong value (the successor's, not the deleted node's).  To handle this,
`remove_node_level` saves the value before calling `remove_single_node`:

```c
intptr_t saved_value = node->value;
node = remove_single_node(node);
dealloc(saved_value);
```

---

## Custom destructors — the `_free` variants

The map stores `intptr_t` values.  In the symbol tables those values are actually
pointers to heap-allocated objects (`Symbol*`, `TypeDef*`, `StructDef*`).  When the map
removes an entry it must free not just the node but the object the value points to.

The `_free` family of functions accepts a callback:

```c
void (*dealloc)(intptr_t value)
```

The caller supplies a function that knows how to interpret the `intptr_t` and free it.
For the symbol table this is:

```c
static void symtab_destroy_callback(intptr_t ptr)
{
    free_symbol((Symbol *)ptr);
}
```

`free_symbol` handles the details: freeing the name string, the type, and any nested
TAC initializer list.  The map does not need to know any of this — it just calls the
callback with the raw `intptr_t` and the callback does the right thing.

This pattern lets a single generic map implementation serve all three symbol tables, each
with its own object type and its own cleanup logic.

---

## Iteration — `map_iterate`

```c
void map_iterate(StringMap *map,
                 void (*func)(intptr_t value, const void *arg),
                 const void *arg);
```

An in-order traversal (left subtree, current node, right subtree) visits every entry in
ascending alphabetical order by key.  The callback receives the value and an opaque `arg`
pointer that the caller can use to pass any additional context.

---

## How it is used in the compiler

| Consumer | Map variable | Key | Value |
|---|---|---|---|
| `semantic/symtab.c` | `symtab` | identifier name | `Symbol*` |
| `semantic/typetab.c` | `typetab` | typedef name | `TypeDef*` |
| `semantic/structtab.c` | `structtab` | struct/union/enum tag | `StructDef*` |
| `semantic/typecheck.c` (switch) | `seen_cases` | `"%ld"` formatted value | `0` (presence only) |
| `semantic/typecheck.c` (struct) | `seen_members` | member name | `0` (presence only) |

The first three are persistent maps that live for the duration of the compilation unit.
They are initialised once at program startup, populated as declarations are processed,
and purged entry by entry as scopes close.

The last two are temporary maps allocated on the stack inside individual functions.  When
the compiler enters a `switch` statement it allocates a `StringMap seen_cases` locally,
uses it to detect duplicate `case` values, then calls `map_destroy` when the statement
ends.  The same pattern is used when checking for duplicate member names inside a struct
definition.

---

## Performance

| Operation | Time |
|---|---|
| `map_get` | O(log n) |
| `map_insert` | O(log n) |
| `map_remove_key` | O(log n) |
| `map_remove_level` | O(n) — removes many entries at once |
| `map_destroy` | O(n) |

The AVL height guarantee bounds the tree height at ⌊1.44 log₂(n + 2)⌋.  For 1,000
entries that is at most 14 levels; for 1,000,000 it is at most 28.  Every get, insert,
and single-key delete touches at most that many nodes.

Each node costs one allocation of `sizeof(StringNode) + strlen(key)` bytes — the key
string shares the allocation with the node struct itself, so there is no per-entry
overhead from a second `malloc`.
