#ifndef SATURN_CORE_AUDIO_SYNTH_LOGIC_HPP
#define SATURN_CORE_AUDIO_SYNTH_LOGIC_HPP

/* Pure, host-testable procedural PCM. No hardware access, so
 * tests/host/test_audio_synth_logic.cpp links this directly. */

#include <stdint.h>

#include "saturn/core.h"

namespace saturn::core::audio_synth {

/* Linear decay to silence across the whole buffer, in the 0..55 range the
 * blip/noise shapes are scaled against. */
inline int32_t decay(uint32_t index, uint32_t count) {
    return static_cast<int32_t>(((count - index) * 55u) / count);
}

inline sat_result_t blip(int8_t* out, uint32_t count, uint8_t step) {
    if (out == nullptr || count == 0u || step == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    for (uint32_t i = 0; i < count; ++i) {
        const int32_t wave = static_cast<int32_t>((i * step) & 63u) - 32;
        out[i] = static_cast<int8_t>(wave * decay(i, count) / 32);
    }
    return SAT_OK;
}

inline sat_result_t noise(int8_t* out, uint32_t count) {
    if (out == nullptr || count == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    /* Deterministic hash rather than a PRNG: the same buffer every run keeps
     * emulator captures and golden tests reproducible. */
    for (uint32_t i = 0; i < count; ++i) {
        const int32_t wave =
            static_cast<int32_t>(((i * 73u) ^ (i >> 2u)) & 63u) - 32;
        out[i] = static_cast<int8_t>(wave * decay(i, count) / 32);
    }
    return SAT_OK;
}

/* One note of a triangle-wave arpeggio over a steady bass, with a short
 * attack and release so consecutive notes do not click. `period` is the
 * triangle's wavelength in samples; smaller is higher pitched.
 * `bass_phase` continues the bass across notes; pass the sample index the
 * note starts at. */
inline sat_result_t arpeggio_note(
    int8_t* out, uint32_t count, uint8_t period, uint32_t bass_phase) {
    if (out == nullptr || count == 0u || period == 0u || count < 322u) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint32_t edge = 160u;
    for (uint32_t i = 0; i < count; ++i) {
        const int32_t phase =
            static_cast<int32_t>((i % period) * 128u / period);
        const int32_t wave = (phase < 64 ? phase : 128 - phase) - 32;
        const int32_t envelope = static_cast<int32_t>(
            i < edge ? i
                     : (i > (count - 1u) - edge ? (count - 1u) - i : edge));
        int32_t bass = static_cast<int32_t>(((bass_phase + i) % 128u) / 2u);
        bass = (bass < 32 ? bass : 64 - bass) - 16;
        out[i] = static_cast<int8_t>((wave * envelope) / 320 + bass / 4);
    }
    return SAT_OK;
}

}  // namespace saturn::core::audio_synth

#endif /* SATURN_CORE_AUDIO_SYNTH_LOGIC_HPP */
