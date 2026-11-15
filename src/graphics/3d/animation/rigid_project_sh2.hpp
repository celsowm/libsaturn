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
 * The divider: the X divide runs under the Y row, the Y divide under the next
 * vertex's X and W rows. DVSR keeps w between the two, so the Y divide writes
 * only its dividend. Like every user of project_axis_start, this assumes no
 * interrupt handler leaves the divider changed. */
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

#if defined(__sh__) && !defined(SAT_SKIN_RIGID_PROJECT_REFERENCE)
#define SAT_SKIN_RIGID_PROJECT_SH2 1
static_assert(offsetof(Job,rigid)==0u && offsetof(Job,joints)==4u && offsetof(Job,out)==8u && offsetof(Job,count)==12u && offsetof(Job,width)==16u &&
    offsetof(Job,y_scale)==20u && offsetof(Job,mult)==24u && offsetof(Job,w_last)==28u,
    "rigid_project::Job layout is fixed by the SH-2 routine");
static_assert(saturn::core::math3d::kDivuDivisorAddress==0xFFFFFF00u &&
    saturn::core::math3d::kDivuDividendHighAddress==0xFFFFFF10u &&
    saturn::core::math3d::kDivuDividendLowQuotientAddress==0xFFFFFF14u,
    "the SH-2 routine addresses the divider from 0xFFFFFF00");
extern "C" uint32_t sat_skin_rigid_project_sh2(const Job* job);

/* Registers through the loop:
 *   r4 record   r5 joints   r6 output vertex   r7 vertices left
 *   r8 32768 (row rounding)   r9 divider base   r10 width   r11 -1   r12 mult
 *   r13 reach = w * mult - 1   r14 2 * reach
 *   r1 x, then y   r2 w   r3 joint row pointer   r0 scratch
 * Stack: @(0..8) the doubled position   @(12) w_last   @(16) y_scale
 *        @(20) count */

/* One clip row into `out`: r3 walks the row, r0 the position. */
#define SAT_RIGID_ROW(out) \
    "\tmov r15,r0\n" \
    "\tclrmac\n" \
    "\tlds r8,macl\n" \
    "\tmac.l @r3+,@r0+\n" \
    "\tmac.l @r3+,@r0+\n" \
    "\tmac.l @r3+,@r0+\n" \
    "\tsts mach,r0\n" \
    "\tsts macl," out "\n" \
    "\txtrct r0," out "\n" \
    "\tmov.l @r3+,r0\n" \
    "\tadd r0," out "\n"

/* Reads one record and forms its X row (r1) and W row (r2); r3 ends past the
 * W row. */
#define SAT_RIGID_XW \
    "\tmov.w @r4+,r0\n" \
    "\tmov.w @r4+,r1\n" \
    "\tmov.w @r4+,r2\n" \
    "\tmov.w @r4+,r3\n" \
    "\tadd r0,r0\n" \
    "\tadd r1,r1\n" \
    "\tadd r2,r2\n" \
    "\tmov.l r0,@r15\n" \
    "\tmov.l r1,@(4,r15)\n" \
    "\tmov.l r2,@(8,r15)\n" \
    "\textu.w r3,r3\n" \
    "\tshlr8 r3\n" \
    "\tshll8 r3\n" \
    "\tshlr2 r3\n" \
    "\tadd r5,r3\n" \
    SAT_RIGID_ROW("r1") \
    "\tadd #16,r3\n" \
    SAT_RIGID_ROW("r2")

/* The divider's quotient halved toward zero, in r0. */
#define SAT_RIGID_QUOTIENT \
    "\tmov.l @(20,r9),r0\n" \
    "\tcmp/pz r0\n" \
    "\tsubc r11,r0\n" \
    "\tshar r0\n"

