#pragma once
/* The two scans of painter keys that come before the bucket placement, each
 * as one routine with one job:
 *
 *   quantise  every live key's depth rescaled and capped, its group kept
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

/* Every live key's depth (its bits under `low_mask`) becomes
 * min(max, the high word of depth * multiplier); the bits above stay. With
 * multiplier = ceil(2^32 / d) that is depth / d exactly while
 * depth * (multiplier * d - 2^32) < 2^32. */
struct Quantise {
    uint32_t* keys;
    uint32_t count;
    uint32_t low_mask, multiplier, max;
};

inline void quantise_reference(const Quantise& job) {
    for (uint32_t f = 0; f < job.count; ++f) {
        const uint32_t key = job.keys[f];
        if (key == kSkip) continue;
        uint32_t depth = static_cast<uint32_t>(
            (static_cast<uint64_t>(key & job.low_mask) * job.multiplier) >> 32u);
        if (depth > job.max) depth = job.max;
        job.keys[f] = (key & ~job.low_mask) | depth;
    }
}

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
    offsetof(Spread,multiplier)==16u && offsetof(Quantise,keys)==0u && offsetof(Quantise,count)==4u &&
    offsetof(Quantise,low_mask)==8u && offsetof(Quantise,multiplier)==12u && offsetof(Quantise,max)==16u,
    "paint_keys layouts are fixed by the SH-2 routines");
extern "C" void sat_paint_keys_quantise_sh2(const Quantise* job);
extern "C" uint32_t sat_paint_keys_range_sh2(const uint32_t* keys, uint32_t count, Range* out);
extern "C" void sat_paint_keys_spread_sh2(const Spread* job);
/* Experimental final stages. Preconditions: aligned uint32_t words, valid
 * bucket indices or kSkip, sum of counts <= 65535, disjoint buffers.
 * The prefix writes descending starts; placement advances each start
 * while walking ascending face indices. Do not dispatch by default. */
extern "C" void sat_paint_prefix_sh2(uint32_t* words, uint32_t word_count);
extern "C" void sat_paint_place8_sh2(const uint32_t* keys,uint32_t count,uint8_t* out,uint16_t* start);
extern "C" void sat_paint_place16_sh2(const uint32_t* keys,uint32_t count,uint16_t* out,uint16_t* start);
/* Assembled from paint_keys_sh2.s. */
inline uint32_t range(const uint32_t* keys, uint32_t count, Range* out) {
    return sat_paint_keys_range_sh2(keys, count, out);
}
inline void spread(const Spread& job) { sat_paint_keys_spread_sh2(&job); }
inline void quantise(const Quantise& job) { sat_paint_keys_quantise_sh2(&job); }
#else
inline uint32_t range(const uint32_t* keys, uint32_t count, Range* out) {
    return range_reference(keys, count, out);
}
inline void spread(const Spread& job) { spread_reference(job); }
inline void quantise(const Quantise& job) { quantise_reference(job); }
#endif

} // namespace saturn::core::render3d::paint_keys
