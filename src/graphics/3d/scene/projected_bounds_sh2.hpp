#pragma once
/* The screen bounds and nearest depth of a run of projected vertices, as one
 * routine with one job. submit_instance uses them to prove every face of an
 * instance drawable at once.
 *
 * The SH-2 version is hand-laid assembly; `reference` is the same contract in
 * C++, used on the host and as the definition of the result. A new minimum
 * or maximum is rare after the first few vertices, so the assembly falls
 * straight through the five tests and branches only to update. X is tested
 * on the vertex's packed XY word: X is its high half, so the smallest and
 * largest words carry the smallest and largest X. */
#include <stddef.h>
#include <stdint.h>
#include "saturn/render3d.h"

namespace saturn::scene3d::projected_bounds {

/* Field order is the assembly's: change both together. */
struct Bounds {
    int32_t min_x,max_x,min_y,max_y;
    sat_fx16_t min_w;
};
static_assert(offsetof(Bounds,min_x)==0u && offsetof(Bounds,max_x)==4u && offsetof(Bounds,min_y)==8u &&
    offsetof(Bounds,max_y)==12u && offsetof(Bounds,min_w)==16u,
    "projected_bounds::Bounds layout is fixed by the SH-2 routine");
static_assert(sizeof(sat_projected_vertex_t)==8u && offsetof(sat_projected_vertex_t,x)==0u &&
    offsetof(sat_projected_vertex_t,y)==2u && offsetof(sat_projected_vertex_t,w)==4u,
    "projected vertex layout is fixed by the SH-2 routine");

/* `count` is at least 1. */
inline void reference(const sat_projected_vertex_t* vertices,uint32_t count,Bounds* out) {
    int32_t min_x=vertices[0].x,max_x=min_x;
    int32_t min_y=vertices[0].y,max_y=min_y;
    sat_fx16_t min_w=vertices[0].w;
    for(uint32_t v=1u;v<count;++v) {
        const sat_projected_vertex_t& p=vertices[v];
        if(p.x<min_x)min_x=p.x;
        if(p.x>max_x)max_x=p.x;
        if(p.y<min_y)min_y=p.y;
        if(p.y>max_y)max_y=p.y;
        if(p.w<min_w)min_w=p.w;
    }
    *out={min_x,max_x,min_y,max_y,min_w};
}

#if defined(__sh__)
#define SAT_SCENE3D_PROJECTED_BOUNDS_SH2 1
extern "C" void sat_scene3d_projected_bounds_sh2(const sat_projected_vertex_t* vertices,
    uint32_t count,Bounds* out);
/* Assembled from projected_bounds_sh2.s. */
inline void run(const sat_projected_vertex_t* vertices,uint32_t count,Bounds* out) {
    sat_scene3d_projected_bounds_sh2(vertices,count,out);
}
#else
inline void run(const sat_projected_vertex_t* vertices,uint32_t count,Bounds* out) {
    reference(vertices,count,out);
}
#endif

} // namespace saturn::scene3d::projected_bounds
