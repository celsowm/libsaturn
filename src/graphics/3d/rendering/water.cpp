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

/* Per-channel RGB555 blend at the same fixed endpoint order as the cut, so a
 * reversed shared edge gets the same color at both ends. Bit 15 is kept from
 * the lower endpoint. */
uint16_t blend_color(uint16_t low, uint16_t high, uint64_t numerator, uint64_t denominator) {
    uint16_t result = low & 0x8000u;
    for (unsigned shift = 0; shift < 15; shift += 5) {
        const int a = (low >> shift) & 31, b = (high >> shift) & 31;
        const int delta = b - a;
        const int offset = static_cast<int>(
            static_cast<int64_t>(delta < 0 ? -delta : delta) * numerator / denominator);
        result |= static_cast<uint16_t>((a + (delta < 0 ? -offset : offset)) << shift);
    }
    return result;
}

struct Corner {
    sat_vec3_t point;
    uint16_t color;
};

Corner cut_corner(const Corner& a, const Corner& b, sat_fx16_t y) {
    const bool a_low = a.point.y < b.point.y;
    const Corner& low = a_low ? a : b;
    const Corner& high = a_low ? b : a;
    const uint64_t n = static_cast<int64_t>(y) - low.point.y;
    const uint64_t d = static_cast<int64_t>(high.point.y) - low.point.y;
    return {intersection(a.point, b.point, y), blend_color(low.color, high.color, n, d)};
}

struct ShadedPolygon {
    Corner points[6];
    uint8_t count;

    void push(const Corner& c) {
        if (count == 0 || !same(points[count - 1].point, c.point)) points[count++] = c;
    }

    void emit(sat_water_shaded_piece_t* out, uint8_t& written, uint8_t submerged) {
        if (count > 1 && same(points[0].point, points[count - 1].point)) --count;
        // Pair adjacent fan triangles into quads, preserving cyclic order.
        for (uint8_t i = 1; i + 1 < count; i += 2) {
            const uint8_t last = i + 2 < count ? i + 2 : i + 1;
            const uint8_t order[4] = {0, i, static_cast<uint8_t>(i + 1), last};
            sat_water_shaded_piece_t& piece = out[written++];
            piece.piece.submerged = submerged;
            for (unsigned v = 0; v < 4; ++v) {
                piece.piece.quad.v[v] = points[order[v]].point;
                piece.color[v] = points[order[v]].color;
            }
        }
    }
};

sat_result_t split_shaded(const sat_quad3_t* quad, const uint16_t* colors, sat_fx16_t surface,
                   sat_water_shaded_piece_t out[SAT_WATER_SPLIT_PIECES_MAX], uint8_t* count) {
    Corner corners[4];
    bool above = false;
    bool below = false;
    for (unsigned i = 0; i < 4; ++i) {
        corners[i] = {quad->v[i], colors[i]};
        above |= quad->v[i].y > surface;
        below |= quad->v[i].y < surface;
    }
    if (!(above && below)) {
        out[0].piece = {*quad, static_cast<uint8_t>(below)};
        for (unsigned i = 0; i < 4; ++i) out[0].color[i] = corners[i].color;
        *count = 1;
        return SAT_OK;
    }

    ShadedPolygon dry{}, submerged{};
    for (unsigned i = 0; i < 4; ++i) {
        const Corner& a = corners[(i + 3) % 4];
        const Corner& b = corners[i];
        if ((a.point.y < surface && b.point.y > surface) ||
            (a.point.y > surface && b.point.y < surface)) {
            const Corner cut = cut_corner(a, b, surface);
            dry.push(cut);
            submerged.push(cut);
        }
        if (b.point.y >= surface) dry.push(b);
        if (b.point.y <= surface) submerged.push(b);
    }
    uint8_t written = 0;
    dry.emit(out, written, 0);
    submerged.emit(out, written, 1);
    *count = written;
    return SAT_OK;
}

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

extern "C" sat_result_t sat_water_split_quad_shaded(const sat_quad3_t* quad,
    const uint16_t colors[4], sat_fx16_t surface,
    sat_water_shaded_piece_t out[SAT_WATER_SPLIT_PIECES_MAX], uint8_t* count) {
    if (!quad || !colors || !out || !count) return SAT_ERR_INVALID_ARG;
    return split_shaded(quad, colors, surface, out, count);
}
