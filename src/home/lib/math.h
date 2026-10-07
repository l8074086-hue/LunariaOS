#ifndef MATH_H
#define MATH_H

/* Only the pieces the compiler core needs: infinities for its own parser and
   ldexpl() for scaling long-double literals. */

#define HUGE_VAL  (__builtin_huge_val())
#define HUGE_VALF (__builtin_huge_valf())
#define HUGE_VALL (__builtin_huge_vall())

#define INFINITY  (__builtin_inff())
#define NAN       (__builtin_nanf(""))

double ldexp(double x, int exp);
float ldexpf(float x, int exp);
long double ldexpl(long double x, int exp);

#endif
