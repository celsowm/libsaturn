/* VDP2 VRAM and CRAM allocators.
 *
 * The VRAM allocator is a single-process, first-fit free-list with
 * alignment-driven splitting. The CRAM allocator is a first-fit bitmap
 * over banks 1..7 (bank 0 is the default VDP1 palette, never allocated).
 * The reason both exist is the same: examples used to hand-pick the same
 * byte address / palette bank and only one could win. The allocate/free
 * pair centralises the decision, and the CRAM bitmap can be snapshotted
 * for the transactional resource plan.
 *
 * Both allocators are intentionally single-threaded. Examples run
 * on the SH-2 master and the dual_sh2 contract excludes the slave
 * from these. A future cleanup may add mutex guards; today's call
 * sites do not need them.
 */
#include <stdint.h>

#include "saturn/vdp2_alloc.h"

#include "vram_internal.hpp"

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
 * request of 0 or 1 word means no alignment. 32-bit: a bitmap needs
 * 0x10000 words (0x20000 bytes). */
static uint32_t vram_align_up(uint32_t addr, uint32_t alignment_words) {
    if (alignment_words <= 1u) return addr;
    const uint32_t mask = alignment_words * 2u - 1u;
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

/* Carves [aligned, aligned + bytes) out of the free block `block`
 * (aligned >= block->base, within the block). The alignment slack below
 * the base stays free. Returns 1 on success; 0 when the block cannot host
 * the range. Never mutates the free list on failure. */
static int vram_carve(vram_block_t* block, uint32_t aligned, uint32_t bytes) {
    const uint32_t slack = aligned - block->base;
    if (block->size < slack + bytes) return 0;
    /* A successful allocation must never land on the failure sentinel. */
    if (aligned == SAT_VDP2_VRAM_NO_BASE) return 0;
    if (slack > 0u) {
        vram_block_t* slack_block = vram_slot_alloc();
        if (slack_block == nullptr) {
            /* No slot to represent the slack: refuse rather than hand
             * out a misaligned base (the old code silently lost the
             * alignment here). The caller treats it as exhaustion. */
            return 0;
        }
        slack_block->base = block->base;
        slack_block->size = slack;
        slack_block->state = SLOT_FREE;
        block->base = aligned;
        block->size -= slack;
    }
    /* Now block->base == aligned; split off the allocated portion. */
    vram_split_block(block, bytes, SLOT_ALLOCATED);
    return 1;
}

/* Find a free block big enough to satisfy `bytes` from the alignment
 * point onward. Returns the block (and via *out_aligned, the actual
 * byte-aligned base that alloc would return). */
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
    if (!vram_carve(block, aligned, bytes)) return SAT_VDP2_VRAM_NO_BASE;
    return block->base;
}

/* ---- Internal range-aware extensions (src/hal/vdp2/vram_internal.hpp) -- */

/* Places `bytes` first-fit inside [start_byte, end_byte) of the configured
 * address space. Same ownership as sat_vdp2_vram_alloc. */
int vram_alloc_in(uint32_t start_byte, uint32_t end_byte, uint32_t bytes,
                  uint32_t alignment_words, uint32_t* out_base) {
    if (out_base == nullptr || bytes == 0u) return 0;
    if (start_byte >= end_byte || end_byte > 0x80000u) return 0;
    if (!g_vram_inited) {
        sat_vdp2_vram_alloc_init(SAT_VDP2_VRAM_DEFAULT_START,
                                  SAT_VDP2_VRAM_DEFAULT_END);
    }
    for (uint32_t i = 0u; i < SAT_VDP2_VRAM_MAX_BLOCKS; ++i) {
        if (g_vram_blocks[i].state != SLOT_FREE) continue;
        const uint32_t block_end = g_vram_blocks[i].base + g_vram_blocks[i].size;
        if (block_end <= start_byte) continue;
        const uint32_t from = g_vram_blocks[i].base > start_byte
            ? g_vram_blocks[i].base : start_byte;
        const uint32_t aligned = vram_align_up(from, alignment_words);
        if (aligned + bytes > block_end) continue;   /* no wrap: from <= 0x80000 */
        if (aligned + bytes > end_byte) continue;
        if (!vram_carve(&g_vram_blocks[i], aligned, bytes)) continue;
        *out_base = g_vram_blocks[i].base;
        return 1;
    }
    return 0;
}

/* Carves exactly [base_byte, base_byte + bytes). Fails when any of those
 * bytes are not free or the range is out of bounds. */
int vram_alloc_at(uint32_t base_byte, uint32_t bytes) {
    if (bytes == 0u || base_byte >= 0x80000u) return 0;
    if (base_byte + bytes > 0x80000u) return 0;
    if (!g_vram_inited) {
        sat_vdp2_vram_alloc_init(SAT_VDP2_VRAM_DEFAULT_START,
                                  SAT_VDP2_VRAM_DEFAULT_END);
    }
    for (uint32_t i = 0u; i < SAT_VDP2_VRAM_MAX_BLOCKS; ++i) {
        if (g_vram_blocks[i].state != SLOT_FREE) continue;
        const uint32_t block_end = g_vram_blocks[i].base + g_vram_blocks[i].size;
        if (base_byte >= g_vram_blocks[i].base
            && base_byte + bytes <= block_end) {
            return vram_carve(&g_vram_blocks[i], base_byte, bytes);
        }
    }
    return 0;
}

/* Bytes free within [start_byte, end_byte), clamped to the configured
 * address space. */
