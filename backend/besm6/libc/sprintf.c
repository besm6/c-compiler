/*
 * Formatted output into an unbounded caller buffer.
 *
 * Equivalent to snprintf with no size limit: writes the formatted string plus a
 * terminating NUL into buf and returns its length.  Shares the engine __doprnt
 * with printf (see doprnt.c).  A large nominal size stands in for "unbounded".
 */
extern int __doprnt(char *fmt, int *ap, char *buf, int size, int to_buf);

int sprintf(char *buf, char *fmt, int args, ...)
{
    return __doprnt(fmt, &args, buf, 1 << 24, 1);
}
