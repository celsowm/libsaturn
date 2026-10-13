/* test_core_logic.cpp — host tests for core logic helpers */

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include "saturn/core.h"
#include "src/core/runtime/logic.hpp"

#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
    exit(1); } } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while(0)

TEST(fx16_to_int_zero) {
    ASSERT_EQ(saturn::core::fx16_to_int_impl(0), 0);
}

TEST(fx16_to_int_one) {
    ASSERT_EQ(saturn::core::fx16_to_int_impl(1 * SAT_FX16_ONE), 1);
}

TEST(fx16_to_int_negative_one) {
    /* -1 * SAT_FX16_ONE = 0xFFFF0000 as int32_t */
    sat_fx16_t neg_one = static_cast<sat_fx16_t>(-1 * SAT_FX16_ONE);
    ASSERT_EQ(saturn::core::fx16_to_int_impl(neg_one), -1);
}

TEST(validate_video_config_null) {
    ASSERT_EQ(saturn::core::validate_video_config(nullptr), SAT_ERR_INVALID_ARG);
}

TEST(validate_video_config_standards) {
    sat_video_config_t cfg = {320, 224, SAT_VIDEO_PAL, 0};
    ASSERT_EQ(saturn::core::validate_video_config(&cfg), SAT_OK);
    cfg.ntsc = SAT_VIDEO_NTSC;
    ASSERT_EQ(saturn::core::validate_video_config(&cfg), SAT_OK);
    cfg.ntsc = SAT_VIDEO_AUTO;
    ASSERT_EQ(saturn::core::validate_video_config(&cfg), SAT_OK);
    cfg.ntsc = 3;
    ASSERT_EQ(saturn::core::validate_video_config(&cfg), SAT_ERR_INVALID_ARG);
}

TEST(validate_video_config_valid) {
    sat_video_config_t cfg = {320, 224, 1, 0};
    ASSERT_EQ(saturn::core::validate_video_config(&cfg), SAT_OK);
}

TEST(compute_map_base_words) {
    /* One page is 0x2000 bytes = 0x1000 words = 1 << 12. */
    ASSERT_EQ(saturn::core::compute_map_base_words(0x0010), 0x0010u << 12u);
}

TEST(compute_map_row_offset) {
    uint16_t plane_index = 0x0010;
    uint16_t map_w = 64;
    /* row 0 = base */
    ASSERT_EQ(saturn::core::compute_map_row_offset(plane_index, map_w, 0),
              saturn::core::compute_map_base_words(plane_index));
    /* row 1 = base + 64 */
    ASSERT_EQ(saturn::core::compute_map_row_offset(plane_index, map_w, 1),
              saturn::core::compute_map_base_words(plane_index) + 64u);
}

/* SAT_FOURCC packs big-endian: 'L','I','T','1' reads as 0x4C495431 in a hex
 * dump. The byte-by-byte check guards the channel order against typos. */
TEST(fourcc_roundtrip) {
    ASSERT_EQ(SAT_FOURCC('L', 'I', 'T', '1'), 0x4C495431u);
    ASSERT_EQ(SAT_FOURCC('S', 'B', 'T', 'M'), 0x5342544Du);
    ASSERT_EQ(SAT_FOURCC('D', 'I', 'N', '1'), 0x44494E31u);
    /* All-zero FOURCC stays zero. */
    ASSERT_EQ(SAT_FOURCC(0, 0, 0, 0), 0u);
    /* Each byte lives in its lane: the high byte is the first arg, the
     * low byte is the last. Probe the lanes by shift so endianness does not
     * leak through memcpy. */
    uint32_t v = SAT_FOURCC(0x12, 0x34, 0x56, 0x78);
    ASSERT_EQ((v >> 24) & 0xFFu, 0x12u);
    ASSERT_EQ((v >> 16) & 0xFFu, 0x34u);
    ASSERT_EQ((v >> 8) & 0xFFu, 0x56u);
    ASSERT_EQ(v & 0xFFu, 0x78u);
}

/* SAT_MIN/MAX/CLAMP are intentionally double-evaluation macros. To keep the
 * tests independent of the macro semantics, only pass plain lvalues here. */
