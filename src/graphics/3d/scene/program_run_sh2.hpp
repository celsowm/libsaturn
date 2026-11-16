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

/* Registers through the loop:
 *   r4 order   r5 indices   r6 entries   r7 command slot   r8 faces left
 *   r9 cached faces   r10 24 (entry size)   r11 -1   r13 the job
 *   r1 entry index, then entry   r2 program
 *   r3 kind:cached:CMDGRDA, then CMDGRDA   r0, r12 scratch
 * One loop per way of finding the entry, chosen once. */

/* The face whose entry index is in r1; loops to `face`. */
#define SAT_PR_FACE(face, mode, shaded) \
    "\tmulu.w r10,r1\n" \
    "\tsts macl,r1\n" \
    "\tadd r6,r1\n" \
    "\tmov.l @(20,r1),r3\n" \
    "\tmov.l @(16,r1),r2\n" \
    /* kind == program, else stop before this face. */ \
    "\tmov r3,r0\n" \
    "\tshlr16 r0\n" \
    "\tmov r0,r12\n" \
    "\tshlr8 r12\n" \
    "\tdt r12\n" \
    "\tbf .Lpr_stop\n" \
    "\ttst #255,r0\n" \
    "\tsubc r11,r9\n" \
    "\textu.w r3,r3\n" \
    /* CMDCTRL, then the draw mode and colour. */ \
    "\tmov.w @r2,r0\n" \
    "\ttst r3,r3\n" \
    "\tshll16 r0\n" \
    "\tbf.s " shaded "\n" \
    "\tmov.l r0,@r7\n" \
    "\tmov.l @(4,r2),r0\n" \
    mode ":\n" \
    "\tmov.l @(8,r2),r12\n" \
    "\tmov.l r0,@(4,r7)\n" \
    "\tmov.l @r1,r0\n" \
    "\tmov.l r12,@(8,r7)\n" \
    "\tmov.l @(4,r1),r12\n" \
    "\tmov.l r0,@(12,r7)\n" \
    "\tmov.l @(8,r1),r0\n" \
    "\tmov.l r12,@(16,r7)\n" \
    "\tmov.l @(12,r1),r12\n" \
    "\tmov.l r0,@(20,r7)\n" \
    "\tshll16 r3\n" \
    "\tmov.l r12,@(24,r7)\n" \
    "\tmov.l r3,@(28,r7)\n" \
    "\tdt r8\n" \
    "\tbf.s " face "\n" \
    "\tadd #32,r7\n"

/* Shaded: the program's shaded draw mode beside its colour word. */
#define SAT_PR_SHADED(mode, shaded) \
    shaded ":\n" \
    "\tmov.w @(6,r2),r0\n" \
    "\textu.w r0,r12\n" \
    "\tmov.w @(2,r2),r0\n" \
    "\tshll16 r0\n" \
    "\tbra " mode "\n" \
    "\tor r12,r0\n"

__asm__(
    "\t.section .text.sat_scene3d_program_run_sh2,\"ax\",@progbits\n"
    "\t.align 2\n"
    "\t.global _sat_scene3d_program_run_sh2\n"
    "\t.type _sat_scene3d_program_run_sh2,@function\n"
    "_sat_scene3d_program_run_sh2:\n"
    "\tmov.l r8,@-r15\n"
    "\tmov.l r9,@-r15\n"
    "\tmov.l r10,@-r15\n"
    "\tmov.l r11,@-r15\n"
    "\tmov.l r12,@-r15\n"
    "\tmov.l r13,@-r15\n"
    "\tmov r4,r13\n"
    "\tmov.l @(4,r13),r5\n"
    "\tmov.l @(8,r13),r6\n"
    "\tmov.l @(12,r13),r7\n"
    "\tmov.l @(16,r13),r8\n"
    "\tmov.l @r13,r4\n"
    "\tmov #0,r9\n"
    "\tmov #24,r10\n"
    "\ttst r5,r5\n"
    "\tbt.s .Lpr_direct\n"
    "\tmov #-1,r11\n"
    "\t.align 2\n"
    ".Lpr_indirect:\n"
    "\tmov.w @r4+,r0\n"
    "\textu.w r0,r0\n"
    "\tadd r0,r0\n"
    "\tmov.w @(r0,r5),r1\n"
    "\textu.w r1,r1\n"
    SAT_PR_FACE(".Lpr_indirect", ".Lpr_mode_i", ".Lpr_shaded_i")
    ".Lpr_stop:\n"
    /* Written = max - faces left. */
    "\tmov.l @(16,r13),r0\n"
    "\tmov.l r9,@(20,r13)\n"
    "\tsub r8,r0\n"
    "\tmov.l @r15+,r13\n"
    "\tmov.l @r15+,r12\n"
    "\tmov.l @r15+,r11\n"
    "\tmov.l @r15+,r10\n"
    "\tmov.l @r15+,r9\n"
    "\trts\n"
    "\tmov.l @r15+,r8\n"
    SAT_PR_SHADED(".Lpr_mode_i", ".Lpr_shaded_i")
    "\t.align 2\n"
    ".Lpr_direct:\n"
    "\tmov.w @r4+,r1\n"
    "\textu.w r1,r1\n"
    SAT_PR_FACE(".Lpr_direct", ".Lpr_mode_d", ".Lpr_shaded_d")
    "\tbra .Lpr_stop\n"
    "\tnop\n"
    SAT_PR_SHADED(".Lpr_mode_d", ".Lpr_shaded_d")
    "\t.size _sat_scene3d_program_run_sh2,.-_sat_scene3d_program_run_sh2\n"
    "\t.text\n");
#undef SAT_PR_SHADED
#undef SAT_PR_FACE

inline uint32_t run(Job& job) { return sat_scene3d_program_run_sh2(&job); }
#else
inline uint32_t run(Job& job) { return reference(job); }
#endif

} // namespace saturn::scene3d::program_run
