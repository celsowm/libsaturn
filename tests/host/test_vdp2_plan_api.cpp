/* test_vdp2_plan_api.cpp — host tests for the transactional resource
 * plan's C API (src/hal/vdp2/plan.cpp) against the real VRAM byte free
 * list (src/hal/vdp2/allocator.cpp). Both are reset between cases with
 * sat_vdp2_plan_reset + sat_vdp2_vram_alloc_reset, exactly like an
 * example boot. */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/vdp2_alloc.h"
#include "saturn/vdp2_layers.h"    /* SAT_VDP2_BANK_* masks */

#define ASSERT_EQ(a, b) do { auto __ae = (a); auto __be = (b); \
    if (__ae != __be) { \
    fprintf(stderr, "FAIL %s:%d: %s=%lld != %s=%lld\n", __FILE__, __LINE__, \
        #a, (long long)__ae, #b, (long long)__be); \
    exit(1); } } while (0)
#define ASSERT_TRUE(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); } } while (0)

static void reset_all(void) {
    sat_vdp2_vram_alloc_reset();
    sat_vdp2_palette_reset();
    sat_vdp2_plan_reset();
}

static void test_rollback_restores_exact_free_space(void) {
    reset_all();
    const uint32_t before = sat_vdp2_vram_largest_free();
    uint32_t a = 0u, b = 0u;
    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_OK);
    /* 0x30000 characters cross the A0/A1 edge: the layer manager times
     * both banks, so the plan places them across the edge. */
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_CHARACTERS, 0x30000u, 2u, &a), SAT_OK);
    ASSERT_EQ(a, 0u);
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_LINE_SCROLL, 0x1000u, 2u, &b), SAT_OK);
    ASSERT_TRUE(b != SAT_VDP2_VRAM_NO_BASE);
    ASSERT_TRUE(sat_vdp2_vram_largest_free() < before);
    ASSERT_EQ(sat_vdp2_plan_rollback(), SAT_OK);
    ASSERT_EQ(sat_vdp2_vram_largest_free(), before);
    /* Nothing is committed: the same space can be taken again. */
    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_CHARACTERS, 0x30000u, 2u, &a), SAT_OK);
    ASSERT_EQ(a, 0u);           /* first fit from the bottom, unchanged */
    ASSERT_EQ(sat_vdp2_plan_rollback(), SAT_OK);
}

static void test_commit_keeps_and_release_restores(void) {
    reset_all();
    const uint32_t before = sat_vdp2_vram_largest_free();
    uint32_t a = 0u;
    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_PATTERN_NAMES, 0x2000u,
                                    0x2000u, &a), SAT_OK);
    ASSERT_EQ(a, 0u);           /* 0x2000-byte plane alignment at 0 */
    ASSERT_EQ(sat_vdp2_plan_commit(), SAT_OK);
    ASSERT_EQ(sat_vdp2_vram_largest_free(), before - 0x2000u);
    ASSERT_EQ(sat_vdp2_plan_release(a), SAT_OK);
    ASSERT_EQ(sat_vdp2_vram_largest_free(), before);
    /* Releasing anything else is refused. */
    ASSERT_EQ(sat_vdp2_plan_release(a), SAT_ERR_NOT_FOUND);
    /* release_all on an empty plan is fine. */
    ASSERT_EQ(sat_vdp2_plan_release_all(), SAT_OK);
}

static void test_reserve_at_conflicts_leave_nothing_behind(void) {
    reset_all();
    const uint32_t before = sat_vdp2_vram_largest_free();
    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_PARAMS,
                                       0x00004u, 0x60u), SAT_OK);
    /* The exact same range: no longer free. */
    ASSERT_EQ(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_PARAMS,
                                       0x00004u, 0x60u), SAT_ERR_CAPACITY);
    /* Overlapping: refused. */
    ASSERT_EQ(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_SCRATCH,
                                       0x00020u, 0x20u), SAT_ERR_CAPACITY);
    /* Misaligned base for the kind: refused before touching the free list. */
    ASSERT_EQ(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_WINDOW_TABLE,
                                        0x00102u, 0x10u), SAT_ERR_INVALID_ARG);
    /* A refusal changes nothing: the tail block lost the slack below the
     * base plus the range itself (largest block 0x80000 - 0x64). */
    ASSERT_EQ(sat_vdp2_vram_largest_free(), before - 0x64u);
    ASSERT_EQ(sat_vdp2_plan_rollback(), SAT_OK);
    ASSERT_EQ(sat_vdp2_vram_largest_free(), before);
}

