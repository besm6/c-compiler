/*
 * Core formatting engine shared by printf / sprintf / snprintf.
 *
 * Derived from the FreeBSD kernel printf, heavily rewritten for the BESM-6 C
 * runtime.  Key adaptations:
 *
 *   - No <stdarg.h>.  Variadic arguments are read through an explicit pointer
 *     (`ap`) into the caller's parameter block; every BESM-6 argument occupies
 *     exactly one word, so a single `int *ap` walks all argument types.
 *
 *   - No C preprocessor (the scanner handles only `#` line markers, not macros),
 *     so the sink is a file-scope state block driven by emit(), and named
 *     constants are enum values.
 *
 *   - The output device buffers KOI7 and folds letters to upper case.  Format
 *     conversion letters therefore arrive UPPER CASE ('%d' and '%D' both reach
 *     the engine as 'D'), and there is no lower-case output: %x/%X, %e/%E and
 *     %g/%G are indistinguishable and always print upper case.
 *
 *   - `long` == `int` and `double` == `float` (one word each), so the length
 *     modifiers 'L'/'H' are accepted and ignored.
 *
 * If `g_buf` is null the formatted bytes go to putbyte(); otherwise they are
 * stored into the caller buffer (at most g_size-1, NUL-terminated) for the
 * sprintf/snprintf family.  g_len always counts the total length that would be
 * produced, which is the return value.
 */
extern void putbyte(int b);
extern double modf(double x, double *iptr);

enum {
    MAXNBUF = 32, /* digits buffer: 48-bit octal (16) + sign + prefix + slack */
    DBL_DIG = 12, /* max meaningful significant digits for a 48-bit double     */
    FLT_DIG = 6,  /* default precision for %f/%e and significant digits for %g */
};

/*
 * Output sink, set up by __doprnt before formatting.  The destination is chosen
 * by the integer flag g_to_buf rather than by testing g_buf for null: a null
 * char* is encoded as a fat pointer (marker bit set), so its truthiness is not a
 * reliable mode switch.
 */
static int g_to_buf;  /* 1 = store into g_buf, 0 = emit via putbyte         */
static char *g_buf;   /* target buffer when g_to_buf                        */
static int g_size;    /* capacity of g_buf                                  */
static int g_len;     /* characters produced so far (the would-be length)   */

static void emit(int c)
{
    if (g_to_buf) {
        if (g_len < g_size - 1)
            g_buf[g_len] = (char)c;
    } else {
        putbyte(c);
    }
    ++g_len;
}

static void emit_pad(int c, int count)
{
    int n = count;

    while (n > 0) {
        emit(c);
        --n;
    }
}

/* Map a value 0..15 to its upper-case hex digit. */
static int mkhex(int ch)
{
    ch = ch & 15;
    if (ch > 9)
        return ch + 'A' - 10;
    return ch + '0';
}

/*
 * Convert `ul` to base `base` digits, written into nbuf in reverse order.
 * nbuf[0] is a 0 sentinel; digits occupy nbuf[1..].  At least `prec` digits are
 * produced (zero padded).  Returns the index of the most-significant digit (so
 * the caller walks the index downward to nbuf[1] to print MSD..LSD) and stores
 * the digit count in *lenp.
 */
static int ksprintn(char *nbuf, unsigned ul, int base, int prec, int *lenp)
{
    int p = 0;

    nbuf[0] = 0;
    for (;;) {
        ++p;
        nbuf[p] = (char)mkhex((int)(ul % (unsigned)base));
        ul      = ul / (unsigned)base;
        if (--prec > 0)
            continue;
        if (!ul)
            break;
    }
    if (lenp)
        *lenp = p;
    return p;
}

/*
 * Floating-point conversion (defined at the bottom of the file).  Works on the
 * caller buffer b[0..bsize-1] using integer indices, because this backend's
 * char* relational comparisons and pointer arithmetic are unreliable.  Stores
 * the digits, sets *startidx to the index of the first character, and returns
 * the character count.  b[0] is reserved for a rounding carry.
 */
static int cvt(double number, int prec, int sharpflag, int *negp, int fmtch, char *b, int bsize,
               int *startidx);

