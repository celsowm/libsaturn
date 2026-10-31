/* test_vdp2_palette_alloc.cpp - host tests for the CRAM palette
 * bitmap allocator. Each test calls sat_vdp2_palette_reset() at the
 * top so the global allocation bitmap is clean. */

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

static void test_free_reuses_the_lowest_range(void) {
    sat_vdp2_palette_reset();
    sat_vdp2_palette_range_t a, b, c, d;
    ASSERT_EQ(sat_vdp2_palette_reserve(16u, &a), SAT_OK);
    ASSERT_EQ(sat_vdp2_palette_reserve(32u, &b), SAT_OK);
    ASSERT_EQ(a.bank, 1u); ASSERT_EQ(a.code, 0u);
    ASSERT_EQ(b.bank, 1u); ASSERT_EQ(b.code, 16u);
    ASSERT_EQ(sat_vdp2_palette_free(a), SAT_OK);
    ASSERT_EQ(sat_vdp2_palette_reserve(16u, &c), SAT_OK);
    ASSERT_EQ(c.bank, 1u); ASSERT_EQ(c.code, 0u);
    ASSERT_EQ(sat_vdp2_palette_reserve(16u, &d), SAT_OK);
    ASSERT_EQ(d.bank, 1u); ASSERT_EQ(d.code, 48u);
    ASSERT_EQ(sat_vdp2_palette_free(b), SAT_OK);
    ASSERT_EQ(sat_vdp2_palette_reserve(32u, &b), SAT_OK);
    ASSERT_EQ(b.bank, 1u); ASSERT_EQ(b.code, 16u);
    /* A range that is not fully allocated is a caller error. */
    const sat_vdp2_palette_range_t bad = {1u, 0u, 65u};
    ASSERT_EQ(sat_vdp2_palette_free(bad), SAT_ERR_NOT_FOUND);
}

static void test_usage_reports_bank_counts(void) {
    sat_vdp2_palette_reset();
    sat_vdp2_palette_range_t r;
    ASSERT_EQ(sat_vdp2_palette_reserve(16u, &r), SAT_OK);
    ASSERT_EQ(sat_vdp2_palette_reserve(32u, &r), SAT_OK);
    sat_vdp2_palette_usage_t usage;
    sat_vdp2_palette_usage(&usage);
    ASSERT_EQ(usage.used_codes[0], 0u);
    ASSERT_EQ(usage.used_codes[1], 48u);
    ASSERT_EQ(usage.free_codes[1], 208u);
    ASSERT_EQ(usage.refusals, 0u);
}

int main() {
    test_reserve_returns_first_bank_with_room();
    test_reserve_rotates_to_next_bank_when_full();
    test_reserve_skips_bank_zero();
    test_invalid_count_rejected();
    test_free_reuses_the_lowest_range();
    test_usage_reports_bank_counts();
    printf("PASS: test_vdp2_palette_alloc.cpp (6 tests)\n");
    return 0;
}
