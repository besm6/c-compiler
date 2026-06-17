/*
 * Formatted output into a bounded caller buffer.
 *
 * Writes at most size-1 characters plus a terminating NUL into buf, and returns
 * the number of characters that would have been written had the buffer been
 * large enough.  Shares the engine __doprnt with printf (see doprnt.c); the
 * variadic arguments are reached via the address of the anchor parameter `args`.
 */
extern int __doprnt(char *fmt, int *ap, char *buf, int size, int to_buf);

int snprintf(char *buf, int size, char *fmt, int args, ...)
{
    return __doprnt(fmt, &args, buf, size, 1);
}
