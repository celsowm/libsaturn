#include "saturn/water.h"

namespace {

bool same(const sat_vec3_t& a, const sat_vec3_t& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

/* A fixed endpoint order makes cuts agree across reversed shared edges.
 * Each product is bounded by (2^32 - 1)^2, even for full-range 16.16 input. */
sat_fx16_t interpolate(sat_fx16_t low, sat_fx16_t high,
                       uint64_t numerator, uint64_t denominator) {
    const int64_t delta = static_cast<int64_t>(high) - low;
    const uint64_t magnitude = static_cast<uint64_t>(delta < 0 ? -delta : delta);
    const int64_t offset = static_cast<int64_t>(magnitude * numerator / denominator);
    return static_cast<sat_fx16_t>(low + (delta < 0 ? -offset : offset));
}

sat_vec3_t intersection(const sat_vec3_t& a, const sat_vec3_t& b, sat_fx16_t y) {
    const sat_vec3_t& low = a.y < b.y ? a : b;
    const sat_vec3_t& high = a.y < b.y ? b : a;
    const uint64_t n = static_cast<int64_t>(y) - low.y;
    const uint64_t d = static_cast<int64_t>(high.y) - low.y;
    return {interpolate(low.x, high.x, n, d), y, interpolate(low.z, high.z, n, d)};
}

struct Polygon {
    sat_vec3_t points[6];
    uint8_t count;

    void push(const sat_vec3_t& p) {
        if (count == 0 || !same(points[count - 1], p)) points[count++] = p;
    }

    void emit(sat_water_piece_t* out, uint8_t& written, uint8_t submerged) {
        if (count > 1 && same(points[0], points[count - 1])) --count;
        // Pair adjacent fan triangles into quads, preserving cyclic order.
        for (uint8_t i = 1; i + 1 < count; i += 2) {
            const uint8_t last = i + 2 < count ? i + 2 : i + 1;
            out[written++] = {{{points[0], points[i], points[i + 1], points[last]}},
                              submerged};
        }
    }
};

}

extern "C" sat_result_t sat_water_split_quad(const sat_quad3_t* quad, sat_fx16_t surface,
    sat_water_piece_t out[SAT_WATER_SPLIT_PIECES_MAX], uint8_t* count) {
    if (!quad || !out || !count) return SAT_ERR_INVALID_ARG;
    bool above = false;
    bool below = false;
    for (const auto& v : quad->v) {
        above |= v.y > surface;
        below |= v.y < surface;
    }
    if (!(above && below)) {
        out[0] = {*quad, static_cast<uint8_t>(below)};
        *count = 1;
        return SAT_OK;
    }

    Polygon dry{}, submerged{};
    for (unsigned i = 0; i < 4; ++i) {
        const sat_vec3_t& a = quad->v[(i + 3) % 4];
        const sat_vec3_t& b = quad->v[i];
        if ((a.y < surface && b.y > surface) || (a.y > surface && b.y < surface)) {
            const sat_vec3_t cut = intersection(a, b, surface);
            dry.push(cut);
            submerged.push(cut);
        }
        if (b.y >= surface) dry.push(b);
        if (b.y <= surface) submerged.push(b);
    }
    uint8_t written = 0;
    dry.emit(out, written, 0);
    submerged.emit(out, written, 1);
    *count = written;
    return SAT_OK;
}
