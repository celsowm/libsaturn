#ifndef SATURN_HAL_VDP2_RBG0_LOGIC_HPP
#define SATURN_HAL_VDP2_RBG0_LOGIC_HPP

#include <stdint.h>

namespace saturn::hal::vdp2::rbg0 {

constexpr uint32_t kVramBankWords = 0x10000u;
constexpr uint16_t kBitmapCyclePattern = 0x9E9Eu;
constexpr uint16_t kParamCyclePattern = 0x8E8Eu;

/* A0, A1, B0 and B1, in that order. */
struct CyclePlan {
    uint16_t low[4];
    uint16_t high[4];
    bool changed[4];
};

inline uint16_t bank_field(uint16_t ramctl, uint8_t bank) {
    const uint16_t shift = static_cast<uint16_t>((bank & 3u) * 2u);
    return static_cast<uint16_t>((ramctl >> shift) & 3u);
}

inline uint16_t replace_bank_field(uint16_t ramctl, uint8_t bank, uint16_t value) {
    const uint16_t shift = static_cast<uint16_t>((bank & 3u) * 2u);
    return static_cast<uint16_t>(
        (ramctl & static_cast<uint16_t>(~(3u << shift))) |
        static_cast<uint16_t>((value & 3u) << shift));
}

inline uint8_t bank_of(uint32_t word_offset) {
    return static_cast<uint8_t>((word_offset >> 16u) & 3u);
}

inline uint8_t bank_count(uint32_t word_count) {
    return static_cast<uint8_t>((word_count + kVramBankWords - 1u) /
                                kVramBankWords);
}

inline uint8_t bitmap_bank_mask(uint32_t bitmap_base_word, uint32_t bitmap_words) {
    const uint8_t first = bank_of(bitmap_base_word);
    const uint8_t count = bank_count(bitmap_words);
    uint8_t mask = 0u;
    for (uint8_t i = 0u; i < count && static_cast<uint8_t>(first + i) < 4u; ++i) {
        mask = static_cast<uint8_t>(mask | static_cast<uint8_t>(1u << (first + i)));
    }
    return mask;
}

inline uint16_t clear_owned_fields(uint16_t ramctl, uint8_t bitmap_mask,
                                   uint8_t parameter_bank) {
    for (uint8_t bank = 0u; bank < 4u; ++bank) {
        if ((bitmap_mask & static_cast<uint8_t>(1u << bank)) != 0u) {
            ramctl = replace_bank_field(ramctl, bank, 0u);
        }
    }
    if (parameter_bank < 4u) {
        ramctl = replace_bank_field(ramctl, parameter_bank, 0u);
    }
    return ramctl;
}

/* RDBS fields are replaced exactly. The 0x1100 baseline is the established
 * RBG0/NBG0 layout and is intentionally kept separate from the four 2-bit
 * bank fields. `previous_*` identifies fields owned by the preceding RBG0
 * setup, so a reconfiguration removes its old classifications while fields
 * owned by other layers remain untouched. If the coefficient bank overlaps a
 * bitmap bank, the parameter classification is applied last, matching the
 * hardware setup order. */
inline uint16_t compose_ramctl(uint16_t previous, bool preserve_previous,
                               uint8_t previous_bitmap_mask,
                               uint8_t previous_parameter_bank,
                               uint32_t bitmap_base_word, uint32_t bitmap_words,
                               uint32_t rot_param_base_word) {
    uint16_t ramctl = preserve_previous
        ? static_cast<uint16_t>(previous | 0x1100u)
        : 0x1100u;
    if (preserve_previous) {
        ramctl = clear_owned_fields(ramctl, previous_bitmap_mask,
                                    previous_parameter_bank);
    }
    const uint8_t bitmap_mask = bitmap_bank_mask(bitmap_base_word, bitmap_words);
    for (uint8_t bank = 0u; bank < 4u; ++bank) {
        if ((bitmap_mask & static_cast<uint8_t>(1u << bank)) != 0u) {
            ramctl = replace_bank_field(ramctl, bank, 3u);
        }
    }
    return replace_bank_field(ramctl, bank_of(rot_param_base_word), 1u);
}

inline CyclePlan make_cycle_plan(const uint16_t previous_low[4],
                                 const uint16_t previous_high[4],
                                 uint32_t bitmap_base_word, uint32_t bitmap_words,
                                 uint32_t rot_param_base_word) {
    CyclePlan plan = {{previous_low[0], previous_low[1], previous_low[2], previous_low[3]},
                      {previous_high[0], previous_high[1], previous_high[2], previous_high[3]},
                      {false, false, false, false}};
    const uint8_t bitmap_mask = bitmap_bank_mask(bitmap_base_word, bitmap_words);
    for (uint8_t bank = 0u; bank < 4u; ++bank) {
        if ((bitmap_mask & static_cast<uint8_t>(1u << bank)) != 0u) {
            plan.low[bank] = kBitmapCyclePattern;
            plan.high[bank] = kBitmapCyclePattern;
            plan.changed[bank] = true;
        }
    }
    const uint8_t parameter_bank = bank_of(rot_param_base_word);
    plan.low[parameter_bank] = kParamCyclePattern;
    plan.high[parameter_bank] = kParamCyclePattern;
    plan.changed[parameter_bank] = true;
    return plan;
}

}  // namespace saturn::hal::vdp2::rbg0

#endif /* SATURN_HAL_VDP2_RBG0_LOGIC_HPP */
