#include <stdio.h>
/* Shared stdout line-buffer state, also referenced by putch.c and flush.c. */
int out_cnt;
int out_shft;
int out_buff[22];
/* Empty stub: real byte emission for the Unix target is a later task. */
void putbyte(int b) { (void)b; }
