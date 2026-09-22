/* test_vdp1_logic.cpp — host tests for VDP1 logic helpers */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/vdp1.h"
#include "src/core/runtime/logic.hpp"

#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
    exit(1); } } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while(0)

TEST(resolve_sprite_cmd_null_cmd) {
    saturn::core::ResolvedSprite out = {};
    ASSERT_EQ(saturn::core::resolve_sprite_cmd(nullptr, &out), SAT_ERR_INVALID_ARG);
}

TEST(resolve_sprite_cmd_null_texture) {
    sat_sprite_cmd_t cmd = {};
    cmd.texture = nullptr;
    saturn::core::ResolvedSprite out = {};
    ASSERT_EQ(saturn::core::resolve_sprite_cmd(&cmd, &out), SAT_ERR_INVALID_ARG);
}

TEST(resolve_sprite_cmd_invalid_texture) {
    sat_vdp1_texture_t tex = {};
    tex.valid = 0;
    sat_sprite_cmd_t cmd = {};
    cmd.texture = &tex;
    saturn::core::ResolvedSprite out = {};
    ASSERT_EQ(saturn::core::resolve_sprite_cmd(&cmd, &out), SAT_ERR_INVALID_ARG);
}

TEST(resolve_sprite_cmd_defaults) {
    sat_vdp1_texture_t tex = {};
    tex.valid = 1;
    tex.width = 32;
    tex.height = 32;
    tex.srca = 0x8000;
    tex.palette = 0;

    sat_sprite_cmd_t cmd = {};
    cmd.x = 10 * SAT_FX16_ONE;
    cmd.y = 20 * SAT_FX16_ONE;
    cmd.width = 0;  /* default to texture */
    cmd.height = 0; /* default to texture */
    cmd.texture = &tex;
    cmd.palette_override = 0;

    saturn::core::ResolvedSprite out = {};
    ASSERT_EQ(saturn::core::resolve_sprite_cmd(&cmd, &out), SAT_OK);
    ASSERT_EQ(out.x, 10);
    ASSERT_EQ(out.y, 20);
    ASSERT_EQ(out.width, 32);
    ASSERT_EQ(out.height, 32);
    ASSERT_EQ(out.srca, 0x8000);
    ASSERT_EQ(out.palette, 0);
}

TEST(resolve_sprite_cmd_palette_override) {
    sat_vdp1_texture_t tex = {};
    tex.valid = 1;
    tex.width = 16;
    tex.height = 16;
    tex.srca = 0x8000;
    tex.palette = 0;

    sat_sprite_cmd_t cmd = {};
    cmd.x = 0;
    cmd.y = 0;
    cmd.width = 0;
    cmd.height = 0;
    cmd.texture = &tex;
    cmd.palette_override = 3;  /* override */

    saturn::core::ResolvedSprite out = {};
    ASSERT_EQ(saturn::core::resolve_sprite_cmd(&cmd, &out), SAT_OK);
    ASSERT_EQ(out.palette, 3);
}

/* --- Polygon / polyline / line command encoding ------------------- */

TEST(polygon_ctrl_command_selects) {
    using namespace saturn::core;
    ASSERT_EQ(compose_polygon_ctrl(kVdp1CmdPolygon, false), 0x0004);
    ASSERT_EQ(compose_polygon_ctrl(kVdp1CmdPolyline, false), 0x0005);
    ASSERT_EQ(compose_polygon_ctrl(kVdp1CmdLine, false), 0x0006);
    ASSERT_EQ(compose_polygon_ctrl(kVdp1CmdUserClip, false), 0x0008);
}

TEST(polygon_ctrl_end_bit) {
    using namespace saturn::core;
    /* End bit is CMDCTRL bit 15; command select must survive alongside it. */
    ASSERT_EQ(compose_polygon_ctrl(kVdp1CmdPolygon, true), 0x8004);
    ASSERT_EQ(compose_polygon_ctrl(kVdp1CmdPolyline, true), 0x8005);
    ASSERT_EQ(compose_polygon_ctrl(kVdp1CmdLine, true), 0x8006);
    ASSERT_EQ(compose_polygon_ctrl(kVdp1CmdNormalSprite, true), 0x8000);
}

TEST(polygon_ctrl_masks_high_bits) {
    using namespace saturn::core;
    /* Only bits 3..0 are the command select; anything above must be dropped
     * so a caller cannot accidentally set the end bit through the select. */
    ASSERT_EQ(compose_polygon_ctrl(0xFFFFu, false), 0x000F);
}

