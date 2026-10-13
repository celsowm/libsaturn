/* test_vdp2_palette_alloc.cpp — host tests for the CRAM palette
 * bank allocator. Each test calls sat_vdp2_palette_reset() at the
 * top so the global ring is clean. */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/vdp2_alloc.h"

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
    exit(1); } } while (0)
#define ASSERT_TRUE(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); } } while (0)

static void test_reserve_returns_first_bank_with_room(void) {
    sat_vdp2_palette_reset();
    sat_vdp2_palette_range_t r;
    ASSERT_EQ(sat_vdp2_palette_reserve(16u, &r), SAT_OK);
    ASSERT_EQ(r.bank, 1u);
    ASSERT_EQ(r.code, 0u);
    ASSERT_EQ(r.count, 16u);
    /* A second call within the same bank should land at code 16. */
    sat_vdp2_palette_range_t r2;
    ASSERT_EQ(sat_vdp2_palette_reserve(32u, &r2), SAT_OK);
    ASSERT_EQ(r2.bank, 1u);
    ASSERT_EQ(r2.code, 16u);
    ASSERT_EQ(r2.count, 32u);
}

static void test_reserve_rotates_to_next_bank_when_full(void) {
    sat_vdp2_palette_reset();
    sat_vdp2_palette_range_t r;
    /* 240 codes in bank 1. */
    ASSERT_EQ(sat_vdp2_palette_reserve(240u, &r), SAT_OK);
    ASSERT_EQ(r.bank, 1u);
    ASSERT_EQ(r.count, 240u);
    /* Next reservation cannot fit in bank 1 (only 16 codes left) so it
     * must rotate to bank 2. */
    sat_vdp2_palette_range_t r2;
    ASSERT_EQ(sat_vdp2_palette_reserve(32u, &r2), SAT_OK);
    ASSERT_EQ(r2.bank, 2u);
    ASSERT_EQ(r2.code, 0u);
    ASSERT_EQ(r2.count, 32u);
}

static void test_reserve_skips_bank_zero(void) {
    sat_vdp2_palette_reset();
    sat_vdp2_palette_range_t r;
    ASSERT_EQ(sat_vdp2_palette_reserve(128u, &r), SAT_OK);
    /* Bank 0 is the default VDP1 palette and never returned. */
    ASSERT_TRUE(r.bank >= 1u && r.bank <= 7u);
}

static void test_invalid_count_rejected(void) {
    sat_vdp2_palette_reset();
    sat_vdp2_palette_range_t r;
    ASSERT_EQ(sat_vdp2_palette_reserve(0u, &r), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_vdp2_palette_reserve(257u, &r), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_vdp2_palette_reserve(16u, nullptr),
              SAT_ERR_INVALID_ARG);
}

int main() {
    test_reserve_returns_first_bank_with_room();
    test_reserve_rotates_to_next_bank_when_full();
    test_reserve_skips_bank_zero();
    test_invalid_count_rejected();
    printf("PASS: test_vdp2_palette_alloc.cpp (4 tests)\n");
    return 0;
}
