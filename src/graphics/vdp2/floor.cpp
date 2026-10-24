#include "saturn/vdp2_floor.h"

#include "src/core/math3d/logic.hpp"
#include "src/hal/vdp2/rotation_logic.hpp"

namespace {

namespace rot = saturn::hal::vdp2::rot;

constexpr int64_t kOne = 65536;
/* |right.y| above this (16.16, about 0.25 degree) is a rolled camera. */
constexpr int64_t kRollLimit = 0x120;

uint64_t isqrt(uint64_t v) {
    uint64_t r = 0, bit = uint64_t{1} << 62;
    while (bit > v) bit >>= 2;
    while (bit != 0) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return r;
}

int64_t length(const int32_t* row) {
    return static_cast<int64_t>(isqrt(static_cast<uint64_t>(
        static_cast<int64_t>(row[0]) * row[0] + static_cast<int64_t>(row[1]) * row[1] +
        static_cast<int64_t>(row[2]) * row[2])));
}

int32_t wrap(int64_t v, int32_t period) {
    if (period <= 0) return static_cast<int32_t>(v);
    v %= period;
    return static_cast<int32_t>(v < 0 ? v + period : v);
}

}  // namespace

/* With view_proj = P * V, row 0 is fx * right, row 1 fy * up and row 3 the
 * forward axis f (clip w is the depth along it). Without roll the right axis
 * is horizontal, the horizontal forward is g = (0, 1, 0) x right, and screen
 * line v (NDC) shows the floor line at depth w = h / -(f.y + v u.y / fy),
 * whose points are eye + w (f + v u / fy) + w (x / focal_x) right for the
 * dot x from the centre. Its horizontal part is w ((f.g + v u.g / fy) g +
 * (x / focal_x) right), so with the coefficient k = w density / focal_x the
 * rotation maps Xs = x to the right axis and Ys = focal_x (f.g + v u.g / fy),
 * which is linear in the line, to g. */
extern "C" sat_result_t sat_vdp2_floor_from_view(const sat_vdp2_floor_t* floor, const sat_mat4_t* view_proj,
                                                 const sat_vec3_t* eye, sat_fx16_t near_z, sat_fx16_t far_z,
                                                 sat_vdp2_rotation_params_t* params, uint32_t* coefficients) {
    if (floor == nullptr || view_proj == nullptr || eye == nullptr || params == nullptr ||
        coefficients == nullptr || floor->width < 2u || floor->height < 2u || floor->density <= 0 ||
        floor->period < 0 || near_z <= 0 || far_z <= near_z ||
        (floor->far_lines != nullptr && floor->far_step == 0u)) return SAT_ERR_INVALID_ARG;
    const int32_t* m = view_proj->m;
    const int64_t fx = length(m), fy = length(m + 4), fl = length(m + 12);
    if (fx == 0 || fy == 0 || fl == 0) return SAT_ERR_UNSUPPORTED;
    /* Unit axes; the focal terms against a unit forward axis. */
    const int64_t right[3] = {m[0] * kOne / fx, m[1] * kOne / fx, m[2] * kOne / fx};
    const int64_t up[3] = {m[4] * kOne / fy, m[5] * kOne / fy, m[6] * kOne / fy};
    const int64_t forward[3] = {m[12] * kOne / fl, m[13] * kOne / fl, m[14] * kOne / fl};
    const int64_t focal_x = fx * kOne / fl, focal_y = fy * kOne / fl;
    const int64_t height = static_cast<int64_t>(eye->y) - floor->plane_y;
    if (right[1] > kRollLimit || right[1] < -kRollLimit || height <= 0) return SAT_ERR_UNSUPPORTED;
    const int64_t g[2] = {right[2], -right[0]};   /* x, z */
    const int64_t f_h = (forward[0] * g[0] + forward[2] * g[1]) / kOne;
    const int64_t u_h = (up[0] * g[0] + up[2] * g[1]) / kOne;
    const int64_t half_w = floor->width / 2u, half_h = floor->height / 2u;
    const int64_t focal_px = focal_x * half_w;    /* 16.16 dots */
    if (focal_px <= 0 || focal_px > INT32_MAX) return SAT_ERR_UNSUPPORTED;
    /* Line y looks down dir_y = forward.y + v up.y / fy with v = (half_h -
     * y) / half_h, linear in y: kept in 16 extra fractional bits. Its floor
     * point lies at depth w = h / -dir_y with coefficient w density /
     * focal_px = scaled / -dir_y, so a line costs one 64/32 division (the
     * SH-2's divider) and the depth tests are multiplications. */
    const int64_t hk = height * kOne;
    const int64_t dir_step = up[1] * (kOne << 16) / (focal_y * half_h);
    int64_t dir = forward[1] * kOne + half_h * dir_step;
    /* scaled = hk density / focal_px, split so it cannot overflow. Past
     * 127 << 34 every line's coefficient reaches 127 (|dir_y| stays below 4
     * short of a 150-degree view). */
    const int64_t whole = hk / focal_px, rest = hk % focal_px;
    const int64_t scaled = whole > (int64_t{127} << 34) / floor->density
        ? INT64_MAX : whole * floor->density + rest * floor->density / focal_px;
    uint16_t far_index = 0, far_count = 0;
    for (uint16_t y = 0; y < floor->height; ++y, dir -= dir_step) {
        const int64_t dir_y = dir >> 16;
        int32_t k = 0;
        bool hidden = true;
        int64_t far = far_z;
        if (floor->far_lines != nullptr) {
            const sat_fx16_t line_far = floor->far_lines[far_index];
            if (line_far > 0 && line_far < far) far = line_far;
            if (++far_count == floor->far_step) { far_count = 0; ++far_index; }
        }
        /* w = hk / -dir_y (truncated) within [near_z, far], and the
         * coefficient in (0, 127). */
        if (dir_y < 0 && hk >= near_z * -dir_y && hk < (far + 1) * -dir_y &&
            scaled >= -dir_y && scaled < 127 * kOne * -dir_y) {
            k = saturn::core::math3d::div_s64_s32(scaled, static_cast<int32_t>(-dir_y));
            hidden = false;
        }
        coefficients[y] = rot::encode_coefficient(k, hidden);
    }
    *params = sat_vdp2_rotation_params_t{};
    params->xst = static_cast<int32_t>(-half_w * kOne);
    params->dx = static_cast<int32_t>(kOne);
    params->yst = static_cast<int32_t>(focal_px * (f_h + u_h * kOne / focal_y) / kOne);
    params->dyst = static_cast<int32_t>(-focal_px * u_h / (focal_y * half_h));
    params->matrix[0] = static_cast<int32_t>(right[0]);
    params->matrix[1] = static_cast<int32_t>(g[0]);
    params->matrix[3] = static_cast<int32_t>(right[2]);
    params->matrix[4] = static_cast<int32_t>(g[1]);
    params->mx = wrap(floor->origin_x + static_cast<int64_t>(floor->density) * eye->x / kOne, floor->period);
    params->my = wrap(floor->origin_y + static_cast<int64_t>(floor->density) * eye->z / kOne, floor->period);
    params->kx = params->ky = static_cast<int32_t>(kOne);
    params->coefficient_step = static_cast<int32_t>(kOne);
    return SAT_OK;
}
