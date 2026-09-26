#ifndef SATURN_HAL_SCSP_DSP_LOGIC_HPP
#define SATURN_HAL_SCSP_DSP_LOGIC_HPP

#include <stdint.h>

/* Pure policy of the SCSP DSP loader: how gains, delays and the ring buffer map
 * onto the DSP registers. The program data itself comes from tools/scsp_dsp.py
 * (scsp_dsp_presets.h). */
namespace saturn::hal::scsp::dsp_logic {

constexpr uint32_t kProgramSteps = 128u;
constexpr uint32_t kProgramWords = kProgramSteps * 4u;
constexpr uint32_t kCoefCount = 64u;
constexpr uint32_t kMadrsCount = 32u;
constexpr uint32_t kSampleRate = 44100u;

constexpr uint8_t kMaxLevel = 7u;
constexpr uint8_t kRingLengthCodes = 4u;           /* 8K, 16K, 32K, 64K words */
constexpr uint32_t kRingUnitBytes = 0x2000u;       /* RBP counts 4K words = 8 KiB */
constexpr uint32_t kSoundRamBytes = 0x80000u;

/* One 16.16 gain to the DSP's 13-bit two's complement coefficient (12 fractional
 * bits). 1.0 does not fit; it becomes the largest coefficient, 4095/4096. */
constexpr uint16_t coefficient_from_fx16(int32_t gain) {
    int32_t value = gain >> 4;
    if (value > 0x0FFF) value = 0x0FFF;
    if (value < -0x1000) value = -0x1000;
    return static_cast<uint16_t>(value & 0x1FFF);
}

/* The coefficient sits in bits 15-3 of its register. */
constexpr uint16_t coefficient_register(uint16_t coefficient13) {
    return static_cast<uint16_t>((coefficient13 & 0x1FFFu) << 3u);
}

constexpr uint32_t samples_from_ms(uint32_t ms) {
    return static_cast<uint32_t>((static_cast<uint64_t>(ms) * kSampleRate + 500u) / 1000u);
}

constexpr uint32_t ring_words(uint8_t code) {
    return 0x2000u << (code & 3u);
}

constexpr uint32_t ring_bytes(uint8_t code) {
    return ring_words(code) * 2u;
}

/* Smallest ring longer than the delay, or kRingLengthCodes when none is. */
constexpr uint8_t ring_length_for_delay(uint32_t delay_samples) {
    for (uint8_t code = 0u; code < kRingLengthCodes; ++code) {
        if (delay_samples < ring_words(code)) return code;
    }
    return kRingLengthCodes;
}

constexpr bool ring_placement_ok(uint32_t byte_offset, uint8_t code) {
    return code < kRingLengthCodes && (byte_offset % kRingUnitBytes) == 0u &&
           byte_offset / kRingUnitBytes < 128u && byte_offset + ring_bytes(code) <= kSoundRamBytes;
}

/* Register 0x402: RBP in bits 6-0, RBL in bits 8-7. */
constexpr uint16_t ring_register(uint32_t byte_offset, uint8_t code) {
    return static_cast<uint16_t>((byte_offset / kRingUnitBytes) | (static_cast<uint32_t>(code & 3u) << 7u));
}

/* Slot register 0x14: IMXL (bits 2-0, 0 = off, 7 = loudest) and ISEL (bits 6-3). */
constexpr uint16_t send_word(uint8_t level, uint8_t mixs_index) {
    return static_cast<uint16_t>((level > kMaxLevel ? kMaxLevel : level) | ((mixs_index & 0x0Fu) << 3u));
}

/* The low byte of slot register 0x16: EFSDL (bits 7-5) and EFPAN (bits 4-0), the
 * return of EFREG n through slot n. */
constexpr uint16_t return_bits(uint8_t level, uint8_t pan5) {
    return static_cast<uint16_t>(((level > kMaxLevel ? kMaxLevel : level) << 5u) | (pan5 & 0x1Fu));
}

}  // namespace saturn::hal::scsp::dsp_logic

#endif  // SATURN_HAL_SCSP_DSP_LOGIC_HPP
