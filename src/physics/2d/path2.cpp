#include "saturn/path2.h"

#include <limits.h>

#include "src/core/math2d/logic.hpp"

/* Path2: lines, polylines, arcs, circles and Bezier curves addressed by arc length.
 * The contract is documented in include/saturn/path2.h. */

namespace {

namespace m2 = saturn::core::math2d;

constexpr int64_t kOne = SAT_FX16_ONE;
constexpr int kBezierChords = 32;       /* chords used to estimate a Bezier's length at init */
constexpr int kNearestSamples = 32;     /* coarse samples of the Bezier nearest-point search */
constexpr int kNearestRefinements = 14; /* ternary-search steps around the best coarse sample */
constexpr int64_t kTwoPiQ16 = 411775;   /* 2 * pi * 65536 */

inline int32_t clamp32(int64_t v) {
    return v > INT32_MAX ? INT32_MAX : (v < INT32_MIN ? INT32_MIN : static_cast<int32_t>(v));
}
inline int64_t floor_mod(int64_t a, int64_t n) {
    const int64_t r = a % n;
    return r < 0 ? r + n : r;
}
inline sat_vec2_t sub(sat_vec2_t a, sat_vec2_t b) { return {clamp32(static_cast<int64_t>(a.x) - b.x), clamp32(static_cast<int64_t>(a.y) - b.y)}; }
inline uint64_t dist2(sat_vec2_t a, sat_vec2_t b) {
    const int64_t dx = static_cast<int64_t>(a.x) - b.x;
    const int64_t dy = static_cast<int64_t>(a.y) - b.y;
    return static_cast<uint64_t>(dx * dx) + static_cast<uint64_t>(dy * dy);
}
inline sat_vec2_t left_normal(sat_vec2_t tangent) { return {tangent.y, clamp32(-static_cast<int64_t>(tangent.x))}; }

inline bool is_bezier(const sat_path2_t& p) { return p.kind == SAT_PATH2_QUADRATIC || p.kind == SAT_PATH2_CUBIC; }
inline bool is_arc(const sat_path2_t& p) { return p.kind == SAT_PATH2_ARC || p.kind == SAT_PATH2_CIRCLE; }
inline bool ready(const sat_path2_t* p) { return p && p->kind >= SAT_PATH2_LINE && p->kind <= SAT_PATH2_CUBIC && p->length > 0; }

/* ----- Bezier evaluation, parameter t in 16.16 [0, 1] ----- */

sat_vec2_t bezier_point(const sat_path2_t& p, int64_t t) {
    const int64_t u = kOne - t;
    int64_t w[4];
    if (p.kind == SAT_PATH2_QUADRATIC) {
        w[0] = (u * u) >> 16;
        w[1] = (2 * u * t) >> 16;
        w[2] = (t * t) >> 16;
        w[3] = 0;
    } else {
        const int64_t uu = (u * u) >> 16, tt = (t * t) >> 16;
        w[0] = (uu * u) >> 16;
        w[1] = (3 * uu * t) >> 16;
        w[2] = (3 * u * tt) >> 16;
        w[3] = (tt * t) >> 16;
    }
    const int n = p.kind == SAT_PATH2_QUADRATIC ? 3 : 4;
    int64_t x = 0, y = 0;
    for (int i = 0; i < n; ++i) {
        x += w[i] * p.p[i].x;
        y += w[i] * p.p[i].y;
    }
    return {clamp32(x >> 16), clamp32(y >> 16)};
}

sat_vec2_t bezier_derivative(const sat_path2_t& p, int64_t t) {
    const int64_t u = kOne - t;
    int64_t x, y;
    if (p.kind == SAT_PATH2_QUADRATIC) {
        const int64_t a = 2 * u, b = 2 * t;
        x = a * (static_cast<int64_t>(p.p[1].x) - p.p[0].x) + b * (static_cast<int64_t>(p.p[2].x) - p.p[1].x);
        y = a * (static_cast<int64_t>(p.p[1].y) - p.p[0].y) + b * (static_cast<int64_t>(p.p[2].y) - p.p[1].y);
    } else {
        const int64_t a = 3 * ((u * u) >> 16), b = 6 * ((u * t) >> 16), c = 3 * ((t * t) >> 16);
        x = a * (static_cast<int64_t>(p.p[1].x) - p.p[0].x) + b * (static_cast<int64_t>(p.p[2].x) - p.p[1].x) +
            c * (static_cast<int64_t>(p.p[3].x) - p.p[2].x);
        y = a * (static_cast<int64_t>(p.p[1].y) - p.p[0].y) + b * (static_cast<int64_t>(p.p[2].y) - p.p[1].y) +
            c * (static_cast<int64_t>(p.p[3].y) - p.p[2].y);
    }
    return {clamp32(x >> 16), clamp32(y >> 16)};
}

/* Unit tangent of a Bezier; at a cusp (zero derivative) it is read a little further in. */
sat_vec2_t bezier_tangent(const sat_path2_t& p, int64_t t) {
    sat_vec2_t d = bezier_derivative(p, t);
    for (int step = 1; d.x == 0 && d.y == 0 && step <= 8; ++step) {
        const int64_t shifted = t < kOne / 2 ? t + step * (kOne / 64) : t - step * (kOne / 64);
        d = bezier_derivative(p, shifted);
    }
    return m2::normalize(d);
}

/* Curve parameter for a distance, through the arc-length table when there is one. */
int64_t bezier_param(const sat_path2_t& p, int64_t s) {
    if (p.count < 2 || !p.cumulative) return (s << 16) / p.length;
    const sat_fx16_t* tab = p.cumulative;
    int lo = 0, hi = p.count - 2; /* largest i with tab[i] <= s */
    while (lo < hi) {
        const int mid = (lo + hi + 1) >> 1;
        if (tab[mid] <= s) lo = mid;
        else hi = mid - 1;
    }
    const int64_t span = static_cast<int64_t>(tab[lo + 1]) - tab[lo];
    const int64_t frac = span <= 0 ? 0 : ((s - tab[lo]) << 16) / span;
    return (((static_cast<int64_t>(lo) << 16) + (frac > kOne ? kOne : frac)) / (p.count - 1));
}

/* The inverse: distance of a curve parameter. */
int64_t bezier_distance(const sat_path2_t& p, int64_t t) {
    if (p.count < 2 || !p.cumulative) return (t * p.length) >> 16;
    const int64_t x = t * (p.count - 1);
    int i = static_cast<int>(x >> 16);
    if (i > p.count - 2) i = p.count - 2;
    const int64_t f = x - (static_cast<int64_t>(i) << 16);
    return p.cumulative[i] + (((static_cast<int64_t>(p.cumulative[i + 1]) - p.cumulative[i]) * f) >> 16);
}

/* ----- polyline segments ----- */

inline uint32_t segment_count(const sat_path2_t& p) { return p.closed ? p.count : static_cast<uint32_t>(p.count - 1); }

inline void segment_ends(const sat_path2_t& p, uint32_t i, sat_vec2_t& a, sat_vec2_t& b) {
    a = p.points[i];
    b = p.points[(i + 1) % p.count];
}

uint32_t polyline_segment(const sat_path2_t& p, int64_t s) {
    const uint32_t segs = segment_count(p);
    uint32_t lo = 0, hi = segs - 1; /* largest i with cumulative[i] <= s */
    while (lo < hi) {
        const uint32_t mid = (lo + hi + 1) >> 1;
        if (p.cumulative[mid] <= s) lo = mid;
        else hi = mid - 1;
    }
    while (lo > 0 && p.cumulative[lo + 1] == p.cumulative[lo]) --lo; /* skip a zero-length tail */
    return lo;
}

/* ----- arcs ----- */

inline int64_t arc_length_of(int64_t radius, int64_t sweep_abs) {
    const int64_t arc = (radius * sweep_abs) >> 16; /* radius times the fraction of a turn */
    return ((arc * kTwoPiQ16) >> 16);
}

/* ----- sampling ----- */

int64_t normalise_distance(const sat_path2_t& p, int64_t d) {
    if (p.closed) return floor_mod(d, p.length);
    return d < 0 ? 0 : (d > p.length ? p.length : d);
}

void fill_sample(const sat_path2_t& p, int64_t s, sat_path2_sample_t& out) {
    out.distance = clamp32(s);
    sat_vec2_t position = {0, 0}, tangent = {0, 0};
    switch (p.kind) {
    case SAT_PATH2_LINE: {
        const int64_t f = (s << 16) / p.length;
        position = m2::lerp(p.p[0], p.p[1], clamp32(f));
        tangent = m2::normalize(sub(p.p[1], p.p[0]));
        break;
    }
    case SAT_PATH2_POLYLINE: {
        const uint32_t i = polyline_segment(p, s);
        sat_vec2_t a, b;
        segment_ends(p, i, a, b);
        const int64_t span = static_cast<int64_t>(p.cumulative[i + 1]) - p.cumulative[i];
        const int64_t f = span > 0 ? ((s - p.cumulative[i]) << 16) / span : 0;
        position = m2::lerp(a, b, clamp32(f > kOne ? kOne : f));
        tangent = m2::normalize(sub(b, a));
        break;
    }
    case SAT_PATH2_ARC:
    case SAT_PATH2_CIRCLE: {
        const int64_t f = (s << 16) / p.length;
        const sat_angle16_t angle = static_cast<sat_angle16_t>(static_cast<int64_t>(p.start_angle) + ((static_cast<int64_t>(p.sweep) * f) >> 16));
        const sat_fx16_t c = m2::cos16(angle), sn = m2::sin16(angle);
        position = {clamp32(static_cast<int64_t>(p.center.x) + ((static_cast<int64_t>(p.radius) * c) >> 16)),
                    clamp32(static_cast<int64_t>(p.center.y) + ((static_cast<int64_t>(p.radius) * sn) >> 16))};
        tangent = p.sweep > 0 ? sat_vec2_t{-sn, c} : sat_vec2_t{sn, -c};
        break;
    }
    case SAT_PATH2_QUADRATIC:
    case SAT_PATH2_CUBIC: {
        const int64_t t = bezier_param(p, s);
        position = bezier_point(p, t > kOne ? kOne : t);
        tangent = bezier_tangent(p, t > kOne ? kOne : t);
        break;
    }
    }
    out.position = position;
    out.tangent = tangent;
    out.normal = left_normal(tangent);
}

/* ----- nearest ----- */

struct Candidate {
    int64_t distance;
    uint64_t gap2;
};

/* Closest point of the segment a-b to `q`: its fraction (16.16) along the segment. */
int64_t segment_fraction(sat_vec2_t a, sat_vec2_t b, sat_vec2_t q) {
    const sat_vec2_t ab = sub(b, a), aq = sub(q, a);
    int64_t num = m2::dot_raw(aq, ab);
    int64_t den = m2::dot_raw(ab, ab);
    if (den == 0 || num <= 0) return 0;
    if (num >= den) return kOne;
    while (den >= (1LL << 46)) { den >>= 1; num >>= 1; }
    return den == 0 ? 0 : (num << 16) / den;
}

Candidate nearest_on_polyline(const sat_path2_t& p, sat_vec2_t q) {
    Candidate best = {0, UINT64_MAX};
    const uint32_t segs = segment_count(p);
    for (uint32_t i = 0; i < segs; ++i) {
        sat_vec2_t a, b;
        segment_ends(p, i, a, b);
        const int64_t span = static_cast<int64_t>(p.cumulative[i + 1]) - p.cumulative[i];
        if (span <= 0) continue;
        const int64_t f = segment_fraction(a, b, q);
        const sat_vec2_t at = m2::lerp(a, b, clamp32(f));
        const uint64_t g = dist2(at, q);
        if (g < best.gap2) best = {p.cumulative[i] + ((span * f) >> 16), g};
    }
    return best;
}

Candidate nearest_on_line(const sat_path2_t& p, sat_vec2_t q) {
    const int64_t f = segment_fraction(p.p[0], p.p[1], q);
    const sat_vec2_t at = m2::lerp(p.p[0], p.p[1], clamp32(f));
    return {(p.length * f) >> 16, dist2(at, q)};
}

Candidate nearest_on_arc(const sat_path2_t& p, sat_vec2_t q) {
    const sat_vec2_t v = sub(q, p.center);
    int64_t d = 0;
    if (v.x != 0 || v.y != 0) {
        const sat_angle16_t angle = m2::atan2_16(v.y, v.x);
        const int64_t span = p.sweep < 0 ? -static_cast<int64_t>(p.sweep) : p.sweep;
        const int64_t rel = floor_mod(p.sweep > 0 ? static_cast<int64_t>(angle) - p.start_angle
                                                   : static_cast<int64_t>(p.start_angle) - angle, 65536);
        if (rel <= span) {
            d = (p.length * rel) / span;
        } else if (rel - span < 65536 - rel) { /* closer to the end than to the start */
            d = p.length;
        } else {
            d = 0;
        }
    }
    sat_path2_sample_t s;
    fill_sample(p, normalise_distance(p, d), s);
    return {d, dist2(s.position, q)};
}

Candidate nearest_on_bezier(const sat_path2_t& p, sat_vec2_t q) {
    int64_t best_t = 0;
    uint64_t best = UINT64_MAX;
    for (int i = 0; i <= kNearestSamples; ++i) {
        const int64_t t = (static_cast<int64_t>(i) << 16) / kNearestSamples;
        const uint64_t g = dist2(bezier_point(p, t), q);
        if (g < best) { best = g; best_t = t; }
    }
    int64_t lo = best_t - kOne / kNearestSamples, hi = best_t + kOne / kNearestSamples;
    if (lo < 0) lo = 0;
    if (hi > kOne) hi = kOne;
    for (int i = 0; i < kNearestRefinements && hi - lo > 1; ++i) { /* ternary search of the bracket */
        const int64_t m1 = lo + (hi - lo) / 3, m2v = hi - (hi - lo) / 3;
        if (dist2(bezier_point(p, m1), q) <= dist2(bezier_point(p, m2v), q)) hi = m2v;
        else lo = m1;
    }
    const int64_t t = (lo + hi) >> 1;
    const uint64_t g = dist2(bezier_point(p, t), q);
    if (g <= best) { best = g; best_t = t; }
    return {bezier_distance(p, best_t), best};
}

} // namespace

