#ifndef SATURN_CORE_RENDER3D_LOGIC_HPP
#define SATURN_CORE_RENDER3D_LOGIC_HPP

/* Pure, host-testable world-quad projection and shading.
 *
 * No hardware access, so tests/host/test_render3d_logic.cpp links this
 * directly. The public C API in include/saturn/render3d.h is a thin wrapper;
 * only the two drawing entry points there touch the VDP1.
 */

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/render3d.h"
#include "src/core/math3d_logic.hpp"

namespace saturn::core::render3d {

using saturn::core::math3d::fx_mul;
using saturn::core::math3d::fx_to_int;
using saturn::core::math3d::mat4_transform_vec4;

/* VDP1 vertex fields hold far more range than a 320x224 screen needs, but not
 * the full int32 a near-plane projection can produce. Clamping well outside
 * the screen keeps a partly off-screen quad roughly the right shape while
 * making it impossible for a huge value to wrap its sign and fold the quad
 * inside out. */
constexpr int32_t kCoordLimit = 2047;

inline int16_t clamp_coord64(int64_t v) {
    if (v < -static_cast<int64_t>(kCoordLimit)) {
        return static_cast<int16_t>(-kCoordLimit);
    }
    if (v > static_cast<int64_t>(kCoordLimit)) {
        return static_cast<int16_t>(kCoordLimit);
    }
    return static_cast<int16_t>(v);
}

/* Projects one world point straight to NATIVE VDP1 coordinates.
 *
 * The screen-space form is ((ndc + 1) * half) for x and ((1 - ndc) * half)
 * for y; native coordinates put the origin at the screen centre, so the
 * half-screen offset cancels and only the scaled NDC term survives -- which
 * also flips y, since world +Y is up and native +Y is down.
 *
 * The divide and the screen scale are fused into one 64-bit expression rather
 * than going through 16.16 helpers. A point close to the camera plane has a
 * tiny w, and both the normalise and the scale then produce values far outside
 * int32; in 16.16 arithmetic those wrap silently, and a wrapped coordinate
 * flips sign and folds the quad inside out instead of merely clamping to the
 * edge of the screen. Keeping full width until the final clamp makes the
 * clamp the only thing that ever bounds the result. */
inline bool project_native(
    const sat_fx16_t* view_proj,
    sat_fx16_t x,
    sat_fx16_t y,
    sat_fx16_t z,
    int16_t screen_w,
    int16_t screen_h,
    int16_t* out_x,
    int16_t* out_y
) {
    const sat_vec4_t clip = mat4_transform_vec4(view_proj, x, y, z, SAT_FX16_ONE);
    if (clip.w <= 0) {
        return false;
    }
    /* Both operands are 16.16, so the fixed-point scaling cancels in the
     * ratio and the result is already in whole pixels. */
    const int64_t w2 = static_cast<int64_t>(clip.w) * 2;
    const int64_t nx =
        (static_cast<int64_t>(clip.x) * static_cast<int64_t>(screen_w)) / w2;
    const int64_t ny =
        (static_cast<int64_t>(clip.y) * static_cast<int64_t>(screen_h)) / w2;
    *out_x = clamp_coord64(nx);
    *out_y = clamp_coord64(-ny);
    return true;
}

/* A quad is drawable only if every corner is in front of the camera: the VDP1
 * draws from four corners with no clipper, so a straddling quad would be
 * folded across the screen rather than cut at the near plane. */
inline bool project_quad(
    const sat_fx16_t* view_proj,
    const sat_quad3_t* quad,
    int16_t screen_w,
    int16_t screen_h,
    sat_quad2_t* out
) {
    if (view_proj == nullptr || quad == nullptr || out == nullptr) {
        return false;
    }
    for (int i = 0; i < 4; ++i) {
        if (!project_native(
                view_proj,
                quad->v[i].x,
                quad->v[i].y,
                quad->v[i].z,
                screen_w,
                screen_h,
                &out->x[i],
                &out->y[i])) {
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Quad construction                                                   */
/* ------------------------------------------------------------------ */

inline void make_wall(
    sat_quad3_t* out,
    sat_fx16_t x0,
    sat_fx16_t z0,
    sat_fx16_t x1,
    sat_fx16_t z1,
    sat_fx16_t height
) {
    if (out == nullptr) {
        return;
    }
    out->v[0].x = x0; out->v[0].y = height; out->v[0].z = z0;
    out->v[1].x = x1; out->v[1].y = height; out->v[1].z = z1;
    out->v[2].x = x1; out->v[2].y = 0;      out->v[2].z = z1;
    out->v[3].x = x0; out->v[3].y = 0;      out->v[3].z = z0;
}

inline void make_floor(
    sat_quad3_t* out,
    sat_fx16_t cx,
    sat_fx16_t y,
    sat_fx16_t cz,
    sat_fx16_t half
) {
    if (out == nullptr) {
        return;
    }
    const sat_fx16_t x0 = cx - half;
    const sat_fx16_t x1 = cx + half;
    const sat_fx16_t z0 = cz - half;
    const sat_fx16_t z1 = cz + half;
    out->v[0].x = x0; out->v[0].y = y; out->v[0].z = z0;
    out->v[1].x = x1; out->v[1].y = y; out->v[1].z = z0;
    out->v[2].x = x1; out->v[2].y = y; out->v[2].z = z1;
    out->v[3].x = x0; out->v[3].y = y; out->v[3].z = z1;
}

inline void make_billboard(
    sat_quad3_t* out,
    sat_fx16_t cx,
    sat_fx16_t cz,
    sat_fx16_t right_x,
    sat_fx16_t right_z,
    sat_fx16_t half_w,
    sat_fx16_t height
) {
    if (out == nullptr) {
        return;
    }
    const sat_fx16_t ox = fx_mul(right_x, half_w);
    const sat_fx16_t oz = fx_mul(right_z, half_w);
    out->v[0].x = cx - ox; out->v[0].y = height; out->v[0].z = cz - oz;
    out->v[1].x = cx + ox; out->v[1].y = height; out->v[1].z = cz + oz;
    out->v[2].x = cx + ox; out->v[2].y = 0;      out->v[2].z = cz + oz;
    out->v[3].x = cx - ox; out->v[3].y = 0;      out->v[3].z = cz - oz;
}

/* ------------------------------------------------------------------ */
/* Painter's algorithm                                                 */
/* ------------------------------------------------------------------ */

inline void sort_indices_desc(uint8_t* indices, const uint32_t* keys, uint16_t count) {
    if (indices == nullptr || keys == nullptr || count > 255u) {
        return;
    }
    for (uint16_t i = 1u; i < count; ++i) {
        const uint8_t value = indices[i];
        const uint32_t key = keys[value];
        int j = static_cast<int>(i) - 1;
        while (j >= 0 && keys[indices[j]] < key) {
            indices[j + 1] = indices[j];
            --j;
        }
        indices[j + 1] = value;
    }
}

/* Distances between maze-scale points fit comfortably in 32 bits, but a
 * degenerate camera position must not wrap the key and invert the sort. */
inline uint32_t ground_distance_sq(sat_fx16_t ax, sat_fx16_t az, sat_fx16_t bx, sat_fx16_t bz) {
    const int64_t dx = static_cast<int64_t>(fx_to_int(ax)) - static_cast<int64_t>(fx_to_int(bx));
    const int64_t dz = static_cast<int64_t>(fx_to_int(az)) - static_cast<int64_t>(fx_to_int(bz));
    const int64_t d = (dx * dx) + (dz * dz);
    if (d > static_cast<int64_t>(0xFFFFFFFFu)) {
        return 0xFFFFFFFFu;
    }
    return static_cast<uint32_t>(d);
}

/* ------------------------------------------------------------------ */
/* Flat shading                                                        */
/* ------------------------------------------------------------------ */

inline uint16_t shade_rgb555(uint16_t rgb555, sat_fx16_t intensity) {
    if (intensity < 0) {
        intensity = 0;
    }
    const uint16_t code = static_cast<uint16_t>(rgb555 & 0x8000u);
    uint32_t channel[3];
    channel[0] = static_cast<uint32_t>((rgb555 >> 10u) & 0x1Fu);
    channel[1] = static_cast<uint32_t>((rgb555 >> 5u) & 0x1Fu);
    channel[2] = static_cast<uint32_t>(rgb555 & 0x1Fu);
    for (int i = 0; i < 3; ++i) {
        uint32_t v = static_cast<uint32_t>(
            (static_cast<int64_t>(channel[i]) * static_cast<int64_t>(intensity)) >> 16);
        if (v > 31u) {
            v = 31u;
        }
        channel[i] = v;
    }
    return static_cast<uint16_t>(
        code | (channel[0] << 10u) | (channel[1] << 5u) | channel[2]);
}

/* Fixed directional light, normalised on the ground plane. Chosen off-axis so
 * that the four axis-aligned wall orientations of a maze all receive distinct
 * intensities -- with a light parallel to an axis, two of them would match and
 * corners would disappear. */
constexpr sat_fx16_t kLightX = 42427;  /* ~0.6474 */
constexpr sat_fx16_t kLightZ = 49933;  /* ~0.7619 */

/* The same light lifted off the ground plane, for solids whose faces point in
 * any direction. Its horizontal component runs the same way as kLightX/kLightZ
 * so a scene mixing walls with meshes stays consistent; the +Y component is
 * what stops every top face from being the same brightness as the floor. */
constexpr sat_fx16_t kLight3X = 25458;  /* ~0.3884 */
constexpr sat_fx16_t kLight3Y = 52429;  /* ~0.8000 */
constexpr sat_fx16_t kLight3Z = 29959;  /* ~0.4571 */

inline sat_fx16_t clamp_floor(sat_fx16_t floor_intensity) {
    if (floor_intensity < 0) {
        return 0;
    }
    if (floor_intensity > SAT_FX16_ONE) {
        return SAT_FX16_ONE;
    }
    return floor_intensity;
}

/* Intensity for a normal of ANY length.
 *
 * dot(n_hat, L) is dot(n, L) / |n|, so an unnormalised normal costs one
 * square root and one 64-bit divide here instead of the root and three
 * divides normalising it would have cost first. That matters: with a mesh
 * scene the normal path was measured at 64% of the frame. */
inline sat_fx16_t face_intensity3_scaled(const sat_vec3_t& normal, sat_fx16_t floor_intensity) {
    const sat_fx16_t floor_value = clamp_floor(floor_intensity);
    const sat_vec3_t light = {kLight3X, kLight3Y, kLight3Z};
    const int64_t raw = saturn::core::math3d::vec3_dot_raw(normal, light);
    if (raw <= 0) {
        return floor_value;
    }
    const sat_fx16_t len =
        saturn::core::math3d::fx_len3(normal.x, normal.y, normal.z);
    if (len == 0) {
        return floor_value;
    }
    sat_fx16_t dot = static_cast<sat_fx16_t>(raw / static_cast<int64_t>(len));
    if (dot > SAT_FX16_ONE) {
        dot = SAT_FX16_ONE;
    }
    return floor_value + fx_mul(SAT_FX16_ONE - floor_value, dot);
}

inline sat_fx16_t face_intensity3(const sat_vec3_t& normal, sat_fx16_t floor_intensity) {
    const sat_fx16_t floor_value = clamp_floor(floor_intensity);
    sat_fx16_t dot = static_cast<sat_fx16_t>(
        fx_mul(normal.x, kLight3X) + fx_mul(normal.y, kLight3Y) + fx_mul(normal.z, kLight3Z));
    if (dot < 0) {
        dot = 0;
    }
    if (dot > SAT_FX16_ONE) {
        dot = SAT_FX16_ONE;
    }
    return floor_value + fx_mul(SAT_FX16_ONE - floor_value, dot);
}

inline sat_fx16_t face_intensity(sat_fx16_t nx, sat_fx16_t nz, sat_fx16_t floor_intensity) {
    if (floor_intensity < 0) {
        floor_intensity = 0;
    }
    if (floor_intensity > SAT_FX16_ONE) {
        floor_intensity = SAT_FX16_ONE;
    }
    sat_fx16_t dot = fx_mul(nx, kLightX) + fx_mul(nz, kLightZ);
    if (dot < 0) {
        dot = 0;
    }
    if (dot > SAT_FX16_ONE) {
        dot = SAT_FX16_ONE;
    }
    /* floor + (1 - floor) * dot */
    return floor_intensity + fx_mul(SAT_FX16_ONE - floor_intensity, dot);
}

}  // namespace saturn::core::render3d

#endif /* SATURN_CORE_RENDER3D_LOGIC_HPP */