TEST(min_max_clamp_edges) {
    ASSERT_EQ(SAT_MIN(3, 5), 3);
    ASSERT_EQ(SAT_MIN(-2, 7), -2);
    ASSERT_EQ(SAT_MIN(0, 0), 0);
    ASSERT_EQ(SAT_MAX(3, 5), 5);
    ASSERT_EQ(SAT_MAX(-2, 7), 7);
    /* CLAMP(v, lo, hi) returns lo when v < lo and hi when v > hi. */
    ASSERT_EQ(SAT_CLAMP(5, 0, 10), 5);
    ASSERT_EQ(SAT_CLAMP(-1, 0, 10), 0);
    ASSERT_EQ(SAT_CLAMP(11, 0, 10), 10);
    /* Clamp range can be inverted only if the caller asks for it; the macro
     * returns the first argument that wins, so unequal bounds matter. */
    ASSERT_EQ(SAT_CLAMP(5, 10, 0), 10);
}

/* sat_isqrt32 — floor(sqrt(v)) for a uint32 input. Cover perfect squares,
 * a couple of non-squares at well-known points, and the trivial endpoints. */
TEST(isqrt32_perfect_squares) {
    ASSERT_EQ(sat_isqrt32(0u), 0u);
    ASSERT_EQ(sat_isqrt32(1u), 1u);
    ASSERT_EQ(sat_isqrt32(4u), 2u);
    ASSERT_EQ(sat_isqrt32(9u), 3u);
    ASSERT_EQ(sat_isqrt32(100u), 10u);
    ASSERT_EQ(sat_isqrt32(65536u), 256u);   /* 2^16 */
    ASSERT_EQ(sat_isqrt32(0xFFFE0001u), 0xFFFFu);
    /* Non-squares must round down: floor(sqrt(10)) = 3. */
    ASSERT_EQ(sat_isqrt32(10u), 3u);
    ASSERT_EQ(sat_isqrt32(99u), 9u);
    ASSERT_EQ(sat_isqrt32(101u), 10u);
    /* Maximum uint32 input must not overflow. */
    ASSERT_EQ(sat_isqrt32(0xFFFFFFFFu), 0xFFFFu);
}

/* sat_triangle_wave(step, period, amp): 0 at the boundaries, peak at
 * the half-period and quarter peaks, descending mirror. Period < 2 returns
 * peak amplitude (degenerate but documented). */
TEST(triangle_wave_quarter_points) {
    /* period=4, amp=8: step 0 -> 0, step 1 -> 4, step 2 -> 8, step 3 -> 4. */
    ASSERT_EQ(sat_triangle_wave(0u, 4u, 8), 0);
    ASSERT_EQ(sat_triangle_wave(1u, 4u, 8), 4);
    ASSERT_EQ(sat_triangle_wave(2u, 4u, 8), 8);
    ASSERT_EQ(sat_triangle_wave(3u, 4u, 8), 4);
    /* Wraps: step 4 is the next period boundary, also 0. */
    ASSERT_EQ(sat_triangle_wave(4u, 4u, 8), 0);
    /* period=6, amp=10: 0, 3, 6, 10, 6, 3 (the apex is the half-period). */
    ASSERT_EQ(sat_triangle_wave(0u, 6u, 10), 0);
    ASSERT_EQ(sat_triangle_wave(1u, 6u, 10), 3);   /* half-up-slope */
    ASSERT_EQ(sat_triangle_wave(2u, 6u, 10), 6);   /* quarter-step */
    ASSERT_EQ(sat_triangle_wave(3u, 6u, 10), 10);  /* peak */
    ASSERT_EQ(sat_triangle_wave(4u, 6u, 10), 6);
    ASSERT_EQ(sat_triangle_wave(5u, 6u, 10), 3);
    /* Degenerate period: period < 2 is a documented no-op returning 0 (no
     * wave can fit in zero or one samples). Avoid in calling code. */
    ASSERT_EQ(sat_triangle_wave(0u, 1u, 5), 0);
    ASSERT_EQ(sat_triangle_wave(7u, 0u, 9), 0);
    /* period=2, amp=10: half=1, every phase >= half folds back to up=1,
     * peak at every phase except exactly 0 (boundary). */
    ASSERT_EQ(sat_triangle_wave(0u, 2u, 10), 0);   /* boundary */
    ASSERT_EQ(sat_triangle_wave(1u, 2u, 10), 10);  /* peak (falls on the only other sample) */
}

int main() {
    fx16_to_int_zero();
    fx16_to_int_one();
    fx16_to_int_negative_one();
    validate_video_config_null();
    validate_video_config_standards();
    validate_video_config_valid();
    compute_map_base_words();
    compute_map_row_offset();
    fourcc_roundtrip();
    min_max_clamp_edges();
    isqrt32_perfect_squares();
    triangle_wave_quarter_points();

    printf("PASS: test_core_logic.cpp (%d tests)\n", 12);
    return 0;
}
