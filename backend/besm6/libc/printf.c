/*
 * Formatted output to standard output.
 *
 * Thin wrapper over the shared formatting engine __doprnt (see doprnt.c).  There
 * is no <stdarg.h>; the variadic arguments are reached by taking the address of
 * the named anchor parameter `args` (the first conversion argument, at r6+1) and
 * walking the parameter block one word per argument.
 *
 * Supported conversions: %d %i %u %o %x %X %c %s %p %f %e %g %% with the flags
 * '- 0 + space #', field width (incl. '*') and precision (incl. '.*').  Because
 * output is KOI7 with case folding, all letters print upper case and %x/%X,
 * %e/%E, %g/%G are indistinguishable.
 */
extern int __doprnt(char *fmt, int *ap, char *buf, int size, int to_buf);

int printf(char *fmt, int args, ...)
{
    return __doprnt(fmt, &args, 0, 0, 0);
}
