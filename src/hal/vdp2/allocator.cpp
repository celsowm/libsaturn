/* VDP2 VRAM and CRAM allocators.
 *
 * The VRAM allocator is a single-process, first-fit free-list with
 * alignment-driven splitting. The CRAM allocator is a bank-rotating
 * ring over banks 1..7 (bank 0 is the default VDP1 palette, never
 * allocated). The reason both exist is the same: examples used to
 * hand-pick the same byte address / palette bank and only one could
 * win. The allocate/free pair centralises the decision.
 *
 * Both allocators are intentionally single-threaded. Examples run
 * on the SH-2 master and the dual_sh2 contract excludes the slave
 * from these. A future cleanup may add mutex guards; today's call
 * sites do not need them.
 */
#include <stdint.h>

#include "saturn/vdp2_alloc.h"

#define SAT_VDP2_VRAM_DEFAULT_START 0x00000u
#define SAT_VDP2_VRAM_DEFAULT_END   0x80000u

#define SAT_VDP2_VRAM_MAX_BLOCKS 32u

extern "C" {

/* Free-block list entries. base is the inclusive byte address; size
 * is the byte length. The state field separates empty slots, free
 * blocks (available to alloc) and allocated blocks (need a matching
 * free). The list is sparse; only the live slots carry data. */
enum {
    SLOT_EMPTY     = 0u,
    SLOT_FREE      = 1u,
    SLOT_ALLOCATED = 2u,
};

typedef struct {
    uint32_t base;
    uint32_t size;
    uint8_t  state;
} vram_block_t;

static vram_block_t g_vram_blocks[SAT_VDP2_VRAM_MAX_BLOCKS];
static uint32_t g_vram_start = SAT_VDP2_VRAM_DEFAULT_START;
static uint32_t g_vram_end   = SAT_VDP2_VRAM_DEFAULT_END;
static uint8_t  g_vram_inited = 0u;

static void vram_reset_impl(void) {
    for (uint32_t i = 0u; i < SAT_VDP2_VRAM_MAX_BLOCKS; ++i) {
        g_vram_blocks[i].base = 0u;
        g_vram_blocks[i].size = 0u;
        g_vram_blocks[i].state = SLOT_EMPTY;
    }
    g_vram_blocks[0].base = g_vram_start;
    g_vram_blocks[0].size = g_vram_end - g_vram_start;
    g_vram_blocks[0].state = SLOT_FREE;
}

/* Round `addr` up to the next `alignment_words` word boundary. A
 * request of 0 or 1 word means no alignment. */
static uint32_t vram_align_up(uint32_t addr, uint16_t alignment_words) {
    if (alignment_words <= 1u) return addr;
    const uint32_t mask = (uint32_t)alignment_words * 2u - 1u;
    return (addr + mask) & ~mask;
}

/* Find a free slot to host a new block. Returns NULL if the table
 * is full (a fatal misuse; examples never approach this limit). */
static vram_block_t* vram_slot_alloc(void) {
    for (uint32_t i = 0u; i < SAT_VDP2_VRAM_MAX_BLOCKS; ++i) {
        if (g_vram_blocks[i].state == SLOT_EMPTY) {
            return &g_vram_blocks[i];
        }
    }
    return nullptr;
}

/* Split `block` (a free block) at `split_at`. The lower portion
 * keeps the same address; the upper portion becomes a new free
 * block in a fresh slot. The lower block's state is set to `lower_state`
 * (typically SLOT_ALLOCATED or kept as SLOT_FREE for alignment). */
static void vram_split_block(vram_block_t* block, uint32_t split_at,
                              uint8_t lower_state) {
    if (split_at == 0u) {
        block->state = lower_state;
        return;
    }
    if (block->size <= split_at) {
        /* Asked to split at the end: nothing to do, lower keeps the
         * whole thing. */
        block->state = lower_state;
        return;
    }
    vram_block_t* upper = vram_slot_alloc();
    if (upper == nullptr) {
        /* No slot for the upper piece: shrink the lower piece and
         * forget the slack. */
        block->size = split_at;
        block->state = lower_state;
        return;
    }
    upper->base = block->base + split_at;
    upper->size = block->size - split_at;
    upper->state = SLOT_FREE;
    block->size = split_at;
    block->state = lower_state;
}

/* Find a free block big enough to satisfy `bytes` from the alignment
 * point onward. Returns the block (and via *out_aligned_out, the
 * actual byte-aligned base that alloc would return). */
static vram_block_t* vram_find_fit(uint32_t bytes, uint16_t alignment_words,
                                    uint32_t* out_aligned) {
    for (uint32_t i = 0u; i < SAT_VDP2_VRAM_MAX_BLOCKS; ++i) {
        if (g_vram_blocks[i].state != SLOT_FREE) continue;
        const uint32_t aligned = vram_align_up(g_vram_blocks[i].base, alignment_words);
        const uint32_t slack = aligned - g_vram_blocks[i].base;
        if (g_vram_blocks[i].size < slack + bytes) continue;
        *out_aligned = aligned;
        return &g_vram_blocks[i];
    }
    return nullptr;
}

sat_result_t sat_vdp2_vram_alloc_init(uint32_t start_byte, uint32_t end_byte) {
    if (end_byte <= start_byte) return SAT_ERR_INVALID_ARG;
    if (start_byte > 0x80000u || end_byte > 0x80000u) return SAT_ERR_INVALID_ARG;
    g_vram_start = start_byte;
    g_vram_end = end_byte;
    vram_reset_impl();
    g_vram_inited = 1u;
    return SAT_OK;
}

void sat_vdp2_vram_alloc_reset(void) {
    if (!g_vram_inited) {
        g_vram_start = SAT_VDP2_VRAM_DEFAULT_START;
        g_vram_end   = SAT_VDP2_VRAM_DEFAULT_END;
        g_vram_inited = 1u;
    }
    vram_reset_impl();
}

uint32_t sat_vdp2_vram_alloc(uint32_t bytes, uint16_t alignment_words) {
    /* Any successful allocation that lands at 0xFFFFFFFF is reserved
     * as the failure sentinel so callers can tell exhaustion from a
     * valid base of 0 (legal when the configured range starts there). */
    if (bytes == 0u) return SAT_VDP2_VRAM_NO_BASE;
    if (!g_vram_inited) {
        sat_vdp2_vram_alloc_init(SAT_VDP2_VRAM_DEFAULT_START,
                                  SAT_VDP2_VRAM_DEFAULT_END);
    }
    uint32_t aligned = 0u;
    vram_block_t* block = vram_find_fit(bytes, alignment_words, &aligned);
    if (block == nullptr) return SAT_VDP2_VRAM_NO_BASE;  /* CAPACITY */
    /* The chosen base must not be the failure sentinel. Refuse early
     * to keep the post-split allocation routine simple. */
    if (aligned == SAT_VDP2_VRAM_NO_BASE) return SAT_VDP2_VRAM_NO_BASE;
    const uint32_t slack = aligned - block->base;
    /* Carve off any alignment slack as a brand-new free block. We need
     * to grow the block downward to fit the slack, then split. */
    if (slack > 0u) {
        /* Find a free slot for the slack piece. */
        vram_block_t* slack_block = vram_slot_alloc();
        if (slack_block != nullptr) {
            slack_block->base = block->base;
            slack_block->size = slack;
            slack_block->state = SLOT_FREE;
            block->base = aligned;
            block->size -= slack;
        } else {
            /* No room for the slack piece: lose the slack bytes by
             * shrinking the original block but keep the alignment. */
            block->size = slack + bytes;
            if (block->size <= bytes) return 0u;  /* impossibly degenerate */
        }
    }
    /* Now block->base == aligned; split off the allocated portion. */
    vram_split_block(block, bytes, SLOT_ALLOCATED);
    return block->base;
}

sat_result_t sat_vdp2_vram_free(uint32_t base_byte) {
    /* Find the allocated block at this base. */
    vram_block_t* target = nullptr;
    for (uint32_t i = 0u; i < SAT_VDP2_VRAM_MAX_BLOCKS; ++i) {
        if (g_vram_blocks[i].state == SLOT_ALLOCATED
            && g_vram_blocks[i].base == base_byte) {
            target = &g_vram_blocks[i];
            break;
        }
    }
    if (target == nullptr) return SAT_ERR_NOT_FOUND;
    target->state = SLOT_FREE;
    /* Two-step coalesce: scan the table for free neighbours and grow
     * into them, dropping the absorbed block's slot. */
    int merged = 1;
    while (merged) {
        merged = 0;
        for (uint32_t i = 0u; i < SAT_VDP2_VRAM_MAX_BLOCKS; ++i) {
            if (i == (target - g_vram_blocks)) continue;
            if (g_vram_blocks[i].state != SLOT_FREE) continue;
            if (g_vram_blocks[i].base + g_vram_blocks[i].size == target->base) {
                target->base = g_vram_blocks[i].base;
                target->size += g_vram_blocks[i].size;
                g_vram_blocks[i].state = SLOT_EMPTY;
                g_vram_blocks[i].base = 0u;
                g_vram_blocks[i].size = 0u;
                merged = 1;
                break;
            }
            if (target->base + target->size == g_vram_blocks[i].base) {
                target->size += g_vram_blocks[i].size;
                g_vram_blocks[i].state = SLOT_EMPTY;
                g_vram_blocks[i].base = 0u;
                g_vram_blocks[i].size = 0u;
                merged = 1;
                break;
            }
        }
    }
    return SAT_OK;
}

uint32_t sat_vdp2_vram_largest_free(void) {
    uint32_t best = 0u;
    for (uint32_t i = 0u; i < SAT_VDP2_VRAM_MAX_BLOCKS; ++i) {
        if (g_vram_blocks[i].state != SLOT_FREE) continue;
        if (g_vram_blocks[i].size > best) best = g_vram_blocks[i].size;
    }
    return best;
}

/* ------------------------------------------------------------------ */
/* CRAM bank allocator                                                 */
/* ------------------------------------------------------------------ */

static uint16_t g_cram_banks[8] = {0u};   /* codes used per bank; bank 0 ignored */

void sat_vdp2_palette_reset(void) {
    for (uint32_t i = 0u; i < 8u; ++i) g_cram_banks[i] = 0u;
}

sat_result_t sat_vdp2_palette_reserve(uint16_t count,
                                       sat_vdp2_palette_range_t* out) {
    if (count == 0u || count > 256u) return SAT_ERR_INVALID_ARG;
    if (out == nullptr) return SAT_ERR_INVALID_ARG;
    /* Find the first bank (1..7) with enough room. */
    for (uint16_t bank = 1u; bank < 8u; ++bank) {
        if (g_cram_banks[bank] + count > 256u) continue;
        out->bank = bank;
        out->code = g_cram_banks[bank];
        out->count = count;
        g_cram_banks[bank] = (uint16_t)(g_cram_banks[bank] + count);
        return SAT_OK;
    }
    out->bank = 0u;
    out->code = 0u;
    out->count = 0u;
    return SAT_ERR_CAPACITY;
}

sat_result_t sat_vdp2_palette_free(sat_vdp2_palette_range_t range) {
    if (range.bank < 1u || range.bank > 7u) return SAT_ERR_INVALID_ARG;
    if (range.count == 0u) return SAT_ERR_INVALID_ARG;
    if ((uint32_t)range.code + (uint32_t)range.count > 256u) return SAT_ERR_INVALID_ARG;
    /* The counter allocator can't actually reuse codes below the bank
     * watermark, so free is a permissive no-op for now. Examples only
     * allocate a handful of palettes per boot, which is the use case
     * this allocator was designed for. */
    (void)range;
    return SAT_OK;
}

}  /* extern "C" */
