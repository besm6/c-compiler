// write(int fd, char *buf, int n) — Unix v7 syscall leaf for the b6sim target.
//
// The compiler's C-call convention already matches b6sim's syscall ABI: the
// earlier arguments sit just below the stack pointer (fd at M[017]-2, buf at
// M[017]-1) and the last argument (n) stays in the accumulator, so no shuffling
// is needed — issue the trap and return.  buf is the compiler's char* fat
// pointer, which b6sim decodes directly (see fat_to_byteptr in the simulator).
// Returns the byte count in the accumulator (-1 with errno in r14 on error).
//
    .text
    .globl write
write:
    $77 4       // SYS_write
 13 uj          // return via r13
