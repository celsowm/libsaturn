/* test_core_logic.cpp — host tests for core logic helpers */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/core.h"
#include "src/core/logic.hpp"

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

TEST(validate_video_config_ntsc_zero) {
    sat_video_config_t cfg = {320, 224, 0, 0};
    ASSERT_EQ(saturn::core::validate_video_config(&cfg), SAT_ERR_UNSUPPORTED);
}

TEST(validate_video_config_valid) {
    sat_video_config_t cfg = {320, 224, 1, 0};
    ASSERT_EQ(saturn::core::validate_video_config(&cfg), SAT_OK);
}

TEST(compute_map_base_words) {
    ASSERT_EQ(saturn::core::compute_map_base_words(0x0010), 0x0010u << 10u);
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

int main() {
    fx16_to_int_zero();
    fx16_to_int_one();
    fx16_to_int_negative_one();
    validate_video_config_null();
    validate_video_config_ntsc_zero();
    validate_video_config_valid();
    compute_map_base_words();
    compute_map_row_offset();

    printf("PASS: test_core_logic.cpp (%d tests)\n", 8);
    return 0;
}
