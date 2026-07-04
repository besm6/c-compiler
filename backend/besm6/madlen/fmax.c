/*
 * fmax — the larger of x and y (C11 §7.12.12.2).
 *
 * C says fmax returns the non-NaN argument when exactly one is NaN; that rule is
 * moot on BESM-6, which has no NaN, so a plain FP comparison suffices.
 */

#include <math.h>

double fmax(double x, double y)
{
    return x < y ? y : x;
}
