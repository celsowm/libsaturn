#pragma once
/* Experimental exact static projection. Not selected by production dispatch.
 * Unlike Q15 skinning, each Q16 product is truncated before the row sum.
 * Assembly accepts a nonempty prefix with positive W and conservatively
 * bounded X/Y. Refused records may have W written but X/Y stay unchanged.
 * The caller owns fallback, storage, divider/interrupt policy and dispatch. */
#include <stddef.h>
#include <stdint.h>
#include "src/graphics/3d/rendering/logic.hpp"

namespace saturn::core::render3d::static_project {
struct Job {
    const sat_vec3_t* points;
    const sat_fx16_t* matrix;
    sat_projected_vertex_t* out;
    uint32_t count;
    int32_t width, y_scale;
    uint32_t mult, w_last;
};
static_assert(sizeof(sat_vec3_t)==12 && offsetof(sat_vec3_t,z)==8 &&
    sizeof(sat_projected_vertex_t)==8 && offsetof(sat_projected_vertex_t,w)==4);
#if defined(__sh__)
static_assert(offsetof(Job,points)==0 && offsetof(Job,matrix)==4 &&
    offsetof(Job,out)==8 && offsetof(Job,count)==12 && offsetof(Job,width)==16 &&
    offsetof(Job,y_scale)==20 && offsetof(Job,mult)==24 && offsetof(Job,w_last)==28);
extern "C" uint32_t sat_static_project_sh2(const Job* job);
#endif

/* Returns the number of vertices handled by the prefix kernel (for evidence).
 * A failure at one point consumes that point through the scalar fallback,
 * then retries the next prefix. Sparse batches are deliberately measured too;
 * production will retain whichever dispatch policy is supported by evidence.
 * Inputs must satisfy the existing projection API's storage/math contract. */
inline uint32_t candidate(const sat_fx16_t* matrix,const sat_vec3_t* points,
    uint16_t count,int16_t width,int16_t height,sat_projected_vertex_t* out) {
#if defined(__sh__)
    const int32_t largest=width>height?width:height;
    if(width>0 && height>0 && largest<=4096) {
        const uint32_t mult=4096u/static_cast<uint32_t>(largest);
        const uint32_t last=(1u<<29)/mult-1u;
        uint32_t fast=0, i=0;
        while(i<count) {
            const Job job{points+i,matrix,out+i,uint32_t(count)-i,width,
                          -int32_t(height),mult,last};
            const uint32_t n=sat_static_project_sh2(&job);
            fast+=n;i+=n;
            if(i<count) {
                project_vertex(matrix,points[i],width,height,out+i);
                ++i;
            }
        }
        return fast;
    }
#endif
    project_vertices(matrix,points,count,width,height,out);
    return 0;
}
/* V2 policy experiment: a single prefix attempt, followed by the original
 * optimized batch for the entire refused suffix. The 32-vertex gate is a
 * benchmark hypothesis, not a production decision. Keep sparse camera
 * classification identical to project_vertices. No pre-scan or new state.
 * Returns accepted prefix length for diagnostic coverage. */
constexpr uint16_t kGuardedMinCount=32;
[[gnu::noinline]] inline uint32_t candidate_guarded(
    const sat_fx16_t* matrix,const sat_vec3_t* points,uint16_t count,
    int16_t width,int16_t height,sat_projected_vertex_t* out) {
#if defined(__sh__)
    const int32_t largest=width>height?width:height;
    if(count>=kGuardedMinCount &&
       (matrix[1]||matrix[2]||matrix[4]||matrix[12]) &&
       width>0 && height>0 && largest<=4096) {
        const uint32_t mult=4096u/static_cast<uint32_t>(largest);
        const uint32_t last=(1u<<29)/mult-1u;
        const Job job{points,matrix,out,count,width,-int32_t(height),mult,last};
        const uint32_t n=sat_static_project_sh2(&job);
        if(n<count)
            project_vertices_general(matrix,points+n,uint16_t(count-n),width,height,out+n);
        return n;
    }
#endif
    project_vertices(matrix,points,count,width,height,out);
    return 0;
}
} // namespace saturn::core::render3d::static_project
