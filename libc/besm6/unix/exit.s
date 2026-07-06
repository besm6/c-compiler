// exit(int status) — terminate the program via the Unix v7 syscall trap.
// No return.  The status is the single C argument, already in the accumulator;
// b6sim uses its low 8 bits as the host process exit code.
//
// (The Madlen original issues the Dubna finish extracode $74, which is an
// illegal extracode under b6sim — here it becomes SYS_exit, $77 1.)
//
    .text
    .globl exit
exit:
    $77 1       // SYS_exit
