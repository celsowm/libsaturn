#pragma once
/* Concatenates placed joints with the camera's clip X, Y and W rows, as one
 * routine with one job. sat_skin_clip_joints validates the camera and the
 * placement scale and hands the per-joint arithmetic here.
 *
 * The SH-2 version is hand-scheduled assembly; `reference` is the same
 * contract in C++, used on the host and as the definition of the result:
 *
 *   origin[k]  = scale * m[k][3] + offset[k]                     (fits int32)
 *   c[r][i]    = (sum_k scaled[r][k] * m[k][i] + 32768) >> 16    (fits int32)
 *   c[r][3]    = ((sum_k rows[r][k] * origin[k] + 32768) >> 16) + rows[r][3]
 *   c[3][0..3] = 0
 *
 * with every m[k][i] (i < 3) inside (-2^29, 2^29). A joint that breaks a
 * bound stops the routine with `false`; outputs written so far stay.
 *
 * The assembly sums each value's products in the 64-bit MAC, so a compiler's
 * 64-bit carry chains and library shifts are gone. It first copies the
 * joint's 3x3 by columns, because mac.l walks both operands upward. A row's
 * translation rides in as a fourth product with 65536, which is the same
 * 64-bit value as adding it after the shift. */
#include <stddef.h>
#include <stdint.h>
#include "saturn/skin3d.h"

namespace saturn::skin3d::clip_joints {

/* Field order is the assembly's: change both together. `scaled` is the
 * camera's three rows times the placement scale (3 x 3); `rows` is those
 * three camera rows with their translations (3 x 4). */
struct Job {
    const sat_mat4_t* joints;
    sat_mat4_t* out;
    uint32_t count;
    const int32_t* scaled;
    const int32_t* rows;
    const sat_skin_placement_t* placement;
};
static_assert(sizeof(sat_mat4_t)==64u && offsetof(sat_skin_placement_t,scale)==0u &&
    offsetof(sat_skin_placement_t,offset)==4u,
    "clip_joints record layouts are fixed by the SH-2 routine");

inline bool reference_fits(int64_t v) { return v >= INT32_MIN && v <= INT32_MAX; }

inline bool reference(const Job& job) {
    constexpr int64_t kInput = INT64_C(1) << 29;
    const sat_skin_placement_t& p = *job.placement;
    for (uint32_t j = 0; j < job.count; ++j) {
        const sat_fx16_t* m = job.joints[j].m;
        int32_t origin[3];
        for (uint8_t k = 0; k < 3; ++k) {
            for (uint8_t i = 0; i < 3; ++i)
                if (m[k * 4 + i] <= -kInput || m[k * 4 + i] >= kInput) return false;
            const int64_t placed = (int64_t)p.scale * m[k * 4 + 3] + p.offset[k];
            if (!reference_fits(placed)) return false;
            origin[k] = (int32_t)placed;
        }
        sat_fx16_t* c = job.out[j].m;
        for (uint8_t r = 0; r < 3; ++r) {
            const int32_t* s = &job.scaled[r * 3];
            for (uint8_t i = 0; i < 3; ++i) {
                /* Each product < 2^60, so the sum fits int64. */
                const int64_t t = (int64_t)s[0] * m[i] + (int64_t)s[1] * m[4 + i] +
                                  (int64_t)s[2] * m[8 + i];
                const int64_t rounded = (t + 32768) >> 16;
                if (!reference_fits(rounded)) return false;
                c[r * 4 + i] = (sat_fx16_t)rounded;
            }
            const int32_t* v = &job.rows[r * 4];
            const int64_t t = (int64_t)v[0] * origin[0] + (int64_t)v[1] * origin[1] +
                              (int64_t)v[2] * origin[2];
            const int64_t translated = ((t + 32768) >> 16) + v[3];
            if (!reference_fits(translated)) return false;
            c[r * 4 + 3] = (sat_fx16_t)translated;
        }
        c[12] = c[13] = c[14] = c[15] = 0;
    }
    return true;
}

#if defined(__sh__)
#define SAT_SKIN_CLIP_JOINTS_SH2 1
static_assert(offsetof(Job,joints)==0u && offsetof(Job,out)==4u && offsetof(Job,count)==8u &&
    offsetof(Job,scaled)==12u && offsetof(Job,rows)==16u && offsetof(Job,placement)==20u,
    "clip_joints::Job layout is fixed by the SH-2 routine");
/* Returns 0 when every joint was written. */
extern "C" uint32_t sat_skin_clip_joints_sh2(const Job* job);
/* Assembled from clip_joints_sh2.s. */
inline bool run(const Job& job) { return sat_skin_clip_joints_sh2(&job) == 0u; }
#else
inline bool run(const Job& job) { return reference(job); }
#endif

} // namespace saturn::skin3d::clip_joints
