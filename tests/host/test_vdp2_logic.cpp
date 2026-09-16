/* test_vdp2_logic.cpp — host tests for VDP2 logic helpers */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/vdp2.h"
#include "src/core/logic.hpp"

#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
    exit(1); } } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while(0)

/* ---- validate_nbg0_config ---- */

TEST(validate_nbg0_config_null) {
    ASSERT_EQ(saturn::core::validate_nbg0_config(nullptr), SAT_ERR_INVALID_ARG);
}

TEST(validate_nbg0_config_valid) {
    sat_vdp2_nbg0_config_t cfg = {};
    cfg.char_size = SAT_VDP2_CHAR_SIZE_1X1;
    cfg.color_mode = SAT_VDP2_COLOR_MODE_16;
    cfg.map_plane_index = 0x0010;
    cfg.transparent_code_enabled = 1;
    ASSERT_EQ(saturn::core::validate_nbg0_config(&cfg), SAT_OK);
}

TEST(validate_nbg0_config_invalid_char_size) {
    sat_vdp2_nbg0_config_t cfg = {};
    cfg.char_size = static_cast<sat_vdp2_char_size_t>(2);  /* invalid */
    cfg.color_mode = SAT_VDP2_COLOR_MODE_16;
    cfg.map_plane_index = 0x0010;
    ASSERT_EQ(saturn::core::validate_nbg0_config(&cfg), SAT_ERR_INVALID_ARG);
}

TEST(validate_nbg0_config_invalid_color_mode) {
    sat_vdp2_nbg0_config_t cfg = {};
    cfg.char_size = SAT_VDP2_CHAR_SIZE_1X1;
    cfg.color_mode = static_cast<sat_vdp2_color_mode_t>(5);  /* invalid */
    cfg.map_plane_index = 0x0010;
    ASSERT_EQ(saturn::core::validate_nbg0_config(&cfg), SAT_ERR_INVALID_ARG);
}

TEST(validate_nbg0_config_invalid_plane_index) {
    sat_vdp2_nbg0_config_t cfg = {};
    cfg.char_size = SAT_VDP2_CHAR_SIZE_1X1;
    cfg.color_mode = SAT_VDP2_COLOR_MODE_16;
    cfg.map_plane_index = 0x0040;  /* > 0x3F */
    ASSERT_EQ(saturn::core::validate_nbg0_config(&cfg), SAT_ERR_INVALID_ARG);
}

/* ---- validate_vdp2_palette_upload ---- */

TEST(validate_palette_upload_within_bounds) {
    ASSERT_EQ(saturn::core::validate_vdp2_palette_upload(256, 0), SAT_OK);
}

TEST(validate_palette_upload_beyond_bounds) {
    ASSERT_EQ(saturn::core::validate_vdp2_palette_upload(1024, 1025), SAT_ERR_CAPACITY);
}

/* ---- validate_vdp2_vram_write ---- */

TEST(validate_vram_write_null) {
    ASSERT_EQ(saturn::core::validate_vdp2_vram_write(0, 0), SAT_ERR_INVALID_ARG);
}

TEST(validate_vram_write_within_bounds) {
    ASSERT_EQ(saturn::core::validate_vdp2_vram_write(0, 1024), SAT_OK);
}

TEST(validate_vram_write_overflow) {
    ASSERT_EQ(saturn::core::validate_vdp2_vram_write(262140, 100), SAT_ERR_CAPACITY);
}

/* ---- RBG0 helpers ---- */

TEST(rbg0_bitmap_size_support) {
    ASSERT_TRUE(saturn::core::is_supported_rbg0_bitmap_size(SAT_VDP2_RBG0_BITMAP_512x256));
    ASSERT_TRUE(saturn::core::is_supported_rbg0_bitmap_size(SAT_VDP2_RBG0_BITMAP_512x512));
    ASSERT_TRUE(!saturn::core::is_supported_rbg0_bitmap_size(SAT_VDP2_RBG0_BITMAP_256x256));
    ASSERT_TRUE(!saturn::core::is_supported_rbg0_bitmap_size(SAT_VDP2_RBG0_BITMAP_1024x1024));
}

