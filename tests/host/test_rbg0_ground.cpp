/* test_rbg0_ground.cpp — host tests for the vdp2_rbg0_ground Mode-7 math.
 *
 * Exercises examples/vdp2_rbg0_ground/rbg0_math.h directly — the exact same
 * header compiled unmodified into the Saturn ROM (main.c). A failure here
 * means the shipped rotation/coefficient tables are wrong, not that a
 * hand-copied "test model" of the math drifted from the real thing.
 *
 * test_yst_differs_from_py is the regression test for the PR1 bug: the
 * example originally set Yst == Py, which collapses Ysp = E*(Yst-Py) to 0 on
 * every scanline, so every line samples the same texture row (radial streaks
 * instead of a foreshortened floor). See docs/sega_saturn_hardware/hard/
 * vdp2/hon/p06_10.md for the hardware formula this reimplements.
 */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "examples/vdp2_rbg0_ground/rbg0_math.h"

#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
    exit(1); } } while(0)
#define ASSERT_NE(a, b) do { if ((a) == (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s == %s (expected different)\n", __FILE__, __LINE__, #a, #b); \
    exit(1); } } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while(0)

/* Same constants as examples/vdp2_rbg0_ground/main.c. */
static const uint32_t kScreenWidth = 320;
static const uint32_t kScreenHeight = 224;
static const uint32_t kBitmapWidth = 512;
static const uint32_t kBitmapHeight = 256;
static const uint32_t kHorizon = 96;
static const uint32_t kCx = kScreenWidth / 2;
static const uint32_t kFocal = 96;
static const uint32_t kMinDepth = 8;
static const uint32_t kGroundForward = 96;
static const uint32_t kCoefBaseWord = 0x12000;

static rbg0_ground_config_t make_config() {
    rbg0_ground_config_t cfg;
    cfg.bitmap_width = kBitmapWidth;
    cfg.bitmap_height = kBitmapHeight;
    cfg.cx = kCx;
    cfg.horizon = kHorizon;
    cfg.focal = kFocal;
    cfg.min_depth = kMinDepth;
    cfg.ground_forward = kGroundForward;
    cfg.coef_base_word = kCoefBaseWord;
    return cfg;
}

/* Samples tex_x/tex_y for screen column screen_x on scanline y, camera at
 * (cam_x, cam_y). Fails the test outright if the line is unexpectedly
 * transparent (only expected for y <= horizon).
 */
static void sample(const rbg0_ground_config_t& cfg, int32_t cam_x, int32_t cam_y,
                    uint32_t y, int32_t screen_x, int32_t* tex_x, int32_t* tex_y) {
    uint16_t params[48];
    uint16_t w0, w1;
    rbg0_ground_build_params(&cfg, cam_x, cam_y, params);
    rbg0_ground_encode_coefficient(&cfg, y, &w0, &w1);
    int visible = rbg0_ground_sample_point(params, w0, w1, screen_x, tex_x, tex_y);
    ASSERT_TRUE(visible);
}

/* The PR1 regression: Yst (word 2) must differ from Py (word 27), or every
 * scanline collapses onto the same texture row. */
TEST(yst_differs_from_py) {
    rbg0_ground_config_t cfg = make_config();
    uint16_t params[48];
    rbg0_ground_build_params(&cfg, 0, 0, params);
    ASSERT_NE(params[2], params[27]);
}

/* The property that actually would have caught the bug: two different rows
 * below the horizon must sample different texture Y coordinates. Under the
 * Yst == Py bug, sample_y was constant (== cam_y) for every row. */
TEST(sample_y_varies_by_row) {
    rbg0_ground_config_t cfg = make_config();
    int32_t tx1, ty1, tx2, ty2;
    sample(cfg, 0, 0, kHorizon + 1, (int32_t)kCx, &tx1, &ty1);
    sample(cfg, 0, 0, kScreenHeight - 1, (int32_t)kCx, &tx2, &ty2);
    ASSERT_NE(ty1, ty2);
}

/* tex_y must never move away from the camera (must not increase, since
 * larger tex_y is farther away — see main.c's UP/DOWN comment) as y
 * increases toward the bottom of the screen. Not strict: at large depths
 * k(y) is small enough that consecutive rows can round to the same integer
 * tex_y (verified against the actual encode_coefficient output), but it must
 * never go the wrong way. Catches sign flips and stray matrix terms. */