uint32_t vram_free_bytes(uint32_t start_byte, uint32_t end_byte) {
    if (end_byte <= start_byte) return 0u;
    const uint32_t lo = start_byte < g_vram_start ? g_vram_start : start_byte;
    const uint32_t hi = end_byte > g_vram_end ? g_vram_end : end_byte;
    if (lo >= hi) return 0u;
    uint32_t total = 0u;
    for (uint32_t i = 0u; i < SAT_VDP2_VRAM_MAX_BLOCKS; ++i) {
        if (g_vram_blocks[i].state != SLOT_FREE) continue;
        const uint32_t b_lo = g_vram_blocks[i].base > lo
            ? g_vram_blocks[i].base : lo;
        const uint32_t block_end = g_vram_blocks[i].base + g_vram_blocks[i].size;
        const uint32_t b_hi = block_end < hi ? block_end : hi;
        if (b_hi > b_lo) total += b_hi - b_lo;
    }
    return total;
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
            if (&g_vram_blocks[i] == target) continue;
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

/* 7 banks x 256 codes, one bit per code. Bank 0 is not allocatable. */
static uint64_t g_cram_used[7][4] = {};
static uint32_t g_cram_refusals = 0u;

static bool cram_get(uint16_t bank, uint16_t code) {
    const uint16_t b = (uint16_t)(bank - 1u);
    return ((g_cram_used[b][code >> 6u] >> (code & 63u)) & 1u) != 0u;
}

static void cram_set(uint16_t bank, uint16_t code, bool used) {
    const uint16_t b = (uint16_t)(bank - 1u);
    const uint64_t mask = (uint64_t)1u << (code & 63u);
    if (used) g_cram_used[b][code >> 6u] |= mask;
    else g_cram_used[b][code >> 6u] &= ~mask;
}

static bool cram_range_free(uint16_t bank, uint16_t code, uint16_t count) {
    for (uint16_t i = 0u; i < count; ++i) {
        if (cram_get(bank, (uint16_t)(code + i))) return false;
    }
    return true;
}

void sat_vdp2_palette_reset(void) {
    for (uint32_t b = 0u; b < 7u; ++b) {
        for (uint32_t w = 0u; w < 4u; ++w) g_cram_used[b][w] = 0u;
    }
    g_cram_refusals = 0u;
}

sat_result_t sat_vdp2_palette_reserve(uint16_t count,
                                       sat_vdp2_palette_range_t* out) {
    if (count == 0u || count > 256u || out == nullptr) {
        ++g_cram_refusals;
        return SAT_ERR_INVALID_ARG;
    }
    /* First fit: lowest bank, then lowest code. */
    for (uint16_t bank = 1u; bank < 8u; ++bank) {
        for (uint16_t code = 0u; code + count <= 256u; ++code) {
            if (!cram_range_free(bank, code, count)) continue;
            for (uint16_t i = 0u; i < count; ++i) {
                cram_set(bank, (uint16_t)(code + i), true);
            }
            out->bank = bank;
            out->code = code;
            out->count = count;
            return SAT_OK;
        }
    }
    out->bank = 0u;
    out->code = 0u;
    out->count = 0u;
    ++g_cram_refusals;
    return SAT_ERR_CAPACITY;
}

sat_result_t sat_vdp2_palette_free(sat_vdp2_palette_range_t range) {
    if (range.bank < 1u || range.bank > 7u) {
        ++g_cram_refusals;
        return SAT_ERR_INVALID_ARG;
    }
    if (range.count == 0u) {
        ++g_cram_refusals;
        return SAT_ERR_INVALID_ARG;
    }
    if ((uint32_t)range.code + (uint32_t)range.count > 256u) {
        ++g_cram_refusals;
        return SAT_ERR_INVALID_ARG;
    }
    /* Every code must currently be allocated; a partial or already
     * freed range is a caller error, not a silent no-op. */
    for (uint16_t i = 0u; i < range.count; ++i) {
        if (!cram_get(range.bank, (uint16_t)(range.code + i))) {
            ++g_cram_refusals;
            return SAT_ERR_NOT_FOUND;
        }
    }
    for (uint16_t i = 0u; i < range.count; ++i) {
        cram_set(range.bank, (uint16_t)(range.code + i), false);
    }
    return SAT_OK;
}

void sat_vdp2_palette_usage(sat_vdp2_palette_usage_t* out) {
    if (out == nullptr) return;
    for (uint32_t bank = 0u; bank < 8u; ++bank) {
        out->used_codes[bank] = 0u;
        out->free_codes[bank] = 0u;
    }
    for (uint16_t bank = 1u; bank < 8u; ++bank) {
        uint16_t used = 0u;
        for (uint16_t code = 0u; code < 256u; ++code) {
            if (cram_get(bank, code)) ++used;
        }
        out->used_codes[bank] = used;
        out->free_codes[bank] = (uint16_t)(256u - used);
    }
    out->refusals = g_cram_refusals;
}

/* Internal transaction snapshot: the allocator bitmap only. Refusal
 * counters are telemetry and deliberately survive rollback. */
void vram_cram_snapshot(uint8_t* out) {
    if (out == nullptr) return;
    for (uint32_t b = 0u; b < 7u; ++b) {
        for (uint32_t w = 0u; w < 4u; ++w) {
            const uint64_t value = g_cram_used[b][w];
            for (uint32_t i = 0u; i < 8u; ++i) {
                *out++ = (uint8_t)(value >> (i * 8u));
            }
        }
    }
}

void vram_cram_restore(const uint8_t* in) {
    if (in == nullptr) return;
    for (uint32_t b = 0u; b < 7u; ++b) {
        for (uint32_t w = 0u; w < 4u; ++w) {
            uint64_t value = 0u;
            for (uint32_t i = 0u; i < 8u; ++i) {
                value |= (uint64_t)(*in++) << (i * 8u);
            }
            g_cram_used[b][w] = value;
        }
    }
}

}  /* extern "C" */
