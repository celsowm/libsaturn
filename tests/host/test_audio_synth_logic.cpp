#include <cassert>
#include <cstdio>
#include <stdint.h>

#include "src/audio/synthesis/logic.hpp"

using namespace saturn::core::audio_synth;

/* The shapes these replaced, transcribed from examples/skybridge_3d/main.c
 * before the extraction. Byte-for-byte equality is the whole point: moving
 * generation into the library must not change what the SCSP plays. */
static int8_t old_blip(uint32_t i, uint32_t count, uint8_t step) {
    int32_t wave = (int32_t)((i * step) & 63u) - 32;
    int32_t env = (int32_t)((count - i) * 55u / count);
    return (int8_t)(wave * env / 32);
}
static int8_t old_noise(uint32_t i, uint32_t count) {
    int32_t wave = (int32_t)(((i * 73u) ^ (i >> 2u)) & 63u) - 32;
    int32_t env = (int32_t)((count - i) * 55u / count);
    return (int8_t)(wave * env / 32);
}
static int8_t old_music(uint32_t i) {
    uint32_t note = i / 4096u, local = i % 4096u;
    static const uint8_t periods[8] = {50u,45u,40u,38u,34u,38u,40u,45u};
    uint32_t period = periods[note];
    int32_t phase = (int32_t)((local % period) * 128u / period);
    int32_t wave = (phase < 64 ? phase : 128 - phase) - 32;
    int32_t envelope = (int32_t)(local < 160u ? local
                        : (local > 3935u ? 4095u - local : 160u));
    int32_t bass = (int32_t)((i % 128u) / 2u);
    bass = (bass < 32 ? bass : 64 - bass) - 16;
    return (int8_t)((wave * envelope) / 320 + bass / 4);
}

int main() {
    static int8_t buf[32768];

    const uint8_t steps[5] = {3u,5u,7u,2u,9u};
    for (uint8_t s = 0; s < 5u; ++s) {
        assert(blip(buf, 2048u, steps[s]) == SAT_OK);
        for (uint32_t i = 0; i < 2048u; ++i) assert(buf[i] == old_blip(i, 2048u, steps[s]));
    }
    assert(noise(buf, 2048u) == SAT_OK);
    for (uint32_t i = 0; i < 2048u; ++i) assert(buf[i] == old_noise(i, 2048u));

    /* Per-note calls must reproduce the single global loop exactly, which is
     * what lets the caller show boot progress between notes. */
    static const uint8_t periods[8] = {50u,45u,40u,38u,34u,38u,40u,45u};
    for (uint32_t n = 0; n < 8u; ++n)
        assert(arpeggio_note(&buf[n * 4096u], 4096u, periods[n], n * 4096u) == SAT_OK);
    for (uint32_t i = 0; i < 32768u; ++i) assert(buf[i] == old_music(i));

    /* Both ends of every note reach silence, so notes cannot click. */
    for (uint32_t n = 0; n < 8u; ++n) {
        const int8_t first = buf[n * 4096u];
        const int8_t last = buf[n * 4096u + 4095u];
        assert(first >= -4 && first <= 4);
        assert(last >= -4 && last <= 4);
    }

    assert(blip(nullptr, 16u, 3u) == SAT_ERR_INVALID_ARG);
    assert(blip(buf, 0u, 3u) == SAT_ERR_INVALID_ARG);
    assert(blip(buf, 16u, 0u) == SAT_ERR_INVALID_ARG);
    assert(noise(nullptr, 16u) == SAT_ERR_INVALID_ARG);
    assert(arpeggio_note(buf, 321u, 40u, 0u) == SAT_ERR_INVALID_ARG);
    assert(arpeggio_note(buf, 4096u, 0u, 0u) == SAT_ERR_INVALID_ARG);

    std::puts("audio synth logic: OK");
    return 0;
}
