/*
 * puts — write the string s followed by a newline to stdout (C11 §7.21.7.9).
 *
 * Built on putbyte, which packs each byte into the KOI7 stdout buffer and
 * flushes on a newline; the trailing putbyte('\n') terminates and flushes the
 * line.  s is a fat char* cursor that crosses word boundaries on its own.
 * Returns a non-negative value on success.
 */
#include <stdio.h>

int puts(const char *s)
{
    while (*s != 0) {
        putbyte(*s);
        s++;
    }
    putbyte('\n');
    return 0;
}
