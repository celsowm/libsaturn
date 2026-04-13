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
    validate_map_region_zero_size();
    validate_map_region_out_of_bounds();
    validate_map_region_stride_less_than_width();
    validate_map_region_valid();

    printf("PASS: test_vdp2_logic.cpp (%d tests)\n", 14);
    return 0;
}
