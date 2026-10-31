/* Host tests for the pure TV-mode rules in src/hal/video_mode_logic.hpp.
 *
 * The table values are the VDP2 manual's TV screen mode register (2.4):
 * HRESO 0/1/2/3 = 320/352/640/704, VRESO 0/1/2 = 224/240/256 (256 PAL only),
 * LSMD 0/2/3 = non-interlaced/single-density/double-density (01 prohibited).
 * No hardware is touched here. */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/core.h"
#include "src/hal/video_mode_logic.hpp"

#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
    exit(1); } } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while(0)

using namespace saturn::hal::video_mode;

TEST(widths_are_the_four_tv_modes) {
    ASSERT_TRUE(supported_width(320u));
    ASSERT_TRUE(supported_width(352u));
    ASSERT_TRUE(supported_width(640u));
    ASSERT_TRUE(supported_width(704u));
    ASSERT_TRUE(!supported_width(351u));
    ASSERT_TRUE(!supported_width(512u));
}

TEST(heights_are_the_three_vreso_modes) {
    ASSERT_TRUE(supported_height(224u));
    ASSERT_TRUE(supported_height(240u));
    ASSERT_TRUE(supported_height(256u));
    ASSERT_TRUE(!supported_height(448u));
    ASSERT_TRUE(!supported_height(480u));
}

TEST(registers_encode_the_manual_table) {
    /* 320x224 non-interlaced: the sat_init default. */
    ASSERT_EQ(mode_bits(320u, 224u, SAT_VIDEO_INTERLACE_NONE), 0x0000u);
    /* HRESO alone. */
    ASSERT_EQ(mode_bits(352u, 224u, SAT_VIDEO_INTERLACE_NONE), 0x0001u);
    ASSERT_EQ(mode_bits(640u, 224u, SAT_VIDEO_INTERLACE_NONE), 0x0002u);
    ASSERT_EQ(mode_bits(704u, 224u, SAT_VIDEO_INTERLACE_NONE), 0x0003u);
    /* VRESO shifts into bits 5-4. */
    ASSERT_EQ(mode_bits(320u, 240u, SAT_VIDEO_INTERLACE_NONE), 0x0010u);
    ASSERT_EQ(mode_bits(320u, 256u, SAT_VIDEO_INTERLACE_NONE), 0x0020u);
    /* LSMD shifts into bits 7-6: single 10b, double 11b. */
    ASSERT_EQ(mode_bits(320u, 224u, SAT_VIDEO_INTERLACE_SINGLE), 0x0080u);
    ASSERT_EQ(mode_bits(320u, 240u, SAT_VIDEO_INTERLACE_SINGLE), 0x0090u);
    ASSERT_EQ(mode_bits(640u, 256u, SAT_VIDEO_INTERLACE_DOUBLE), 0x00E2u);
}

TEST(validation_accepts_ntsc_modes) {
    ASSERT_EQ(validate(320u, 224u, SAT_VIDEO_INTERLACE_NONE, false), SAT_OK);
    ASSERT_EQ(validate(352u, 224u, SAT_VIDEO_INTERLACE_NONE, false), SAT_OK);
    ASSERT_EQ(validate(640u, 224u, SAT_VIDEO_INTERLACE_NONE, false), SAT_OK);
    ASSERT_EQ(validate(704u, 224u, SAT_VIDEO_INTERLACE_NONE, false), SAT_OK);
    ASSERT_EQ(validate(320u, 240u, SAT_VIDEO_INTERLACE_NONE, false), SAT_OK);
    ASSERT_EQ(validate(704u, 240u, SAT_VIDEO_INTERLACE_SINGLE, false), SAT_OK);
}

TEST(validation_marks_pal_only_and_unimplemented_modes) {
    /* 256 lines is PAL-only (manual 2.4). */
    ASSERT_EQ(validate(320u, 256u, SAT_VIDEO_INTERLACE_NONE, false), SAT_ERR_UNSUPPORTED);
    ASSERT_EQ(validate(320u, 256u, SAT_VIDEO_INTERLACE_NONE, true), SAT_OK);
    ASSERT_EQ(validate(704u, 256u, SAT_VIDEO_INTERLACE_SINGLE, true), SAT_OK);
    /* Double-density interlace needs per-field VDP1 pictures. */
    ASSERT_EQ(validate(320u, 224u, SAT_VIDEO_INTERLACE_DOUBLE, true), SAT_ERR_UNSUPPORTED);
    ASSERT_EQ(validate(320u, 256u, SAT_VIDEO_INTERLACE_DOUBLE, false), SAT_ERR_UNSUPPORTED);
}

TEST(validation_rejects_impossible_values) {
    ASSERT_EQ(validate(480u, 224u, SAT_VIDEO_INTERLACE_NONE, true), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(validate(320u, 480u, SAT_VIDEO_INTERLACE_NONE, true), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(validate(320u, 240u, 3u, true), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(validate(nullptr, true), SAT_ERR_INVALID_ARG);
}

TEST(config_validation_uses_the_same_rules) {
    sat_video_config_t cfg = {320u, 224u, SAT_VIDEO_AUTO, SAT_VIDEO_INTERLACE_NONE};
    ASSERT_EQ(validate(&cfg, true), SAT_OK);
    cfg.height = 256u;
    ASSERT_EQ(validate(&cfg, false), SAT_ERR_UNSUPPORTED);
    cfg.height = 224u;
    cfg.interlace = SAT_VIDEO_INTERLACE_SINGLE;
    ASSERT_EQ(validate(&cfg, true), SAT_OK);
    cfg.interlace = SAT_VIDEO_INTERLACE_DOUBLE;
    ASSERT_EQ(validate(&cfg, true), SAT_ERR_UNSUPPORTED);
    ASSERT_EQ(validate(static_cast<const sat_video_config_t*>(nullptr), true),
              SAT_ERR_INVALID_ARG);
}

TEST(height_448_is_hires_double_density) {
    /* The positional config {640, 448, AUTO, 0} and the explicit one agree. */
    ASSERT_EQ(validate(640u, 448u, SAT_VIDEO_INTERLACE_NONE, false), SAT_OK);
    ASSERT_EQ(validate(704u, 448u, SAT_VIDEO_INTERLACE_DOUBLE, true), SAT_OK);
    ASSERT_EQ(mode_bits(640u, 448u, SAT_VIDEO_INTERLACE_NONE), 0x00C2u);
    ASSERT_EQ(mode_bits(704u, 448u, SAT_VIDEO_INTERLACE_DOUBLE), 0x00C3u);
    /* No drawing path at low resolution, and single density has its own spelling. */
    ASSERT_EQ(validate(320u, 448u, SAT_VIDEO_INTERLACE_NONE, false), SAT_ERR_UNSUPPORTED);
    ASSERT_EQ(validate(352u, 448u, SAT_VIDEO_INTERLACE_DOUBLE, false), SAT_ERR_UNSUPPORTED);
    ASSERT_EQ(validate(640u, 448u, SAT_VIDEO_INTERLACE_SINGLE, false), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(validate(480u, 448u, SAT_VIDEO_INTERLACE_NONE, false), SAT_ERR_INVALID_ARG);
}

int main() {
    widths_are_the_four_tv_modes();
    heights_are_the_three_vreso_modes();
    registers_encode_the_manual_table();
    validation_accepts_ntsc_modes();
    validation_marks_pal_only_and_unimplemented_modes();
    validation_rejects_impossible_values();
    config_validation_uses_the_same_rules();
    height_448_is_hires_double_density();
    printf("PASS: test_video_mode.cpp (8 tests)\n");
    return 0;
}
