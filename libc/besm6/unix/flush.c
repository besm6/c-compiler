/*
 * Flush the stdout line buffer (Unix target).
 *
 * putbyte packs six KOI7 bytes big-endian into each word of out_buff; out_cnt
 * counts full words and out_shft the bit offset within the current word, so the
 * pending byte count is out_cnt*6 + out_shft/8.  We hand that buffer to the
 * SYS_write leaf as a char* fat pointer (the (char *) cast produces the byte-0
 * fat pointer b6sim decodes), then zero the used words and reset the cursor.
 *
 * Unlike the Madlen flush there is no drum/`fout` path — Unix has only fd 1.
 */
#include <stdio.h>

extern int write(int fd, char *buf, int n);

extern int out_cnt, out_shft;
extern int out_buff[22];

void flush(void)
{
    int len;

    len = out_cnt * 6 + out_shft / 8;
    if (len > 0)
        write(1, (char *)&out_buff[0], len);

    if (out_cnt | out_shft) {
        if (out_shft) {
            ++out_cnt;
            out_shft = 0;
        }
        while (out_cnt > 0)
            out_buff[--out_cnt] = 0;
    }
}