static void test_kind_minimums_reach_the_api(void) {
    reset_all();
    uint32_t base = 0u;
    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_OK);
    /* Below the kind minimum (4 bytes) is invalid and places nothing. */
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_COEFFICIENTS, 0x40u, 2u, &base),
              SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_vdp2_vram_largest_free(), 0x80000u);
    /* A bitmap is placed 0x20000-aligned; exactly two fit in VRAM. */
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_BITMAP, 0x40000u, 0x20000u, &base),
              SAT_OK);
    ASSERT_EQ(base, 0x00000u);
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_BITMAP, 0x40000u, 0x20000u, &base),
              SAT_OK);
    ASSERT_EQ(base, 0x40000u);
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_BITMAP, 0x40000u, 0x20000u, &base),
              SAT_ERR_CAPACITY);
    /* A zero-byte reservation is invalid. */
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_SCRATCH, 0u, 2u, &base),
              SAT_ERR_INVALID_ARG);
    /* Bad kind. */
    ASSERT_EQ(sat_vdp2_plan_reserve(static_cast<sat_vdp2_reserve_kind_t>(77u),
                                    0x10u, 2u, &base), SAT_ERR_INVALID_ARG);
    /* No transaction: everything after a rollback is refused. */
    ASSERT_EQ(sat_vdp2_plan_rollback(), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_SCRATCH, 0x10u, 2u, &base),
              SAT_ERR_NOT_INITIALIZED);
    /* A second begin while open is busy. */
    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_ERR_BUSY);
    ASSERT_EQ(sat_vdp2_plan_rollback(), SAT_OK);
}

static void test_bank_claims_route_reservations(void) {
    reset_all();
    uint32_t base = 0u;
    /* Claim A0 for rotation image data, then take an NBG pattern-name
     * table: it must be routed to A1, not A0. */
    ASSERT_EQ(sat_vdp2_plan_claim_banks(SAT_VDP2_BANK_A0), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_PATTERN_NAMES, 0x2000u,
                                    0x2000u, &base), SAT_OK);
    ASSERT_EQ(base, 0x20000u);
    /* Rotation parameter tables follow the same rule. */
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_ROTATION_PARAMS, 0x60u, 4u, &base),
              SAT_OK);
    ASSERT_EQ(base, 0x22000u);
    /* A line scroll table may use the claimed bank (lowest first fit). */
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_LINE_SCROLL, 0x400u, 2u, &base),
              SAT_OK);
    ASSERT_EQ(base, 0x00000u);
    /* Characters cross edges between unclaimed banks: A1's remaining
     * hole is packed after the rotation table and the range spans the
     * A1/B0 edge at 0x22060 — never the claimed A0. */
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_CHARACTERS, 0x30000u, 2u, &base),
              SAT_OK);
    ASSERT_EQ(base, 0x22060u);
    ASSERT_EQ(sat_vdp2_plan_commit(), SAT_OK);
    /* Claiming a bank that holds committed NBG/rotation reservations is
     * refused: A1 (names, rotation parameters), B0/B1 (the characters
     * that span them). */
    ASSERT_EQ(sat_vdp2_plan_claim_banks(SAT_VDP2_BANK_A1), SAT_ERR_BUSY);
    ASSERT_EQ(sat_vdp2_plan_claim_banks(SAT_VDP2_BANK_B0), SAT_ERR_BUSY);
    /* B1 is untouched by the committed ranges (the characters end at
     * 0x52060), so it can be claimed and released. */
    ASSERT_EQ(sat_vdp2_plan_claim_banks(SAT_VDP2_BANK_B1), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_release_banks(SAT_VDP2_BANK_B1), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_claim_banks(0x00u), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_vdp2_plan_claim_banks(0xF0u), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_vdp2_plan_release_all(), SAT_OK);
    /* With the reservations gone every bank can be claimed. */
    ASSERT_EQ(sat_vdp2_plan_claim_banks(SAT_VDP2_BANK_A1), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_release_banks(SAT_VDP2_BANK_A1), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_claim_banks(SAT_VDP2_BANK_A0), SAT_OK);   /* idempotent */
    ASSERT_EQ(sat_vdp2_plan_release_banks(SAT_VDP2_BANK_A0), SAT_OK);
}

