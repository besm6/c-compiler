/*
 * Formatted output to standard output.
 *
 * Thin wrapper over the shared formatting engine __doprnt (see doprnt.c).  The
 * variadic arguments are reached through <stdarg.h>: va_start aims a va_list at
 * the first conversion argument and the engine walks the parameter block one
 * word per argument.
 *
 * Supported conversions: %d %i %u %o %x %X %c %s %p %f %e %g %% with the flags
 * '- 0 + space #', field width (incl. '*') and precision (incl. '.*').  Because
 * output is KOI7 with case folding, all letters print upper case and %x/%X,
 * %e/%E, %g/%G are indistinguishable.
 */
#include <stdio.h>

extern int __doprnt(const char *fmt, va_list ap, char *buf, int size, int to_buf);

int printf(const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = __doprnt(fmt, ap, 0, 0, 0);
    va_end(ap);
    return n;
}