/* ----- initialisation ----- */

static sat_path2_t blank(sat_path2_kind_t kind) {
    sat_path2_t p = {};
    p.kind = kind;
    return p;
}

extern "C" sat_result_t sat_path2_init_line(sat_path2_t* path, sat_vec2_t from, sat_vec2_t to) {
    if (!path) return SAT_ERR_INVALID_ARG;
    sat_path2_t p = blank(SAT_PATH2_LINE);
    p.p[0] = from;
    p.p[1] = to;
    const int64_t len = m2::length(sub(to, from));
    const int64_t dx = static_cast<int64_t>(to.x) - from.x, dy = static_cast<int64_t>(to.y) - from.y;
    if (len <= 0 || dx > INT32_MAX || dx < INT32_MIN || dy > INT32_MAX || dy < INT32_MIN) return SAT_ERR_INVALID_ARG;
    p.length = static_cast<sat_fx16_t>(len);
    *path = p;
    return SAT_OK;
}

extern "C" uint32_t sat_path2_polyline_entries(uint16_t point_count, int closed) {
    return static_cast<uint32_t>(point_count) + (closed ? 1u : 0u);
}

extern "C" sat_result_t sat_path2_init_polyline(sat_path2_t* path, const sat_vec2_t* points, uint16_t count,
                                                int closed, sat_fx16_t* cumulative, uint32_t entries) {
    if (!path || !points || !cumulative || count < 2 || entries < sat_path2_polyline_entries(count, closed))
        return SAT_ERR_INVALID_ARG;
    const uint32_t segs = closed ? count : static_cast<uint32_t>(count - 1);
    int64_t total = 0;
    cumulative[0] = 0;
    for (uint32_t i = 0; i < segs; ++i) {
        total += m2::length(sub(points[(i + 1) % count], points[i]));
        if (total > INT32_MAX) return SAT_ERR_INVALID_ARG;
        cumulative[i + 1] = static_cast<sat_fx16_t>(total);
    }
    if (total <= 0) return SAT_ERR_INVALID_ARG;
    sat_path2_t p = blank(SAT_PATH2_POLYLINE);
    p.closed = closed ? 1 : 0;
    p.count = count;
    p.points = points;
    p.cumulative = cumulative;
    p.length = static_cast<sat_fx16_t>(total);
    *path = p;
    return SAT_OK;
}

