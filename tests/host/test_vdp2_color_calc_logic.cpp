#include <cstdio>
#include <cstdlib>

#include "src/graphics/vdp2/color_calc_logic.hpp"

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    std::fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
    std::exit(1); } } while (0)

int main() {
    sat_vdp2_sprite_color_calc_config_t cfg = {};
    cfg.enabled = 1u;
    cfg.normal_priority = 6u;
    for (uint8_t i = 0u; i < 8u; ++i) {
        cfg.ratio[i] = static_cast<uint8_t>(i * 4u);
    }

    ASSERT_EQ(saturn::core::vdp2_color_calc::validate_config(&cfg), SAT_OK);
    ASSERT_EQ(saturn::core::vdp2_color_calc::compose_spctl(5u), 0x1500u);
    ASSERT_EQ(saturn::core::vdp2_color_calc::compose_prisa(6u), 0x0506u);
    ASSERT_EQ(saturn::core::vdp2_color_calc::compose_prisa_disabled(6u), 0x0606u);
    ASSERT_EQ(saturn::core::vdp2_color_calc::compose_ratio_pair(0u, 31u), 0x1F00u);
    ASSERT_EQ(saturn::core::vdp2_color_calc::kCcctlSpriteEnable, 0x0040u);

    cfg.normal_priority = 1u;
    ASSERT_EQ(saturn::core::vdp2_color_calc::validate_config(&cfg), SAT_ERR_INVALID_ARG);
    cfg.normal_priority = 6u;
    cfg.ratio[7] = 32u;
    ASSERT_EQ(saturn::core::vdp2_color_calc::validate_config(&cfg), SAT_ERR_INVALID_ARG);

    const uint8_t ratios[8] = {0u, 4u, 8u, 12u, 16u, 20u, 24u, 31u};
    uint8_t slot = 0xFFu;
    uint8_t actual = 0u;
    using saturn::core::vdp2_color_calc::choose_alpha_slot;
    using saturn::core::vdp2_color_calc::ratio_to_alpha;
    ASSERT_EQ(choose_alpha_slot(128u, ratios, false, &slot, &actual), SAT_OK);
    ASSERT_EQ(slot, 4u); // background ratio 16 => approximately 50% alpha
    ASSERT_EQ(actual, 120u); // ratio 16 keeps 15/32 of the sprite
    ASSERT_EQ(choose_alpha_slot(1u, ratios, false, &slot, nullptr), SAT_OK);
    ASSERT_EQ(slot, 7u);
    ASSERT_EQ(choose_alpha_slot(0u, ratios, false, &slot, nullptr), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(choose_alpha_slot(255u, ratios, false, &slot, nullptr), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(choose_alpha_slot(128u, ratios, false, nullptr, nullptr), SAT_ERR_INVALID_ARG);

    /* Ratio -> alpha: the ends of the hardware table (31:1 and 0:32). */
    ASSERT_EQ(ratio_to_alpha(0u), 248u);
    ASSERT_EQ(ratio_to_alpha(15u), 128u);
    ASSERT_EQ(ratio_to_alpha(31u), 0u);

    /* A table with no middle slot: the default snaps to the nearest ratio and
     * reports what the hardware will really show; strict refuses instead. */
    const uint8_t no_middle[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 31u};
    ASSERT_EQ(choose_alpha_slot(128u, no_middle, true, &slot, &actual), SAT_ERR_UNSUPPORTED);
    slot = 0xFFu;
    actual = 0u;
    ASSERT_EQ(choose_alpha_slot(128u, no_middle, false, &slot, &actual), SAT_OK);
    ASSERT_EQ(slot, 0u);
    ASSERT_EQ(actual, 248u);
    ASSERT_EQ(choose_alpha_slot(60u, no_middle, false, &slot, &actual), SAT_OK);
    ASSERT_EQ(slot, 7u);
    ASSERT_EQ(actual, 0u);
    /* Strict still accepts a slot within two units of the target. */
    ASSERT_EQ(choose_alpha_slot(136u, ratios, true, &slot, &actual), SAT_OK);

    std::puts("test_vdp2_color_calc_logic: OK");
    return 0;
}
