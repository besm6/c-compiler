/*
 * Append a multi-character word to the stdout buffer.
 *
 * C rewrite of write.b.  A word holds up to six bytes packed big-endian; this
 * emits them left to right, skipping leading zero bytes.  `unsigned` makes the
 * `>>` a logical shift (matching B).
 */
extern void writeb(int b);

void write(unsigned ch)
{
    int shift = 40, b;

    while (shift > 0) {
        b = (ch >> shift) & 0377;
        if (b)
            goto putchar;
        shift = shift - 8;
    }
    b = ch;
putchar:
    writeb(b);
    if (shift > 0) {
        shift = shift - 8;
        b = ch >> shift;
        goto putchar;
    }
}
