#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

#include "src/audio/music/codecs/adx_pull.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "ADX PULL FAIL %s:%d %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

using namespace saturn::core::audio::adx_adpcm;

static uint32_t next_random(uint32_t& seed) {
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}

/* One track of `blocks` encoded blocks (repeated cyclically, as the disc feed wraps the file), decoded through the
 * pull ring in random push and pull sizes, must equal decode_block run in order over the same bytes. */
static int run_case(uint8_t channels, uint32_t blocks, uint32_t sample_count, uint32_t ring_blocks,
                    uint32_t seed, uint32_t frames_to_check) {
    const uint32_t bb = block_bytes(channels);
    std::vector<uint8_t> file(blocks * bb);
    for (uint32_t i = 0u; i < file.size(); ++i) file[i] = static_cast<uint8_t>(next_random(seed));
    for (uint32_t b = 0u; b < blocks; ++b)
        for (uint32_t ch = 0u; ch < channels; ++ch) file[b * bb + ch * kFrameBytes] &= 0x7Fu;   /* scale: not the end marker */
    const uint8_t header[4] = {0x19, 0xA9, 0xF5, 0xB6};

    /* Reference: decode_block in stream order, with the feed's track rules. */
    std::vector<uint8_t> want_l(frames_to_check * 2u), want_r(frames_to_check * 2u);
    {
        State state{};
        init(state, header);
        uint8_t pcm[2u * kChannelBytes];
        uint32_t position = 0u, made = 0u, b = 0u;
        while (made < frames_to_check) {
            if (!decode_block(file.data() + b * bb, channels, state, pcm)) return 1;
            uint32_t take = kBlockFrames;
            if (take > sample_count - position) take = sample_count - position;
            for (uint32_t i = 0u; i < take * 2u && made * 2u + i < want_l.size(); ++i) {
                want_l[made * 2u + i] = pcm[i];
                if (channels == 2u) want_r[made * 2u + i] = pcm[kChannelBytes + i];
            }
            made += take;
            position += take;
            if (position == sample_count) {
                position = 0u;
                reset_history(state);
            }
            b = (b + 1u) % blocks;
        }
    }

    std::vector<uint8_t> encoded(ring_blocks * bb);
    BlockRing ring{};
    block_ring_start(ring, encoded.data(), ring_blocks, channels, sample_count, header);

    std::vector<uint8_t> got_l(frames_to_check * 2u), got_r(frames_to_check * 2u);
    uint32_t fed = 0u;        /* bytes of the cyclic stream taken so far */
    uint32_t got = 0u;
    uint32_t stalls = 0u;
    while (got < frames_to_check) {
        uint8_t slice[64];
        uint32_t want_bytes = 1u + next_random(seed) % sizeof slice;
        for (uint32_t i = 0u; i < want_bytes; ++i) slice[i] = file[(fed + i) % (blocks * bb)];
        uint32_t pushed = block_ring_push(ring, slice, want_bytes);
        fed += pushed;

        uint32_t ask = 1u + next_random(seed) % 90u;
        if (ask > frames_to_check - got) ask = frames_to_check - got;
        uint32_t pulled = block_ring_pull(ring, got_l.data() + got * 2u,
                                          channels == 2u ? got_r.data() + got * 2u : nullptr, ask);
        OK(!ring.failed);
        OK(pulled <= ask);
        got += pulled;
        stalls = (pushed == 0u && pulled == 0u) ? stalls + 1u : 0u;
        OK(stalls < 4u);      /* a ring that accepts and yields nothing for several rounds is stuck */
    }
    OK(std::memcmp(got_l.data(), want_l.data(), frames_to_check * 2u) == 0);
    if (channels == 2u) OK(std::memcmp(got_r.data(), want_r.data(), frames_to_check * 2u) == 0);
    return 0;
}

int main() {
    /* Ring of one block: every push waits for a pull; a track that ends mid-block (sample_count not a multiple of 32);
     * a track of whole blocks; a long run through many passes and many wraps of the 3-block ring. */
    OK(run_case(2u, 40u, 1269u, 3u, 1u, 40000u) == 0);
    OK(run_case(2u, 7u, 224u, 1u, 2u, 6000u) == 0);
    OK(run_case(2u, 300u, 9600u, 9u, 3u, 60000u) == 0);
    OK(run_case(2u, 300u, 9587u, 16u, 4u, 60000u) == 0);
    OK(run_case(1u, 40u, 1269u, 4u, 5u, 40000u) == 0);
    OK(run_case(1u, 5u, 33u, 2u, 6u, 4000u) == 0);
    std::printf("adx pull ring: stereo and mono match decode_block in order (push/pull slices, wraps, ring of 1-16 blocks)\n");
    return 0;
}