__asm__(
    "\t.section .text.sat_skin_rigid_project_sh2,\"ax\",@progbits\n"
    "\t.align 2\n"
    "\t.global _sat_skin_rigid_project_sh2\n"
    "\t.type _sat_skin_rigid_project_sh2,@function\n"
    "_sat_skin_rigid_project_sh2:\n"
    "\tmov.l r8,@-r15\n"
    "\tmov.l r9,@-r15\n"
    "\tmov.l r10,@-r15\n"
    "\tmov.l r11,@-r15\n"
    "\tmov.l r12,@-r15\n"
    "\tmov.l r13,@-r15\n"
    "\tmov.l r14,@-r15\n"
    "\tadd #-24,r15\n"
    "\tmov.l @(28,r4),r0\n"
    "\tmov.l @(20,r4),r1\n"
    "\tmov.l @(12,r4),r7\n"
    "\tmov.l r0,@(12,r15)\n"
    "\tmov.l r1,@(16,r15)\n"
    "\tmov.l r7,@(20,r15)\n"
    "\tmov.l @(16,r4),r10\n"
    "\tmov.l @(24,r4),r12\n"
    "\tmov.l @(4,r4),r5\n"
    "\tmov.l @(8,r4),r6\n"
    "\tmov.l @r4,r4\n"
    "\tmov #1,r8\n"
    "\tmov #-1,r9\n"
    "\tshll16 r8\n"
    "\tshll8 r9\n"
    "\tshlr r8\n"
    "\tmov #-1,r11\n"
    SAT_RIGID_XW
    "\tbra .Lrp_have\n"
    "\tnop\n"
    "\t.align 2\n"
    ".Lrp_vertex:\n"
    SAT_RIGID_XW
    /* The vertex before's Y, whose divide ran under those rows. */
    SAT_RIGID_QUOTIENT
    "\tmov.w r0,@(2,r6)\n"
    "\tadd #8,r6\n"
    ".Lrp_have:\n"
    /* 0 < w <= w_last, then |x| <= reach. */
    "\tmov.l @(12,r15),r13\n"
    "\tmov r2,r0\n"
    "\tadd #-1,r0\n"
    "\tcmp/hs r13,r0\n"
    "\tbt .Lrp_stop\n"
    "\tmul.l r12,r2\n"
    "\tmov.l r2,@(4,r6)\n"
    "\tsts macl,r13\n"
    "\tadd #-1,r13\n"
    "\tmov r13,r14\n"
    "\tadd r13,r14\n"
    "\tmov r1,r0\n"
    "\tadd r13,r0\n"
    "\tcmp/hi r14,r0\n"
    "\tbt .Lrp_stop\n"
    /* X divide: w, then the 64-bit numerator. */
    "\tdmuls.l r10,r1\n"
    "\tmov.l r2,@r9\n"
    "\tsts mach,r0\n"
    "\tsts macl,r1\n"
    "\tmov.l r0,@(16,r9)\n"
    "\tmov.l r1,@(20,r9)\n"
    /* Y row under it. */
    "\tadd #-32,r3\n"
    SAT_RIGID_ROW("r1")
    "\tmov r1,r0\n"
    "\tadd r13,r0\n"
    "\tcmp/hi r14,r0\n"
    "\tbt .Lrp_stop\n"
    "\tmov.l @(16,r15),r0\n"
    "\tdmuls.l r0,r1\n"
    SAT_RIGID_QUOTIENT
    "\tmov.w r0,@r6\n"
    /* Y divide: the divisor is still w. */
    "\tsts mach,r0\n"
    "\tsts macl,r1\n"
    "\tmov.l r0,@(16,r9)\n"
    "\tmov.l r1,@(20,r9)\n"
    "\tdt r7\n"
    "\tbf .Lrp_vertex\n"
    SAT_RIGID_QUOTIENT
    "\tmov.w r0,@(2,r6)\n"
    /* Finished = count - vertices left. */
    ".Lrp_stop:\n"
    "\tmov.l @(20,r15),r0\n"
    "\tadd #24,r15\n"
    "\tsub r7,r0\n"
    "\tmov.l @r15+,r14\n"
    "\tmov.l @r15+,r13\n"
    "\tmov.l @r15+,r12\n"
    "\tmov.l @r15+,r11\n"
    "\tmov.l @r15+,r10\n"
    "\tmov.l @r15+,r9\n"
    "\trts\n"
    "\tmov.l @r15+,r8\n"
    "\t.size _sat_skin_rigid_project_sh2,.-_sat_skin_rigid_project_sh2\n"
    "\t.text\n");
#undef SAT_RIGID_QUOTIENT
#undef SAT_RIGID_XW
#undef SAT_RIGID_ROW

inline uint32_t run(const Job& job) { return sat_skin_rigid_project_sh2(&job); }
#else
inline uint32_t run(const Job& job) { return reference(job); }
#endif

} // namespace saturn::skin3d::rigid_project
