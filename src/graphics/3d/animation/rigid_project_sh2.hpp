#pragma once
/* The leading run of single-influence Q15 vertices that project plainly inside
 * the coordinate range, as one routine with one job. project_q15 hands every
 * other vertex (behind the eye, or far enough off screen to need the clamp)
 * to the general Projector, so this routine holds no clamp and no 64-bit
 * compare.
 *
 * The SH-2 version is hand-scheduled assembly; `reference` is the same
 * contract in C++, used on the host and as the definition of the result. The
 * values are those of clip_row and Projector::put: rows rounded to nearest,
 * numerator / w / 2 truncated toward zero.
 *
 * A vertex is "plainly inside" when 0 < w <= w_last and both |x| and |y| are
 * below w * mult. With mult = min(x_mult, y_mult) of the Projector that implies
 * its own test on each axis, so the divide is the result. The routine stops at
 * the first vertex that fails and returns how many it finished; the failing
 * vertex's output may be partly written.
 *
 * The divider takes 39 cycles and a quotient read waits for it. The X divide
 * runs under the Y row and the read of the next record, the Y divide under the
 * next vertex's X and W rows and tests, so neither read waits. DVSR keeps w
 * between the two, so the Y divide writes only its dividend. Like every user
 * of project_axis_start, this assumes no interrupt handler leaves the divider
 * changed. */
#include <stddef.h>
#include <stdint.h>
#include "saturn/skin3d.h"
#include "src/graphics/3d/rendering/logic.hpp"

namespace saturn::skin3d::rigid_project {

/* Field order is the assembly's: change both together. `count` is at least 1.
 * `w_last` is (1 << 29) / mult - 1, the largest w whose bound cannot
 * overflow. */
struct Job {
    const sat_skin_rigid_t* rigid;
    const sat_mat4_t* joints;
    sat_projected_vertex_t* out;
    uint32_t count;
    int32_t width, y_scale;
    uint32_t mult, w_last;
};
static_assert(sizeof(sat_skin_rigid_t)==8u && offsetof(sat_skin_rigid_t,joint)==6u &&
    sizeof(sat_mat4_t)==64u && sizeof(sat_projected_vertex_t)==8u &&
    offsetof(sat_projected_vertex_t,x)==0u && offsetof(sat_projected_vertex_t,y)==2u &&
    offsetof(sat_projected_vertex_t,w)==4u,
    "rigid_project record layouts are fixed by the SH-2 routine");

inline int32_t reference_row(const sat_fx16_t* row, const int16_t* pos) {
    return (int32_t)(((int64_t)row[0] * (pos[0] * 2) + (int64_t)row[1] * (pos[1] * 2) +
                      (int64_t)row[2] * (pos[2] * 2) + 32768) >> 16) + row[3];
}

inline uint32_t reference(const Job& job) {
    using namespace saturn::core::render3d;
    for (uint32_t i = 0; i < job.count; ++i) {
        const sat_skin_rigid_t& r = job.rigid[i];
        const sat_fx16_t* const m = job.joints[r.joint].m;
        const int32_t x = reference_row(m, r.pos);
        const int32_t w = reference_row(m + 8, r.pos);
        if ((uint32_t)w - 1u >= job.w_last) return i;
        const uint32_t reach = (uint32_t)w * job.mult - 1u;
        if ((uint32_t)x + reach > 2u * reach) return i;
        const int32_t y = reference_row(m + 4, r.pos);
        if ((uint32_t)y + reach > 2u * reach) return i;
        sat_projected_vertex_t& o = job.out[i];
        o.w = w;
        const int64_t x_numerator = (int64_t)x * job.width;
        project_axis_start(x_numerator, w);
        o.x = project_axis_finish(x_numerator, w);
        const int64_t y_numerator = (int64_t)y * job.y_scale;
        project_axis_start(y_numerator, w);
        o.y = project_axis_finish(y_numerator, w);
    }
    return job.count;
}

#if defined(__sh__)
#define SAT_SKIN_RIGID_PROJECT_SH2 1
static_assert(offsetof(Job,rigid)==0u && offsetof(Job,joints)==4u && offsetof(Job,out)==8u && offsetof(Job,count)==12u && offsetof(Job,width)==16u &&
    offsetof(Job,y_scale)==20u && offsetof(Job,mult)==24u && offsetof(Job,w_last)==28u,
    "rigid_project::Job layout is fixed by the SH-2 routine");
static_assert(saturn::core::math3d::kDivuDivisorAddress==0xFFFFFF00u &&
    saturn::core::math3d::kDivuDividendHighAddress==0xFFFFFF10u &&
    saturn::core::math3d::kDivuDividendLowQuotientAddress==0xFFFFFF14u,
    "the SH-2 routine addresses the divider from 0xFFFFFF00");
extern "C" uint32_t sat_skin_rigid_project_sh2(const Job* job);
/* Assembled from rigid_project_sh2.s. */
inline uint32_t run(const Job& job) { return sat_skin_rigid_project_sh2(&job); }
#else
inline uint32_t run(const Job& job) { return reference(job); }
#endif

} // namespace saturn::skin3d::rigid_project