int __doprnt(char *fmt, int *ap, char *buf, int size, int to_buf)
{
    char nbuf[MAXNBUF];
    int i, c, base, ladjust, sharpflag, neg, dot;
    int n, width, dwidth, sign, blank, extrazeros, padding, msd, dlen;
    unsigned ul;
    char *s;

    g_to_buf = to_buf;
    g_buf    = buf;
    g_size   = size;
    g_len    = 0;

    i = 0;
    for (;;) {
        while ((c = fmt[i]) != '%') {
            if (!c)
                goto done;
            emit(c);
            ++i;
        }
        ++i;
        padding    = ' ';
        width      = 0;
        extrazeros = 0;
        ladjust    = 0;
        sharpflag  = 0;
        neg        = 0;
        sign       = 0;
        blank      = 0;
        dot        = 0;
        dwidth     = -1;

    reswitch:
        c = fmt[i];
        ++i;
        if (c == '.') {
            dot     = 1;
            padding = ' ';
            dwidth  = 0;
            goto reswitch;
        }
        if (c == '#') {
            sharpflag = 1;
            goto reswitch;
        }
        if (c == '+') {
            sign = -1;
            goto reswitch;
        }
        if (c == ' ') {
            blank = 1;
            goto reswitch;
        }
        if (c == '-') {
            ladjust = 1;
            goto reswitch;
        }
        if (c == 'L' || c == 'H') {
            /* length modifiers: long==int, short==int — accept and ignore */
            goto reswitch;
        }
        if (c == '*') {
            if (!dot) {
                width = *ap++;
                if (width < 0) {
                    ladjust = 1;
                    width   = -width;
                }
            } else {
                dwidth = *ap++;
            }
            goto reswitch;
        }
        if (c == '0' && !dot) {
            padding = '0';
            goto reswitch;
        }
        if (c >= '0' && c <= '9') {
            n = 0;
            for (;;) {
                n = n * 10 + c - '0';
                c = fmt[i];
                if (c < '0' || c > '9')
                    break;
                ++i;
            }
            if (dot)
                dwidth = n;
            else
                width = n;
            goto reswitch;
        }

        if (c == '%') {
            emit('%');
            continue;
        }

        if (c == 'C') {
            if (!ladjust)
                emit_pad(' ', width - 1);
            emit(*ap++);
            if (ladjust)
                emit_pad(' ', width - 1);
            continue;
        }

        if (c == 'S') {
            /* Detect a null argument from the raw word, then reconstruct the
             * char* from it.  Reading it back through char** would re-decorate a
             * null word into a (nonzero) fat pointer and hide the null. */
            n = *ap;
            ++ap;
            if (n == 0) {
                /* Null pointer.  Build "(NULL)" in nbuf rather than using a
                 * string literal: per-module string constants share the _strN
                 * namespace, so a literal here would collide with the caller's
                 * own string constants at link time. */
                nbuf[0] = '(';
                nbuf[1] = 'N';
                nbuf[2] = 'U';
                nbuf[3] = 'L';
                nbuf[4] = 'L';
                nbuf[5] = ')';
                nbuf[6] = 0;
                s       = nbuf;
            } else {
                s = *(char **)&n;
            }
            if (!dot) {
                n = 0;
                while (s[n])
                    ++n;
            } else {
                n = 0;
                while (n < dwidth && s[n])
                    ++n;
            }
            width -= n;
            if (!ladjust)
                emit_pad(' ', width);
            for (c = 0; c < n; ++c)
                emit(s[c]);
            if (ladjust)
                emit_pad(' ', width);
            continue;
        }

        /* ---- floating point ---- */
        if (c == 'F' || c == 'E' || c == 'G') {
            double d;
            int sidx, slen;
            d = *(double *)ap;
            ++ap;
            if (dwidth > DBL_DIG) {
                if (c != 'G' || sharpflag)
                    extrazeros = dwidth - DBL_DIG;
                dwidth = DBL_DIG;
            } else if (dwidth == -1) {
                dwidth = FLT_DIG;
            }
            if (d < 0) {
                neg = 1;
                d   = -d;
            }
            nbuf[0] = 0;
            slen    = cvt(d, dwidth, sharpflag, &neg, c, nbuf, MAXNBUF, &sidx);
            dlen    = slen + (neg ? 1 : 0);
            if (!ladjust && padding == ' ' && (width - dlen) > 0)
                emit_pad(' ', width - dlen);
            if (neg)
                emit('-');
            if (!ladjust && padding == '0' && (width - dlen) > 0)
                emit_pad('0', width - dlen);
            for (n = 0; n < slen; ++n) {
                if (extrazeros && nbuf[sidx + n] == 'E') {
                    emit_pad('0', extrazeros);
                    extrazeros = 0;
                }
                emit(nbuf[sidx + n]);
            }
            if (extrazeros)
                emit_pad('0', extrazeros);
            if (ladjust && (width - dlen) > 0)
                emit_pad(' ', width - dlen);
            continue;
        }

        /* ---- integer conversions ---- */
        if (c == 'D' || c == 'I') {
            ul = (unsigned)*ap++;
            if (!sign)
                sign = 1;
            base = 10;
            goto number;
        }
        if (c == 'U') {
            ul   = (unsigned)*ap++;
            base = 10;
            goto nosign;
        }
        if (c == 'O') {
            ul   = (unsigned)*ap++;
            base = 8;
            goto nosign;
        }
        if (c == 'X') {
            ul   = (unsigned)*ap++;
            base = 16;
            goto nosign;
        }
        if (c == 'P') {
            ul        = (unsigned)*ap++;
            base      = 16;
            sharpflag = 1;
            goto nosign;
        }

        /* unknown conversion: echo it verbatim */
        emit('%');
        emit(c);
        continue;

    nosign:
        sign  = 0;
        blank = 0;
    number:
        if (sign) {
            if ((int)ul < 0) {
                neg = '-';
                ul  = (unsigned)(-(int)ul);
            } else if (sign < 0) {
                neg = '+';
            } else if (blank) {
                neg = ' ';
            }
        }
        if (dwidth >= MAXNBUF) {
            extrazeros = dwidth - MAXNBUF + 1;
            dwidth     = MAXNBUF - 1;
        }
        msd = ksprintn(nbuf, ul, base, dwidth, &dlen);
        if (sharpflag && ul != 0) {
            if (base == 8)
                dlen += 1;
            else if (base == 16)
                dlen += 2;
        }
        if (neg)
            ++dlen;

        if (!ladjust && padding == ' ' && (width - dlen) > 0)
            emit_pad(' ', width - dlen);
        if (neg)
            emit(neg);
        if (sharpflag && ul != 0) {
            if (base == 8) {
                emit('0');
            } else if (base == 16) {
                emit('0');
                emit('X');
            }
        }
        if (extrazeros)
            emit_pad('0', extrazeros);
        if (!ladjust && padding == '0' && (width - dlen) > 0)
            emit_pad('0', width - dlen);
        for (n = msd; n >= 1; --n)
            emit(nbuf[n]);
        if (ladjust && (width - dlen) > 0)
            emit_pad(' ', width - dlen);
        continue;
    }

done:
    if (g_to_buf && g_size > 0) {
        n = g_len;
        if (n > g_size - 1)
            n = g_size - 1;
        g_buf[n] = 0;
    }
    return g_len;
}