static void test_fragmentation_is_reported_not_hidden(void) {
    reset_all();
    uint32_t a = 0u, b = 0u, c = 0u;
    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_SCRATCH, 0x10000u, 2u, &a), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_SCRATCH, 0x10000u, 2u, &b), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_SCRATCH, 0x10000u, 2u, &c), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_commit(), SAT_OK);
    /* Free the middle block: a 0x10000 hole at 0x10000. */
    ASSERT_EQ(sat_vdp2_plan_release(b), SAT_OK);
    sat_vdp2_plan_usage_t usage;
    sat_vdp2_plan_usage(&usage);
    ASSERT_EQ(usage.committed_count, 2u);
    ASSERT_EQ(usage.reserved_bytes, 0x20000u);
    /* The hole fits a smaller request ... */
    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_OK);
    uint32_t small = 0u;
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_SCRATCH, 0x8000u, 2u, &small), SAT_OK);
    ASSERT_EQ(small, 0x10000u);
    ASSERT_EQ(sat_vdp2_plan_rollback(), SAT_OK);
    /* ... but a request larger than the biggest hole is refused even
     * though the total free space (0x60000) would allow it: first fit
     * reports the fragmentation as CAPACITY and stages nothing. */
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_SCRATCH, 0x58000u, 2u, &small),
              SAT_ERR_CAPACITY);
    sat_vdp2_plan_usage(&usage);
    ASSERT_EQ(usage.staged_count, 0u);
    ASSERT_EQ(usage.last_status, SAT_ERR_CAPACITY);
    ASSERT_TRUE(usage.refusals >= 1u);
    /* The largest free block is the tail (0x50000), not the total. */
    ASSERT_EQ(sat_vdp2_vram_largest_free(), 0x50000u);
    /* Per bank: A0 keeps a 0x10000 hole (a + released b), A1 keeps a
     * 0x10000 hole above c. */
    ASSERT_EQ(usage.free_bytes_per_bank[0], 0x10000u);
    ASSERT_EQ(usage.free_bytes_per_bank[1], 0x10000u);
    ASSERT_EQ(sat_vdp2_plan_release_all(), SAT_OK);
}

static void test_raw_alloc_and_plan_coexist(void) {
    reset_all();
    /* A raw free-list allocation is respected by the plan: bytes the
     * plan does not own are still not handed out twice. */
    const uint32_t raw = sat_vdp2_vram_alloc(0x8000u, 1u);
    ASSERT_EQ(raw, 0x00000u);
    uint32_t base = 0u;
    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_SCRATCH, 0x00000u, 0x10u),
              SAT_ERR_CAPACITY);          /* the raw block owns them */
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_SCRATCH, 0x10u, 2u, &base),
              SAT_OK);
    ASSERT_EQ(base, 0x8000u);             /* placed after the raw block */
    ASSERT_EQ(sat_vdp2_plan_rollback(), SAT_OK);
    ASSERT_EQ(sat_vdp2_vram_free(raw), SAT_OK);
}

/* RBG0's own image data only lives inside banks claimed for rotation;
 * claims and image data protect each other (p06_20, p06_40). */
static void test_rotation_image_routing(void) {
    reset_all();
    uint32_t base = 0u;
    /* Without a claim there is nowhere rotation names may live. */
    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_ROTATION_NAMES, 0x8000u, 0x2000u, &base),
              SAT_ERR_BUSY);
    ASSERT_EQ(sat_vdp2_plan_rollback(), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_claim_banks(SAT_VDP2_BANK_A0 | SAT_VDP2_BANK_A1), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_OK);
    /* Routed into the first claimed run. */
    ASSERT_EQ(sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_ROTATION_NAMES, 0x8000u, 0x2000u, &base),
              SAT_OK);
    ASSERT_EQ(base, 0x00000u);
    /* Character windows stay 0x8000-aligned inside A1. */
    ASSERT_EQ(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_CHARS, 0x28000u, 0x1000u), SAT_OK);
    /* NBG kinds are now refused in the claimed banks. */
    ASSERT_EQ(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_PATTERN_NAMES, 0x00000u, 0x2000u),
              SAT_ERR_BUSY);
    /* Releasing a bank with live rotation image data is refused. */
    ASSERT_EQ(sat_vdp2_plan_release_banks(SAT_VDP2_BANK_A0), SAT_ERR_BUSY);
    ASSERT_EQ(sat_vdp2_plan_commit(), SAT_OK);
    /* With the data released the banks go free again. */
    ASSERT_EQ(sat_vdp2_plan_release(base), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_release(0x28000u), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_release_banks(SAT_VDP2_BANK_A0 | SAT_VDP2_BANK_A1), SAT_OK);
}