static sat_result_t init_arc(sat_path2_t* path, sat_path2_kind_t kind, sat_vec2_t center, sat_fx16_t radius,
                             sat_angle16_t start, int32_t sweep) {
    const int64_t span = sweep < 0 ? -static_cast<int64_t>(sweep) : sweep;
    if (!path || radius <= 0 || span == 0 || span > 65536) return SAT_ERR_INVALID_ARG;
    const int64_t len = arc_length_of(radius, span);
    if (len <= 0 || len > INT32_MAX) return SAT_ERR_INVALID_ARG;
    sat_path2_t p = blank(kind);
    p.closed = span == 65536 ? 1 : 0;
    p.center = center;
    p.radius = radius;
    p.start_angle = start;
    p.sweep = sweep;
    p.length = static_cast<sat_fx16_t>(len);
    *path = p;
    return SAT_OK;
}

extern "C" sat_result_t sat_path2_init_arc(sat_path2_t* path, sat_vec2_t center, sat_fx16_t radius,
                                           sat_angle16_t start_angle, int32_t sweep) {
    return init_arc(path, SAT_PATH2_ARC, center, radius, start_angle, sweep);
}

extern "C" sat_result_t sat_path2_init_circle(sat_path2_t* path, sat_vec2_t center, sat_fx16_t radius) {
    return init_arc(path, SAT_PATH2_CIRCLE, center, radius, 0, 65536);
}