TEST(rbg0_bitmap_control_word_512x256) {
    ASSERT_EQ(
        saturn::core::compose_rbg0_bitmap_control_word(
            SAT_VDP2_COLOR_MODE_256,
            SAT_VDP2_RBG0_BITMAP_512x256
        ),
        0x1200u
    );
}

TEST(rbg0_bitmap_control_word_512x512) {
    ASSERT_EQ(
        saturn::core::compose_rbg0_bitmap_control_word(
            SAT_VDP2_COLOR_MODE_256,
            SAT_VDP2_RBG0_BITMAP_512x512
        ),
        0x1600u
    );
}

TEST(validate_rbg0_config_null) {
    ASSERT_EQ(saturn::core::validate_rbg0_config(nullptr), SAT_ERR_INVALID_ARG);
}

TEST(validate_rbg0_config_valid) {
    sat_vdp2_rbg0_config_t cfg = {};
    cfg.bitmap_size = SAT_VDP2_RBG0_BITMAP_512x256;
    cfg.color_mode = SAT_VDP2_COLOR_MODE_256;
    cfg.bitmap_base_word = 0x0000u;
    cfg.rot_param_base_word = 0x10000u;
    ASSERT_EQ(saturn::core::validate_rbg0_config(&cfg), SAT_OK);
}

TEST(validate_rbg0_config_invalid_size) {
    sat_vdp2_rbg0_config_t cfg = {};
    cfg.bitmap_size = SAT_VDP2_RBG0_BITMAP_256x256;
    cfg.color_mode = SAT_VDP2_COLOR_MODE_256;
    cfg.bitmap_base_word = 0x0000u;
    cfg.rot_param_base_word = 0x10000u;
    ASSERT_EQ(saturn::core::validate_rbg0_config(&cfg), SAT_ERR_INVALID_ARG);
}

TEST(rbg0_rotation_table_constants) {
    ASSERT_EQ(saturn::core::kRbg0RotationParamWordCount, 48u);
    ASSERT_EQ(saturn::core::kRbg0ScrollWordOffset, 0u);
    ASSERT_EQ(saturn::core::kRbg0ScrollFracWordOffset, 1u);
    ASSERT_EQ(saturn::core::kRbg0ScrollYWordOffset, 2u);
    ASSERT_EQ(saturn::core::kRbg0ScrollYFracWordOffset, 3u);
    ASSERT_EQ(saturn::core::kRbg0MatrixWordOffset, 14u);
    ASSERT_EQ(saturn::core::kRbg0ViewpointWordOffset, 26u);
    ASSERT_EQ(saturn::core::kRbg0CenterWordOffset, 30u);
    ASSERT_EQ(saturn::core::kRbg0ParallelMoveWordOffset, 34u);
    ASSERT_EQ(saturn::core::kRbg0ScalingWordOffset, 38u);
    ASSERT_EQ(saturn::core::kRbg0KastWordOffset, 42u);
    ASSERT_EQ(saturn::core::kRbg0DeltaKastWordOffset, 44u);
    ASSERT_EQ(saturn::core::kRbg0DeltaKaxWordOffset, 46u);
}

TEST(validate_rbg0_rotation_table_offset) {
    ASSERT_EQ(saturn::core::validate_rbg0_rotation_table_offset(0), SAT_OK);
    ASSERT_EQ(
        saturn::core::validate_rbg0_rotation_table_offset(saturn::core::kVdp2VramWordCapacity - 48u),
        SAT_OK
    );
    ASSERT_EQ(
        saturn::core::validate_rbg0_rotation_table_offset(saturn::core::kVdp2VramWordCapacity - 47u),
        SAT_ERR_CAPACITY
    );
}

/* ---- validate_map_region ---- */

