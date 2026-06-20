/*
 * <math.h> — mathematics (C11 §7.12), BESM-6 target.
 *
 * Status: modf() is implemented in libc.bin; the rest are declared for future
 * implementation (TODO).  float == double == long double (one word), so the
 * single double-typed entry points serve every floating type.
 *
 * BESM-6 floating point has NO infinities or NaNs, so INFINITY and NAN are not
 * provided.  HUGE_VAL is approximated by the largest finite value.
 */
#ifndef _MATH_H
#define _MATH_H

#include <float.h>

#define HUGE_VAL  DBL_MAX
#define HUGE_VALF FLT_MAX
#define HUGE_VALL LDBL_MAX

#define M_PI 3.14159265358979
#define M_E  2.71828182845905

/* ---- implemented in libc.bin ---- */
double modf(double x, double *iptr);

/* ---- declared for future implementation (TODO) ---- */
double fabs(double x);
double floor(double x);
double ceil(double x);
double round(double x);
double trunc(double x);
double fmod(double x, double y);
double frexp(double x, int *exp);
double ldexp(double x, int exp);

double sqrt(double x);
double pow(double x, double y);
double exp(double x);
double log(double x);
double log10(double x);

double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);
double sinh(double x);
double cosh(double x);
double tanh(double x);

double hypot(double x, double y);
double fmin(double x, double y);
double fmax(double x, double y);
double copysign(double x, double y);

#endif /* _MATH_H */
