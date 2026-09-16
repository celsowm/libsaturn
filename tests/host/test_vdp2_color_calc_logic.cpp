#include <cstdio>
#include <cstdlib>

#include "src/core/vdp2_color_calc_logic.hpp"

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

    std::puts("test_vdp2_color_calc_logic: OK");
    return 0;
}
