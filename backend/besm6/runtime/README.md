# C Runtime Library for BESM-6

Runtime library for C programs targeting the BESM-6 mainframe.

For basing, use `r14`. No other functions must be called, and no extracodes.

## Compiler-Support Routines

These are internal helpers emitted by the compiler; they are not called directly from C source.

### Calling Convention

| Routine | Source | Description |
|---------|--------|-------------|
| `b/save` | [b_save.madlen](b_save.madlen) | Save registers on function entry (1+ parameters) |
| `b/save0` | [b_save0.madlen](b_save0.madlen) | Save registers on function entry (0 parameters) |
| `b/ret` | [b_ret.madlen](b_ret.madlen) | Restore registers and return from a C function |
| `b/true` | [b_true.madlen](b_true.madlen) | Constant `1` (logical true value) |

### Arithmetic Operators

| Routine | Source | C operator | Description |
|---------|--------|-----------|-------------|
| `b/mul` | [b_mul.madlen](b_mul.madlen) | `*` | Integer multiply |
| `b/div` | [b_div.madlen](b_div.madlen) | `/` | Integer divide |
| `b/mod` | [b_mod.madlen](b_mod.madlen) | `%` | Integer modulo |

### Relational and Logical Operators

| Routine | Source | C operator | Description |
|---------|--------|-----------|-------------|
| `b/not` | [b_not.madlen](b_not.madlen) | `!` | Logical NOT |
| `b/eq` | [b_eq.madlen](b_eq.madlen) | `==` | Equal |
| `b/ne` | [b_ne.madlen](b_ne.madlen) | `!=` | Not equal |
| `b/lt` | [b_lt.madlen](b_lt.madlen) | `<` | Less than |
| `b/le` | [b_le.madlen](b_le.madlen) | `<=` | Less than or equal |
| `b/gt` | [b_gt.madlen](b_gt.madlen) | `>` | Greater than |
| `b/ge` | [b_ge.madlen](b_ge.madlen) | `>=` | Greater than or equal |
