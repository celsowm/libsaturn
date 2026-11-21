#include "saturn/water_plane.h"

namespace {

/* The SH-2 has no 64-bit divide and slow 64-bit arithmetic, so positions are
 * 32-bit: every coordinate and ring extent stays within +-2^29 (8192 units),
 * which build checks. Products that need 64 bits are single 32x32 multiplies. */
constexpr int32_t kRange = 1 << 29;

int32_t abs32(int32_t v) { return v < 0 ? -v : v; }
int32_t max32(int32_t a, int32_t b) { return a > b ? a : b; }

int32_t clamp_to(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* floor(v / step) * step for a positive step. */
int32_t snap(int32_t v, int32_t step) {
    int32_t q = v / step;
    if (v % step < 0) --q;
    return q * step;
}

int channel(uint16_t entry, unsigned shift) { return (entry >> shift) & 31; }

/* t is 16.16 in [0, 1]. A channel difference is at most 31, so the product
 * fits 32 bits. */
uint16_t blend(uint16_t a, uint16_t b, int32_t t, int glint) {
    int out[3];
    for (unsigned c = 0; c < 3; ++c) {
        const int from = channel(a, 5 * c), to = channel(b, 5 * c);
        const int v = from + (((to - from) * t) >> 16) + glint;
        out[c] = v < 0 ? 0 : (v > 31 ? 31 : v);
    }
    return static_cast<uint16_t>((out[2] << 10) | (out[1] << 5) | out[0]);
}

bool in_range(int32_t v) { return v > -kRange && v < kRange; }

bool valid(const sat_water_plane_config_t& c, int32_t camera_x, int32_t camera_z) {
    if (!(c.min_x < c.max_x && c.min_z < c.max_z && c.cell > 0 &&
          (c.cells == 4 || c.cells == 8) && c.rings >= 1 && c.rings <= SAT_WATER_PLANE_RINGS_MAX &&
          (c.wobble == 0 || (c.wobble > 0 && c.period > 0))))
        return false;
    // The outermost ring, doubled for the snap, must stay in range.
    const int64_t outer = (static_cast<int64_t>(c.cell) * c.cells) << c.rings;
    return outer < kRange && in_range(c.min_x) && in_range(c.max_x) && in_range(c.min_z) &&
           in_range(c.max_z) && in_range(camera_x) && in_range(camera_z) && in_range(c.surface);
}

}

extern "C" sat_result_t sat_water_plane_build(sat_water_plane_t* plane,
    const sat_water_plane_config_t* config, sat_fx16_t camera_x, sat_fx16_t camera_z) {
    if (!plane || !config || !valid(*config, camera_x, camera_z)) return SAT_ERR_INVALID_ARG;
    const sat_water_plane_config_t& cfg = *config;
    const int n = cfg.cells, half = n / 2;
    const unsigned stride = n + 1;
    const int32_t outer = (cfg.cell * half) << (cfg.rings - 1);  // the last ring's half extent
    const uint32_t inv_outer = static_cast<uint32_t>((static_cast<int64_t>(1) << 32) / outer);
    int32_t inner_min_x = 0, inner_max_x = 0, inner_min_z = 0, inner_max_z = 0;
    const bool cull = cfg.forward_x != 0 || cfg.forward_z != 0;
    plane->ring_count = cfg.rings;

    // The finest ring's wobble: a sine in x and one in z, each computed once per
    // grid line, and a weight per distance from the centre that reaches zero at
    // the ring's edge so it meets the flat outer rings without a crack.
    int32_t sine_x[SAT_WATER_PLANE_CELLS_MAX + 1], sine_z[SAT_WATER_PLANE_CELLS_MAX + 1];
    int32_t weight[SAT_WATER_PLANE_CELLS_MAX / 2 + 1];
    if (cfg.wobble != 0) {
        for (int m = 0; m <= half; ++m) weight[m] = (half - m) * SAT_FX16_ONE / half;
    }

    for (unsigned k = 0; k < cfg.rings; ++k) {
        sat_water_plane_ring_t& ring = plane->ring[k];
        const int32_t size = cfg.cell << k;
        const int32_t centre_x = snap(camera_x, 2 * size), centre_z = snap(camera_z, 2 * size);
        const int32_t extent = size * half;
        const bool waves = cfg.wobble != 0 && k == 0;
        if (waves) {
            const int32_t phase = static_cast<int32_t>(cfg.tick) * cfg.speed;
            // Whole degrees of phase per grid line: position * 360 / period.
            for (int i = 0; i <= n; ++i) {
                const int32_t x = centre_x + (i - half) * size, z = centre_z + (i - half) * size;
                sine_x[i] = sat_sin_deg_whole(static_cast<int32_t>(static_cast<int64_t>(x) * 360 / cfg.period) + phase);
                sine_z[i] = sat_sin_deg_whole(static_cast<int32_t>(static_cast<int64_t>(z) * 360 / cfg.period) + phase / 2);
            }
        }

        for (int j = 0; j <= n; ++j) {
            const int32_t z = centre_z + (j - half) * size;
            const int32_t cz = clamp_to(z, cfg.min_z, cfg.max_z);
            for (int i = 0; i <= n; ++i) {
                const int32_t x = centre_x + (i - half) * size;
                int32_t y = cfg.surface;
                int glint = 0;
                if (waves) {
                    const int m = max32(abs32(i - half), abs32(j - half));
                    const int32_t wave = static_cast<int32_t>(static_cast<int64_t>(sine_x[i]) * sine_z[j] >> 16);
                    const int32_t shaped = static_cast<int32_t>(static_cast<int64_t>(wave) * weight[m] >> 16);
                    y += static_cast<int32_t>(static_cast<int64_t>(shaped) * cfg.wobble >> 16);
                    glint = static_cast<int>(static_cast<int64_t>(shaped) * cfg.glint >> 16);
                }
                const int32_t away = max32(abs32(x - camera_x), abs32(z - camera_z));
                const int32_t t = away >= outer ? SAT_FX16_ONE
                    : static_cast<int32_t>(static_cast<uint64_t>(static_cast<uint32_t>(away)) * inv_outer >> 16);
                ring.vertices[j * stride + i] = {clamp_to(x, cfg.min_x, cfg.max_x), y, cz};
                ring.light[j * stride + i] = blend(cfg.near_color, cfg.far_color, t, glint);
            }
        }

        uint16_t faces = 0;
        for (int j = 0; j < n; ++j) {
            const int32_t z0 = centre_z + (j - half) * size, z1 = z0 + size;
            for (int i = 0; i < n; ++i) {
                const int32_t x0 = centre_x + (i - half) * size, x1 = x0 + size;
                if (k > 0 && x0 >= inner_min_x && x1 <= inner_max_x &&
                    z0 >= inner_min_z && z1 <= inner_max_z) continue;
                const uint16_t a = static_cast<uint16_t>(j * stride + i);
                const uint16_t b = static_cast<uint16_t>(a + 1);
                const uint16_t d = static_cast<uint16_t>(a + stride);
                const uint16_t c = static_cast<uint16_t>(d + 1);
                const sat_vec3_t* va = &ring.vertices[a];
                const sat_vec3_t* vc = &ring.vertices[c];
                if (va->x == vc->x || va->z == vc->z) continue;
                const sat_vec3_t* vb = &ring.vertices[b];
                const sat_vec3_t* vd = &ring.vertices[d];
                if (vb->x == va->x && vb->z == va->z) continue;
                if (cull) {
                    const sat_vec3_t* corner[4] = {va, vb, vc, vd};
                    bool behind = true;
                    for (const sat_vec3_t* p : corner) {
                        const int64_t dot = static_cast<int64_t>(p->x - camera_x) * cfg.forward_x +
                                            static_cast<int64_t>(p->z - camera_z) * cfg.forward_z;
                        if ((dot >> 16) >= -static_cast<int64_t>(cfg.behind)) { behind = false; break; }
                    }
                    if (behind) continue;
                }
                uint16_t* out = &ring.indices[4u * faces++];
                // Wound so a camera above sees the front: +x then +z reads
                // clockwise from above in this coordinate system.
                if (cfg.underside) { out[0] = a; out[1] = d; out[2] = c; out[3] = b; }
                else { out[0] = a; out[1] = b; out[2] = c; out[3] = d; }
            }
        }
        ring.mesh = {ring.vertices, ring.indices, SAT_WATER_PLANE_VERTICES,
                     static_cast<uint16_t>(stride * stride), SAT_WATER_PLANE_FACES, faces};
        inner_min_x = centre_x - extent;
        inner_max_x = centre_x + extent;
        inner_min_z = centre_z - extent;
        inner_max_z = centre_z + extent;
    }
    for (unsigned k = 0; k < SAT_WATER_PLANE_FACES; ++k) plane->face_materials[k] = 0;
    return SAT_OK;
}

extern "C" sat_result_t sat_water_plane_face_count(const sat_water_plane_t* plane, uint16_t* out) {
    if (!plane || !out || plane->ring_count > SAT_WATER_PLANE_RINGS_MAX) return SAT_ERR_INVALID_ARG;
    uint16_t total = 0;
    for (unsigned k = 0; k < plane->ring_count; ++k) total = static_cast<uint16_t>(total + plane->ring[k].mesh.face_count);
    *out = total;
    return SAT_OK;
}
