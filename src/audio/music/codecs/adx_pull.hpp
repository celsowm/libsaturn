#ifndef SATURN_CORE_AUDIO_ADX_PULL_HPP
#define SATURN_CORE_AUDIO_ADX_PULL_HPP

#include "adx.hpp"

namespace saturn::core::audio::adx_adpcm {

/* Decode-on-pull ADX source. Encoded blocks wait in a ring the caller owns; pull() decodes the head block when
 * the output needs its frames, so only one 32-frame group of PCM exists at a time. A block is atomic, as in the
 * disc feed: the frames past sample_count are dropped, and the predictor history restarts when the track
 * wraps. Decoding the same bytes gives the same PCM as decode_block called in order. */
struct BlockRing {
    uint8_t* encoded;            /* capacity_blocks * block_bytes, owned by the caller */
    uint32_t capacity_blocks;
    uint32_t block_bytes;
    uint32_t head;               /* oldest queued block */
    uint32_t count;              /* queued blocks */
    uint32_t sample_count;       /* frames of the track pass */
    uint32_t position;           /* frame within the track pass, of the next frame to decode */
    uint32_t take;               /* frames the decoded head block yields */
    uint32_t taken;              /* frames of the decoded head block already pulled */
    uint8_t channels;
    uint8_t decoded;             /* the head block is decoded into pcm */
    uint8_t failed;              /* an encoded block was not PCM (end marker or bad channel count) */
    uint8_t carry[2u * kFrameBytes];   /* a block still arriving */
    uint32_t carry_have;
    State state;
    uint8_t pcm[2u * kChannelBytes];   /* the decoded head block, planar s16be */
};

inline void block_ring_start(BlockRing& ring, uint8_t* encoded, uint32_t capacity_blocks, uint8_t channels,
                             uint32_t sample_count, const uint8_t* coefficient_header) {
    ring.encoded = encoded;
    ring.capacity_blocks = capacity_blocks;
    ring.block_bytes = block_bytes(channels);
    ring.head = 0u;
    ring.count = 0u;
    ring.sample_count = sample_count;
    ring.position = 0u;
    ring.take = 0u;
    ring.taken = 0u;
    ring.channels = channels;
    ring.decoded = 0u;
    ring.failed = 0u;
    ring.carry_have = 0u;
    init(ring.state, coefficient_header);
}

/* Takes encoded bytes in stream order; returns how many it took, fewer when the ring is full. The caller passes
 * the rest again after a pull. */
inline uint32_t block_ring_push(BlockRing& ring, const uint8_t* data, uint32_t bytes) {
    uint32_t used = 0u;
    while (used < bytes && ring.count < ring.capacity_blocks) {
        uint32_t copy = ring.block_bytes - ring.carry_have;
        if (copy > bytes - used) copy = bytes - used;
        for (uint32_t i = 0u; i < copy; ++i) ring.carry[ring.carry_have + i] = data[used + i];
        ring.carry_have += copy;
        used += copy;
        if (ring.carry_have == ring.block_bytes) {
            uint8_t* slot = ring.encoded + ((ring.head + ring.count) % ring.capacity_blocks) * ring.block_bytes;
            for (uint32_t i = 0u; i < ring.block_bytes; ++i) slot[i] = ring.carry[i];
            ++ring.count;
            ring.carry_have = 0u;
        }
    }
    return used;
}

/* Writes up to `frames` frames: the left channel into `left`, the right into `right` (unused for mono). Returns the
 * frames written; fewer when the ring runs out of whole blocks or an encoded block fails (then `failed` is set). */
inline uint32_t block_ring_pull(BlockRing& ring, uint8_t* left, uint8_t* right, uint32_t frames) {
    uint32_t done = 0u;
    while (done < frames && ring.count > 0u && !ring.failed) {
        if (!ring.decoded) {
            const uint8_t* source = ring.encoded + ring.head * ring.block_bytes;
            if (!decode_block(source, ring.channels, ring.state, ring.pcm)) {
                ring.failed = 1u;
                break;
            }
            ring.take = kBlockFrames;
            if (ring.take > ring.sample_count - ring.position) ring.take = ring.sample_count - ring.position;
            ring.taken = 0u;
            ring.decoded = 1u;
        }
        uint32_t n = ring.take - ring.taken;
        if (n > frames - done) n = frames - done;
        for (uint32_t i = 0u; i < n * 2u; ++i) left[done * 2u + i] = ring.pcm[ring.taken * 2u + i];
        if (ring.channels == 2u)
            for (uint32_t i = 0u; i < n * 2u; ++i)
                right[done * 2u + i] = ring.pcm[kChannelBytes + ring.taken * 2u + i];
        ring.taken += n;
        ring.position += n;
        done += n;
        if (ring.taken == ring.take) {
            ring.head = (ring.head + 1u) % ring.capacity_blocks;
            --ring.count;
            ring.decoded = 0u;
            if (ring.position == ring.sample_count) {
                ring.position = 0u;
                reset_history(ring.state);
            }
        }
    }
    return done;
}

}  // namespace saturn::core::audio::adx_adpcm

#endif
