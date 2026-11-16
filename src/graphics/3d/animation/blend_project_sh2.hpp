#pragma once
/* The leading run of two-influence, half-weight Q15 vertices that project
 * plainly inside the coordinate range, as one routine with one job: the seam
 * counterpart of rigid_project. project_q15 hands every other vertex (one or
 * three influences, an uneven weight, behind the eye, far off screen) to its
 * general path.
 *
 * The SH-2 version is hand-scheduled assembly (blend_project_sh2.s);
 * `reference` is the same contract in C++, used on the host and as the
 * definition of the result. Each influence's clip rows are rounded as
 * clip_row rounds, the two are averaged as floor((a + b) / 2), and the
 * result is projected as rigid_project projects: "plainly inside" and the
 * Job's `mult` and `w_last` mean what they mean there. The routine stops at
 * the first vertex it does not take and returns how many it finished; that
 * vertex's output may be partly written. */
#include <stddef.h>
#include <stdint.h>
#include "saturn/skin3d.h"
#include "src/graphics/3d/animation/rigid_project_sh2.hpp"

namespace saturn::skin3d::blend_project {

constexpr uint16_t kHalfWeight = 16384u;

/* Field order is the assembly's: change both together. `count` is at least 1. */
struct Job {
    const sat_skin_blend_t* blend;
    const sat_mat4_t* joints;
    sat_projected_vertex_t* out;
    uint32_t count;
    int32_t width, y_scale;
    uint32_t mult, w_last;
};
static_assert(sizeof(sat_skin_blend_t)==26u && offsetof(sat_skin_blend_t,joint)==0u &&
    offsetof(sat_skin_blend_t,count)==3u && offsetof(sat_skin_blend_t,weight)==4u &&
    offsetof(sat_skin_blend_t,pos)==8u,
    "blend_project record layout is fixed by the SH-2 routine");

/* floor((a + b) / 2) without a 33-bit sum. */
inline int32_t reference_half(int32_t a, int32_t b) {
    return (a >> 1) + (b >> 1) + (a & b & 1);
}

inline uint32_t reference(const Job& job) {
    using namespace saturn::core::render3d;
    using saturn::skin3d::rigid_project::reference_row;
    for (uint32_t i = 0; i < job.count; ++i) {
        const sat_skin_blend_t& r = job.blend[i];
        if (r.count != 2u || r.weight[0] != kHalfWeight) return i;
        const sat_fx16_t* const a = job.joints[r.joint[0]].m;
        const sat_fx16_t* const b = job.joints[r.joint[1]].m;
        const int32_t x = reference_half(reference_row(a, r.pos[0]), reference_row(b, r.pos[1]));
        const int32_t w = reference_half(reference_row(a + 8, r.pos[0]), reference_row(b + 8, r.pos[1]));
        if ((uint32_t)w - 1u >= job.w_last) return i;
        const uint32_t reach = (uint32_t)w * job.mult - 1u;
        if ((uint32_t)x + reach > 2u * reach) return i;
        const int32_t y = reference_half(reference_row(a + 4, r.pos[0]), reference_row(b + 4, r.pos[1]));
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
#define SAT_SKIN_BLEND_PROJECT_SH2 1
static_assert(offsetof(Job,blend)==0u && offsetof(Job,joints)==4u && offsetof(Job,out)==8u &&
    offsetof(Job,count)==12u && offsetof(Job,width)==16u && offsetof(Job,y_scale)==20u &&
    offsetof(Job,mult)==24u && offsetof(Job,w_last)==28u,
    "blend_project::Job layout is fixed by the SH-2 routine");
extern "C" uint32_t sat_skin_blend_project_sh2(const Job* job);
/* Assembled from blend_project_sh2.s. */
inline uint32_t run(const Job& job) { return sat_skin_blend_project_sh2(&job); }
#else
inline uint32_t run(const Job& job) { return reference(job); }
#endif

} // namespace saturn::skin3d::blend_project
