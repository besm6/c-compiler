// read(int fd, char *buf, int n) — Unix v7 syscall leaf for the b6sim target.
//
// Same convention as write.s: fd/buf are already in the syscall argument slots
// below M[017] and n is in the accumulator; buf is the compiler's char* fat
// pointer, into which b6sim writes the bytes read.  Returns the byte count in
// the accumulator (0 at EOF, -1 with errno in r14 on error).
//
    .text
    .globl read
read:
    $77 3       // SYS_read
 13 uj          // return via r13
