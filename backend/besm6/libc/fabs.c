/*
 * fabs — absolute value of x (C11 §7.12.7.2).
 *
 * On BESM-6 float == double == long double (one word) and there are no NaN/Inf,
 * so a plain sign test and FP negate suffice — no special-value handling.
 */

#include <math.h>

double fabs(double x)
{
    return x < 0 ? -x : x;
}
