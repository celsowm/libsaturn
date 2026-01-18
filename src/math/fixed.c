#include "saturn/fixed.h"
#include <math.h>

fix16_t fix16_floor(fix16_t x) {
    return x & ~0xFFFF;
}

fix16_t fix16_mod(fix16_t x, fix16_t m) {
    return x % m;
}

fix16_t fix16_abs(fix16_t x) {
    return x < 0 ? -x : x;
}

fix16_t fix16_pow(fix16_t x, int n) {
    fix16_t result = FIX16_ONE;
    fix16_t base = x;
    while (n > 0) {
        if (n & 1) {
            result = fix16_mul(result, base);
        }
        base = fix16_mul(base, base);
        n >>= 1;
    }
    return result;
}

fix16_t fix16_sin(fix16_t angle) {
    const fix16_t PI = 102943;
    const fix16_t TWO_PI = fix16_mul(2, PI);
    fix16_t normalized = fix16_mod(angle, TWO_PI);
    if (normalized > PI) {
        normalized = normalized - TWO_PI;
    }

    fix16_t x = fix16_mul(normalized, normalized);
    fix16_t result = normalized;
    fix16_t term = normalized;

    term = fix16_mul(term, fix16_div(x, -6));
    result += term;

    term = fix16_mul(term, fix16_div(x, -20));
    result += term;

    term = fix16_mul(term, fix16_div(x, -42));
    result += term;

    term = fix16_mul(term, fix16_div(x, -72));
    result += term;

    return result;
}

fix16_t fix16_cos(fix16_t angle) {
    const fix16_t PI = 102943;
    const fix16_t TWO_PI = fix16_mul(2, PI);
    fix16_t normalized = fix16_mod(angle, TWO_PI);
    if (normalized > PI) {
        normalized = normalized - TWO_PI;
    }

    fix16_t x = fix16_mul(normalized, normalized);
    fix16_t result = FIX16_ONE;

    fix16_t term = FIX16_ONE;
    term = fix16_mul(term, fix16_div(x, -2));
    result += term;

    term = fix16_mul(term, fix16_div(x, -12));
    result += term;

    term = fix16_mul(term, fix16_div(x, -30));
    result += term;

    term = fix16_mul(term, fix16_div(x, -56));
    result += term;

    term = fix16_mul(term, fix16_div(x, -90));
    result += term;

    return result;
}

fix16_t fix16_assembly_mul(fix16_t a, fix16_t b) {
    return fix16_mul(a, b);
}
