#include "math.h"

// Use x87 FPU instructions — available in user-mode ring-0 ELFs.

double sin(double x) {
    double r;
    asm volatile ("fsin" : "=t"(r) : "0"(x));
    return r;
}

double cos(double x) {
    double r;
    asm volatile ("fcos" : "=t"(r) : "0"(x));
    return r;
}

double tan(double x) {
    double s = sin(x);
    double c = cos(x);
    return s / c;
}

double sqrt(double x) {
    double r;
    asm volatile ("fsqrt" : "=t"(r) : "0"(x));
    return r;
}

double fabs(double x) {
    double r;
    asm volatile ("fabs" : "=t"(r) : "0"(x));
    return r;
}

double floor(double x) {
    long long i = (long long)x;
    return (double)(x < (double)i ? i - 1 : i);
}

double ceil(double x) {
    long long i = (long long)x;
    return (double)(x > (double)i ? i + 1 : i);
}

// pow via repeated multiplication — good enough for integer exponents;
// for fractional exponents uses exp(exp * log(base)).
// Approximations below are sufficient for typical interpreter use.

// Natural log via x87 fyl2x: log(x) = log2(x) / log2(e)
double log(double x) {
    double r;
    asm volatile (
        "fldln2\n\t"
        "fxch\n\t"
        "fyl2x"
        : "=t"(r) : "0"(x)
    );
    return r;
}

// e^x via x87 f2xm1 + fscale
double exp(double x) {
    double r;
    asm volatile (
        "fldl2e\n\t"   // st0 = log2(e)
        "fmulp\n\t"    // st0 = x * log2(e)
        "fld1\n\t"
        "fscale\n\t"   // st0 = 2^trunc(x*log2e), st1 = x*log2e
        "fxch\n\t"
        "fld1\n\t"
        "fxch\n\t"
        "fprem\n\t"    // st0 = frac(x*log2e)
        "f2xm1\n\t"   // st0 = 2^frac - 1
        "fld1\n\t"
        "faddp\n\t"    // st0 = 2^frac
        "fmulp"        // st0 = 2^trunc * 2^frac = e^x
        : "=t"(r) : "0"(x)
    );
    return r;
}

double pow(double base, double exponent) {
    if (exponent == 0.0) return 1.0;
    return exp(exponent * log(base));
}
