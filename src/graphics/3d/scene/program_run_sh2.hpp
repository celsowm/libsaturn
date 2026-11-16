#pragma once
/* Writes the VDP1 commands of a run of ordered program faces, as one routine
 * with one job. emit_program_run finds the slots, bounds the run and commits
 * it; this routine only turns queue entries into command words.
 *
 * The SH-2 version is hand-scheduled assembly; `reference` is the same
 * contract in C++, used on the host and as the definition of the result. It
 * stops before the first face that is not a program face, or after `max`
 * faces, and returns how many it wrote. Each command word is stored once: the
 * slots are VDP1 VRAM. */
#include <stddef.h>
#include <stdint.h>
#include "saturn/scene3d_faces.h"

namespace saturn::scene3d::program_run {

/* Field order is the assembly's: change both together. `max` is at least 1.
 * `order` holds logical faces; `indices` maps them to `entries`, or is null
 * when a logical face is its own entry. `cached` receives how many written faces were cached projections. */
struct Job {
    const uint16_t* order;
    const uint16_t* indices;
    const sat_scene3d_projected_face_t* entries;
    uint32_t* out;
    uint32_t max;
    uint32_t cached;
};
static_assert(sizeof(sat_scene3d_projected_face_t)==24u || sizeof(void*)!=4u,
    "program_run entry layout is fixed by the SH-2 routine");

inline uint32_t reference_pair(uint16_t first,uint16_t second) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__
    return (static_cast<uint32_t>(second)<<16)|first;
#else
    return (static_cast<uint32_t>(first)<<16)|second;
#endif
}

inline uint32_t reference(Job& job) {
    uint32_t* out=job.out;
    uint32_t n=0u,cached=0u;
    for(;n<job.max;++n) {
        const uint16_t logical=job.order[n];
        const auto& hot=job.entries[job.indices?job.indices[logical]:logical];
        if(hot.kind!=SAT_SCENE3D_PROJECTED_PROGRAM)break;
        const uint16_t* const prefix=hot.program->command;
        /* A shaded face carries the caller's table at CMDGRDA and the
         * program's shaded draw mode. */
        const uint32_t grda=hot.reserved;
        out[0]=reference_pair(prefix[0],0u);
        out[1]=grda?reference_pair(prefix[1],prefix[3]):reference_pair(prefix[2],prefix[3]);
        out[2]=reference_pair(prefix[4],prefix[5]);
        out[3]=hot.xy[0];
        out[4]=hot.xy[1];
        out[5]=hot.xy[2];
        out[6]=hot.xy[3];
        out[7]=reference_pair(static_cast<uint16_t>(grda),0u);
        out+=8;
        if(hot.cached_projected)++cached;
    }
    job.cached=cached;
    return n;
}

#if defined(__sh__)
#define SAT_SCENE3D_PROGRAM_RUN_SH2 1
static_assert(offsetof(Job,order)==0u && offsetof(Job,indices)==4u && offsetof(Job,entries)==8u &&
    offsetof(Job,out)==12u && offsetof(Job,max)==16u && offsetof(Job,cached)==20u,
    "program_run::Job layout is fixed by the SH-2 routine");
static_assert(offsetof(sat_scene3d_projected_face_t,xy)==0u &&
    offsetof(sat_scene3d_projected_face_t,program)==16u &&
    offsetof(sat_scene3d_projected_face_t,kind)==20u &&
    offsetof(sat_scene3d_projected_face_t,cached_projected)==21u &&
    offsetof(sat_scene3d_projected_face_t,reserved)==22u &&
    offsetof(sat_scene3d_material_program_t,command)==0u &&
    SAT_SCENE3D_PROJECTED_PROGRAM==1,
    "projected face layout is fixed by the SH-2 routine");
extern "C" uint32_t sat_scene3d_program_run_sh2(Job* job);
/* Assembled from program_run_sh2.s. */
inline uint32_t run(Job& job) { return sat_scene3d_program_run_sh2(&job); }
#else
inline uint32_t run(Job& job) { return reference(job); }
#endif

} // namespace saturn::scene3d::program_run
