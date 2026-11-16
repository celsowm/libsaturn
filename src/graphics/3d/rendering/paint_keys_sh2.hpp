#pragma once
/* The two scans of painter keys that come before the bucket placement, each
 * as one routine with one job:
 *
 *   range   the smallest and largest live key and how many are live
 *   spread  every live key replaced by its bucket, the buckets counted
 *
 * The SH-2 versions are hand-scheduled assembly; the `*_reference` functions
 * are the same contracts in C++, used on the host and as the definition of
 * the result. A key of 0xFFFFFFFF (kPaintSkip) is not live and is left
 * alone. */
#include <stddef.h>
#include <stdint.h>

namespace saturn::core::render3d::paint_keys {

constexpr uint32_t kSkip = 0xFFFFFFFFu;

/* Field order is the assembly's: change both together. */
struct Range {
    uint32_t lo, hi;
};

/* bucket = key - lo when `multiplier` is 0, else the high word of
 * (key - lo) * multiplier. `counts` has one uint16_t per bucket. */
struct Spread {
    uint32_t* keys;
    uint32_t count;
    uint16_t* counts;
    uint32_t lo, multiplier;
};

inline uint32_t range_reference(const uint32_t* keys, uint32_t count, Range* out) {
    uint32_t lo = 0xFFFFFFFFu, hi = 0u, live = 0u;
    for (uint32_t f = 0; f < count; ++f) {
        const uint32_t key = keys[f];
        if (key == kSkip) continue;
        if (key < lo) lo = key;
        if (key > hi) hi = key;
        ++live;
    }
    out->lo = lo;
    out->hi = hi;
    return live;
}

inline void spread_reference(const Spread& job) {
    for (uint32_t f = 0; f < job.count; ++f) {
        if (job.keys[f] == kSkip) continue;
        const uint32_t v = job.keys[f] - job.lo;
        const uint32_t bucket = job.multiplier == 0u ? v
            : static_cast<uint32_t>((static_cast<uint64_t>(v) * job.multiplier) >> 32u);
        job.keys[f] = bucket;
        ++job.counts[bucket];
    }
}

#if defined(__sh__)
#define SAT_PAINT_KEYS_SH2 1
static_assert(offsetof(Range,lo)==0u && offsetof(Range,hi)==4u && offsetof(Spread,keys)==0u &&
    offsetof(Spread,count)==4u && offsetof(Spread,counts)==8u && offsetof(Spread,lo)==12u &&
    offsetof(Spread,multiplier)==16u, "paint_keys layouts are fixed by the SH-2 routines");
extern "C" uint32_t sat_paint_keys_range_sh2(const uint32_t* keys, uint32_t count, Range* out);
extern "C" void sat_paint_keys_spread_sh2(const Spread* job);

/* range: a new minimum or maximum is rare after the first keys, so the loop
 * falls through its tests and branches only to update.
 *   r4 key   r5 keys left   r6 out   r1 lo   r2 hi   r3 live   r0 the key
 *
 * spread: one loop per bucket rule, chosen once.
 *   r4 key   r5 keys left   r6 counts   r7 lo   r2 multiplier
 *   r0 the key, then twice its bucket   r1 the count
 *
 * logic.hpp reaches many translation units, so the routines are assembled
 * only where SAT_PAINT_KEYS_SH2_EMIT is defined (rendering/api.cpp). */
#if defined(SAT_PAINT_KEYS_SH2_EMIT)
__asm__(
    "\t.section .text.sat_paint_keys_range_sh2,\"ax\",@progbits\n"
    "\t.align 2\n"
    "\t.global _sat_paint_keys_range_sh2\n"
    "\t.type _sat_paint_keys_range_sh2,@function\n"
    "_sat_paint_keys_range_sh2:\n"
    "\tmov #-1,r1\n"
    "\tmov #0,r2\n"
    "\ttst r5,r5\n"
    "\tbt.s .Lpk_range_done\n"
    "\tmov #0,r3\n"
    "\t.align 2\n"
    ".Lpk_range_key:\n"
    "\tmov.l @r4+,r0\n"
    "\tcmp/eq #-1,r0\n"
    "\tbt .Lpk_range_next\n"
    "\tcmp/hi r0,r1\n"
    "\tbt .Lpk_range_lo\n"
    ".Lpk_range_1:\n"
    "\tcmp/hi r2,r0\n"
    "\tbt .Lpk_range_hi\n"
    ".Lpk_range_2:\n"
    "\tadd #1,r3\n"
    ".Lpk_range_next:\n"
    "\tdt r5\n"
    "\tbf .Lpk_range_key\n"
    ".Lpk_range_done:\n"
    "\tmov.l r1,@r6\n"
    "\tmov.l r2,@(4,r6)\n"
    "\trts\n"
    "\tmov r3,r0\n"
    ".Lpk_range_lo:\n"
    "\tbra .Lpk_range_1\n"
    "\tmov r0,r1\n"
    ".Lpk_range_hi:\n"
    "\tbra .Lpk_range_2\n"
    "\tmov r0,r2\n"
    "\t.size _sat_paint_keys_range_sh2,.-_sat_paint_keys_range_sh2\n"
    "\t.section .text.sat_paint_keys_spread_sh2,\"ax\",@progbits\n"
    "\t.align 2\n"
    "\t.global _sat_paint_keys_spread_sh2\n"
    "\t.type _sat_paint_keys_spread_sh2,@function\n"
    "_sat_paint_keys_spread_sh2:\n"
    "\tmov.l @(4,r4),r5\n"
    "\tmov.l @(8,r4),r6\n"
    "\tmov.l @(12,r4),r7\n"
    "\tmov.l @(16,r4),r2\n"
    "\ttst r5,r5\n"
    "\tbt.s .Lpk_spread_done\n"
    "\tmov.l @r4,r4\n"
    "\ttst r2,r2\n"
    "\tbf .Lpk_scaled_key\n"
    "\t.align 2\n"
    ".Lpk_plain_key:\n"
    "\tmov.l @r4,r0\n"
    "\tcmp/eq #-1,r0\n"
    "\tbt .Lpk_plain_next\n"
    "\tsub r7,r0\n"
    "\tmov.l r0,@r4\n"
    "\tadd r0,r0\n"
    "\tmov.w @(r0,r6),r1\n"
    "\tadd #1,r1\n"
    "\tmov.w r1,@(r0,r6)\n"
    ".Lpk_plain_next:\n"
    "\tdt r5\n"
    "\tbf.s .Lpk_plain_key\n"
    "\tadd #4,r4\n"
    ".Lpk_spread_done:\n"
    "\trts\n"
    "\tnop\n"
    "\t.align 2\n"
    ".Lpk_scaled_key:\n"
    "\tmov.l @r4,r0\n"
    "\tcmp/eq #-1,r0\n"
    "\tbt .Lpk_scaled_next\n"
    "\tsub r7,r0\n"
    "\tdmulu.l r2,r0\n"
    "\tsts mach,r0\n"
    "\tmov.l r0,@r4\n"
    "\tadd r0,r0\n"
    "\tmov.w @(r0,r6),r1\n"
    "\tadd #1,r1\n"
    "\tmov.w r1,@(r0,r6)\n"
    ".Lpk_scaled_next:\n"
    "\tdt r5\n"
    "\tbf.s .Lpk_scaled_key\n"
    "\tadd #4,r4\n"
    "\trts\n"
    "\tnop\n"
    "\t.size _sat_paint_keys_spread_sh2,.-_sat_paint_keys_spread_sh2\n"
    "\t.text\n");
#endif

inline uint32_t range(const uint32_t* keys, uint32_t count, Range* out) {
    return sat_paint_keys_range_sh2(keys, count, out);
}
inline void spread(const Spread& job) { sat_paint_keys_spread_sh2(&job); }
#else
inline uint32_t range(const uint32_t* keys, uint32_t count, Range* out) {
    return range_reference(keys, count, out);
}
inline void spread(const Spread& job) { spread_reference(job); }
#endif

} // namespace saturn::core::render3d::paint_keys
