#ifndef SATURN_CORE_COLLIDE2D_LOGIC_HPP
#define SATURN_CORE_COLLIDE2D_LOGIC_HPP

#include <stdint.h>
#include <limits.h>

#include "saturn/collide2d.h"
#include "src/core/math3d/logic.hpp"

namespace saturn::core::collide2d {

using saturn::core::math3d::div_s64_s32;
using saturn::core::math3d::isqrt64;

inline sat_fx16_t absfx(sat_fx16_t v) { return v < 0 ? -v : v; }
inline sat_vec2_t add(sat_vec2_t a, sat_vec2_t b) { return {a.x + b.x, a.y + b.y}; }
inline sat_vec2_t sub(sat_vec2_t a, sat_vec2_t b) { return {a.x - b.x, a.y - b.y}; }
inline sat_vec2_t scale(sat_vec2_t a, sat_fx16_t s) {
    return {static_cast<sat_fx16_t>((static_cast<int64_t>(a.x) * s) >> 16),
            static_cast<sat_fx16_t>((static_cast<int64_t>(a.y) * s) >> 16)};
}

inline sat_fx16_t approach(sat_fx16_t value, sat_fx16_t target, sat_fx16_t step) {
    if (step < 0) step = -step;
    if (value < target) {
        const sat_fx16_t next = value + step;
        return next > target ? target : next;
    }
    if (value > target) {
        const sat_fx16_t next = value - step;
        return next < target ? target : next;
    }
    return value;
}

inline sat_vec2_t reflect(sat_vec2_t v, sat_vec2_t n, sat_fx16_t restitution) {
    const int64_t dot = static_cast<int64_t>(v.x) * n.x + static_cast<int64_t>(v.y) * n.y;
    const sat_fx16_t twice = static_cast<sat_fx16_t>((dot * 2) >> 16);
    const sat_fx16_t impulse = static_cast<sat_fx16_t>(
        (static_cast<int64_t>(twice) * (SAT_FX16_ONE + restitution)) >> 16);
    return {v.x - static_cast<sat_fx16_t>((static_cast<int64_t>(impulse) * n.x) >> 16),
            v.y - static_cast<sat_fx16_t>((static_cast<int64_t>(impulse) * n.y) >> 16)};
}

inline bool valid_box(const sat_box2_t* b) {
    return b != nullptr && b->half.x >= 0 && b->half.y >= 0;
}
inline bool valid_circle(const sat_circle_t* c) { return c != nullptr && c->radius >= 0; }

inline bool box_overlap(const sat_box2_t& a, const sat_box2_t& b) {
    const int64_t dx = static_cast<int64_t>(a.center.x) - b.center.x;
    const int64_t dy = static_cast<int64_t>(a.center.y) - b.center.y;
    return (dx < static_cast<int64_t>(a.half.x) + b.half.x &&
            dx > -static_cast<int64_t>(a.half.x) - b.half.x &&
            dy < static_cast<int64_t>(a.half.y) + b.half.y &&
            dy > -static_cast<int64_t>(a.half.y) - b.half.y);
}

inline uint64_t len2(sat_vec2_t v) {
    return static_cast<uint64_t>(static_cast<int64_t>(v.x) * v.x) +
           static_cast<uint64_t>(static_cast<int64_t>(v.y) * v.y);
}

inline bool circle_overlap(const sat_circle_t& a, const sat_circle_t& b) {
    const sat_vec2_t d = sub(a.center, b.center);
    const int64_t radius = static_cast<int64_t>(a.radius) + b.radius;
    return len2(d) < static_cast<uint64_t>(radius * radius);
}

inline sat_fx16_t div_ratio_fx16(int64_t numerator, int64_t denominator) {
    if (denominator == 0) return 0;
    /* Keep the hardware-facing denominator signed 32-bit. Both terms are
     * scaled equally, so the quotient is unchanged; the final shift creates
     * the 16.16 result. */
    while (denominator > INT32_MAX || denominator < INT32_MIN) {
        numerator >>= 1;
        denominator >>= 1;
        if (denominator == 0) return 0;
    }
    if (numerator > (INT64_MAX >> 16)) numerator = INT64_MAX >> 16;
    if (numerator < (INT64_MIN >> 16)) numerator = INT64_MIN >> 16;
    return static_cast<sat_fx16_t>(div_s64_s32(numerator << 16, static_cast<int32_t>(denominator)));
}

inline int box_contact(const sat_box2_t& a, const sat_box2_t& b, sat_contact2_t& out) {
    const int64_t dx = static_cast<int64_t>(a.center.x) - b.center.x;
    const int64_t dy = static_cast<int64_t>(a.center.y) - b.center.y;
    const int64_t px = static_cast<int64_t>(a.half.x) + b.half.x - absfx(static_cast<sat_fx16_t>(dx));
    const int64_t py = static_cast<int64_t>(a.half.y) + b.half.y - absfx(static_cast<sat_fx16_t>(dy));
    if (px <= 0 || py <= 0) return 0;
    if (px <= py) {
        out.normal = {dx < 0 ? -SAT_FX16_ONE : SAT_FX16_ONE, 0};
        out.depth = static_cast<sat_fx16_t>(px);
    } else {
        out.normal = {0, dy < 0 ? -SAT_FX16_ONE : SAT_FX16_ONE};
        out.depth = static_cast<sat_fx16_t>(py);
    }
    return 1;
}

inline int circle_contact(const sat_circle_t& a, const sat_circle_t& b, sat_contact2_t& out) {
    const sat_vec2_t d = sub(a.center, b.center);
    const uint64_t square = len2(d);
    const int64_t radius = static_cast<int64_t>(a.radius) + b.radius;
    if (square >= static_cast<uint64_t>(radius * radius)) return 0;
    if (square == 0) {
        out.normal = {SAT_FX16_ONE, 0};
        out.depth = static_cast<sat_fx16_t>(radius);
        return 1;
    }
    const sat_fx16_t distance = static_cast<sat_fx16_t>(isqrt64(square));
    out.normal = {div_ratio_fx16(d.x, distance), div_ratio_fx16(d.y, distance)};
    out.depth = static_cast<sat_fx16_t>(radius - distance);
    return 1;
}

inline sat_vec2_t clamp_point(sat_vec2_t p, const sat_box2_t& b) {
    const sat_fx16_t minx = b.center.x - b.half.x;
    const sat_fx16_t maxx = b.center.x + b.half.x;
    const sat_fx16_t miny = b.center.y - b.half.y;
    const sat_fx16_t maxy = b.center.y + b.half.y;
    return {p.x < minx ? minx : (p.x > maxx ? maxx : p.x),
            p.y < miny ? miny : (p.y > maxy ? maxy : p.y)};
}

inline int circle_box_contact(const sat_circle_t& c, const sat_box2_t& b, sat_contact2_t& out) {
    const sat_vec2_t q = clamp_point(c.center, b);
    sat_vec2_t d = sub(c.center, q);
    const uint64_t square = len2(d);
    if (square != 0) {
        if (square >= static_cast<uint64_t>(static_cast<int64_t>(c.radius) * c.radius)) return 0;
        const sat_fx16_t distance = static_cast<sat_fx16_t>(isqrt64(square));
        out.normal = {div_ratio_fx16(d.x, distance), div_ratio_fx16(d.y, distance)};
        out.depth = static_cast<sat_fx16_t>(c.radius - distance);
        return 1;
    }
    const int64_t dx = static_cast<int64_t>(b.half.x) - absfx(c.center.x - b.center.x);
    const int64_t dy = static_cast<int64_t>(b.half.y) - absfx(c.center.y - b.center.y);
    if (dx < dy) {
        out.normal = {c.center.x < b.center.x ? -SAT_FX16_ONE : SAT_FX16_ONE, 0};
        out.depth = static_cast<sat_fx16_t>(c.radius + dx);
    } else {
        out.normal = {0, c.center.y < b.center.y ? -SAT_FX16_ONE : SAT_FX16_ONE};
        out.depth = static_cast<sat_fx16_t>(c.radius + dy);
    }
    return out.depth > 0;
}

inline sat_vec2_t point_at(sat_vec2_t o, sat_vec2_t d, sat_fx16_t t) {
    return {o.x + static_cast<sat_fx16_t>((static_cast<int64_t>(d.x) * t) >> 16),
            o.y + static_cast<sat_fx16_t>((static_cast<int64_t>(d.y) * t) >> 16)};
}

inline bool ray_box(const sat_box2_t& box, sat_vec2_t origin, sat_vec2_t delta, sat_hit2_t& out) {
    const sat_fx16_t minx = box.center.x - box.half.x;
    const sat_fx16_t maxx = box.center.x + box.half.x;
    const sat_fx16_t miny = box.center.y - box.half.y;
    const sat_fx16_t maxy = box.center.y + box.half.y;
    sat_fx16_t enter = 0, exit = SAT_FX16_ONE;
    sat_vec2_t normal = {0, 0};
    const sat_fx16_t origins[2] = {origin.x, origin.y};
    const sat_fx16_t deltas[2] = {delta.x, delta.y};
    const sat_fx16_t mins[2] = {minx, miny};
    const sat_fx16_t maxs[2] = {maxx, maxy};
    for (int axis = 0; axis < 2; ++axis) {
        if (deltas[axis] == 0) {
            if (origins[axis] <= mins[axis] || origins[axis] >= maxs[axis]) return false;
            continue;
        }
        sat_fx16_t a = div_ratio_fx16(static_cast<int64_t>(mins[axis]) - origins[axis], deltas[axis]);
        sat_fx16_t b = div_ratio_fx16(static_cast<int64_t>(maxs[axis]) - origins[axis], deltas[axis]);
        sat_vec2_t na = {0, 0};
        if (a > b) {
            const sat_fx16_t tmp = a; a = b; b = tmp;
            na = axis == 0 ? sat_vec2_t{SAT_FX16_ONE, 0} : sat_vec2_t{0, SAT_FX16_ONE};
        } else {
            na = axis == 0 ? sat_vec2_t{-SAT_FX16_ONE, 0} : sat_vec2_t{0, -SAT_FX16_ONE};
        }
        if (a >= enter) { enter = a; normal = na; }
        if (b < exit) exit = b;
        if (enter > exit || exit < 0 || enter > SAT_FX16_ONE) return false;
    }
    if (enter < 0) enter = 0;
    out.t = enter;
    out.point = point_at(origin, delta, enter);
    out.normal = normal;
    return true;
}

inline bool ray_circle(const sat_circle_t& circle, sat_vec2_t origin, sat_vec2_t delta, sat_hit2_t& out) {
    const sat_vec2_t f = sub(origin, circle.center);
    const uint64_t a = len2(delta);
    if (a == 0) return len2(f) < static_cast<uint64_t>(static_cast<int64_t>(circle.radius) * circle.radius);
    const int64_t b = 2 * (static_cast<int64_t>(f.x) * delta.x + static_cast<int64_t>(f.y) * delta.y);
    const int64_t c = static_cast<int64_t>(len2(f)) - static_cast<int64_t>(circle.radius) * circle.radius;
    const int64_t disc = b * b - 4 * static_cast<int64_t>(a) * c;
    if (disc < 0) return false;
    const int64_t root = static_cast<int64_t>(isqrt64(static_cast<uint64_t>(disc)));
    const int64_t den = 2 * static_cast<int64_t>(a);
    sat_fx16_t t = div_ratio_fx16(-b - root, den);
    if (t < 0) t = 0;
    if (t > SAT_FX16_ONE) {
        t = div_ratio_fx16(-b + root, den);
        if (t < 0 || t > SAT_FX16_ONE) return false;
    }
    out.t = t;
    out.point = point_at(origin, delta, t);
    const sat_vec2_t n = sub(out.point, circle.center);
    const sat_fx16_t l = static_cast<sat_fx16_t>(isqrt64(len2(n)));
    out.normal = l ? sat_vec2_t{div_ratio_fx16(n.x, l), div_ratio_fx16(n.y, l)} : sat_vec2_t{0, 0};
    return true;
}

inline bool point_box(const sat_vec2_t& p, const sat_box2_t& b) {
    return p.x >= b.center.x - b.half.x && p.x <= b.center.x + b.half.x &&
           p.y >= b.center.y - b.half.y && p.y <= b.center.y + b.half.y;
}
inline bool point_circle(const sat_vec2_t& p, const sat_circle_t& c) {
    return len2(sub(p, c.center)) <= static_cast<uint64_t>(static_cast<int64_t>(c.radius) * c.radius);
}

} // namespace saturn::core::collide2d

#endif
