/*
 * Print a signed decimal integer.
 *
 * C rewrite of printd.b.  Emits a leading '-' for negatives, then recurses on
 * the high-order digits before emitting the low digit.
 */
extern void writeb(int b);

void print_d(int n)
{
    int a;

    if (n < 0) {
        writeb('-');
        n = -n;
    }
    a = n / 10;
    if (a) {
        print_d(a);
    }
    writeb(n - a * 10 + '0');
}