TEST(validate_map_region_zero_size) {
    ASSERT_EQ(saturn::core::validate_map_region(0, 0, 0, 1, 64, 64, 64), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(saturn::core::validate_map_region(0, 0, 1, 0, 64, 64, 64), SAT_ERR_INVALID_ARG);
}

TEST(validate_map_region_out_of_bounds) {
    ASSERT_EQ(saturn::core::validate_map_region(64, 0, 1, 1, 64, 64, 64), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(saturn::core::validate_map_region(0, 64, 1, 1, 64, 64, 64), SAT_ERR_INVALID_ARG);
}

TEST(validate_map_region_stride_less_than_width) {
    ASSERT_EQ(saturn::core::validate_map_region(0, 0, 10, 10, 64, 64, 5), SAT_ERR_INVALID_ARG);
}

TEST(validate_map_region_valid) {
    ASSERT_EQ(saturn::core::validate_map_region(0, 0, 8, 8, 64, 64, 64), SAT_OK);
}

/* ---- layer priority ---- */

TEST(nbg0_priority_leaves_other_layers_alone) {
    /* PRINA carries NBG1 in bits 8-10. Setting NBG0 must not disturb it --
     * a mask slip here moves a layer nobody touched. */
    const uint16_t prina = 0x0507u; /* NBG1 = 5, NBG0 = 7 */
    ASSERT_EQ(saturn::core::compose_nbg0_priority(prina, 1u), 0x0501u);
    ASSERT_EQ(saturn::core::compose_nbg0_priority(prina, 0u), 0x0500u);
}

TEST(nbg0_priority_clamps_to_three_bits) {
    /* 8 is not a priority; it must not spill into the NBG1 field. */
    ASSERT_EQ(saturn::core::compose_nbg0_priority(0x0500u, 8u), 0x0500u);
    ASSERT_EQ(saturn::core::compose_nbg0_priority(0x0500u, 0xFFu), 0x0507u);
}

TEST(sprite_priority_sets_both_halves) {
    /* Sprite type bits can select either half per pixel; leaving them
     * different makes the priority depend on a bit the caller never set. */
    ASSERT_EQ(saturn::core::compose_sprite_priority(6u), 0x0606u);
    ASSERT_EQ(saturn::core::compose_sprite_priority(1u), 0x0101u);
}

TEST(nbg0_bgon_preserves_rbg0) {
    ASSERT_EQ(saturn::core::compose_nbg0_bgon(0x1010u, true, false), 0x1111u);
    ASSERT_EQ(saturn::core::compose_nbg0_bgon(0x1010u, true, true), 0x1011u);
}

TEST(rbg0_bgon_preserves_nbg0) {
    ASSERT_EQ(saturn::core::compose_rbg0_bgon(0x0101u, true, false), 0x1111u);
    ASSERT_EQ(saturn::core::compose_rbg0_bgon(0x0101u, false, true), 0x0101u);
}

/* ---- tiled image upload ---- */

TEST(cell_is_row_major_from_the_right_block) {
    /* A 16x16 image numbered by position, so a transposed or mirrored cell
     * cannot accidentally pass. */
    uint8_t image[16 * 16];
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            image[(y * 16) + x] = (uint8_t)((y * 16) + x);
        }
    }

    uint8_t cell[64];
    saturn::core::build_cell_indexed8(image, 16u, 1u, 1u, cell);

    /* Tile (1,1) is the bottom-right 8x8: rows 8-15, columns 8-15. */
    ASSERT_EQ(cell[0], (uint8_t)((8 * 16) + 8));   /* first pixel */
    ASSERT_EQ(cell[1], (uint8_t)((8 * 16) + 9));   /* next COLUMN, not row */
    ASSERT_EQ(cell[8], (uint8_t)((9 * 16) + 8));   /* start of the next row */
    ASSERT_EQ(cell[63], (uint8_t)((15 * 16) + 15));
}

