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
#if defined(__GNUC__)
    /* SH-2 has no multi-bit arithmetic shift: GCC calls a libgcc helper for
     * a signed shift by most constants. A logical shift (shlr8/shlr2/...)
     * followed by sign extension of the remaining field is inline and gives
     * the same floor quotient for every input. */
    if (__builtin_constant_p(bits) && bits != 0u) {
        const uint32_t sign = (uint32_t)1u << (31u - bits);
        return (int32_t)((((uint32_t)value >> bits) ^ sign) - sign);
    }
#endif
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
#if defined(__GNUC__)
    /* The same for a constant count below 32, one word at a time: callers
     * that keep only the low word pay for two logical shifts and an OR. */
    if (__builtin_constant_p(bits) && bits != 0u && bits < 32u) {
        const uint64_t u = (uint64_t)value;
        const uint32_t lo = (uint32_t)u, hi = (uint32_t)(u >> 32);
        const uint32_t sign = (uint32_t)1u << (31u - bits);
        const uint32_t high = ((hi >> bits) ^ sign) - sign;
        return (int64_t)(((uint64_t)high << 32) | ((lo >> bits) | (hi << (32u - bits))));
    }
#endif
#if (-1 >> 1) == -1
    return value >> bits;
#else
    return value < 0 ? -(int64_t)((uint64_t)(-(value + 1)) >> bits) - 1
                     : (int64_t)((uint64_t)value >> bits);
#endif
}

#endif
