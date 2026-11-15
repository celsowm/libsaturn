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

#if defined(__sh__) && !defined(SAT_SCENE3D_PROJECTED_BOUNDS_REFERENCE)
#define SAT_SCENE3D_PROJECTED_BOUNDS_SH2 1
extern "C" void sat_scene3d_projected_bounds_sh2(const sat_projected_vertex_t* vertices,
    uint32_t count,Bounds* out);

/* Registers through the loop:
 *   r4 vertex   r5 vertices left   r6 out
 *   r3 smallest XY word   r7 largest XY word   r8 min y   r9 max y   r10 min w
 *   r0 XY word   r1 y   r2 w */
__asm__(
    "\t.section .text.sat_scene3d_projected_bounds_sh2,\"ax\",@progbits\n"
    "\t.align 2\n"
    "\t.global _sat_scene3d_projected_bounds_sh2\n"
    "\t.type _sat_scene3d_projected_bounds_sh2,@function\n"
    "_sat_scene3d_projected_bounds_sh2:\n"
    "\tmov.l r8,@-r15\n"
    "\tmov.l r9,@-r15\n"
    "\tmov.l r10,@-r15\n"
    "\tmov.l @r4+,r3\n"
    "\tmov.l @r4+,r10\n"
    "\tmov r3,r7\n"
    "\texts.w r3,r8\n"
    "\tdt r5\n"
    "\tbt.s .Lpb_done\n"
    "\tmov r8,r9\n"
    "\t.align 2\n"
    ".Lpb_vertex:\n"
    "\tmov.l @r4+,r0\n"
    "\tmov.l @r4+,r2\n"
    "\tcmp/gt r0,r3\n"
    "\tbt .Lpb_min_x\n"
    ".Lpb_1:\n"
    "\tcmp/gt r7,r0\n"
    "\tbt .Lpb_max_x\n"
    ".Lpb_2:\n"
    "\texts.w r0,r1\n"
    "\tcmp/gt r1,r8\n"
    "\tbt .Lpb_min_y\n"
    ".Lpb_3:\n"
    "\tcmp/gt r9,r1\n"
    "\tbt .Lpb_max_y\n"
    ".Lpb_4:\n"
    "\tcmp/gt r2,r10\n"
    "\tbt .Lpb_min_w\n"
    ".Lpb_5:\n"
    "\tdt r5\n"
    "\tbf .Lpb_vertex\n"
    ".Lpb_done:\n"
    "\tswap.w r3,r0\n"
    "\tswap.w r7,r1\n"
    "\texts.w r0,r0\n"
    "\texts.w r1,r1\n"
    "\tmov.l r0,@r6\n"
    "\tmov.l r1,@(4,r6)\n"
    "\tmov.l r8,@(8,r6)\n"
    "\tmov.l r9,@(12,r6)\n"
    "\tmov.l r10,@(16,r6)\n"
    "\tmov.l @r15+,r10\n"
    "\tmov.l @r15+,r9\n"
    "\trts\n"
    "\tmov.l @r15+,r8\n"
    ".Lpb_min_x:\n"
    "\tbra .Lpb_1\n"
    "\tmov r0,r3\n"
    ".Lpb_max_x:\n"
    "\tbra .Lpb_2\n"
    "\tmov r0,r7\n"
    ".Lpb_min_y:\n"
    "\tbra .Lpb_3\n"
    "\tmov r1,r8\n"
    ".Lpb_max_y:\n"
    "\tbra .Lpb_4\n"
    "\tmov r1,r9\n"
    ".Lpb_min_w:\n"
    "\tbra .Lpb_5\n"
    "\tmov r2,r10\n"
    "\t.size _sat_scene3d_projected_bounds_sh2,.-_sat_scene3d_projected_bounds_sh2\n"
    "\t.text\n");

inline void run(const sat_projected_vertex_t* vertices,uint32_t count,Bounds* out) {
    sat_scene3d_projected_bounds_sh2(vertices,count,out);
}
#else
inline void run(const sat_projected_vertex_t* vertices,uint32_t count,Bounds* out) {
    reference(vertices,count,out);
}
#endif

} // namespace saturn::scene3d::projected_bounds
