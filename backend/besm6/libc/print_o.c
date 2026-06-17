/*
 * Print an unsigned integer in octal.
 *
 * C rewrite of printo.b.  Recurses on the high-order octal digits, then emits
 * the low digit.  `unsigned` makes `>>` a logical shift (matching B).
 */
extern void putbyte(int b);

void print_o(unsigned n)
{
    unsigned a = n >> 3;

    if (a) {
        print_o(a);
    }
    putbyte((n & 7) + '0');
}
