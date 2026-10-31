#include <cassert>
#include <cstdint>
#include <cstdio>

#include "saturn/vdp2_sprite_shadow.h"
#include "src/hal/vdp2/sprite_shadow_logic.hpp"

using namespace saturn::hal::vdp2::sprite_shadow;

static void normal_shadow_word_matches_type2() {
    /* Type 2: bit 15 SD, bit 14 priority selector, bits 13-11 CC, 11-bit
     * colour data. Normal shadow data is 0x7FE, so selector 1 is 0x47FE. */
    assert(sat_vdp2_shadow_normal_word(1u, 0u) == 0x47FEu);
    assert(normal_word(1u, 0u) == sat_vdp2_shadow_normal_word(1u, 0u));
    assert(sat_vdp2_shadow_normal_word(0u, 0u) == 0x07FEu);
    assert(sat_vdp2_shadow_normal_word(1u, 7u) == 0x7FFEu);
    assert(sat_vdp2_shadow_normal_word(0u, 7u) == 0x3FFEu);
    /* Values are masked to the hardware fields. */
    assert(sat_vdp2_shadow_normal_word(2u, 0u) == 0x07FEu);
    assert(sat_vdp2_shadow_normal_word(0u, 8u) == 0x07FEu);
    for (uint8_t priority = 0u; priority < 2u; ++priority) {
        for (uint8_t cc = 0u; cc < 8u; ++cc) {
            assert(normal_word(priority, cc) ==
                   sat_vdp2_shadow_normal_word(priority, cc));
        }
    }
}

static void transparent_and_msb_words() {
    /* 0xC000 would be a normal (black) sprite: bit 14 is part of the 15 bits
     * that must be zero for a transparent shadow. */
    assert(sat_vdp2_shadow_transparent_msb_word() == 0x8000u);
    assert(transparent_msb_word() == 0x8000u);

    /* MSB-on preserves the 15 bits already in the frame buffer. */
    assert(sat_vdp2_shadow_msb_word(0x0001u) == 0x8001u);
    assert(sat_vdp2_shadow_msb_word(0x4000u) == 0xC000u);
    assert(sat_vdp2_shadow_msb_word(0x1234u) == 0x9234u);
    assert(sat_vdp2_shadow_msb_word(0xFFFFu) == 0xFFFFu);
    assert(msb_word(0x1234u) == sat_vdp2_shadow_msb_word(0x1234u));
}

static void sdctl_and_prisa_words() {
    assert(sdctl_word(0u, false) == 0x0000u);
    assert(sdctl_word(0x01u, false) == 0x0001u);
    assert(sdctl_word(0x3Fu, false) == 0x003Fu);
    assert(sdctl_word(0x01u, true) == 0x0101u);
    assert(sdctl_word(0x3Fu, true) == 0x013Fu);
    assert((sdctl_word(0x40u, false) & ~kTargetMask) == 0u);
    assert(prisa_word(4u, 7u) == 0x0704u);
    assert(prisa_word(0u, 0u) == 0x0000u);
}

static void config_validation() {
    using enum ConfigStatus;
    assert(validate_config(0u, 0u, 0u, 0u, 7u, false, false) == Off);
    assert(validate_config(1u, 0x01u, 0u, 4u, 7u, false, false) == Ok);
    assert(validate_config(2u, 0x3Fu, 1u, 0u, 7u, false, false) == Ok);
    /* A zero target mask is legal: the silhouette is drawn but darkens
     * nothing, which is the target-restriction A/B control. */
    assert(validate_config(1u, 0u, 0u, 4u, 7u, false, false) == Ok);

    assert(validate_config(3u, 0u, 0u, 4u, 7u, false, false) == InvalidArg);
    assert(validate_config(1u, 0x40u, 0u, 4u, 7u, false, false) == InvalidArg);
    assert(validate_config(1u, 0u, 2u, 4u, 7u, false, false) == InvalidArg);
    assert(validate_config(1u, 0u, 0u, 8u, 7u, false, false) == InvalidArg);
    assert(validate_config(1u, 0u, 0u, 4u, 8u, false, false) == InvalidArg);
    assert(validate_config(1u, 0u, 0u, 4u, 7u, true, false) == Unsupported);
    assert(validate_config(2u, 0u, 1u, 4u, 7u, false, true) == Unsupported);
    /* Turning off stays legal even if the mode would not support shadows. */
    assert(validate_config(0u, 0u, 0u, 0u, 7u, true, true) == Off);
    assert(kType2Spctl == 0x0002u);
}

int main() {
    normal_shadow_word_matches_type2();
    transparent_and_msb_words();
    sdctl_and_prisa_words();
    config_validation();
    std::puts("PASS: test_vdp2_sprite_shadow.cpp");
    return 0;
}
