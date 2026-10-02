#ifndef SATURN_CORE_MATH2D_LOGIC_HPP
#define SATURN_CORE_MATH2D_LOGIC_HPP

/* Pure, host-testable 2D fixed-point helpers. The public C API in
 * include/saturn/math2d.h wraps these. */

#include <stdint.h>

#include "saturn/math2d.h"
#include "src/core/math2d/tables.hpp"
#include "src/core/math3d/logic.hpp"

namespace saturn::core::math2d {

inline sat_fx16_t sin8(sat_angle_t a) { return kSin8[a]; }
inline sat_fx16_t cos8(sat_angle_t a) { return kSin8[static_cast<uint8_t>(a + 64u)]; }

inline sat_fx16_t sin16(sat_angle16_t a) {
    const uint32_t i = static_cast<uint32_t>(a) >> 8;
    const int32_t frac = static_cast<int32_t>(a & 0xFFu);
    const int32_t s0 = kSin8[i];
    const int32_t s1 = kSin8[(i + 1u) & 0xFFu];
    return static_cast<sat_fx16_t>(s0 + (((s1 - s0) * frac) >> 8));
}
inline sat_fx16_t cos16(sat_angle16_t a) {
    return sin16(static_cast<sat_angle16_t>(a + 16384u));
}

inline sat_vec2_t from_angle(sat_angle_t a) { return {cos8(a), sin8(a)}; }

inline int8_t angle_diff(sat_angle_t from, sat_angle_t to) {
    return static_cast<int8_t>(static_cast<uint8_t>(to - from));
}

inline uint8_t angle_quadrant(sat_angle_t a) {
    return static_cast<uint8_t>(((static_cast<uint32_t>(a) + 32u) >> 6) & 3u);
}

inline int64_t dot_raw(sat_vec2_t a, sat_vec2_t b) {
    return static_cast<int64_t>(a.x) * b.x + static_cast<int64_t>(a.y) * b.y;
}
inline sat_fx16_t dot(sat_vec2_t a, sat_vec2_t b) {
    return static_cast<sat_fx16_t>(dot_raw(a, b) >> 16);
}

inline sat_fx16_t length(sat_vec2_t v) {
    const uint64_t sum = static_cast<uint64_t>(static_cast<int64_t>(v.x) * v.x) +
                         static_cast<uint64_t>(static_cast<int64_t>(v.y) * v.y);
    return static_cast<sat_fx16_t>(saturn::core::math3d::isqrt64(sum));
}

inline sat_vec2_t normalize(sat_vec2_t v) {
    const sat_fx16_t len = length(v);
    if (len == 0) return {0, 0};
    return {saturn::core::math3d::div_s64_s32(static_cast<int64_t>(v.x) << 16, len),
            saturn::core::math3d::div_s64_s32(static_cast<int64_t>(v.y) << 16, len)};
}

inline sat_vec2_t perp(sat_vec2_t v) { return {-v.y, v.x}; }

inline sat_vec2_t project(sat_vec2_t v, sat_vec2_t unit) {
    const sat_fx16_t d = dot(v, unit);
    return {static_cast<sat_fx16_t>((static_cast<int64_t>(unit.x) * d) >> 16),
            static_cast<sat_fx16_t>((static_cast<int64_t>(unit.y) * d) >> 16)};
}
inline sat_vec2_t reject(sat_vec2_t v, sat_vec2_t unit) {
    const sat_vec2_t p = project(v, unit);
    return {v.x - p.x, v.y - p.y};
}

inline sat_fx16_t lerp(sat_fx16_t a, sat_fx16_t b, sat_fx16_t t) {
    return static_cast<sat_fx16_t>(
        a + static_cast<sat_fx16_t>((static_cast<int64_t>(b - a) * t) >> 16));
}
inline sat_vec2_t lerp(sat_vec2_t a, sat_vec2_t b, sat_fx16_t t) {
    return {lerp(a.x, b.x, t), lerp(a.y, b.y, t)};
}

/* Angle of (x, y) in 16-bit turns. The first octant is a 64-interval atan
 * table on min/max; the other seven octants fold into it. */
inline sat_angle16_t atan2_16(sat_fx16_t y, sat_fx16_t x) {
    if (x == 0 && y == 0) return 0;
    const int64_t ax = x < 0 ? -static_cast<int64_t>(x) : x;
    const int64_t ay = y < 0 ? -static_cast<int64_t>(y) : y;
    const bool steep = ay > ax;
    const int64_t big = steep ? ay : ax;
    const int64_t small = steep ? ax : ay;
    /* ratio in [0, 64] sixty-fourths, with 16 fractional bits for interpolation */
    const int32_t ratio = static_cast<int32_t>((small << 22) / big); /* 6.16 */
    const uint32_t idx = static_cast<uint32_t>(ratio) >> 16;
    const uint32_t frac = static_cast<uint32_t>(ratio) & 0xFFFFu;
    const uint32_t a0 = kAtan64[idx];
    const uint32_t a1 = idx < 64u ? kAtan64[idx + 1u] : a0;
    uint32_t angle = a0 + (((a1 - a0) * frac) >> 16); /* 0..8192 */
    if (steep) angle = 16384u - angle;                /* fold to 0..90 deg */
    if (x < 0) angle = 32768u - angle;                /* mirror about Y */
    if (y < 0) angle = (65536u - angle) & 0xFFFFu;    /* mirror about X */
    return static_cast<sat_angle16_t>(angle);
}
inline sat_angle_t atan2_8(sat_fx16_t y, sat_fx16_t x) {
    return static_cast<sat_angle_t>((static_cast<uint32_t>(atan2_16(y, x)) + 128u) >> 8);
}

} // namespace saturn::core::math2d

#endif
