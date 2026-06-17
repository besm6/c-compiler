/*
 * Floating-point helpers needed by the printf engine (doprnt.c).
 *
 * The BESM-6 native floating-point format carries a 40-bit mantissa (~12
 * significant decimal digits) and has no IEEE NaN/Inf encodings, so isnan() and
 * isinf() are constant-false.
 *
 * modf() splits x into its integral and fractional parts, both with the sign of
 * x.  Any magnitude at or above 2^40 is already integral in a 40-bit mantissa,
 * so the fractional part is zero; below that the integer part fits in a 41-bit
 * `int`, so a truncating round trip through int recovers it exactly.
 */

/* 2^40 — the smallest magnitude with no fractional bits left in the mantissa. */
static double two40 = 1099511627776.0;

double modf(double x, double *iptr)
{
    double a, ip;

    if (x < 0) {
        a = -x;
        if (a >= two40)
            ip = x;
        else
            ip = -(double)(int)a;
    } else {
        if (x >= two40)
            ip = x;
        else
            ip = (double)(int)x;
    }
    *iptr = ip;
    return x - ip;
}

int isnan(double x)
{
    (void)x;
    return 0;
}

int isinf(double x)
{
    (void)x;
    return 0;
}
