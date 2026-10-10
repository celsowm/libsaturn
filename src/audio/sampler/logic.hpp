#ifndef SATURN_AUDIO_SAMPLER_LOGIC_HPP
#define SATURN_AUDIO_SAMPLER_LOGIC_HPP

/* Pure sampler conversions and voice bookkeeping, no hardware access:
 * tests/host/test_sampler_logic.cpp links this directly. */

#include <stdint.h>

namespace saturn::core::audio::sampler {

/* TL (0.375 dB a step) of a linear amplitude: round(-20 log10(a / 255) / 0.375), 0 -> 255. */
inline constexpr uint8_t kTotalLevel[256] = {
    255, 128, 112, 103,  96,  91,  87,  83,  80,  77,  75,  73,  71,  69,  67,  66,
     64,  63,  61,  60,  59,  58,  57,  56,  55,  54,  53,  52,  51,  50,  50,  49,
     48,  47,  47,  46,  45,  45,  44,  43,  43,  42,  42,  41,  41,  40,  40,  39,
     39,  38,  38,  37,  37,  36,  36,  36,  35,  35,  34,  34,  34,  33,  33,  32,
     32,  32,  31,  31,  31,  30,  30,  30,  29,  29,  29,  28,  28,  28,  27,  27,
     27,  27,  26,  26,  26,  25,  25,  25,  25,  24,  24,  24,  24,  23,  23,  23,
     23,  22,  22,  22,  22,  21,  21,  21,  21,  21,  20,  20,  20,  20,  19,  19,
     19,  19,  19,  18,  18,  18,  18,  18,  17,  17,  17,  17,  17,  17,  16,  16,
     16,  16,  16,  15,  15,  15,  15,  15,  15,  14,  14,  14,  14,  14,  14,  13,
     13,  13,  13,  13,  13,  12,  12,  12,  12,  12,  12,  12,  11,  11,  11,  11,
     11,  11,  11,  10,  10,  10,  10,  10,  10,  10,   9,   9,   9,   9,   9,   9,
      9,   8,   8,   8,   8,   8,   8,   8,   8,   7,   7,   7,   7,   7,   7,   7,
      7,   6,   6,   6,   6,   6,   6,   6,   6,   6,   5,   5,   5,   5,   5,   5,
      5,   5,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   3,   3,   3,   3,
      3,   3,   3,   3,   3,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   0,   0,   0,   0,   0,   0,
};

constexpr uint32_t kNativeRate = 44100u;
constexpr uint16_t kLowestPitch = 0x4000u; /* OCT -8, FNS 0 */
constexpr uint16_t kNoWord = 0xFFFFu;      /* a pitch or level never written */

/* OCT/FNS for a playback rate in Hz, FNS truncated. 0 Hz plays as slowly as the slot can. */
inline uint16_t pitch_word(uint32_t hz) {
    if (hz == 0u) return kLowestPitch;
    int32_t oct = 0;
    while (hz >= 2u * kNativeRate && oct < 7) {
        hz >>= 1;
        ++oct;
    }
    while (hz < kNativeRate && oct > -8) {
        hz <<= 1;
        --oct;
    }
    if (hz >= 2u * kNativeRate) return static_cast<uint16_t>(7u << 11 | 0x3FFu); /* fastest */
    uint32_t fns = hz < kNativeRate ? 0u : ((hz - kNativeRate) << 10) / kNativeRate;
    if (fns > 0x3FFu) fns = 0x3FFu;
    return static_cast<uint16_t>(((static_cast<uint32_t>(oct) & 0xFu) << 11) | fns);
}

/* TL of the louder side in the low byte, DIPAN (bits 12..8) attenuating the quieter side in 3 dB
 * steps (8 TL steps each): bit 4 set attenuates the right side. A silent side gets the full
 * 15 steps (-45 dB). */
inline uint16_t level_word(uint8_t left, uint8_t right, uint8_t attenuation) {
    const uint8_t hi = left > right ? left : right;
    const uint8_t lo = left > right ? right : left;
    uint32_t tl = static_cast<uint32_t>(kTotalLevel[hi]) + attenuation;
    if (tl > 255u) tl = 255u;
    if (hi == 0u) return 0x00FFu;
    uint32_t steps = lo == 0u ? 15u : (static_cast<uint32_t>(kTotalLevel[lo]) - kTotalLevel[hi]) / 8u;
    if (steps > 15u) steps = 15u;
    const uint32_t side = left > right ? 0x10u : 0u;
    return static_cast<uint16_t>(tl | ((side | steps) << 8));
}

/* One voice: two slots used alternately. */
struct Voice {
    uint8_t slot;        /* slot of the current (or last) note: base or base + 1 */
    uint8_t keyed;       /* the current note is keyed on, or staged to be */
    uint8_t fresh;       /* its key-on is staged and not committed: the slot has not sounded */
    uint8_t reserved;
    uint16_t pitch;      /* last word written, kNoWord: none */
    uint16_t level;
    uint32_t at[2];      /* sample offset each slot last played, for mute_range */
};

/* Which slot a new note goes on. A note started twice in one frame reuses the slot staged
 * the first time (it has not sounded); otherwise the other slot. */
inline uint8_t next_slot(const Voice& v, uint8_t base) {
    if (v.keyed != 0u && v.fresh != 0u) return v.slot;
    return static_cast<uint8_t>(v.slot == base ? base + 1u : base);
}

inline bool reads_range(uint32_t at, uint32_t begin, uint32_t end) {
    return at >= begin && at < end;
}

}  // namespace saturn::core::audio::sampler

#endif
