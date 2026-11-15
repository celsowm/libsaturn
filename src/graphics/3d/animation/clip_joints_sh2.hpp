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

#if defined(__sh__) && !defined(SAT_SKIN_CLIP_JOINTS_REFERENCE)
#define SAT_SKIN_CLIP_JOINTS_SH2 1
static_assert(offsetof(Job,joints)==0u && offsetof(Job,out)==4u && offsetof(Job,count)==8u &&
    offsetof(Job,scaled)==12u && offsetof(Job,rows)==16u && offsetof(Job,placement)==20u,
    "clip_joints::Job layout is fixed by the SH-2 routine");
/* Returns 0 when every joint was written. */
extern "C" uint32_t sat_skin_clip_joints_sh2(const Job* job);

/* Registers through the loop:
 *   r4 joint element   r5 output joint   r6 joints left   r7 column / origin walker
 *   r8 32768 (rounding)   r9 scaled walker   r10 rows walker   r11 scale
 *   r12 placement   r13 2^29 - 1   r14 2^30 - 1   r0-r3 scratch
 * Stack: @(0..32) the joint's 3x3 by columns   @(36..44) origin   @(48) 65536 */

/* One 3x3 element: range-checked, stored in its column. */
#define SAT_CJ_ELEMENT(disp, fail) \
    "\tmov.l @r4+,r0\n" \
    "\tmov r0,r1\n" \
    "\tadd r13,r1\n" \
    "\tcmp/hs r14,r1\n" \
    "\tbt " fail "\n" \
    "\tmov.l r0,@(" disp ",r15)\n"

/* origin = scale * translation + offset, as a checked 64-bit sum. */
#define SAT_CJ_ORIGIN(offset, slot, fail) \
    "\tmov.l @r4+,r0\n" \
    "\tdmuls.l r11,r0\n" \
    "\tmov.l @(" offset ",r12),r0\n" \
    "\tmov r0,r3\n" \
    "\tshll r3\n" \
    "\tsubc r3,r3\n" \
    "\tsts macl,r1\n" \
    "\tsts mach,r2\n" \
    "\tclrt\n" \
    "\taddc r0,r1\n" \
    "\taddc r3,r2\n" \
    "\tmov r1,r3\n" \
    "\tshll r3\n" \
    "\tsubc r3,r3\n" \
    "\tcmp/eq r3,r2\n" \
    "\tbf " fail "\n" \
    "\tmov.l r1,@(" slot ",r15)\n"

/* Bits 16..47 of the MAC into r0; fails unless bits 47..63 agree. */
#define SAT_CJ_TAKE(fail) \
    "\tsts mach,r1\n" \
    "\tsts macl,r0\n" \
    "\texts.w r1,r2\n" \
    "\txtrct r1,r0\n" \
    "\tcmp/eq r2,r1\n" \
    "\tbf " fail "\n"

#define SAT_CJ_COEFFICIENT(disp, fail) \
    "\tclrmac\n" \
    "\tlds r8,macl\n" \
    "\tmac.l @r9+,@r7+\n" \
    "\tmac.l @r9+,@r7+\n" \
    "\tmac.l @r9+,@r7+\n" \
    SAT_CJ_TAKE(fail) \
    "\tmov.l r0,@(" disp ",r5)\n"

/* One output row: three coefficients, then the translation. r9 ends on the
 * next scaled row, r10 on the next camera row. */
#define SAT_CJ_ROW(d0, d1, d2, d3, fail) \
    "\tmov r15,r7\n" \
    SAT_CJ_COEFFICIENT(d0, fail) \
    "\tadd #-12,r9\n" \
    SAT_CJ_COEFFICIENT(d1, fail) \
    "\tadd #-12,r9\n" \
    SAT_CJ_COEFFICIENT(d2, fail) \
    "\tclrmac\n" \
    "\tlds r8,macl\n" \
    "\tmac.l @r10+,@r7+\n" \
    "\tmac.l @r10+,@r7+\n" \
    "\tmac.l @r10+,@r7+\n" \
    "\tmac.l @r10+,@r7+\n" \
    SAT_CJ_TAKE(fail) \
    "\tmov.l r0,@(" d3 ",r5)\n"

