#ifndef SATURN_HAL_VDP1_TEXTURE_ARENA_HPP
#define SATURN_HAL_VDP1_TEXTURE_ARENA_HPP

/* Address-space bookkeeping for the VDP1 texture arena. Pure: it hands out and
 * takes back [start, start+size) spans of VRAM offsets and never touches VRAM,
 * so tests/host/test_texture_arena.cpp links it directly.
 *
 * The arena used to be a bump cursor that only ever advanced, so
 * sat_texture_destroy() gave the texture slot back but not its VRAM. A program
 * that streams textures through a small cache (a sprite cache that evicts and
 * re-uploads sprites constantly) therefore filled the 512 KiB VRAM after a few
 * hundred uploads and every later upload failed with SAT_ERR_CAPACITY.
 *
 * Freed spans go on a short address-ordered free list and are coalesced with
 * their neighbours; a span that ends at the bump cursor lowers the cursor
 * instead, so strictly LIFO use costs nothing. Allocation is best-fit from the
 * free list, then bump. If the free list is ever full the freed span is leaked
 * (the old behaviour) rather than corrupting anything. */

#include <stdint.h>

namespace saturn::hal::vdp1 {

class TextureArena {
public:
    static constexpr uint8_t kMaxFreeSpans = 48u;

    /* constexpr, so a global arena is constant-initialised: libsaturn runs no
     * static constructors. */
    constexpr TextureArena(uint32_t base = 0u, uint32_t limit = 0u)
        : base_(base), limit_(limit), cursor_(base) {}

    void reset(uint32_t base, uint32_t limit) {
        base_ = base;
        limit_ = limit;
        cursor_ = base;
        free_count_ = 0u;
        leaked_spans_ = 0u;
    }

    /* Takes `size` bytes aligned to `align` (a power of two). `*recycled` is
     * true when the span lies below the high-water mark, i.e. it was used
     * before and the VDP1 may still be reading its old contents. */
    bool alloc(uint32_t size, uint32_t align, uint32_t* out_start, bool* out_recycled) {
        if (size == 0u || align == 0u || (align & (align - 1u)) != 0u ||
            out_start == nullptr || out_recycled == nullptr) {
            return false;
        }
        /* Checked before rounding: round_up wraps a near-4 GiB size to 0. */
        if (size > limit_ - base_) return false;
        size = round_up(size, kGranule);

        uint8_t best = kMaxFreeSpans;
        uint32_t best_waste = 0xFFFFFFFFu;
        for (uint8_t i = 0u; i < free_count_; ++i) {
            const uint32_t start = round_up(spans_[i].start, align);
            const uint32_t end = spans_[i].start + spans_[i].size;
            if (start >= end || end - start < size) continue;
            const uint32_t waste = (end - start) - size;
            if (waste < best_waste) {
                best_waste = waste;
                best = i;
            }
        }
        if (best != kMaxFreeSpans) {
            const uint32_t span_start = spans_[best].start;
            const uint32_t span_end = span_start + spans_[best].size;
            const uint32_t start = round_up(span_start, align);
            remove_span(best);
            /* The pieces before and after the block stay free. Re-adding can
             * only fail by leaking a few bytes; that is the safe direction. */
            if (start > span_start) insert_span(span_start, start - span_start);
            if (start + size < span_end) insert_span(start + size, span_end - (start + size));
            *out_start = start;
            *out_recycled = true;
            return true;
        }

        const uint32_t start = round_up(cursor_, align);
        if (start < cursor_ || start + size < start || start + size > limit_) return false;
        /* Alignment padding below the block is returned to the free list. */
        if (start > cursor_) insert_span(cursor_, start - cursor_);
        cursor_ = start + size;
        *out_start = start;
        *out_recycled = false;
        return true;
    }

    /* Returns a span obtained from alloc(). False when it is not a span inside
     * the arena, or overlaps free space (a double free). */
    bool free_block(uint32_t start, uint32_t size) {
        if (size == 0u || size > limit_ - base_) return false;
        size = round_up(size, kGranule);
        if (start < base_ || start + size < start || start + size > cursor_) return false;
        for (uint8_t i = 0u; i < free_count_; ++i) {
            const uint32_t s = spans_[i].start;
            const uint32_t e = s + spans_[i].size;
            if (start < e && s < start + size) return false;
        }
        insert_span(start, size);
        trim_cursor();
        return true;
    }

    /* End of the highest span ever handed out and not yet trimmed. Offsets at
     * or above it have never been used since reset. */
    uint32_t cursor() const { return cursor_; }
    uint32_t base() const { return base_; }
    uint32_t limit() const { return limit_; }

    /* Bytes that a fresh alloc could still use. */
    uint32_t available() const {
        uint32_t total = limit_ - cursor_;
        for (uint8_t i = 0u; i < free_count_; ++i) total += spans_[i].size;
        return total;
    }
    uint8_t free_span_count() const { return free_count_; }
    /* Spans that could not be recorded because the free list was full. */
    uint32_t leaked_spans() const { return leaked_spans_; }

private:
    struct Span {
        uint32_t start;
        uint32_t size;
    };

    /* VDP1 character addresses count in 8-byte units. */
    static constexpr uint32_t kGranule = 8u;

    static uint32_t round_up(uint32_t v, uint32_t a) { return (v + (a - 1u)) & ~(a - 1u); }

    void remove_span(uint8_t index) {
        for (uint8_t i = index; i + 1u < free_count_; ++i) spans_[i] = spans_[i + 1u];
        --free_count_;
    }

    /* Keeps spans_ sorted by address and merges touching spans. */
    void insert_span(uint32_t start, uint32_t size) {
        if (size == 0u) return;
        uint8_t pos = 0u;
        while (pos < free_count_ && spans_[pos].start < start) ++pos;

        const bool merge_prev = pos > 0u &&
            spans_[pos - 1u].start + spans_[pos - 1u].size == start;
        const bool merge_next = pos < free_count_ &&
            start + size == spans_[pos].start;

        if (merge_prev && merge_next) {
            spans_[pos - 1u].size += size + spans_[pos].size;
            remove_span(pos);
        } else if (merge_prev) {
            spans_[pos - 1u].size += size;
        } else if (merge_next) {
            spans_[pos].start = start;
            spans_[pos].size += size;
        } else if (free_count_ < kMaxFreeSpans) {
            for (uint8_t i = free_count_; i > pos; --i) spans_[i] = spans_[i - 1u];
            spans_[pos] = {start, size};
            ++free_count_;
        } else {
            ++leaked_spans_;
        }
    }

    /* A free span touching the cursor is just unused arena again. */
    void trim_cursor() {
        while (free_count_ > 0u &&
               spans_[free_count_ - 1u].start + spans_[free_count_ - 1u].size == cursor_) {
            cursor_ = spans_[free_count_ - 1u].start;
            --free_count_;
        }
    }

    uint32_t base_;
    uint32_t limit_;
    uint32_t cursor_;
    Span spans_[kMaxFreeSpans] = {};
    uint8_t free_count_ = 0u;
    uint32_t leaked_spans_ = 0u;
};

}  /* namespace saturn::hal::vdp1 */

#endif /* SATURN_HAL_VDP1_TEXTURE_ARENA_HPP */