TEST(pattern_name_advances_two_per_cell) {
    /* An 8bpp cell is 64 bytes and a character number counts 32-byte units,
     * so consecutive cells are two apart. Numbering starts at 0x100 because
     * character data does not begin at the base of the addressable window --
     * see kVdp2FirstCharNumber. */
    ASSERT_EQ(saturn::core::compose_pattern_name(0u, 4u, 0u, 0u), 0x0100u);
    ASSERT_EQ(saturn::core::compose_pattern_name(0u, 4u, 1u, 0u), 0x0102u);
    ASSERT_EQ(saturn::core::compose_pattern_name(0u, 4u, 0u, 1u), 0x0108u); /* row of 4 */
    ASSERT_EQ(saturn::core::compose_pattern_name(0u, 4u, 3u, 2u), 0x0116u);
}

TEST(pattern_name_carries_the_palette_bank) {
    ASSERT_EQ(saturn::core::compose_pattern_name(1u, 4u, 1u, 0u), 0x1102u);
    ASSERT_EQ(saturn::core::compose_pattern_name(7u, 4u, 0u, 0u), 0x7100u);
}

TEST(nbg0_image_must_be_whole_cells) {
    ASSERT_EQ(saturn::core::validate_nbg0_image(100u, 64u, 0x3Bu), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(saturn::core::validate_nbg0_image(64u, 100u, 0x3Bu), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(saturn::core::validate_nbg0_image(0u, 64u, 0x3Bu), SAT_ERR_INVALID_ARG);
}

TEST(nbg0_image_must_fit_under_the_map_plane) {
    /* Character data and the map share one addressable window, and the map
     * plane index decides where the cells have to stop. At plane 0x3B the
     * window holds 0xA000 words, i.e. 1280 cells of 32 words each:
     * 256x256 is 1024 cells and fits, 320x320 is 1600 and does not. */
    ASSERT_EQ(saturn::core::validate_nbg0_image(256u, 256u, 0x3Bu), SAT_OK);
    ASSERT_EQ(saturn::core::validate_nbg0_image(320u, 320u, 0x3Bu), SAT_ERR_CAPACITY);

    /* A plane low enough to sit inside the character window leaves no room
     * at all, rather than silently overwriting the cells with the map. */
    ASSERT_EQ(saturn::core::validate_nbg0_image(64u, 64u, 0x10u), SAT_ERR_INVALID_ARG);
}

TEST(nbg0_image_cannot_exceed_the_plane) {
    /* The plane is 64 cells across; a wider image could not be addressed. */
    ASSERT_EQ(saturn::core::validate_nbg0_image(520u, 64u, 0x3Bu), SAT_ERR_CAPACITY);
}

int main() {
    validate_nbg0_config_null();
    validate_nbg0_config_valid();
    validate_nbg0_config_invalid_char_size();
    validate_nbg0_config_invalid_color_mode();
    validate_nbg0_config_invalid_plane_index();
    validate_palette_upload_within_bounds();
    validate_palette_upload_beyond_bounds();
    validate_vram_write_null();
    validate_vram_write_within_bounds();
    validate_vram_write_overflow();
    rbg0_bitmap_size_support();
    rbg0_bitmap_control_word_512x256();
    rbg0_bitmap_control_word_512x512();
    validate_rbg0_config_null();
    validate_rbg0_config_valid();
    validate_rbg0_config_invalid_size();
    rbg0_rotation_table_constants();
    validate_rbg0_rotation_table_offset();
    validate_map_region_zero_size();
    validate_map_region_out_of_bounds();
    validate_map_region_stride_less_than_width();
    validate_map_region_valid();
    nbg0_priority_leaves_other_layers_alone();
    nbg0_priority_clamps_to_three_bits();
    sprite_priority_sets_both_halves();
    nbg0_bgon_preserves_rbg0();
    rbg0_bgon_preserves_nbg0();
    cell_is_row_major_from_the_right_block();
    pattern_name_advances_two_per_cell();
    pattern_name_carries_the_palette_bank();
    nbg0_image_must_be_whole_cells();
    nbg0_image_must_fit_under_the_map_plane();
    nbg0_image_cannot_exceed_the_plane();

    printf("PASS: test_vdp2_logic.cpp (%d tests)\n", 33);
    return 0;
}
