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

/* floor(p / 4096), the ADX predictor's arithmetic shift. The SH-2 has no barrel shifter: GCC calls a helper
 * (__ashiftrt_r4_12) for a signed shift by 12, but a logical one is three instructions. 2^31 is a multiple of 4096,
 * so biasing p into the unsigned range and back gives the floor for every int32_t. */
inline int32_t floor_div_4096(int32_t p) {
    return static_cast<int32_t>((static_cast<uint32_t>(p) + 0x80000000u) >> 12u) - 0x80000;
}

/* One sample from a 4-bit two's-complement nibble `delta` (sign already extended). */
inline int32_t next_sample(int32_t delta, int32_t scale, int32_t c0, int32_t c1, int32_t& prev1, int32_t& prev2) {
    const int32_t sample = clamp(delta * scale + floor_div_4096(c0 * prev1 + c1 * prev2));
    prev2 = prev1;
    prev1 = sample;
    return sample;
}

inline void store_s16be(uint8_t* dest, int32_t sample) {
    dest[0] = static_cast<uint8_t>(static_cast<uint32_t>(sample) >> 8u);
    dest[1] = static_cast<uint8_t>(sample);
}

/* Decodes one frame per channel: 32 S16BE samples to `left`, and with two channels to `right`. */
inline bool decode_block_to(const uint8_t* source, uint8_t channels,
                            State& state, uint8_t* left, uint8_t* right) {
    if (source == nullptr || left == nullptr || (channels == 2u && right == nullptr) ||
        (channels != 1u && channels != 2u)) return false;
    const int32_t c0 = state.coefficient[0];
    const int32_t c1 = state.coefficient[1];
    for (uint8_t ch = 0u; ch < channels; ++ch) {
        const uint8_t* frame = source + ch * kFrameBytes;
        const uint32_t scale =
            (static_cast<uint32_t>(frame[0]) << 8u) | frame[1];
        if (scale & 0x8000u) return false; // ADX end marker is not PCM
        int32_t prev1 = state.previous[ch][0];
        int32_t prev2 = state.previous[ch][1];
        uint8_t* dest = ch == 0u ? left : right;
        const int32_t s = static_cast<int32_t>(scale);
        // A byte holds two samples, the high nibble first; (n ^ 8) - 8 sign-extends a 4-bit n.
        for (uint32_t i = 0u; i < kBlockFrames / 2u; ++i) {
            const uint32_t packed = frame[2u + i];
            const int32_t high = static_cast<int32_t>((packed >> 4u) ^ 8u) - 8;
            const int32_t low = static_cast<int32_t>((packed & 15u) ^ 8u) - 8;
            store_s16be(dest, next_sample(high, s, c0, c1, prev1, prev2));
            store_s16be(dest + 2u, next_sample(low, s, c0, c1, prev1, prev2));
            dest += 4u;
        }
        state.previous[ch][0] = static_cast<int16_t>(prev1);
        state.previous[ch][1] = static_cast<int16_t>(prev2);
    }
    return true;
}

/* As decode_block_to, the channels one after the other in `planar_s16be` (kChannelBytes each). */
inline bool decode_block(const uint8_t* source, uint8_t channels,
                         State& state, uint8_t* planar_s16be) {
    if (planar_s16be == nullptr) return false;
    return decode_block_to(source, channels, state, planar_s16be, planar_s16be + kChannelBytes);
}

} // namespace saturn::core::audio::adx_adpcm
#endif