TEST(polygon_pmod_required_bits) {
    using namespace saturn::core;
    /* ECD (bit7) and SPD (bit6) set; color mode (bits 5..3) and color
     * calculation (bits 2..0) both 000B. */
    const uint16_t pmod = compose_polygon_pmod(0);
    ASSERT_EQ(pmod & 0x003Fu, 0x0000u);  /* color mode + calculation 000B */
    ASSERT_TRUE((pmod & 0x0040u) != 0u); /* SPD = 1 */
    ASSERT_TRUE((pmod & 0x0080u) != 0u); /* ECD = 1 */
    ASSERT_EQ(pmod, kVdp1PolygonPmod);
}

TEST(polygon_pmod_opaque_flag) {
    using namespace saturn::core;
    ASSERT_EQ(compose_polygon_pmod(SAT_SPRITE_FLAG_OPAQUE),
              static_cast<uint16_t>(kVdp1PolygonPmod | 0x0040u));
    /* Unrelated flag bits must not leak into PMOD. */
    ASSERT_EQ(compose_polygon_pmod(0xFFFEu), static_cast<uint16_t>(kVdp1PolygonPmod | 0x0103u));
}

/* --- Scaled / distorted sprite resolution ------------------------ */

TEST(resolve_scaled_sprite_cmd_defaults) {
    sat_vdp1_texture_t tex = {};
    tex.valid = 1;
    tex.width = 32;
    tex.height = 16;
    tex.srca = 0x1234;
    tex.palette = 2;

    sat_scaled_sprite_cmd_t cmd = {};
    cmd.x0 = -40; cmd.y0 = -20; cmd.x1 = 40; cmd.y1 = 40;
    cmd.texture = &tex;

    saturn::core::ResolvedScaledSprite out = {};
    ASSERT_EQ(saturn::core::resolve_scaled_sprite_cmd(&cmd, &out), SAT_OK);
    ASSERT_EQ(out.x0, -40);
    ASSERT_EQ(out.y0, -20);
    ASSERT_EQ(out.x1, 40);
    ASSERT_EQ(out.y1, 40);
    ASSERT_EQ(out.width, 32);
    ASSERT_EQ(out.height, 16);
    ASSERT_EQ(out.srca, 0x1234);
    ASSERT_EQ(out.palette, 2);
}

TEST(resolve_scaled_sprite_cmd_rejects_bad_texture) {
    sat_scaled_sprite_cmd_t cmd = {};
    saturn::core::ResolvedScaledSprite out = {};
    ASSERT_EQ(saturn::core::resolve_scaled_sprite_cmd(&cmd, &out), SAT_ERR_INVALID_ARG);
}

TEST(resolve_scaled_sprite_cmd_palette_override) {
    sat_vdp1_texture_t tex = {};
    tex.valid = 1; tex.width = 8; tex.height = 8; tex.srca = 0; tex.palette = 1;
    sat_scaled_sprite_cmd_t cmd = {};
    cmd.texture = &tex;
    cmd.palette_override = 5;
    saturn::core::ResolvedScaledSprite out = {};
    ASSERT_EQ(saturn::core::resolve_scaled_sprite_cmd(&cmd, &out), SAT_OK);
    ASSERT_EQ(out.palette, 5);
}

TEST(resolve_distorted_sprite_cmd_defaults) {
    sat_vdp1_texture_t tex = {};
    tex.valid = 1; tex.width = 16; tex.height = 16; tex.srca = 0x2000; tex.palette = 3;
    sat_distorted_sprite_cmd_t cmd = {};
    cmd.x[0] = -10; cmd.y[0] = -10;
    cmd.x[1] = 10;  cmd.y[1] = -12;
    cmd.x[2] = 12;  cmd.y[2] = 14;
    cmd.x[3] = -14; cmd.y[3] = 11;
    cmd.texture = &tex;

    saturn::core::ResolvedDistortedSprite out = {};
    ASSERT_EQ(saturn::core::resolve_distorted_sprite_cmd(&cmd, &out), SAT_OK);
    ASSERT_EQ(out.x[0], -10);
    ASSERT_EQ(out.y[1], -12);
    ASSERT_EQ(out.x[2], 12);
    ASSERT_EQ(out.y[3], 11);
    ASSERT_EQ(out.width, 16);
    ASSERT_EQ(out.height, 16);
    ASSERT_EQ(out.srca, 0x2000);
    ASSERT_EQ(out.palette, 3);
}

TEST(sprite_pmod_base_matches_normal_sprite) {
    using namespace saturn::core;
    /* Color mode 100B (bits 5..3) + ECD (bit 7); transparency stays enabled. */
    ASSERT_EQ(compose_sprite_pmod(0), 0x00A0);
    ASSERT_TRUE((compose_sprite_pmod(0) & 0x0040u) == 0u);
}

TEST(sprite_pmod_opaque_flag) {
    using namespace saturn::core;
    ASSERT_EQ(compose_sprite_pmod(SAT_SPRITE_FLAG_OPAQUE), 0x00E0);
    ASSERT_EQ(compose_sprite_pmod(0xFFFEu), 0x01A0);
}

