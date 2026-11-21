#ifndef CV_MATH_H
#define CV_MATH_H
#ifdef CV_FREESTANDING
/* Compiler intrinsics only; compile with -fno-math-errno so x87 fsqrt does
 * not introduce a CRT call. All fmax arguments in this module are finite. */
#define sqrt(x) __builtin_sqrt(x)
#define fabs(x) __builtin_fabs(x)
#define isfinite(x) __builtin_isfinite(x)
static inline double cv_max_finite(double a,double b){return a>b?a:b;}
#define fmax(a,b) cv_max_finite((a),(b))
#else
#include <math.h>
#endif
#endif
