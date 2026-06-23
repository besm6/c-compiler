/*
 * fmin — the smaller of x and y (C11 §7.12.12.3).
 *
 * As with fmax, C's "return the non-NaN argument" rule is moot on BESM-6 (no
 * NaN), so a plain FP comparison suffices.
 */

#include <math.h>

double fmin(double x, double y)
{
    return x < y ? x : y;
}
