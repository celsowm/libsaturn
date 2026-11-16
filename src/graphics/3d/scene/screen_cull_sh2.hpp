#pragma once
/* The screen-culled append of one pre-projected instance, as one routine with
 * one job: test each face's projected area and write the survivors' queue
 * entries and painter keys. It owns nothing else: the caller fills the order
 * table and the scene's count from the returned number of survivors.
 *
 * The SH-2 version is hand-scheduled assembly; `reference` is the same
 * contract in C++, used on the host and as the definition of the result.
 * The compiled loop (append_screen_culled in faces.cpp) spends its time on
 * what a 16-register machine forces on a compiler here: values reloaded from
 * the stack, a multiply to index the entries, and a use right after each
 * load. The assembly keeps every per-face value in a register, walks the
 * arrays by post-increment, and places each load two instructions ahead of
 * its use. */
#include <stddef.h>
#include <stdint.h>
#include "saturn/scene3d_faces.h"

namespace saturn::scene3d::screen_cull {

/* Field order is the assembly's: change both together. */
struct Job {
    const uint16_t* offsets;          /* 4 per face: byte offsets into `projected`, each < 32768 */
    const sat_projected_vertex_t* projected;
    const uint8_t* double_sided;      /* 1 per face, never NULL: non-zero skips the area test */
    const uint16_t* face_materials;   /* 1 per face: index into `programs`, each < 32768 */
    uint32_t face_count;
    sat_scene3d_projected_face_t* entry; /* first entry to write */
    uint32_t* key;                    /* first painter key to write */
    const sat_scene3d_material_program_t* programs;
    uint32_t pass_bits;               /* the key's pass field, already in place */
};

constexpr uint32_t kDepthMax=(1u<<24u)-2u;
constexpr uint32_t kProgramWord=static_cast<uint32_t>(SAT_SCENE3D_PROJECTED_PROGRAM)<<24u;

/* What the assembly assumes about the records it reads and writes. */
static_assert(sizeof(void*)!=4u ||
    (offsetof(Job,offsets)==0u && offsetof(Job,projected)==4u && offsetof(Job,double_sided)==8u &&
     offsetof(Job,face_materials)==12u && offsetof(Job,face_count)==16u && offsetof(Job,entry)==20u &&
     offsetof(Job,key)==24u && offsetof(Job,programs)==28u && offsetof(Job,pass_bits)==32u),
    "screen_cull::Job layout is fixed by the SH-2 routine");
static_assert(sizeof(sat_projected_vertex_t)==8u && offsetof(sat_projected_vertex_t,x)==0u &&
    offsetof(sat_projected_vertex_t,y)==2u && offsetof(sat_projected_vertex_t,w)==4u,
    "projected vertex layout is fixed by the SH-2 routine");
static_assert(sizeof(void*)!=4u ||
    (sizeof(sat_scene3d_projected_face_t)==24u && offsetof(sat_scene3d_projected_face_t,program)==16u &&
     offsetof(sat_scene3d_projected_face_t,kind)==20u && sizeof(sat_scene3d_material_program_t)==16u),
    "queue entry and program layouts are fixed by the SH-2 routine");
static_assert(kProgramWord==0x01000000u && kDepthMax==0x00FFFFFEu,
    "constants in the SH-2 routine's literal pool");

/* Survivors written. Entries and keys are written in face order, with no
 * gaps; nothing else is touched. */
inline uint32_t reference(const Job& job) {
    sat_scene3d_projected_face_t* entry=job.entry;
    uint32_t* key=job.key;
    const auto* const base=reinterpret_cast<const uint8_t*>(job.projected);
    for(uint32_t f=0u;f<job.face_count;++f) {
        const sat_projected_vertex_t* corner[4];
        for(unsigned i=0u;i<4u;++i)
            corner[i]=reinterpret_cast<const sat_projected_vertex_t*>(base+job.offsets[f*4u+i]);
        const auto& a=*corner[0];const auto& b=*corner[1];
        const auto& c=*corner[2];const auto& d=*corner[3];
        if(!job.double_sided[f] &&
           (static_cast<int32_t>(c.x)-a.x)*(static_cast<int32_t>(d.y)-b.y)<=
           (static_cast<int32_t>(d.x)-b.x)*(static_cast<int32_t>(c.y)-a.y)) continue;
        for(unsigned i=0u;i<4u;++i) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__
            entry->xy[i]=static_cast<uint32_t>(static_cast<uint16_t>(corner[i]->y))<<16u|
                static_cast<uint16_t>(corner[i]->x);
#else
            entry->xy[i]=static_cast<uint32_t>(static_cast<uint16_t>(corner[i]->x))<<16u|
                static_cast<uint16_t>(corner[i]->y);
#endif
        }
        entry->program=&job.programs[job.face_materials[f]];
        entry->kind=SAT_SCENE3D_PROJECTED_PROGRAM;entry->cached_projected=0u;entry->reserved=0u;
        const uint32_t w0=static_cast<uint32_t>(a.w),w1=static_cast<uint32_t>(b.w);
        const uint32_t w2=static_cast<uint32_t>(c.w),w3=static_cast<uint32_t>(d.w);
        uint32_t depth=kDepthMax;
        if(((w0|w1|w2|w3)>>30u)==0u) {
            const uint32_t quotient=(w0+w1+w2+w3)>>6u;
            if(quotient<depth)depth=quotient;
        }
        *key++=job.pass_bits|depth;
        ++entry;
    }
    return static_cast<uint32_t>(key-job.key);
}

