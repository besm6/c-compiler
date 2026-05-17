# Type Coercion in C11

This document describes the implicit type conversion rules defined by the C11
standard and explains how this compiler implements them.  It is intended as a
companion to `semantic/coercion_tests.cpp`.

---

## 1. Where coercion fires

Implicit conversions happen in exactly three contexts:

| Context | Standard reference | Compiler site |
|---|---|---|
| Assignment `lhs = rhs` | §6.5.16.1 | `coerce_for_assignment()` called from `initializers.c` and `expressions.c` (EXPR_ASSIGN) |
| Function call argument | §6.5.2.2 p7 | `coerce_for_assignment()` called for each argument in `expressions.c` (EXPR_CALL) |
| Return statement | §6.8.6.4 p3 | `coerce_for_assignment()` called in `statements.c` (STMT_RETURN) |

All three contexts apply the **assignment conversion rules** described below.
Binary and unary expressions additionally apply **integer promotions** and the
**usual arithmetic conversions**.

---

## 2. Integer promotions (§6.3.1.1)

Before most arithmetic and bitwise operations every operand narrower than `int`
is widened:

- If `int` can represent all values of the original type → promote to **`int`**
- Otherwise → promote to **`unsigned int`**

**Types affected**: `char`, `signed char`, `unsigned char`, `short`,
`unsigned short`, and single-bit `_Bool`.

**When they apply**: unary `+`, `-`, `~`; both operands of binary `<<`, `>>`,
`&`, `|`, `^`; and as the first step of the usual arithmetic conversions.

The compiler inserts an `EXPR_CAST` node for each promoted operand.  The cast
target kind is always `TYPE_INT` (the compiler uses `int`-width integers as the
minimum result type; `unsigned int` promotion of very-wide `unsigned char` is
not currently distinguished from the signed case).

---

## 3. Usual arithmetic conversions (§6.3.1.8)

Applied to both operands of `+`, `-`, `*`, `/`, `%`, `&`, `|`, `^`, `==`,
`!=`, `<`, `>`, `<=`, `>=` to find a **common type**:

1. Apply integer promotions to both operands.
2. If both operands now have the same type → done; that is the result type.
3. If either operand is `double` → convert the other to `double`.
4. If either operand is `float` → convert the other to `float`.
5. If both are integers of the same size:
   - if one is unsigned and the other signed → result is unsigned.
6. Otherwise → the operand with the larger size wins.

The result is computed by `get_common_type()` in `semantic/typecheck.c`.

### Current implementation gaps

- **`float` fast-path missing** (step 4 above).  `get_common_type()` checks for
  `double` but not `float`.  When `sizeof(float) == sizeof(int)` the code falls
  through to `is_signed(float)` which calls `fatal_error()`.  Correct C11
  behaviour: `float + int → float`.  The fix is to add a `float` branch before
  the size comparison, mirroring the existing `double` branch.  Exposed by the
  `CoercionTest.FloatPlusInt_GivesFloat` test.

- **`short` not promoted in unary contexts**.  The unary `+`, `-`, and `~`
  handlers only promote `char`/`schar`/`uchar` to `int`; they leave `short` and
  `unsigned short` at their original width.  C11 §6.3.1.1 requires that any
  integer whose rank is less than `int` be promoted.  In binary expressions this
  is partially masked because `get_common_type()` returns the wider type, but
  `short + short` yields `short` rather than `int`.  Documented by the
  `CoercionTest.ShortUnaryPlus_NotPromoted` test.

- **`long double` not handled**.  The compiler does not expose `long double` as
  a distinct type, so this is not a gap in practice.

---

## 4. Assignment constraints (§6.5.16.1)

The left-hand side must be a modifiable lvalue.  The conversion rules for the
right-hand side are:

| RHS type | LHS type | Allowed? | Conversion |
|---|---|---|---|
| Arithmetic | Arithmetic | **Yes** | Implicit widening or narrowing cast |
| Any object pointer | `void *` | **Yes** | Implicit cast |
| `void *` | Any object pointer | **Yes** | Implicit cast |
| Integer constant `0` (null pointer constant) | Any pointer | **Yes** | Implicit cast to null pointer |
| Same struct/union type | Same struct/union type | **Yes** | No cast (direct copy) |
| Incompatible pointer | Any pointer | **No** | `fatal_error` |
| Integer (non-zero) | Any pointer | **No** | `fatal_error` |
| Any pointer | Integer | **No** | `fatal_error` |
| Different struct/union type | Any struct/union type | **No** | `fatal_error` (see known gap below) |

### Null pointer constant

A **null pointer constant** is an integer constant expression with value `0`, or
`(void *)0`.  Either form may be implicitly converted to any pointer type,
yielding a null pointer.  The compiler recognises the integer literal form in
`is_null_pointer_constant()`.  The `(void *)0` cast form is handled
automatically by the void-pointer compatibility rule above.

### void* compatibility

`void *` converts implicitly to and from any object pointer.  This rule does
**not** apply to function pointers.

### Known implementation gap — struct/union tag check

`coerce_for_assignment()` tests only the `TypeKind` of both sides.  Because
distinct `struct` types share the same kind (`TYPE_STRUCT`), assigning
`struct B` to `struct A` is not rejected even though it violates §6.5.16.1.
The fix is to compare the struct tag strings when `e_type->kind == TYPE_STRUCT`.
The test `CoercionTest.DiffStructError` documents this bug.

---

## 5. Function argument coercion (§6.5.2.2)

With a visible prototype, each argument undergoes implicit conversion as if by
assignment to the corresponding parameter type.  The same rules as §4 above
apply.  Variadic arguments (`...`) additionally undergo the **default argument
promotions**: `float` → `double`, and integer promotions; the compiler does not
implement variadic promotion separately because calls to variadic functions are
currently typed via the fixed-parameter portion of the prototype.

---

## 6. Return statement (§6.8.6.4)

The return value is converted as if by assignment to the function's declared
return type.  The same rules as §4 above apply.  Returning any value from a
`void` function, or returning no value from a non-`void` function, produces a
`fatal_error`.

---

## 7. Compiler implementation summary

### `coerce_for_assignment(e, target_type)` — `semantic/typecheck.c:256`

```
resolve typedef aliases on both sides
if kinds match AND (not pointer OR target pointer-target kinds match)
    → return e unchanged  (no cast)
if both arithmetic
    → convert_to_type(e, target_type)  (insert EXPR_CAST)
if e is a null pointer constant AND target is pointer
    → convert_to_type(e, target_type)
if (target is void* AND e is any pointer)
   OR (target is any pointer AND e is void*)
    → convert_to_type(e, target_type)
otherwise
    → fatal_error("Cannot convert type for assignment")
```

### `get_common_type(t1, t2)` — `semantic/typecheck.c:184`

```
promote char operands to int
if same kind → return t1
if either is double → return double          ← float branch is missing here
if same size → return unsigned one (calls is_signed — fatal_error on float!)
return the larger type
```

### `convert_to_type(e, target_type)` — `semantic/typecheck.c:151`

Wraps `e` in an `EXPR_CAST` node unless the types are already the same kind.

### `convert_to_kind(e, target_kind)` — `semantic/typecheck.c:168`

Wraps `e` in an `EXPR_CAST` node with a fresh `Type` of the given kind, unless
the expression already has that kind.  Used for integer promotions.

---

## 8. References

- C11 standard draft N1570, available at <https://port70.net/~nsz/c/c11/n1570.html>
- cppreference — [Implicit conversions](https://en.cppreference.com/w/c/language/conversion)
- cppreference — [Assignment operators](https://en.cppreference.com/w/c/language/operator_assignment)
