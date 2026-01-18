#ifndef SATURN_FIXED_H
#define SATURN_FIXED_H

#include "saturn/types.h"

static inline fix16_t fix16_mul(fix16_t a, fix16_t b) {
    return (fix16_t)(((s64)a * (s64)b) >> 16);
}

static inline fix16_t fix16_div(fix16_t a, fix16_t b) {
    return (fix16_t)(((s64)a << 16) / b);
}

static inline fix16_t fix16_add(fix16_t a, fix16_t b) {
    return a + b;
}

static inline fix16_t fix16_sub(fix16_t a, fix16_t b) {
    return a - b;
}

fix16_t fix16_sin(fix16_t angle);
fix16_t fix16_cos(fix16_t angle);

fix16_t fix16_assembly_mul(fix16_t a, fix16_t b);

#endif
