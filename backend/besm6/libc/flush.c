/*
 * Flush the buffered output line.
 *
 * C rewrite of flush.b.  Normally the line goes to stdout via b/tout; when fout
 * is set it is written to drum via wrcard.  Afterwards the buffer is cleared.
 *
 * b$tout names the hand-written helper b/tout: the '$' is sanitized to '/' by
 * the BESM-6 backend.  It takes the buffer address in the accumulator (as the
 * single argument) and preserves the C frame registers.
 */
extern void putbyte(int b);
extern void b$tout(int *buf);
extern void wrcard(int dev, int *buf);

extern int out_cnt, out_shft;
extern int out_buff[22];

int fout;

void flush(void)
{
    if (fout) {
        /* write to drum */
        while (out_cnt < 14) {
            putbyte(' ');
        }
        wrcard(0, &out_buff[0]);
        putbyte(0);
    } else {
        /* write to standard output */
        b$tout(&out_buff[0]);
    }

    if (out_cnt | out_shft) {
        /* clear buffer */
        if (out_shft) {
            ++out_cnt;
            out_shft = 0;
        }
        while (out_cnt > 0) {
            out_buff[--out_cnt] = 0;
        }
    }
}