/* CRAM ranges join the same transaction: rollback restores the allocator
 * bitmap, commit keeps the range, release frees it for immediate reuse. */
static void test_palette_transaction_rolls_back_and_reuses(void) {
    reset_all();
    sat_vdp2_palette_range_t first{}, again{}, reused{};
    ASSERT_EQ(sat_vdp2_plan_palette_reserve(16u, &first), SAT_ERR_NOT_INITIALIZED);
    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_palette_reserve(16u, &first), SAT_OK);
    ASSERT_EQ(first.bank, 1u);
    ASSERT_EQ(first.code, 0u);
    ASSERT_EQ(first.count, 16u);
    ASSERT_EQ(sat_vdp2_plan_palette_release(first), SAT_ERR_BUSY);
    ASSERT_EQ(sat_vdp2_plan_rollback(), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_palette_reserve(16u, &again), SAT_OK);
    ASSERT_EQ(again.bank, first.bank);
    ASSERT_EQ(again.code, first.code);
    ASSERT_EQ(sat_vdp2_plan_commit(), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_palette_release(again), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_palette_release(again), SAT_ERR_NOT_FOUND);
    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_OK);
    ASSERT_EQ(sat_vdp2_plan_palette_reserve(16u, &reused), SAT_OK);
    ASSERT_EQ(reused.bank, first.bank);
    ASSERT_EQ(reused.code, first.code);
    ASSERT_EQ(sat_vdp2_plan_rollback(), SAT_OK);
    sat_vdp2_palette_usage_t usage;
    sat_vdp2_palette_usage(&usage);
    ASSERT_EQ(usage.used_codes[1], 0u);
    ASSERT_EQ(usage.free_codes[1], 256u);
}

/* The palette ledger has eight slots; the allocator has seven banks. Fill
 * both, prove the refusal leaves the staged ranges intact, then roll back. */
static void test_palette_capacity_and_ledger(void) {
    reset_all();
    sat_vdp2_palette_range_t r{};
    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_OK);
    for (uint16_t bank = 1u; bank <= 7u; ++bank) {
        ASSERT_EQ(sat_vdp2_plan_palette_reserve(256u, &r), SAT_OK);
        ASSERT_EQ(r.bank, bank);
        ASSERT_EQ(r.code, 0u);
        ASSERT_EQ(r.count, 256u);
    }
    ASSERT_EQ(sat_vdp2_plan_palette_reserve(1u, &r), SAT_ERR_CAPACITY);
    ASSERT_EQ(sat_vdp2_plan_rollback(), SAT_OK);
    sat_vdp2_palette_usage_t usage;
    sat_vdp2_palette_usage(&usage);
    for (uint16_t bank = 1u; bank <= 7u; ++bank) {
        ASSERT_EQ(usage.used_codes[bank], 0u);
        ASSERT_EQ(usage.free_codes[bank], 256u);
    }

    ASSERT_EQ(sat_vdp2_plan_begin(), SAT_OK);
    for (uint16_t i = 0u; i < 8u; ++i) {
        ASSERT_EQ(sat_vdp2_plan_palette_reserve(1u, &r), SAT_OK);
    }
    ASSERT_EQ(sat_vdp2_plan_palette_reserve(1u, &r), SAT_ERR_CAPACITY);
    ASSERT_EQ(sat_vdp2_plan_rollback(), SAT_OK);
}

int main(void) {
    test_rollback_restores_exact_free_space();
    test_commit_keeps_and_release_restores();
    test_reserve_at_conflicts_leave_nothing_behind();
    test_kind_minimums_reach_the_api();
    test_bank_claims_route_reservations();
    test_rotation_image_routing();
    test_fragmentation_is_reported_not_hidden();
    test_raw_alloc_and_plan_coexist();
    test_palette_transaction_rolls_back_and_reuses();
    test_palette_capacity_and_ledger();
    printf("test_vdp2_plan_api: all tests passed\n");
    return 0;
}
