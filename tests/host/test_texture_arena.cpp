/* test_texture_arena.cpp — host tests for the VDP1 texture arena bookkeeping */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "src/hal/vdp1/texture_arena.hpp"

#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s (%ld) != %s (%ld)\n", __FILE__, __LINE__, \
            #a, (long)(a), #b, (long)(b)); \
    exit(1); } } while(0)
#define ASSERT_TRUE(x) ASSERT_EQ(!!(x), 1)

using saturn::hal::vdp1::TextureArena;

constexpr uint32_t kBase = 0x14000u;
constexpr uint32_t kLimit = 0x80000u;

static uint32_t take(TextureArena& a, uint32_t size, uint32_t align = 8u,
                     bool* recycled = nullptr) {
    uint32_t start = 0u;
    bool r = false;
    ASSERT_TRUE(a.alloc(size, align, &start, &r));
    if (recycled) *recycled = r;
    return start;
}

TEST(bump_allocation_is_contiguous_and_aligned) {
    TextureArena a;
    a.reset(kBase, kLimit);
    ASSERT_EQ(take(a, 100u), kBase);          /* rounds to 104 */
    ASSERT_EQ(a.cursor(), kBase + 104u);
    ASSERT_EQ(take(a, 8u), kBase + 104u);
    uint32_t lut = take(a, 32u, 32u);         /* 32-byte aligned LUT */
    ASSERT_EQ(lut % 32u, 0u);
    ASSERT_TRUE(lut >= kBase + 112u);
}

TEST(rejects_bad_requests_and_overflow) {
    TextureArena a;
    a.reset(kBase, kLimit);
    uint32_t start = 0u;
    bool r = false;
    ASSERT_EQ(a.alloc(0u, 8u, &start, &r), false);
    ASSERT_EQ(a.alloc(8u, 3u, &start, &r), false);       /* align not a power of 2 */
    ASSERT_EQ(a.alloc(8u, 8u, nullptr, &r), false);
    ASSERT_EQ(a.alloc(kLimit, 8u, &start, &r), false);   /* bigger than the arena */
    ASSERT_EQ(a.alloc(0xFFFFFFFFu, 8u, &start, &r), false);
    ASSERT_EQ(a.cursor(), kBase);                        /* failures change nothing */
}

TEST(freed_top_block_lowers_the_cursor) {
    TextureArena a;
    a.reset(kBase, kLimit);
    uint32_t x = take(a, 64u);
    uint32_t y = take(a, 64u);
    ASSERT_TRUE(a.free_block(y, 64u));
    ASSERT_EQ(a.cursor(), x + 64u);
    ASSERT_EQ(a.free_span_count(), 0u);
    ASSERT_TRUE(a.free_block(x, 64u));
    ASSERT_EQ(a.cursor(), kBase);
}

TEST(freed_hole_is_reused_before_bumping) {
    TextureArena a;
    a.reset(kBase, kLimit);
    uint32_t x = take(a, 64u);
    take(a, 64u);
    ASSERT_TRUE(a.free_block(x, 64u));
    const uint32_t cursor = a.cursor();
    bool recycled = false;
    ASSERT_EQ(take(a, 64u, 8u, &recycled), x);
    ASSERT_TRUE(recycled);
    ASSERT_EQ(a.cursor(), cursor);
}

TEST(smaller_request_splits_a_hole) {
    TextureArena a;
    a.reset(kBase, kLimit);
    uint32_t x = take(a, 128u);
    take(a, 8u);  /* keeps the hole off the cursor */
    ASSERT_TRUE(a.free_block(x, 128u));
    ASSERT_EQ(take(a, 32u), x);
    ASSERT_EQ(a.free_span_count(), 1u);
    ASSERT_EQ(take(a, 96u), x + 32u);
    ASSERT_EQ(a.free_span_count(), 0u);
}

TEST(adjacent_frees_coalesce) {
    TextureArena a;
    a.reset(kBase, kLimit);
    uint32_t b0 = take(a, 64u);
    uint32_t b1 = take(a, 64u);
    uint32_t b2 = take(a, 64u);
    take(a, 8u);  /* guard so nothing touches the cursor */
    ASSERT_TRUE(a.free_block(b0, 64u));
    ASSERT_TRUE(a.free_block(b2, 64u));
    ASSERT_EQ(a.free_span_count(), 2u);
    ASSERT_TRUE(a.free_block(b1, 64u));          /* bridges both */
    ASSERT_EQ(a.free_span_count(), 1u);
    ASSERT_EQ(take(a, 192u), b0);                /* one 192-byte block fits */
}