__asm__(
    "\t.section .text.sat_skin_clip_joints_sh2,\"ax\",@progbits\n"
    "\t.align 2\n"
    "\t.global _sat_skin_clip_joints_sh2\n"
    "\t.type _sat_skin_clip_joints_sh2,@function\n"
    "_sat_skin_clip_joints_sh2:\n"
    "\tmov.l r8,@-r15\n"
    "\tmov.l r9,@-r15\n"
    "\tmov.l r10,@-r15\n"
    "\tmov.l r11,@-r15\n"
    "\tmov.l r12,@-r15\n"
    "\tmov.l r13,@-r15\n"
    "\tmov.l r14,@-r15\n"
    "\tadd #-52,r15\n"
    "\tmov.l @(8,r4),r6\n"
    "\tmov.l @(4,r4),r5\n"
    "\tmov.l @(12,r4),r9\n"
    "\tmov.l @(16,r4),r10\n"
    "\tmov.l @(20,r4),r12\n"
    "\tmov.l @r4,r4\n"
    "\ttst r6,r6\n"
    "\tbf .Lcj_start\n"
    "\tbra .Lcj_ok\n"
    "\tnop\n"
    ".Lcj_start:\n"
    "\tmov.l @r12,r11\n"
    "\tmov #1,r0\n"
    "\tshll16 r0\n"
    "\tmov.l r0,@(48,r15)\n"
    "\tmov r0,r8\n"
    "\tshlr r8\n"
    "\tmov r0,r13\n"
    "\tshll8 r13\n"
    "\tshll2 r13\n"
    "\tshll2 r13\n"
    "\tshll r13\n"
    "\tmov r13,r14\n"
    "\tadd #-1,r13\n"
    "\tadd r14,r14\n"
    "\tadd #-1,r14\n"
    "\t.align 2\n"
    ".Lcj_joint:\n"
    SAT_CJ_ELEMENT("0", ".Lcj_fail_a")
    SAT_CJ_ELEMENT("12", ".Lcj_fail_a")
    SAT_CJ_ELEMENT("24", ".Lcj_fail_a")
    SAT_CJ_ORIGIN("4", "36", ".Lcj_fail_a")
    SAT_CJ_ELEMENT("4", ".Lcj_fail_a")
    SAT_CJ_ELEMENT("16", ".Lcj_fail_a")
    SAT_CJ_ELEMENT("28", ".Lcj_fail_a")
    SAT_CJ_ORIGIN("8", "40", ".Lcj_fail_a")
    SAT_CJ_ELEMENT("8", ".Lcj_fail_a")
    SAT_CJ_ELEMENT("20", ".Lcj_fail_a")
    SAT_CJ_ELEMENT("32", ".Lcj_fail_a")
    SAT_CJ_ORIGIN("12", "44", ".Lcj_fail_a")
    "\tbra .Lcj_rows\n"
    "\tadd #16,r4\n"
    ".Lcj_fail_a:\n"
    "\tbra .Lcj_fail\n"
    "\tnop\n"
    ".Lcj_rows:\n"
    SAT_CJ_ROW("0", "4", "8", "12", ".Lcj_fail_a")
    SAT_CJ_ROW("16", "20", "24", "28", ".Lcj_fail_b")
    "\tbra .Lcj_last\n"
    "\tmov #0,r3\n"
    ".Lcj_fail_b:\n"
    "\tbra .Lcj_fail\n"
    "\tnop\n"
    ".Lcj_last:\n"
    SAT_CJ_ROW("32", "36", "40", "44", ".Lcj_fail_b")
    "\tmov.l r3,@(48,r5)\n"
    "\tmov.l r3,@(52,r5)\n"
    "\tmov.l r3,@(56,r5)\n"
    "\tmov.l r3,@(60,r5)\n"
    "\tadd #-36,r9\n"
    "\tadd #-48,r10\n"
    "\tdt r6\n"
    "\tbt.s .Lcj_ok\n"
    "\tadd #64,r5\n"
    "\tbra .Lcj_joint\n"
    "\tnop\n"
    ".Lcj_fail:\n"
    "\tbra .Lcj_return\n"
    "\tmov #1,r0\n"
    ".Lcj_ok:\n"
    "\tmov #0,r0\n"
    ".Lcj_return:\n"
    "\tadd #52,r15\n"
    "\tmov.l @r15+,r14\n"
    "\tmov.l @r15+,r13\n"
    "\tmov.l @r15+,r12\n"
    "\tmov.l @r15+,r11\n"
    "\tmov.l @r15+,r10\n"
    "\tmov.l @r15+,r9\n"
    "\trts\n"
    "\tmov.l @r15+,r8\n"
    "\t.size _sat_skin_clip_joints_sh2,.-_sat_skin_clip_joints_sh2\n"
    "\t.text\n");
#undef SAT_CJ_ROW
#undef SAT_CJ_COEFFICIENT
#undef SAT_CJ_TAKE
#undef SAT_CJ_ORIGIN
#undef SAT_CJ_ELEMENT

inline bool run(const Job& job) { return sat_skin_clip_joints_sh2(&job) == 0u; }
#else
inline bool run(const Job& job) { return reference(job); }
#endif

} // namespace saturn::skin3d::clip_joints
