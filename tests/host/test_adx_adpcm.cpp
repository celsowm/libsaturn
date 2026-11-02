#include <cstdio>
#include <cstdint>
#include <cstring>
#include "src/audio/playback/adx_adpcm.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "ADX FAIL %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

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
    std::puts("ADX ADPCM: OK");
    return 0;
}