static sat_result_t init_bezier(sat_path2_t* path, sat_path2_kind_t kind, const sat_vec2_t* pts, int n) {
    if (!path) return SAT_ERR_INVALID_ARG;
    sat_path2_t p = blank(kind);
    for (int i = 0; i < n; ++i) p.p[i] = pts[i];
    int64_t total = 0;
    sat_vec2_t prev = bezier_point(p, 0);
    for (int i = 1; i <= kBezierChords; ++i) {
        const sat_vec2_t at = bezier_point(p, (static_cast<int64_t>(i) << 16) / kBezierChords);
        total += m2::length(sub(at, prev));
        prev = at;
    }
    if (total <= 0 || total > INT32_MAX) return SAT_ERR_INVALID_ARG;
    p.length = static_cast<sat_fx16_t>(total);
    *path = p;
    return SAT_OK;
}

extern "C" sat_result_t sat_path2_init_quadratic(sat_path2_t* path, sat_vec2_t p0, sat_vec2_t p1, sat_vec2_t p2) {
    const sat_vec2_t pts[3] = {p0, p1, p2};
    return init_bezier(path, SAT_PATH2_QUADRATIC, pts, 3);
}

extern "C" sat_result_t sat_path2_init_cubic(sat_path2_t* path, sat_vec2_t p0, sat_vec2_t p1, sat_vec2_t p2,
                                             sat_vec2_t p3) {
    const sat_vec2_t pts[4] = {p0, p1, p2, p3};
    return init_bezier(path, SAT_PATH2_CUBIC, pts, 4);
}

