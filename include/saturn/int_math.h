#ifndef SATURN_INT_MATH_H
#define SATURN_INT_MATH_H

#include <stdint.h>

/* Floor division by 2^bits, preserving the input width and sign. Counts at
 * least as large as the width return -1 for negative inputs and 0 otherwise.
 * No allocation, runtime initialization, division unit or target API is used.
 * A constant count in the i32 variant stays single-word on SH-2. Callers must
 * prove their product/accumulator fits before choosing the narrower variant. */
static inline int32_t sat_floor_shift_i32(int32_t value, unsigned bits) {
    if (bits >= 32u) return value < 0 ? -1 : 0;
#if (-1 >> 1) == -1
    return value >> bits;
#else
    /* -(value+1) remains representable even for INT32_MIN. */
    return value < 0 ? -(int32_t)((uint32_t)(-(value + 1)) >> bits) - 1
                     : (int32_t)((uint32_t)value >> bits);
#endif
}

static inline int64_t sat_floor_shift_i64(int64_t value, unsigned bits) {
    if (bits >= 64u) return value < 0 ? -1 : 0;
#if (-1 >> 1) == -1
    return value >> bits;
#else
    return value < 0 ? -(int64_t)((uint64_t)(-(value + 1)) >> bits) - 1
                     : (int64_t)((uint64_t)value >> bits);
#endif
}

#endif
