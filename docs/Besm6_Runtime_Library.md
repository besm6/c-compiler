## Compiler-Support Routines

These routines are emitted automatically by the compiler. They implement the B calling
convention and the operators that have no direct BESM-6 instruction equivalents.

For a detailed description of the calling convention, see
[Besm6-Calling-Conventions.md](Besm6-Calling-Conventions.md).

---

### `b/save` — [b_save.madlen](../besm6/libb/b_save.madlen)

Called on entry to every B function that has **one or more parameters**.

The compiler emits:

```
   ,its, 13        ; push return address
13 ,vjm, b/save    ; call b/save
```

**Actions:**

1. Adjusts r15 (stack pointer) by the argument count from r14: `r15 += r14`
   (since r14 is negative, this *decrements* r15 past the arguments already on the stack).
2. Saves r7, r6, and the accumulator (which holds return address r13) to the stack.
3. Sets r6 = address of argument #1 (the parameter pointer).
4. Sets r7 = current r15 (the auto-variable pointer, pointing just above the saved registers).
5. Extracts the real return address from the saved value and puts it in r13.
6. Jumps to r13 to continue the function body.

---

### `b/save0` — [b_save0.madlen](../besm6/libb/b_save0.madlen)

Called on entry to every B function with **no parameters**.

The compiler emits:

```
   ,its, 13        ; push return address
13 ,vjm, b/save0   ; call b/save0
```

Identical to `b/save` except that it first increments r15 by 1 to allocate a dummy
argument slot, so that the resulting stack frame layout is uniform with parameterized
functions. This allows `b/ret` to use the same unwind logic regardless of parameter count.

---

### `b/ret` — [b_ret.madlen](../besm6/libb/b_ret.madlen)

Called at every exit point of a B function. The return value must be in the accumulator
before the jump to `b/ret`.

**Actions:**

1. Computes the number of arguments from r6 and r7 to determine how far to unwind r15.
2. Restores saved r6, r7, r13 from the stack frame.
3. Restores r15 to the caller's stack level.
4. Jumps to r13 (the caller's return address).

---

### `b/true` — [b_true.madlen](../besm6/libb/b_true.madlen)

A single word containing the integer value `1`, used as the canonical "true" result
by all relational and logical operators. Because BESM-6 has no load-immediate instruction
for arbitrary values, the operators load this word with `xta b/true`.

---

### Arithmetic Operators

Each routine receives two arguments in the standard way:

- The first operand (`a`) is at the top of the stack (r15 points just past it).
- The second operand (`b`) is in the accumulator.

The result is left in the accumulator; r15 is not changed (the compiler adjusts the
stack after the call). Results are masked to 36 bits to match B integer width.

#### `b/mul` — [b_mul.madlen](../besm6/libb/b_mul.madlen)

Computes `a * b`.

Uses the BESM-6 `a*x` (multiply) instruction with 36-bit normalization via `ntr` and
`a+x =:64`. The final result is masked with `=37 7777 7777 7777` to keep only the
lower 36 bits.

#### `b/div` — [b_div.madlen](../besm6/libb/b_div.madlen)

Computes `a / b`.

BESM-6 division (`a/x`) operates on normalized floating-point representations.
The routine converts both operands to the required form using `ntr` (normalize) and
`avx` (absolute value exchange), performs the division, then extracts the 36-bit integer
result.

#### `b/mod` — [b_mod.madlen](../besm6/libb/b_mod.madlen)

Computes `a % b` (remainder).

Implemented as `a - (a / b) * b`, using the same sign-normalizing sequence as `b/div`.
Both `a` and the intermediate `a / b` are preserved on the stack during the computation.

---

### Relational and Logical Operators

Each routine receives two arguments the same way as the arithmetic operators.
It returns `1` (loaded from `b/true`) if the condition is true, or `0` otherwise.

All seven routines share the same two-branch template:

1. Perform a subtraction or exchange to set the accumulator sign.
2. Branch to `true` on the appropriate sign condition (`uza` = branch if zero,
   `u1a` = branch if non-zero/positive).
3. Fall-through path: load `0` from the accumulator's implicit zero and return.
4. `true` path: load `1` via `xta b/true` and return.

| Routine | Source | B op | Computation | Branch |
|---------|--------|------|-------------|--------|
| `b/not` | [b_not.madlen](../besm6/libb/b_not.madlen) | `!` | `aex` (swap A and X) | `uza`: branch if `b == 0` |
| `b/eq` | [b_eq.madlen](../besm6/libb/b_eq.madlen) | `==` | `aex` (swap A and X, then `a-x`) | `uza`: branch if `a == b` |
| `b/ne` | [b_ne.madlen](../besm6/libb/b_ne.madlen) | `!=` | `aex` | `u1a`: branch if `a != b` |
| `b/lt` | [b_lt.madlen](../besm6/libb/b_lt.madlen) | `<` | `x-a` (compute `stack - acc`) | `u1a`: branch if `a < b` |
| `b/le` | [b_le.madlen](../besm6/libb/b_le.madlen) | `<=` | `a-x` (compute `acc - stack`) | `uza`: branch if `a <= b` |
| `b/gt` | [b_gt.madlen](../besm6/libb/b_gt.madlen) | `>` | `a-x` | `u1a`: branch if `a > b` |
| `b/ge` | [b_ge.madlen](../besm6/libb/b_ge.madlen) | `>=` | `x-a` | `uza`: branch if `a >= b` |

> **Note on `b/eq`:** The `aex` instruction exchanges accumulator A and extension register X.
> For equality, `aex` followed by `uza` tests whether the original stack value (now in A)
> equals the accumulator value (now in X) — effectively `a - b == 0` after the exchange.
