#include "saturn/int_math.h"
#include <cassert>
#include <climits>
#include <cstdio>

static int64_t reference(int64_t value, unsigned bits) {
    if (bits >= 63u) return value < 0 ? -1 : 0;
    const int64_t divisor = INT64_C(1) << bits;
    return value / divisor - (value < 0 && value % divisor != 0);
}

int main() {
    const int64_t edges[] = {INT64_MIN, INT64_MIN + 1, INT32_MIN,
        INT32_MIN + 1, -65537, -4097, -4096, -1, 0, 1, 4095, 4096,
        65537, INT32_MAX, INT64_MAX - 1, INT64_MAX};
    uint64_t state = UINT64_C(0x18471da249);
    for (unsigned trial = 0; trial < 10016u; ++trial) {
        state = state * UINT64_C(6364136223846793005) + 1u;
        const int64_t value = trial < 16u ? edges[trial]
            : (state & 1u ? -(int64_t)(state >> 1) - 1 : (int64_t)(state >> 1));
        const uint32_t low = (uint32_t)state;
        const int32_t narrow = trial < 16u && value >= INT32_MIN && value <= INT32_MAX
            ? (int32_t)value
            : (low & 1u ? -(int32_t)(low >> 1) - 1 : (int32_t)(low >> 1));
        for (unsigned bits = 0; bits < 81u; ++bits) {
            assert(sat_floor_shift_i64(value, bits) == reference(value, bits));
            assert(sat_floor_shift_i32(narrow, bits) == reference(narrow, bits));
        }
        assert(sat_floor_shift_i64(value, UINT_MAX) == (value < 0 ? -1 : 0));
        assert(sat_floor_shift_i32(narrow, UINT_MAX) == (narrow < 0 ? -1 : 0));
    }
    puts("integer floor shifts: signed extrema, negative fractions and all widths passed");
}
