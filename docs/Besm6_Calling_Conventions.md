# Calling Conventions for C on BESM-6

## Registers

On BESM-6 we have fifteen general purpose registers named r1-15.
We use decimal numbering.

## On Call

 * The calling function pushes the arguments onto the stack in direct order.
 * The last argument passed remains in the accumulator.
 * Register r14 contains the number of passed arguments, *negative*.
 * Register r13 contains the return address to the calling function.

## On Return

 * The result value is returned in the accumulator.
 * Registers r1-r7 must be preserved by a called function.

### Returning a struct by value

A struct (or union) return value is classified by size:

 * **One machine word or smaller** (≤ 6 bytes): returned in the accumulator, exactly
   like a scalar.
 * **Larger than one word**: returned via a hidden pointer (the *sret* convention).
   The caller allocates the result storage and passes its address as an implicit
   **first argument**, shifting the declared parameters to slots 1, 2, …  The callee
   writes the whole struct through that pointer and also returns the pointer in the
   accumulator.  This lowering is performed entirely in the translator, so the machine
   backend never sees a struct-valued `RETURN` or a struct-valued call result.

 * Register r15 (stack pointer) has to be decremented by a called function
   by the number of arguments passed.
 * When the called function has 1 or more **parameters**, on return it should
   decrement r15 by the number of **passed arguments** minus 1.
 * When the called function has no **parameters**, it should return r15 unchanged.

## Example 1

Calling function with no arguments (and parameters). In C:

    flush()

In assembler:

    13 ,vjm, flush

## Example 2

Calling function with one argument. In C:

    putch(a)

In assembler:

       ,xta, a
    14 ,vtm, -1
    13 ,vjm, putch

## Example 3

Calling function with three arguments an a result. In C:

    result = foobar(a, b, c)

In assembler:

       ,xta, a
       ,xts, b
       ,xts, c
    14 ,vtm, -3
    13 ,vjm, foobar
       ,atx, result

# Stack Frame

The stack on BESM-6 grows towards increasing addresses.
In the pictures it means downwards.

Here is the stack layout at the moment of calling a function with N arguments.
Note that the last argument #N is located in the accumulator.

    ┌────────────────────┐
    │    argument #1     │◀── r15 initially (and after return)
    ├────────────────────┤
    │        ...         │
    ├────────────────────┤
    │    argument #N-1   │
    ├────────────────────┤
    │                    │◀── r15 after pushing arguments on stack

Here is the stack layout of the called function.

    ┌────────────────────┐
    │    argument #1     │◀── r6 as Parameter Pointer
    ├────────────────────┤
    │        ...         │
    ├────────────────────┤
    │    argument #N     │
    ├────────────────────┤
    │    saved r13       │
    ├────────────────────┤
    │    saved r7        │
    ├────────────────────┤
    │    saved r6        │
    ├────────────────────┤
    │  auto variable #1  │◀── r7 as Auto Pointer
    ├────────────────────┤
    │        ...         │
    ├────────────────────┤
    │  auto variable #N  │
    ├────────────────────┤
    │                    │◀── r15 as Stack Pointer


## Save Context

Every C function should start with:

       ,its, 13
    13 ,vjm, c/save

The c/save routine performs the following actions:

 * Save registers r13, r7 and r6 to the stack
 * Set r6 to point to the first argument
 * Set r7 to point to the first automatic variable (which will be allocated later)

Example:

    c/save: ,name,
         15 ,j+m, 14
            ,its, 7
            ,its, 6
            ,its,
         14 ,mtj, 6
         15 ,mtj, 7
         13 ,uj,

## Restore Context

On return, every C function should perform:

        ,uj, c/ret

The c/ret routine does the following:

 * Restore saved registers r13, r7 and r6
 * Restore stack pointer r15
 * Jump to the calling function

Example:

     c/ret: ,name,
          6 ,mtj, 14
          7 ,mtj, 15
          7 ,stx, -4
            ,sti, 6
            ,sti, 7
            ,sti, 13
         14 ,mtj, 15
         13 ,uj,