/*
 * Round the decimal digits b[*startp .. endp] up by one unit in the last place,
 * propagating carries (index-based; see the cvt note).  `exp` (when non-null)
 * carries the e-format exponent so a carry out of the leading digit bumps it;
 * otherwise an f-format carry extends left into the reserved slot and decrements
 * *startp.  Mirrors the FreeBSD cvtround.
 */
static void cvtround(double fract, int *exp, char *b, int *startp, int endp, int ch, int *negp)
{
    double tmp;
    int up, start, end;

    start = *startp;
    end   = endp;

    if (fract) {
        modf(fract * 10, &tmp);
        up = (int)tmp;
    } else {
        up = ch - '0';
    }
    if (up > 4) {
        for (;; --end) {
            if (b[end] == '.')
                --end;
            ++b[end];
            if (b[end] <= '9')
                break;
            b[end] = '0';
            if (end == start) {
                if (exp) { /* e/E: increment exponent */
                    b[end] = '1';
                    ++*exp;
                } else { /* f: prepend a digit into the reserved slot */
                    --end;
                    b[end] = '1';
                    --start;
                }
                break;
            }
        }
    } else if (*negp) {
        /* "%.3f" of -0.0004 must not print a negative zero */
        for (;; --end) {
            if (b[end] == '.')
                --end;
            if (b[end] != '0')
                break;
            if (end == start)
                *negp = 0;
        }
    }
    *startp = start;
}

/* Append the exponent suffix ("E+NN") to b at index t; return the new index. */
static int exponent(char *b, int tin, int expin, int fmtch)
{
    char eb[8];
    int t, exp, k;

    t   = tin;
    exp = expin;

    b[t++] = (char)fmtch;
    if (exp < 0) {
        exp    = -exp;
        b[t++] = '-';
    } else {
        b[t++] = '+';
    }
    k = 8;
    if (exp > 9) {
        do {
            --k;
            eb[k] = (char)(exp % 10 + '0');
            exp   = exp / 10;
        } while (exp > 9);
        --k;
        eb[k] = (char)(exp + '0');
        for (; k < 8; ++k)
            b[t++] = eb[k];
    } else {
        b[t++] = '0';
        b[t++] = (char)(exp + '0');
    }
    return t;
}

