#ifndef SATURN_CORE_AUDIO_ADX_ADPCM_HPP
#define SATURN_CORE_AUDIO_ADX_ADPCM_HPP

#include <stdint.h>

/* CRI ADX encoding 3, 18-byte frames, 32 PCM samples per channel.
 * The asset payload stores two big-endian signed LPC coefficients followed
 * by the native ADX frames (without the CRI file header / EOF marker).
 * Unlike IMA's independent blocks, the predictor must be carried across
 * successive frames and reset on a musical loop.
 */
#if defined(__sh__)
/* adx_sh2.cpp: one channel's frame (scale included) in SH-2 assembly, as decode_channel<true>; history is {p1, p2}. */
extern "C" void sat_adx_decode_channel_s16be(const uint8_t* frame, const int16_t* coefficients, int16_t* history,
                                             int16_t* dest);
#endif

namespace saturn::core::audio::adx_adpcm {

/* The per-sample helpers must inline into the loop even where the caller is built -Os (disc_feed.cpp): out of line,
 * GCC passes the history through the stack and calls once per sample. */
#define ADX_HOT_INLINE inline __attribute__((always_inline))

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

ADX_HOT_INLINE int32_t clamp(int32_t n) {
    if (n < -32768) return -32768;
    if (n > 32767) return 32767;
    return n;
}

/* floor(p / 4096), the ADX predictor's arithmetic shift. The SH-2 has no barrel shifter: GCC calls a helper
 * (__ashiftrt_r4_12) for a signed shift by 12, but a logical one is three instructions. 2^31 is a multiple of 4096,
 * so biasing p into the unsigned range and back gives the floor for every int32_t. */
ADX_HOT_INLINE int32_t floor_div_4096(int32_t p) {
    return static_cast<int32_t>((static_cast<uint32_t>(p) + 0x80000000u) >> 12u) - 0x80000;
}

/* One sample: `step` is the nibble's delta times the frame's scale. The coefficients and the clamped history fit
 * in 16 bits, so the products are 16x16 (MULS.W on the SH-2, faster than MUL.L). */
ADX_HOT_INLINE int32_t next_sample(int32_t step, int16_t c0, int16_t c1, int16_t& prev1, int16_t& prev2) {
    const int32_t prediction = static_cast<int32_t>(c0) * prev1 + static_cast<int32_t>(c1) * prev2;
    const int32_t sample = clamp(step + floor_div_4096(prediction));
    prev2 = prev1;
    prev1 = static_cast<int16_t>(sample);
    return sample;
}

ADX_HOT_INLINE void store_s16be(uint8_t* dest, int32_t sample) {
    dest[0] = static_cast<uint8_t>(static_cast<uint32_t>(sample) >> 8u);
    dest[1] = static_cast<uint8_t>(sample);
}

/* 32 samples of one channel. kAligned: `dest` is 2-byte aligned and the target big-endian, so a halfword store
 * is the S16BE sample. */
template <bool kAligned>
ADX_HOT_INLINE void decode_channel(const uint8_t* nibbles, const int32_t* steps, int16_t c0, int16_t c1,
                           int16_t& prev1, int16_t& prev2, uint8_t* dest) {
    for (uint32_t i = 0u; i < kBlockFrames / 2u; ++i) {
        const uint32_t packed = nibbles[i];
        const int32_t high = next_sample(steps[packed >> 4u], c0, c1, prev1, prev2);
        const int32_t low = next_sample(steps[packed & 15u], c0, c1, prev1, prev2);
        if constexpr (kAligned) {
            int16_t* const out = reinterpret_cast<int16_t*>(dest);
            out[0] = static_cast<int16_t>(high);
            out[1] = static_cast<int16_t>(low);
        } else {
            store_s16be(dest, high);
            store_s16be(dest + 2u, low);
        }
        dest += 4u;
    }
}

/* Decodes one frame per channel: 32 S16BE samples to `left`, and with two channels to `right`. */
inline bool decode_block_to(const uint8_t* source, uint8_t channels,
                            State& state, uint8_t* left, uint8_t* right) {
    if (source == nullptr || left == nullptr || (channels == 2u && right == nullptr) ||
        (channels != 1u && channels != 2u)) return false;
    const int16_t c0 = state.coefficient[0];
    const int16_t c1 = state.coefficient[1];
    for (uint8_t ch = 0u; ch < channels; ++ch) {
        const uint8_t* frame = source + ch * kFrameBytes;
        const uint32_t scale =
            (static_cast<uint32_t>(frame[0]) << 8u) | frame[1];
        if (scale & 0x8000u) return false; // ADX end marker is not PCM
        uint8_t* const dest = ch == 0u ? left : right;
#if defined(__sh__)
        if ((reinterpret_cast<uintptr_t>(dest) & 1u) == 0u) {
            sat_adx_decode_channel_s16be(frame, state.coefficient, state.previous[ch],
                                         reinterpret_cast<int16_t*>(dest));
            continue;
        }
#endif
        // A nibble n is the delta (n ^ 8) - 8; steps[n] is that delta times the scale, built with adds.
        int32_t steps[16];
        int32_t step = -8 * static_cast<int32_t>(scale);
        for (uint32_t k = 0u; k < 16u; ++k) {
            steps[k ^ 8u] = step;
            step += static_cast<int32_t>(scale);
        }
        int16_t prev1 = state.previous[ch][0];
        int16_t prev2 = state.previous[ch][1];
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        if ((reinterpret_cast<uintptr_t>(dest) & 1u) == 0u)
            decode_channel<true>(frame + 2u, steps, c0, c1, prev1, prev2, dest);
        else
#endif
            decode_channel<false>(frame + 2u, steps, c0, c1, prev1, prev2, dest);
        state.previous[ch][0] = prev1;
        state.previous[ch][1] = prev2;
    }
    return true;
}

/* As decode_block_to, the channels one after the other in `planar_s16be` (kChannelBytes each). */
inline bool decode_block(const uint8_t* source, uint8_t channels,
                         State& state, uint8_t* planar_s16be) {
    if (planar_s16be == nullptr) return false;
    return decode_block_to(source, channels, state, planar_s16be, planar_s16be + kChannelBytes);
}

#undef ADX_HOT_INLINE

} // namespace saturn::core::audio::adx_adpcm
#endif
