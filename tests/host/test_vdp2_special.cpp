#include <cassert>
#include <cstdint>
#include <cstdio>

#include "saturn/vdp2_special.h"
#include "src/hal/vdp2/special_logic.hpp"

using namespace saturn::hal::vdp2::special;

static void pattern_name_two_word_layout() {
    /* Auxiliary word first: V H PR CC, five unused bits, then palette 6-0.
     * Character number is the second word, 15 bits. */
    assert(sat_vdp2_pattern_name_2word(0x1234u, 0x55u, 1u, 0u, 1u, 1u) == 0x70551234u);
    assert(sat_vdp2_pattern_name_2word(0x1234u, 0x55u, 0u, 1u, 0u, 0u) == 0x80551234u);
    assert(sat_vdp2_pattern_name_2word(0xFFFFu, 0xFFu, 0u, 0u, 0u, 0u) == 0x007F7FFFu);
    assert(sat_vdp2_pattern_name_2word(0x0000u, 0x00u, 0u, 0u, 0u, 0u) == 0u);
    /* Palette is 7 bits and the character number 15 bits; both saturate. */
    assert((sat_vdp2_pattern_name_2word(0x0001u, 0x80u, 0u, 0u, 0u, 0u) & 0x00FF0000u) == 0u);
}

static void priority_modes_follow_the_name_and_format() {
    const LayerTraits cell2{true, false, false};
    const LayerTraits cell1{false, false, false};
    const LayerTraits bitmap2{true, true, false};
    const LayerTraits rgb2{true, false, true};

    assert(priority_mode_supported(0u, cell2) && priority_mode_supported(1u, cell2) &&
           priority_mode_supported(2u, cell2));
    assert(!priority_mode_supported(3u, cell2));
    assert(priority_mode_supported(0u, cell1) && !priority_mode_supported(1u, cell1) &&
           !priority_mode_supported(2u, cell1));
    assert(priority_mode_supported(0u, bitmap2) && !priority_mode_supported(1u, bitmap2));
    assert(priority_mode_supported(0u, rgb2) && priority_mode_supported(1u, rgb2) &&
           !priority_mode_supported(2u, rgb2));
}

static void color_modes_follow_the_name_and_format() {
    const LayerTraits cell2{true, false, false};
    const LayerTraits cell1{false, false, false};
    const LayerTraits bitmap2{true, true, false};
    const LayerTraits rgb2{true, false, true};

    for (uint8_t mode = 0u; mode < 4u; ++mode) assert(color_mode_supported(mode, cell2));
    assert(!color_mode_supported(4u, cell2));
    assert(color_mode_supported(0u, cell1) && !color_mode_supported(1u, cell1) &&
           !color_mode_supported(2u, cell1) && color_mode_supported(3u, cell1));
    assert(color_mode_supported(0u, bitmap2) && !color_mode_supported(1u, bitmap2) &&
           !color_mode_supported(2u, bitmap2) && color_mode_supported(3u, bitmap2));
    assert(color_mode_supported(0u, rgb2) && color_mode_supported(1u, rgb2) &&
           !color_mode_supported(2u, rgb2) && color_mode_supported(3u, rgb2));
}

static void register_words_pack_by_layer() {
    const uint8_t priority[kLayerCount] = {1u, 2u, 0u, 0u};
    assert(compose_modes(priority) == 0x0009u);
    const uint8_t all[kLayerCount] = {3u, 3u, 3u, 3u};
    assert(compose_modes(all) == 0x00FFu);
    assert(compose_mode_bits(2u, 3u) == 0x0030u);

    const uint8_t selects[kLayerCount] = {1u, 0u, 1u, 0u};
    assert(compose_function_select(selects) == 0x0005u);
    assert(compose_function_code(0x08u, 0x80u) == 0x8008u);
}

static void function_code_bit_covers_two_dot_codes() {
    /* SFCODE bit n covers dot colour codes 2n and 2n+1 (p10_30). */
    for (uint8_t code = 0u; code < 16u; ++code) {
        const uint8_t bit = static_cast<uint8_t>(1u << (code >> 1));
        assert((code >> 1) < 8u);
        assert((bit & 0x01u) == ((code == 0u || code == 1u) ? 1u : 0u));
        assert((bit & 0x08u) == ((code == 6u || code == 7u) ? 8u : 0u));
    }
}

int main() {
    pattern_name_two_word_layout();
    priority_modes_follow_the_name_and_format();
    color_modes_follow_the_name_and_format();
    register_words_pack_by_layer();
    function_code_bit_covers_two_dot_codes();
    std::puts("PASS: test_vdp2_special.cpp");
    return 0;
}