/*
 * Format the magnitude `number` into b[] for %f/%e/%g.  Index-based throughout
 * because this backend's char* relational comparisons and pointer arithmetic
 * are unreliable.  b[0] is reserved for a rounding carry; formatting runs from
 * index 1.  *startidx receives the index of the first character (0 if a carry
 * extended left); the character count is returned.
 */
static int cvt(double number, int precin, int sharpflag, int *negp, int fmtch, char *b, int bsize,
               int *startidx)
{
    double fract, integer, tmp;
    int p, t, start, endp, expcnt, gformat, dotrim, prec, ftc;

    prec    = precin;
    ftc     = fmtch;
    expcnt  = 0;
    gformat = 0;
    dotrim  = 0;
    fract   = modf(number, &integer);

    endp  = bsize - 1;
    start = 1; /* reserved rounding slot at b[0] */
    t     = 1;

    /* integer part, least-significant first, into the top of the buffer */
    p = endp - 1;
    while (integer) {
        tmp  = modf(integer / 10, &integer);
        b[p] = (char)((int)((tmp + 0.01) * 10) + '0');
        --p;
        ++expcnt;
    }

    if (fmtch == 'F') {
        if (expcnt) {
            ++p;
            while (p < endp) {
                b[t++] = b[p];
                ++p;
            }
        } else {
            b[t++] = '0';
        }
        if (prec || sharpflag)
            b[t++] = '.';
        if (fract) {
            if (prec) {
                do {
                    fract  = modf(fract * 10, &tmp);
                    b[t++] = (char)((int)tmp + '0');
                } while (--prec && fract);
            }
            if (fract)
                cvtround(fract, 0, b, &start, t - 1, '0', negp);
        }
        for (; prec > 0; --prec)
            b[t++] = '0';
        *startidx = start;
        return t - start;
    }

    if (fmtch == 'E') {
    eformat:
        if (expcnt) {
            ++p;
            b[t++] = b[p];
            if (prec || sharpflag)
                b[t++] = '.';
            for (;;) {
                if (!prec)
                    break;
                ++p;
                if (p >= endp)
                    break;
                b[t++] = b[p];
                --prec;
            }
            if (!prec) {
                ++p;
                if (p < endp) {
                    fract = 0;
                    cvtround(0, &expcnt, b, &start, t - 1, b[p], negp);
                }
            }
            --expcnt;
        } else if (fract) {
            for (expcnt = -1;; --expcnt) {
                fract = modf(fract * 10, &tmp);
                if (tmp)
                    break;
            }
            b[t++] = (char)((int)tmp + '0');
            if (prec || sharpflag)
                b[t++] = '.';
        } else {
            b[t++] = '0';
            if (prec || sharpflag)
                b[t++] = '.';
        }
        if (fract) {
            if (prec) {
                do {
                    fract  = modf(fract * 10, &tmp);
                    b[t++] = (char)((int)tmp + '0');
                } while (--prec && fract);
            }
            if (fract)
                cvtround(fract, &expcnt, b, &start, t - 1, '0', negp);
        }
        for (; prec > 0; --prec)
            b[t++] = '0';
        if (gformat && !sharpflag) {
            while (t > start) {
                --t;
                if (b[t] != '0')
                    break;
            }
            if (b[t] == '.')
                --t;
            ++t;
        }
        t = exponent(b, t, expcnt, ftc);
        *startidx = start;
        return t - start;
    }

    /* fmtch == 'G' */
    if (!prec)
        ++prec;
    if (expcnt > prec || (!expcnt && fract && fract < 0.0001)) {
        --prec;
        ftc     = 'E';
        gformat = 1;
        goto eformat;
    }
    if (expcnt) {
        for (;;) {
            ++p;
            if (p >= endp)
                break;
            b[t++] = b[p];
            --prec;
        }
    } else {
        b[t++] = '0';
    }
    if (prec || sharpflag) {
        dotrim = 1;
        b[t++] = '.';
    }
    while (prec && fract) {
        fract  = modf(fract * 10, &tmp);
        b[t++] = (char)((int)tmp + '0');
        --prec;
    }
    if (fract)
        cvtround(fract, 0, b, &start, t - 1, '0', negp);
    if (sharpflag) {
        for (; prec > 0; --prec)
            b[t++] = '0';
    } else if (dotrim) {
        while (t > start) {
            --t;
            if (b[t] != '0')
                break;
        }
        if (b[t] != '.')
            ++t;
    }
    *startidx = start;
    return t - start;
}