#if defined(__sh__)
#define SAT_SCENE3D_SCREEN_CULL_SH2 1
extern "C" uint32_t sat_scene3d_screen_cull_sh2(const Job* job);

/* Registers through the loop:
 *   r4 offsets   r5 projected   r6 double_sided   r7 face_materials
 *   r8 faces left   r9 entry   r10 key
 *   r1 r2 r3 r13 corners a b c d   r0 r11 r12 r14 scratch
 * Stack: @(0) first key, @(4) programs, @(8) pass bits. */
__asm__(
    "\t.section .text.sat_scene3d_screen_cull_sh2,\"ax\",@progbits\n"
    "\t.align 2\n"
    "\t.global _sat_scene3d_screen_cull_sh2\n"
    "\t.type _sat_scene3d_screen_cull_sh2,@function\n"
    "_sat_scene3d_screen_cull_sh2:\n"
    "\tmov.l r8,@-r15\n"
    "\tmov.l r9,@-r15\n"
    "\tmov.l r10,@-r15\n"
    "\tmov.l r11,@-r15\n"
    "\tmov.l r12,@-r15\n"
    "\tmov.l r13,@-r15\n"
    "\tmov.l r14,@-r15\n"
    "\tmov.l @(32,r4),r0\n"
    "\tmov.l @(28,r4),r1\n"
    "\tmov.l @(24,r4),r10\n"
    "\tmov.l r0,@-r15\n"
    "\tmov.l r1,@-r15\n"
    "\tmov.l r10,@-r15\n"
    "\tmov.l @(20,r4),r9\n"
    "\tmov.l @(16,r4),r8\n"
    "\tmov.l @(12,r4),r7\n"
    "\tmov.l @(8,r4),r6\n"
    "\tmov.l @(4,r4),r5\n"
    "\tmov.l @r4,r4\n"
    "\ttst r8,r8\n"
    "\tbt .Lsc_done\n"
    "\t.align 2\n"
    /* The face's four corners; a double-sided face skips the area test. */
    ".Lsc_face:\n"
    "\tmov.w @r4+,r1\n"
    "\tmov.w @r4+,r2\n"
    "\tmov.w @r4+,r3\n"
    "\tmov.w @r4+,r13\n"
    "\tmov.b @r6+,r0\n"
    "\tadd r5,r1\n"
    "\tadd r5,r2\n"
    "\tadd r5,r3\n"
    "\ttst r0,r0\n"
    "\tbf.s .Lsc_two_sided\n"
    "\tadd r5,r13\n"
    /* (c.x-a.x)*(d.y-b.y) > (d.x-b.x)*(c.y-a.y), or the face is culled.
     * The second product's operands load while the first multiplies. */
    "\tmov.w @(2,r13),r0\n"
    "\tmov.w @r3,r11\n"
    "\tmov r0,r12\n"
    "\tmov.w @(2,r2),r0\n"
    "\tmov.w @r1,r14\n"
    "\tsub r0,r12\n"
    "\tmov.w @(2,r3),r0\n"
    "\tsub r14,r11\n"
    "\tmul.l r12,r11\n"
    "\tmov r0,r12\n"
    "\tmov.w @(2,r1),r0\n"
    "\tmov.w @r13,r11\n"
    "\tmov.w @r2,r14\n"
    "\tsub r0,r12\n"
    "\tsts macl,r0\n"
    "\tsub r14,r11\n"
    "\tmul.l r12,r11\n"
    "\tmov.w @r7+,r12\n"
    "\tsts macl,r11\n"
    "\tcmp/gt r11,r0\n"
    "\tbf .Lsc_next\n"
    /* The entry: four XY words, the program, the kind word. r12 holds the
     * face's material index. */
    ".Lsc_keep:\n"
    "\tmov.l @r1,r0\n"
    "\tmov.l @r2,r11\n"
    "\tmov.l r0,@r9\n"
    "\tmov.l @r3,r0\n"
    "\tmov.l r11,@(4,r9)\n"
    "\tmov.l @r13,r11\n"
    "\tmov.l r0,@(8,r9)\n"
    "\tmov.l @(4,r15),r0\n"
    "\tshll2 r12\n"
    "\tmov.l r11,@(12,r9)\n"
    "\tshll2 r12\n"
    "\tmov.l @(4,r1),r1\n"
    "\tadd r0,r12\n"
    "\tmov.l @(4,r2),r2\n"
    "\tmov.l r12,@(16,r9)\n"
    "\tmov.l .Lsc_program_word,r0\n"
    "\tmov.l @(4,r3),r3\n"
    "\tmov.l r0,@(20,r9)\n"
    "\tmov.l @(4,r13),r13\n"
    /* The painter key: the four depths' sum over 64, or the maximum when
     * any depth has one of its top two bits set or the quotient reaches it. */
    "\tmov r1,r0\n"
    "\tor r2,r0\n"
    "\tadd r2,r1\n"
    "\tor r3,r0\n"
    "\tadd r3,r1\n"
    "\tor r13,r0\n"
    "\tadd r13,r1\n"
    "\tmov.l .Lsc_top_bits,r11\n"
    "\tmov.l .Lsc_depth_max,r2\n"
    "\ttst r11,r0\n"
    "\tbf.s .Lsc_key\n"
    "\tshlr2 r1\n"
    "\tshlr2 r1\n"
    "\tshlr2 r1\n"
    "\tcmp/hi r1,r2\n"
    "\tbf .Lsc_key\n"
    "\tmov r1,r2\n"
    ".Lsc_key:\n"
    "\tmov.l @(8,r15),r0\n"
    "\tadd #24,r9\n"
    "\tor r0,r2\n"
    "\tmov.l r2,@r10\n"
    "\tadd #4,r10\n"
    ".Lsc_next:\n"
    "\tdt r8\n"
    "\tbf .Lsc_face\n"
    ".Lsc_done:\n"
    "\tmov.l @r15+,r1\n"
    "\tmov r10,r0\n"
    "\tsub r1,r0\n"
    "\tadd #8,r15\n"
    "\tshlr2 r0\n"
    "\tmov.l @r15+,r14\n"
    "\tmov.l @r15+,r13\n"
    "\tmov.l @r15+,r12\n"
    "\tmov.l @r15+,r11\n"
    "\tmov.l @r15+,r10\n"
    "\tmov.l @r15+,r9\n"
    "\trts\n"
    "\tmov.l @r15+,r8\n"
    ".Lsc_two_sided:\n"
    "\tbra .Lsc_keep\n"
    "\tmov.w @r7+,r12\n"
    "\t.align 2\n"
    ".Lsc_program_word:\n"
    "\t.long 0x01000000\n"
    ".Lsc_top_bits:\n"
    "\t.long 0xC0000000\n"
    ".Lsc_depth_max:\n"
    "\t.long 0x00FFFFFE\n"
    "\t.size _sat_scene3d_screen_cull_sh2,.-_sat_scene3d_screen_cull_sh2\n"
    "\t.text\n");

inline uint32_t run(const Job& job) { return sat_scene3d_screen_cull_sh2(&job); }
#else
inline uint32_t run(const Job& job) { return reference(job); }
#endif

} // namespace saturn::scene3d::screen_cull