TEST(sample_y_monotonic_toward_camera) {
    rbg0_ground_config_t cfg = make_config();
    int32_t prev_ty = 0;
    int32_t tx, ty;
    bool saw_strict_decrease = false;
    for (uint32_t y = kHorizon + 1; y < kScreenHeight; y++) {
        sample(cfg, 0, 0, y, (int32_t)kCx, &tx, &ty);
        if (y > kHorizon + 1) {
            ASSERT_TRUE(ty <= prev_ty);
            if (ty < prev_ty) {
                saw_strict_decrease = true;
            }
        }
        prev_ty = ty;
    }
    /* Guard against a degenerate "always equal" pass (e.g. GROUND_FORWARD
     * silently zeroed again): most of the range must actually vary. */
    ASSERT_TRUE(saw_strict_decrease);
}

/* The screen-center column samples straight ahead: tex_x == cam_x regardless
 * of depth, since Xst = 0 => Xsp = -Px = -Cx, cancelled by screen_x == Cx.
 * cam_x must stay within [0, bitmap_width) here or texel wrapping (the
 * bitmap repeating every 512 texels) changes the expected value. */
TEST(center_column_samples_camera_x) {
    rbg0_ground_config_t cfg = make_config();
    int32_t tx, ty;
    const int32_t cam_x = 200;
    ASSERT_TRUE(cam_x < (int32_t)kBitmapWidth);
    sample(cfg, cam_x, 0, kHorizon + 20, (int32_t)kCx, &tx, &ty);
    ASSERT_EQ(tx, cam_x);
}

/* Rows at or above the horizon are transparent (sky shows through). */
TEST(horizon_and_above_transparent) {
    rbg0_ground_config_t cfg = make_config();
    uint16_t w0, w1;
    rbg0_ground_encode_coefficient(&cfg, 0, &w0, &w1);
    ASSERT_TRUE((w0 & 0x8000u) != 0u);
    rbg0_ground_encode_coefficient(&cfg, kHorizon, &w0, &w1);
    ASSERT_TRUE((w0 & 0x8000u) != 0u);
    rbg0_ground_encode_coefficient(&cfg, kHorizon + 1, &w0, &w1);
    ASSERT_TRUE((w0 & 0x8000u) == 0u);
}

/* The coefficient integer field is 7 bits (Figure 6.7); k = FOCAL/depth peaks
 * just below the horizon and must never overflow it. */
TEST(coefficient_integer_within_7_bits) {
    rbg0_ground_config_t cfg = make_config();
    for (uint32_t y = kHorizon + 1; y < kScreenHeight; y++) {
        uint16_t w0, w1;
        rbg0_ground_encode_coefficient(&cfg, y, &w0, &w1);
        ASSERT_TRUE((w0 & 0x007Fu) == w0); /* no bits set outside the 7-bit field */
        (void)w1;
    }
}

/* Negative camera coordinates must wrap into [0, bitmap_{width,height}). */
TEST(negative_camera_wraps_positive) {
    rbg0_ground_config_t cfg = make_config();
    int32_t mx = rbg0_ground_wrap_translation(-1, cfg.bitmap_width, cfg.cx);
    int32_t my = rbg0_ground_wrap_translation(-1, cfg.bitmap_height, cfg.horizon);
    /* wrapped(-1, 512) = 511, minus center cx=160 => 351 */
    ASSERT_EQ(mx, (int32_t)(cfg.bitmap_width - 1) - (int32_t)cfg.cx);
    ASSERT_EQ(my, (int32_t)(cfg.bitmap_height - 1) - (int32_t)cfg.horizon);
}

/* KAst encoding: COEF_BASE_WORD 0x12000 (word addr) => byte 0x24000 => KAst
 * 0x9000, per Table 6.3 (LSB = 4H for 2-word coefficient data). */
TEST(kast_word_encoding) {
    ASSERT_EQ(rbg0_ground_kast_word(0x12000u), 0x9000u);
}

int main() {
    yst_differs_from_py();
    sample_y_varies_by_row();
    sample_y_monotonic_toward_camera();
    center_column_samples_camera_x();
    horizon_and_above_transparent();
    coefficient_integer_within_7_bits();
    negative_camera_wraps_positive();
    kast_word_encoding();

    printf("PASS: test_rbg0_ground.cpp (%d tests)\n", 8);
    return 0;
}