TEST(best_fit_picks_the_tightest_hole) {
    TextureArena a;
    a.reset(kBase, kLimit);
    uint32_t big = take(a, 256u);
    take(a, 8u);
    uint32_t small = take(a, 64u);
    take(a, 8u);
    ASSERT_TRUE(a.free_block(big, 256u));
    ASSERT_TRUE(a.free_block(small, 64u));
    ASSERT_EQ(take(a, 56u), small);              /* not the 256-byte hole */
}

TEST(double_free_and_foreign_spans_are_refused) {
    TextureArena a;
    a.reset(kBase, kLimit);
    uint32_t x = take(a, 64u);
    take(a, 8u);
    ASSERT_TRUE(a.free_block(x, 64u));
    ASSERT_EQ(a.free_block(x, 64u), false);          /* double free */
    ASSERT_EQ(a.free_block(x + 8u, 8u), false);      /* inside a free span */
    ASSERT_EQ(a.free_block(kBase - 8u, 8u), false);  /* before the arena */
    ASSERT_EQ(a.free_block(a.cursor(), 8u), false);  /* never allocated */
    ASSERT_EQ(a.free_block(x, 0u), false);
}

TEST(alignment_padding_is_not_lost) {
    TextureArena a;
    a.reset(kBase, kLimit);
    take(a, 8u);                                 /* cursor = kBase + 8 */
    uint32_t lut = take(a, 32u, 32u);
    ASSERT_EQ(lut % 32u, 0u);
    uint32_t before = a.available();
    /* The gap below the LUT is a free span an 8-byte upload can use. */
    ASSERT_TRUE(a.free_span_count() >= 1u || lut == kBase + 8u);
    uint32_t next = take(a, 8u);
    ASSERT_TRUE(a.available() <= before);
    (void)next;
}

TEST(streaming_forever_never_exhausts_the_arena) {
    /* The streaming-cache failure: a 32-entry LRU cache of ~2-10 KiB sprites,
     * evicted and re-uploaded for ever. With a bump-only arena this died after
     * ~(512 KiB / size) uploads. */
    TextureArena a;
    a.reset(kBase, kLimit);
    constexpr int kSlots = 32;
    uint32_t addr[kSlots] = {};
    uint32_t size[kSlots] = {};
    uint32_t seed = 12345u;
    for (int i = 0; i < kSlots; ++i) {
        seed = seed * 1664525u + 1013904223u;
        size[i] = 8u * (200u + (seed >> 16) % 1200u);   /* 1.6 KiB .. 11 KiB */
        addr[i] = take(a, size[i]);
    }
    for (int round = 0; round < 20000; ++round) {
        seed = seed * 1664525u + 1013904223u;
        const int slot = static_cast<int>((seed >> 16) % kSlots);
        ASSERT_TRUE(a.free_block(addr[slot], size[slot]));
        seed = seed * 1664525u + 1013904223u;
        size[slot] = 8u * (200u + (seed >> 16) % 1200u);
        uint32_t start = 0u;
        bool recycled = false;
        if (!a.alloc(size[slot], 8u, &start, &recycled)) {
            fprintf(stderr, "arena exhausted in round %d, %u free spans, %u leaked\n",
                    round, (unsigned)a.free_span_count(), (unsigned)a.leaked_spans());
            exit(1);
        }
        addr[slot] = start;
        /* No two live blocks may overlap. */
        for (int j = 0; j < kSlots; ++j) {
            if (j == slot) continue;
            ASSERT_TRUE(addr[slot] + size[slot] <= addr[j] || addr[j] + size[j] <= addr[slot]);
        }
        ASSERT_TRUE(start >= kBase && start + size[slot] <= kLimit);
    }
    ASSERT_EQ(a.leaked_spans(), 0u);
}

TEST(full_free_list_leaks_instead_of_corrupting) {
    TextureArena a;
    a.reset(kBase, kLimit);
    uint32_t blocks[TextureArena::kMaxFreeSpans + 4u];
    for (auto& b : blocks) {
        b = take(a, 8u);
        take(a, 8u);                              /* spacer keeps holes apart */
    }
    for (auto b : blocks) ASSERT_TRUE(a.free_block(b, 8u));
    ASSERT_EQ(a.free_span_count(), TextureArena::kMaxFreeSpans);
    ASSERT_EQ(a.leaked_spans(), 4u);
}

int main() {
    bump_allocation_is_contiguous_and_aligned();
    rejects_bad_requests_and_overflow();
    freed_top_block_lowers_the_cursor();
    freed_hole_is_reused_before_bumping();
    smaller_request_splits_a_hole();
    adjacent_frees_coalesce();
    best_fit_picks_the_tightest_hole();
    double_free_and_foreign_spans_are_refused();
    alignment_padding_is_not_lost();
    streaming_forever_never_exhausts_the_arena();
    full_free_list_leaks_instead_of_corrupting();
    puts("texture_arena: OK");
    return 0;
}