extern "C" uint32_t sat_path2_table_requirements(uint32_t entries) {
    return entries * static_cast<uint32_t>(sizeof(sat_fx16_t));
}

extern "C" sat_result_t sat_path2_build_table(sat_path2_t* path, sat_fx16_t* storage, uint32_t entries) {
    if (!path || !is_bezier(*path) || !storage || entries < SAT_PATH2_TABLE_MIN || entries > SAT_PATH2_TABLE_MAX)
        return SAT_ERR_INVALID_ARG;
    int64_t total = 0;
    sat_vec2_t prev = bezier_point(*path, 0);
    storage[0] = 0;
    for (uint32_t i = 1; i < entries; ++i) {
        const sat_vec2_t at = bezier_point(*path, (static_cast<int64_t>(i) << 16) / (entries - 1));
        total += m2::length(sub(at, prev));
        if (total > INT32_MAX) return SAT_ERR_INVALID_ARG;
        storage[i] = static_cast<sat_fx16_t>(total);
        prev = at;
    }
    if (total <= 0) return SAT_ERR_INVALID_ARG;
    path->cumulative = storage;
    path->count = static_cast<uint16_t>(entries);
    path->length = static_cast<sat_fx16_t>(total);
    return SAT_OK;
}

extern "C" sat_result_t sat_path2_attach_table(sat_path2_t* path, const sat_fx16_t* table, uint32_t entries) {
    if (!path || !is_bezier(*path) || !table || entries < SAT_PATH2_TABLE_MIN || entries > SAT_PATH2_TABLE_MAX ||
        table[0] != 0 || table[entries - 1] <= 0)
        return SAT_ERR_INVALID_ARG;
    for (uint32_t i = 1; i < entries; ++i)
        if (table[i] < table[i - 1]) return SAT_ERR_INVALID_ARG;
    path->cumulative = table;
    path->count = static_cast<uint16_t>(entries);
    path->length = table[entries - 1];
    return SAT_OK;
}

