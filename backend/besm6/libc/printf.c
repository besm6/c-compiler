/*
 * Formatted output: %d decimal, %o octal, %c character, %s string, %% literal.
 *
 * C rewrite of printf.b.  There is no <stdarg.h>; instead we walk the parameter
 * block directly.  Arguments are pushed in direct order at increasing addresses
 * (arg#1 at r6+0, arg#2 at r6+1, ...), so taking the address of the last named
 * parameter and incrementing the pointer steps through the variadic arguments.
 *
 * Format letters are written uppercase: a format string is transcoded to KOI7,
 * which folds lowercase letters onto the uppercase codes, so a caller's "%d" and
 * "%D" both arrive as 'D'.  The KOI7-identity uppercase constant matches either.
 */
extern void putbyte(int b);
extern void putch(unsigned ch);
extern void print_d(int n);
extern void print_o(unsigned n);

void printf(char *fmt, int args, ...)
{
    int *ap, a, c, i, n;

    i  = 0;
    ap = &args;
loop:
    while ((c = fmt[i]) != '%') {
        if (c == '\0')
            return;
        putbyte(c);
        ++i;
    }
    ++i;
    c = fmt[i];
    if (c == '%') {
        putbyte('%');
        ++i;
        goto loop;
    }
    a = *ap;
    if (c == 'D') {
        print_d(a);
    } else if (c == 'O') {
        print_o(a);
    } else if (c == 'C') {
        putch(a);
    } else if (c == 'S') {
        char *s = (char *)a;
        n       = 0;
        while ((c = s[n]) != '\0') {
            putbyte(c);
            ++n;
        }
    } else {
        /* bad format specification, ignore */
        putbyte('%');
        goto loop;
    }
    ++i;
    ++ap;
    goto loop;
}