TEST(user_clip_pmod_enables_inside_clip_only) {
    using namespace saturn::core;
    const uint16_t sprite = compose_inside_user_clip_pmod(compose_sprite_pmod(0), true);
    ASSERT_TRUE((sprite & 0x0400u) != 0u);
    ASSERT_TRUE((sprite & 0x0200u) == 0u);
    ASSERT_EQ(compose_inside_user_clip_pmod(compose_sprite_pmod(0), false),
              compose_sprite_pmod(0));
}

TEST(sprite_colr_shifts_bank_to_high_byte) {
    using namespace saturn::core;
    ASSERT_EQ(compose_sprite_colr(0), 0x0000);
    ASSERT_EQ(compose_sprite_colr(1), 0x0100);
    ASSERT_EQ(compose_sprite_colr(0x1F), 0x1F00);
}

TEST(sprite_cmd_select_values) {
    using namespace saturn::core;
    ASSERT_EQ(compose_polygon_ctrl(kVdp1CmdScaledSprite, false), 0x0001);
    ASSERT_EQ(compose_polygon_ctrl(kVdp1CmdDistortedSprite, false), 0x0002);
}

/* CRAM is 2048 words = 8 banks of 256. hal::vdp1::upload_palette indexes
 * CRAM[bank * 256] unchecked, so an out-of-range bank writes past the end of
 * CRAM and, in practice, wraps onto bank 0 -- silently repainting whatever
 * palette the VDP2 layers are using. */
TEST(palette_bank_must_be_in_range) {
    using namespace saturn::core;
    ASSERT_EQ(validate_palette_bank(0), SAT_OK);
    ASSERT_EQ(validate_palette_bank(7), SAT_OK);
    ASSERT_EQ(validate_palette_bank(8), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(validate_palette_bank(255), SAT_ERR_INVALID_ARG);
    /* The last valid bank must exactly fill CRAM. */
    ASSERT_EQ((uint32_t)kPaletteBankCount * 256u, 2048u);
}

TEST(indexed8_dims_reject_zero_and_misaligned_width) {
    using namespace saturn::core;
    ASSERT_EQ(validate_indexed8_texture_dims(0, 8), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(validate_indexed8_texture_dims(8, 0), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(validate_indexed8_texture_dims(12, 8), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(validate_indexed8_texture_dims(7, 8), SAT_ERR_INVALID_ARG);
}

TEST(indexed8_dims_accept_legal_sizes) {
    using namespace saturn::core;
    ASSERT_EQ(validate_indexed8_texture_dims(8, 1), SAT_OK);
    ASSERT_EQ(validate_indexed8_texture_dims(16, 16), SAT_OK);
    /* Source images may be 256x256, but VDP1 character patterns top out at
     * 255 rows (manual 6.6), so baked face textures must stay at or below. */
    ASSERT_EQ(validate_indexed8_texture_dims(256, 248), SAT_OK);
    /* Documented maxima from VDP1 manual 5.1/6.6. */
    ASSERT_EQ(validate_indexed8_texture_dims(504, 255), SAT_OK);
    ASSERT_EQ(kVdp1MaxTextureWidth, 504u);
    ASSERT_EQ(kVdp1MaxTextureHeight, 255u);
}

TEST(indexed8_dims_reject_over_maximum) {
    using namespace saturn::core;
    ASSERT_EQ(validate_indexed8_texture_dims(512, 8), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(validate_indexed8_texture_dims(8, 256), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(validate_indexed8_texture_dims(504, 256), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(validate_indexed8_texture_dims(256, 256), SAT_ERR_INVALID_ARG);
}

int main() {
    resolve_sprite_cmd_null_cmd();
    resolve_sprite_cmd_null_texture();
    resolve_sprite_cmd_invalid_texture();
    resolve_sprite_cmd_defaults();
    resolve_sprite_cmd_palette_override();
    polygon_ctrl_command_selects();
    polygon_ctrl_end_bit();
    polygon_ctrl_masks_high_bits();
    polygon_pmod_required_bits();
    polygon_pmod_opaque_flag();
    resolve_scaled_sprite_cmd_defaults();
    resolve_scaled_sprite_cmd_rejects_bad_texture();
    resolve_scaled_sprite_cmd_palette_override();
    resolve_distorted_sprite_cmd_defaults();
    sprite_pmod_base_matches_normal_sprite();
    sprite_pmod_opaque_flag();
    user_clip_pmod_enables_inside_clip_only();
    sprite_colr_shifts_bank_to_high_byte();
    sprite_cmd_select_values();
    palette_bank_must_be_in_range();
    indexed8_dims_reject_zero_and_misaligned_width();
    indexed8_dims_accept_legal_sizes();
    indexed8_dims_reject_over_maximum();

    printf("PASS: test_vdp1_logic.cpp (%d tests)\n", 24);
    return 0;
}
