#include <cassert>
#include <cstdio>

#include "src/hal/scsp/dsp_logic.hpp"
#include "src/hal/scsp/scsp_dsp_presets.h"

using namespace saturn::hal::scsp::dsp_logic;

int main() {
    // gains: 16.16 to a 13-bit two's complement coefficient, 12 fractional bits
    assert(coefficient_from_fx16(0x8000) == 0x0800u);            // 0.5
    assert(coefficient_from_fx16(0) == 0u);
    assert(coefficient_from_fx16(0x10000) == 0x0FFFu);           // 1.0 saturates just below
    assert(coefficient_from_fx16(0x7FFFFFFF) == 0x0FFFu);
    assert(coefficient_from_fx16(-0x8000) == 0x1800u);           // -0.5
    assert(coefficient_from_fx16(-0x20000) == 0x1000u);          // clamps at -1.0
    assert(coefficient_register(0x0800u) == 0x4000u);            // bits 15-3
    assert(coefficient_register(0x1800u) == 0xC000u);

    // times
    assert(samples_from_ms(0u) == 0u && samples_from_ms(1000u) == 44100u);
    assert(samples_from_ms(50u) == 2205u);

    // ring buffer: the smallest ring longer than the delay
    assert(ring_words(0u) == 8192u && ring_words(3u) == 65536u);
    assert(ring_bytes(1u) == 32768u);
    assert(ring_length_for_delay(1u) == 0u && ring_length_for_delay(8191u) == 0u);
    assert(ring_length_for_delay(8192u) == 1u && ring_length_for_delay(44100u) == 3u);
    assert(ring_length_for_delay(65535u) == 3u && ring_length_for_delay(65536u) == kRingLengthCodes);

    // placement: 8 KiB aligned, inside Sound RAM
    assert(ring_placement_ok(0xC000u, 0u) && ring_placement_ok(0xC000u, 3u));
    assert(!ring_placement_ok(0xC001u, 0u) && !ring_placement_ok(0xD000u, 0u));
    assert(ring_placement_ok(0x7C000u, 0u) && !ring_placement_ok(0x7E000u, 0u) && !ring_placement_ok(0x7C000u, 1u));
    assert(!ring_placement_ok(0x0u, 4u));
    assert(ring_register(0xC000u, 0u) == 6u && ring_register(0xC000u, 3u) == (6u | (3u << 7u)));
    assert(ring_register(0x7E000u, 0u) == 0x3Fu);

    // slot registers
    assert(send_word(7u, 0u) == 7u && send_word(3u, 2u) == (3u | (2u << 3u)));
    assert(send_word(200u, 0u) == 7u);                            // clamped
    assert(return_bits(7u, 0u) == 0xE0u && return_bits(2u, 0x11u) == (0x40u | 0x11u));
    assert(return_bits(9u, 0u) == 0xE0u);

    // the generated presets fit the hardware
    assert(kScspDspEchoSteps <= kProgramSteps && kScspDspReverbSteps <= kProgramSteps);
    assert(kScspDspCoefZero < kCoefCount && kScspDspCoefWet < kScspDspCoefZero);
    assert(kScspDspReverbCombs * 2u <= kMadrsCount);
    for (unsigned i = 0u; i < kScspDspReverbCombs; ++i) {
        // every comb keeps clear of the next one's write position, and the ring holds all of them
        assert(kScspDspReverbDelays[i] < kScspDspReverbSpacing);
    }
    assert(kScspDspReverbSpacing * kScspDspReverbCombs <= ring_words(kScspDspReverbRingLength));

    std::puts("PASS: test_scsp_dsp_logic.cpp");
    return 0;
}
