#include <cstdio>
#include <cstdlib>

#include "src/graphics/vdp2/color_offset_logic.hpp"

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    std::fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
    std::exit(1); } } while (0)

int main() {
    using namespace saturn::core::vdp2_color_offset;

    /* 9-bit two's complement (VDP2 manual figure 13.1). */
    ASSERT_EQ(encode_channel(0), 0x0000u);
    ASSERT_EQ(encode_channel(255), 0x00FFu);
    ASSERT_EQ(encode_channel(-1), 0x01FFu);
    ASSERT_EQ(encode_channel(-64), 0x01C0u);
    ASSERT_EQ(encode_channel(-256), 0x0100u);

    sat_vdp2_color_offset_t offset = {-256, 0, 255};
    ASSERT_EQ(validate_offset(&offset), SAT_OK);
    offset.r = -257;
    ASSERT_EQ(validate_offset(&offset), SAT_ERR_INVALID_ARG);
    offset.r = 0;
    offset.b = 256;
    ASSERT_EQ(validate_offset(&offset), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(validate_offset(nullptr), SAT_ERR_INVALID_ARG);

    /* SUBTRACT: the output clamps at 0 and 255. */
    ASSERT_EQ(apply_channel(200u, -64), 136u);
    ASSERT_EQ(apply_channel(40u, -64), 0u);
    ASSERT_EQ(apply_channel(250u, 64), 255u);

    /* Tint white to (255, 128, 64): darkening offsets only. */
    const sat_vdp2_color_offset_t tint = sat_vdp2_color_offset_tint(255u, 128u, 64u);
    ASSERT_EQ(tint.r, 0);
    ASSERT_EQ(tint.g, -127);
    ASSERT_EQ(tint.b, -191);
    ASSERT_EQ(apply_channel(255u, tint.g), 128u);

    /* CLOFEN / CLOFSL composition: bank per layer, other layers untouched. */
    Layers layers = {0u, 0u};
    ASSERT_EQ(enable_layers(layers, SAT_VDP2_LAYER_NBG0 | SAT_VDP2_LAYER_BACK,
                            SAT_VDP2_COLOR_OFFSET_B), SAT_OK);
    ASSERT_EQ(layers.clofen, 0x0021u);
    ASSERT_EQ(layers.clofsl, 0x0021u);
    ASSERT_EQ(enable_layers(layers, SAT_VDP2_LAYER_SPRITE, SAT_VDP2_COLOR_OFFSET_A), SAT_OK);
    ASSERT_EQ(layers.clofen, 0x0061u);
    ASSERT_EQ(layers.clofsl, 0x0021u);
    ASSERT_EQ(enable_layers(layers, SAT_VDP2_LAYER_NBG0, SAT_VDP2_COLOR_OFFSET_A), SAT_OK);
    ASSERT_EQ(layers.clofsl, 0x0020u);
    ASSERT_EQ(disable_layers(layers, SAT_VDP2_LAYER_BACK), SAT_OK);
    ASSERT_EQ(layers.clofen, 0x0041u);
    ASSERT_EQ(enable_layers(layers, 0x80u, SAT_VDP2_COLOR_OFFSET_A), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(enable_layers(layers, 0u, SAT_VDP2_COLOR_OFFSET_A), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(enable_layers(layers, SAT_VDP2_LAYER_NBG1, 2u), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(disable_layers(layers, 0x80u), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(layers.clofen, 0x0041u);

    std::puts("test_vdp2_color_offset_logic: OK");
    return 0;
}
