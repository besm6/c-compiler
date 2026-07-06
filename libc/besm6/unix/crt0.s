// crt0.s — C startup for the BESM-6 Unix (b6as/b6ld/b6sim) target.
//
// b6sim's loader seeds r15 (the stack pointer, M[017]) and the heap break just
// past bss, and sets PC to this object's entry (a_entry).  Link crt0.o FIRST
// (or pass `-e _start`) so `_start` is the executable's entry point.
//
// Startup calls main(argc, argv) with argc=0 and a dummy argv[] (one NULL
// entry), following the C convention: arg1 (argc) pushed, last arg (argv) in
// the accumulator, r14 = -argc_count.  main's int result is left in the
// accumulator and passed straight to _exit (SYS_exit, $77 1).
//
    .text
    .globl _start
_start:
    xta             // ACC = argc = 0 (bare xta reads mem[0], architecturally 0)
    xts argvp       // push argc; ACC = argv (word address of the dummy argv[])
 14 vtm -2          // r14 = -argc_count (two args)
 13 vjm main        // call main(argc, argv); int result left in ACC
    $77 1           // _exit(status): SYS_exit, status already in ACC
//
    .data
argv0:
    .word 0         // argv[0] = NULL terminator (argc == 0)
argvp:
    .word argv0     // char **argv (plain word pointer, not a fat byte pointer)