/* ----- queries ----- */

extern "C" sat_fx16_t sat_path2_length(const sat_path2_t* path) { return path ? path->length : 0; }
extern "C" int sat_path2_is_closed(const sat_path2_t* path) { return path && path->closed; }

extern "C" sat_result_t sat_path2_sample(const sat_path2_t* path, sat_fx16_t distance, sat_path2_sample_t* out) {
    if (!ready(path) || !out) return SAT_ERR_INVALID_ARG;
    fill_sample(*path, normalise_distance(*path, distance), *out);
    return SAT_OK;
}

extern "C" sat_result_t sat_path2_advance(const sat_path2_t* path, sat_fx16_t distance, sat_fx16_t delta,
                                          sat_path2_edge_t edge, sat_fx16_t* out_distance, uint8_t* flags) {
    if (!ready(path) || !out_distance || (edge != SAT_PATH2_CLAMP && edge != SAT_PATH2_WRAP)) return SAT_ERR_INVALID_ARG;
    const int64_t len = path->length;
    uint8_t f = 0;
    int64_t d = static_cast<int64_t>(normalise_distance(*path, distance)) + delta;
    if (edge == SAT_PATH2_CLAMP) {
        if (d <= 0) { d = 0; f |= SAT_PATH2_HIT_START; }
        else if (d >= len) { d = len; f |= SAT_PATH2_HIT_END; }
    } else if (d < 0 || d >= len) {
        d = floor_mod(d, len);
        f |= SAT_PATH2_WRAPPED;
    }
    *out_distance = static_cast<sat_fx16_t>(d);
    if (flags) *flags = f;
    return SAT_OK;
}

extern "C" sat_result_t sat_path2_nearest(const sat_path2_t* path, sat_vec2_t point, sat_path2_nearest_t* out) {
    if (!ready(path) || !out) return SAT_ERR_INVALID_ARG;
    Candidate c;
    switch (path->kind) {
    case SAT_PATH2_LINE: c = nearest_on_line(*path, point); break;
    case SAT_PATH2_POLYLINE: c = nearest_on_polyline(*path, point); break;
    case SAT_PATH2_ARC:
    case SAT_PATH2_CIRCLE: c = nearest_on_arc(*path, point); break;
    default: c = nearest_on_bezier(*path, point); break;
    }
    fill_sample(*path, normalise_distance(*path, c.distance), out->sample);
    out->gap = m2::length(sub(point, out->sample.position));
    return SAT_OK;
}
