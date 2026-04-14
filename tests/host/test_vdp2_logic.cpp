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

    printf("PASS: test_vdp2_logic.cpp (%d tests)\n", 21);
    return 0;
}
