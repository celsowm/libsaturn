#ifndef SATURN_CORE_AUDIO_ADX_ADPCM_HPP
#define SATURN_CORE_AUDIO_ADX_ADPCM_HPP

#include <stdint.h>

/* CRI ADX encoding 3, 18-byte frames, 32 PCM samples per channel.
 * The asset payload stores two big-endian signed LPC coefficients followed
 * by the native ADX frames (without the CRI file header / EOF marker).
 * Unlike IMA's independent blocks, the predictor must be carried across
 * successive frames and reset on a musical loop.
 */
namespace saturn::core::audio::adx_adpcm {

constexpr uint32_t kBlockFrames = 32u;
constexpr uint32_t kFrameBytes = 18u;
constexpr uint32_t kChannelBytes = kBlockFrames * 2u;
constexpr uint32_t kCoefficientBytes = 4u;

inline uint32_t block_bytes(uint8_t channels) {
    return kFrameBytes * channels;
}

struct State {
    int16_t coefficient[2];
    int16_t previous[2][2];  // most recent and previous sample per channel
};

inline void reset_history(State& state) {
    for (uint32_t channel = 0u; channel < 2u; ++channel) {
        state.previous[channel][0] = 0;
        state.previous[channel][1] = 0;
    }
}

inline int16_t read_be_s16(const uint8_t* value) {
    return static_cast<int16_t>(
        (static_cast<uint16_t>(value[0]) << 8u) | value[1]);
}

inline void init(State& state, const uint8_t* coefficient_header) {
    state.coefficient[0] = read_be_s16(coefficient_header);
    state.coefficient[1] = read_be_s16(coefficient_header + 2u);
    reset_history(state);
}

inline int32_t clamp(int32_t n) {
    if (n < -32768) return -32768;
    if (n > 32767) return 32767;
    return n;
}

inline bool decode_block(const uint8_t* source, uint8_t channels,
                         State& state, uint8_t* planar_s16be) {
    if (source == nullptr || planar_s16be == nullptr ||
        (channels != 1u && channels != 2u)) return false;
    for (uint8_t ch = 0u; ch < channels; ++ch) {
        const uint8_t* frame = source + ch * kFrameBytes;
        const uint32_t scale =
            (static_cast<uint32_t>(frame[0]) << 8u) | frame[1];
        if (scale & 0x8000u) return false; // ADX end marker is not PCM
        int32_t prev1 = state.previous[ch][0];
        int32_t prev2 = state.previous[ch][1];
        uint8_t* dest = planar_s16be + ch * kChannelBytes;
        for (uint32_t i = 0u; i < kBlockFrames; ++i) {
            const uint8_t packed = frame[2u + i / 2u];
            const uint8_t nibble = (i & 1u) == 0u ? packed >> 4u : packed & 15u;
            const int32_t delta = nibble < 8u ? nibble : static_cast<int32_t>(nibble) - 16;
            // Explicit floor division implements the signed arithmetic shift
            // used by the ADX predictor without relying on implementation-defined
            // right shifts of negative signed integers.
            const int32_t prediction = static_cast<int32_t>(state.coefficient[0]) * prev1 +
                                       static_cast<int32_t>(state.coefficient[1]) * prev2;
            const int32_t predicted = prediction >= 0 ? prediction / 4096 :
                -static_cast<int32_t>((-static_cast<int64_t>(prediction) + 4095) / 4096);
            const int32_t sample = clamp(delta * static_cast<int32_t>(scale) + predicted);
            prev2 = prev1;
            prev1 = sample;
            const uint16_t word = static_cast<uint16_t>(static_cast<int16_t>(sample));
            dest[i * 2u] = static_cast<uint8_t>(word >> 8u);
            dest[i * 2u + 1u] = static_cast<uint8_t>(word);
        }
        state.previous[ch][0] = static_cast<int16_t>(prev1);
        state.previous[ch][1] = static_cast<int16_t>(prev2);
    }
    return true;
}

} // namespace saturn::core::audio::adx_adpcm
#endif
