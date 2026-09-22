#include "saturn/surface3d.h"

namespace {
int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
}

extern "C" sat_result_t sat_surface3d_height(const sat_surface3d_desc_t* surface,
    int32_t x, int32_t z, int32_t* out_y) {
    if (!surface || !out_y || surface->half_x <= 0 || surface->half_z <= 0) {
        return SAT_ERR_INVALID_ARG;
    }
    const int32_t dz = clamp_i32(z - surface->center_z,
        -surface->half_z, surface->half_z);
    *out_y = surface->base_y + static_cast<int32_t>(
        static_cast<int64_t>(surface->tilt_z) * dz / surface->half_z);
    (void)x;
    return SAT_OK;
}

extern "C" uint8_t sat_surface3d_split(const sat_surface3d_desc_t* surface,
    sat_surface3d_rect_t out_slices[4]) {
    if (!surface || !out_slices || surface->half_x <= 0 || surface->half_z <= 0) return 0u;
    const int32_t lx = surface->center_x - surface->half_x;
    const int32_t rx = surface->center_x + surface->half_x;
    const int32_t bz = surface->center_z - surface->half_z;
    const int32_t fz = surface->center_z + surface->half_z;
    if (!surface->holes || surface->hole_count == 0u) {
        out_slices[0] = {lx, rx, bz, fz};
        return 1u;
    }
    /* The public contract intentionally accepts one aperture: callers with
     * several authored holes submit separate bounded surfaces. */
    const sat_surface3d_rect_t& h = surface->holes[0];
    const int32_t hl = h.min_x < lx ? lx : h.min_x;
    const int32_t hr = h.max_x > rx ? rx : h.max_x;
    const int32_t hb = h.min_z < bz ? bz : h.min_z;
    const int32_t hf = h.max_z > fz ? fz : h.max_z;
    uint8_t count = 0u;
    if (lx < hl && bz < fz) out_slices[count++] = {lx, hl, bz, fz};
    if (hr < rx && bz < fz) out_slices[count++] = {hr, rx, bz, fz};
    if (bz < hb && hl < hr) out_slices[count++] = {hl, hr, bz, hb};
    if (hf < fz && hl < hr) out_slices[count++] = {hl, hr, hf, fz};
    return count;
}

extern "C" uint8_t sat_surface3d_supports_footprint(
    const sat_surface3d_desc_t* surface, int32_t x, int32_t z,
    int32_t half_x, int32_t half_z) {
    if (!surface || half_x < 0 || half_z < 0) return 0u;
    sat_surface3d_rect_t slices[4];
    const uint8_t count = sat_surface3d_split(surface, slices);
    for (uint8_t i = 0u; i < count; ++i) {
        if (x - half_x >= slices[i].min_x && x + half_x <= slices[i].max_x &&
            z - half_z >= slices[i].min_z && z + half_z <= slices[i].max_z) return 1u;
    }
    return 0u;
}
