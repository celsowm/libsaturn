#include <cstdio>
#include <cstdlib>

#include "src/core/vdp1_color_calc_logic.hpp"

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    std::fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
    std::exit(1); } } while (0)

int main() {
    uint16_t selector = 0u;

    ASSERT_EQ(
        saturn::core::vdp1_color_calc::encode_palette_selector(1u, 0u, &selector),
        SAT_OK);
    ASSERT_EQ(selector, 0x0041u);

    ASSERT_EQ(
        saturn::core::vdp1_color_calc::encode_palette_selector(7u, 7u, &selector),
        SAT_OK);
    ASSERT_EQ(selector, 0x007Fu);

    ASSERT_EQ(
        saturn::core::vdp1_color_calc::encode_palette_selector(8u, 0u, &selector),
        SAT_ERR_INVALID_ARG);
    ASSERT_EQ(
        saturn::core::vdp1_color_calc::encode_palette_selector(0u, 8u, &selector),
        SAT_ERR_INVALID_ARG);
    ASSERT_EQ(
        saturn::core::vdp1_color_calc::encode_palette_selector(0u, 0u, nullptr),
        SAT_ERR_INVALID_ARG);

    std::puts("test_vdp1_color_calc_logic: OK");
    return 0;
}
