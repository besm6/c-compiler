/*
 * memcpy — copy n bytes from src to dest (C11 §7.24.2.1).
 *
 * The objects must not overlap; use memmove for overlapping regions.  On the
 * BESM-6 char* / void* are fat pointers, so the byte cursors d and s advance
 * across word boundaries on their own (b/pinc).
 */
#include <string.h>

void *memcpy(void *dest, const void *src, size_t n)
{
    char *d = dest;
    const char *s = src;
    while (n > 0) {
        *d = *s;
        d++;
        s++;
        n--;
    }
    return dest;
}
