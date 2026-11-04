#include <cstdio>
#include <cstdint>
#include <cstring>
#include "src/audio/music/codecs/adx.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "ADX FAIL %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

namespace {

/* The predictor written out with exact floor division in 64 bits, one sample at a time: what decode_block's shift
 * and nibble tricks must reproduce. */
void reference_block(const uint8_t* frame, const int16_t coefficient[2], int32_t& prev1, int32_t& prev2,
                     int16_t* out) {
    const int64_t scale = (static_cast<int64_t>(frame[0]) << 8) | frame[1];
    for (unsigned i = 0; i < 32u; ++i) {
        const unsigned nibble = (i & 1u) == 0u ? frame[2u + i / 2u] >> 4u : frame[2u + i / 2u] & 15u;
        const int64_t delta = nibble < 8u ? nibble : static_cast<int64_t>(nibble) - 16;
        const int64_t p = static_cast<int64_t>(coefficient[0]) * prev1 + static_cast<int64_t>(coefficient[1]) * prev2;
        const int64_t floor = p >= 0 ? p / 4096 : -((-p + 4095) / 4096);
        int64_t sample = delta * scale + floor;
        if (sample < -32768) sample = -32768;
        if (sample > 32767) sample = 32767;
        prev2 = prev1;
        prev1 = static_cast<int32_t>(sample);
        out[i] = static_cast<int16_t>(sample);
    }
}

uint32_t g_rng = 0x12345678u;
uint32_t rnd() {
    g_rng ^= g_rng << 13u;
    g_rng ^= g_rng >> 17u;
    g_rng ^= g_rng << 5u;
    return g_rng;
}

/* Random frames, scales up to 0x7FFF (clamping), and coefficients of both signs; also the floor itself near every
 * multiple of 4096 and at the int32_t ends. */
int random_blocks_match_reference() {
    using namespace saturn::core::audio::adx_adpcm;
    const int32_t edges[] = {INT32_MIN, INT32_MIN + 1, -4097, -4096, -4095, -1, 0, 1, 4095, 4096, 4097, INT32_MAX};
    for (int32_t p : edges) {
        const int64_t q = p >= 0 ? static_cast<int64_t>(p) / 4096 : -((-static_cast<int64_t>(p) + 4095) / 4096);
        OK(floor_div_4096(p) == q);
    }
    for (unsigned trial = 0; trial < 2000u; ++trial) {
        State state{};
        const int16_t c0 = static_cast<int16_t>(rnd() % 8192u), c1 = static_cast<int16_t>(-static_cast<int32_t>(rnd() % 4096u));
        const uint8_t header[4] = {static_cast<uint8_t>(c0 >> 8), static_cast<uint8_t>(c0),
                                   static_cast<uint8_t>(static_cast<uint16_t>(c1) >> 8), static_cast<uint8_t>(c1)};
        init(state, header);
        int32_t ref_prev[2][2] = {};
        for (unsigned block = 0; block < 8u; ++block) {
            uint8_t frames[36];
            for (uint8_t& b : frames) b = static_cast<uint8_t>(rnd());
            const uint32_t max_scale = trial % 4u == 0u ? 0x7FFFu : 0x0FFFu;
            for (unsigned ch = 0; ch < 2u; ++ch) {
                const uint32_t scale = rnd() % (max_scale + 1u);
                frames[ch * 18u] = static_cast<uint8_t>(scale >> 8);
                frames[ch * 18u + 1u] = static_cast<uint8_t>(scale);
            }
            uint8_t decoded[128];
            OK(decode_block(frames, 2u, state, decoded));
            for (unsigned ch = 0; ch < 2u; ++ch) {
                int16_t want[32];
                reference_block(frames + ch * 18u, state.coefficient, ref_prev[ch][0], ref_prev[ch][1], want);
                for (unsigned i = 0; i < 32u; ++i) OK(read_be_s16(decoded + ch * 64u + 2u * i) == want[i]);
                OK(state.previous[ch][0] == ref_prev[ch][0] && state.previous[ch][1] == ref_prev[ch][1]);
            }
        }
    }
    return 0;
}

}  // namespace

int main() {
    using namespace saturn::core::audio::adx_adpcm;
    // Generated with FFmpeg's ADX encoder at 22050 Hz / cutoff 500 Hz.
    // Expected PCM is independently decoded by FFmpeg's ADX decoder.
    const uint8_t header[4] = {0x19, 0xA9, 0xF5, 0xB6}; // 6569, -2634
    const uint8_t blocks[36] = {
        0x00,0xE8,0x07,0x32,0x42,0x33,0x32,0x32,0x21,
        0x21,0x01,0x0F,0xF0,0xEE,0xEE,0xDE,0xDD,0xDD,
        0x00,0x60,0xA9,0xAA,0xAC,0xCD,0xDF,0xF0,0x12,
        0x33,0x45,0x66,0x67,0x77,0x77,0x67,0x65,0x54
    };
    const int16_t expected[64] = {
        0, 1624, 3300, 4712, 6362, 7636, 8851, 9980,
        11009, 11701, 12382, 12797, 13024, 12890, 12761, 12408,
        11693, 11005, 10130, 8937, 7586, 6419, 4952, 3349,
        1722, 144, -1573, -3080, -4625, -6133, -7558, -8874,
        -9948, -10920, -11692, -12305, -12792, -12987, -12986, -12763,
        -12406, -11785, -11019, -10094, -9007, -7762, -6369, -4935,
        -3435, -1856, -192, 1461, 3042, 4611, 6110, 7505,
        8779, 9925, 10847, 11685, 12340, 12756, 13002, 13033
    };
    State state{};
    init(state, header);
    OK(state.coefficient[0] == 6569 && state.coefficient[1] == -2634);
    uint8_t decoded[64] = {};
    for (unsigned block = 0; block < 2; ++block) {
        OK(decode_block(blocks + block * 18u, 1, state, decoded));
        for (unsigned i = 0; i < 32; ++i) {
            OK(read_be_s16(decoded + 2u * i) == expected[block * 32u + i]);
        }
    }
    const uint8_t eof[18] = {0x80, 0x01};
    OK(!decode_block(eof, 1u, state, decoded));
    OK(!decode_block(blocks, 3u, state, decoded));
    OK(!decode_block(nullptr, 1u, state, decoded));
    reset_history(state);
    OK(state.previous[0][0] == 0 && state.previous[0][1] == 0);
    // Two distinct voices must have independent predictors and planar output.
    State stereo{};
    init(stereo, header);
    uint8_t two[36];
    std::memcpy(two, blocks, 18);
    std::memcpy(two + 18, blocks, 18);
    uint8_t out[128] = {};
    OK(decode_block(two, 2u, stereo, out));
    OK(std::memcmp(out, out + 64, 64) == 0);
    if (random_blocks_match_reference() != 0) return 1;
    std::puts("ADX ADPCM: OK");
    return 0;
}
