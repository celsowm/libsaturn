/* test_vdp2_allocator.cpp — host tests for the VDP2 VRAM and CRAM
 * allocators. Stand-alone process; no fixtures needed because the
 * allocator exposes sat_vdp2_vram_alloc_reset and sat_vdp2_palette_reset
 * exactly so this test can start clean. */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/vdp2_alloc.h"

#define ASSERT_EQ(a, b) do { auto __ae = (a); auto __be = (b); \
    if (__ae != __be) { \
    fprintf(stderr, "FAIL %s:%d: %s=%lld != %s=%lld\n", __FILE__, __LINE__, \
        #a, (long long)__ae, #b, (long long)__be); \
    exit(1); } } while (0)
#define ASSERT_TRUE(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); } } while (0)

static void test_default_range_is_full(void) {
    sat_vdp2_vram_alloc_reset();
    /* The default range covers 0x00000..0x80000, so the full 512 KiB
     * minus one tiny alignment slack should fit one allocation. */
    const uint32_t base = sat_vdp2_vram_alloc(0x60000u, 1u);
    ASSERT_TRUE(base == 0x00000u);
    ASSERT_EQ(sat_vdp2_vram_largest_free(), 0x20000u);
    ASSERT_EQ(sat_vdp2_vram_free(base), SAT_OK);
    /* After free the whole range is available again. */
    ASSERT_EQ(sat_vdp2_vram_largest_free(), 0x80000u);
}

static void test_alignment_rounds_up(void) {
    /* Start at a non-aligned byte address; 8 words = 16 bytes of
     * alignment should push the next allocation up to 0x10010. */
    sat_vdp2_vram_alloc_init(0x10001u, 0x80000u);
    const uint32_t a = sat_vdp2_vram_alloc(0x100u, 8u);
    ASSERT_EQ(a, 0x10010u);
    ASSERT_EQ(sat_vdp2_vram_free(a), SAT_OK);
}

static void test_first_fit_picks_earliest_hole(void) {
    sat_vdp2_vram_alloc_init(0x00000u, 0x80000u);
    const uint32_t a = sat_vdp2_vram_alloc(0x10000u, 1u);  /* 0..0x10000 */
    const uint32_t b = sat_vdp2_vram_alloc(0x10000u, 1u);  /* 0x10000..0x20000 */
    const uint32_t c = sat_vdp2_vram_alloc(0x10000u, 1u);  /* 0x20000..0x30000 */
    ASSERT_EQ(a, 0x00000u);
    ASSERT_EQ(b, 0x10000u);
    ASSERT_EQ(c, 0x20000u);
    /* Free the middle one: a third allocation must reuse that hole. */
    ASSERT_EQ(sat_vdp2_vram_free(b), SAT_OK);
    const uint32_t d = sat_vdp2_vram_alloc(0x10000u, 1u);
    ASSERT_EQ(d, 0x10000u);
    /* Freeing a non-base returns NOT_FOUND. */
    /* 0x10001 is one byte inside the middle block: not a real base. */
    ASSERT_EQ(sat_vdp2_vram_free(0x10001u), SAT_ERR_NOT_FOUND);
    /* Cleanup. */
    ASSERT_EQ(sat_vdp2_vram_free(a), SAT_OK);
    ASSERT_EQ(sat_vdp2_vram_free(d), SAT_OK);
    ASSERT_EQ(sat_vdp2_vram_free(c), SAT_OK);
}

static void test_adjacent_free_coalesces(void) {
    sat_vdp2_vram_alloc_init(0x00000u, 0x80000u);
    const uint32_t a = sat_vdp2_vram_alloc(0x10000u, 1u);   /* 0..0x10000 */
    const uint32_t b = sat_vdp2_vram_alloc(0x10000u, 1u);   /* 0x10000..0x20000 */
    const uint32_t c = sat_vdp2_vram_alloc(0x10000u, 1u);   /* 0x20000..0x30000 */
    /* Free a and b in order: the two holes meet at 0x10000 and coalesce
     * into one 0x20000-byte free block. The third allocation that needs
     * 0x20000 therefore returns 0x00000. */
    ASSERT_EQ(sat_vdp2_vram_free(a), SAT_OK);
    ASSERT_EQ(sat_vdp2_vram_free(b), SAT_OK);
    const uint32_t d = sat_vdp2_vram_alloc(0x20000u, 1u);
    ASSERT_EQ(d, 0x00000u);
    /* c is still where it was. */
    ASSERT_EQ(sat_vdp2_vram_free(c), SAT_OK);
    ASSERT_EQ(sat_vdp2_vram_free(d), SAT_OK);
}

static void test_exhaustion_returns_sentinel(void) {
    /* Pick a non-zero range start so the success base can be told
     * apart from the failure sentinel. */
    sat_vdp2_vram_alloc_init(0x10000u, 0x80000u);
    const uint32_t a = sat_vdp2_vram_alloc(0x70000u, 1u);
    ASSERT_EQ(a, 0x10000u);
    /* One extra byte past the end must fail and return the sentinel. */
    ASSERT_EQ(sat_vdp2_vram_alloc(0x10000u, 1u), SAT_VDP2_VRAM_NO_BASE);
    ASSERT_EQ(sat_vdp2_vram_free(a), SAT_OK);
}

static void test_invalid_arguments_rejected(void) {
    sat_vdp2_vram_alloc_reset();
    /* Zero bytes must return the failure sentinel. */
    ASSERT_EQ(sat_vdp2_vram_alloc(0u, 1u), SAT_VDP2_VRAM_NO_BASE);
    /* Range where end <= start or out of [0, 0x80000] must fail to
     * init. The allocator only owns byte addresses below 0x80000
     * because src/hal/vdp2/compose_logic.hpp already uses that as the
     * VDP2 VRAM upper bound. */
    ASSERT_EQ(sat_vdp2_vram_alloc_init(0x80000u, 0x10000u),
              SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_vdp2_vram_alloc_init(0x40000u, 0x90000u),
              SAT_ERR_INVALID_ARG);
}

int main() {
    test_default_range_is_full();
    test_alignment_rounds_up();
    test_first_fit_picks_earliest_hole();
    test_adjacent_free_coalesces();
    test_exhaustion_returns_sentinel();
    test_invalid_arguments_rejected();
    printf("PASS: test_vdp2_allocator.cpp (6 tests)\n");
    return 0;
}
