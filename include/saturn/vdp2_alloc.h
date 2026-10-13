#ifndef SATURN_VDP2_ALLOC_H
#define SATURN_VDP2_ALLOC_H

/* SPDX-License-Identifier: MIT */
#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------
 * VDP2 VRAM byte allocator
 *
 * Most VDP2 demos hand-pick raw byte addresses for their character,
 * name and coefficient tables (0x10000, 0x11000, 0x40000, ...). The
 * addresses collide by accident: today the dino demo and a second
 * VDP2 demo both want the same 0x10000 region, and only one of them
 * can have it. This API is the one-source-of-truth so each example
 * asks for `bytes` at `alignment_words` and the allocator returns a
 * free base address, freeing it again on shutdown.
 *
 * Scope: one allocator per process. The free list is a static global
 * that survives across calls but is reset by sat_vdp2_vram_alloc_reset,
 * which tests use and which every example calls once at boot before
 * allocating. Examples run single-threaded on the SH-2 master; the
 * dual_sh2 contract excludes this allocator on the slave side.
 *
 * Address space: byte offsets within VDP2 VRAM, 0x00000..0x80000.
 * The address validation in src/hal/vdp2/compose_logic.hpp already
 * uses 0x80000 as the upper bound and `byte_address & 3u` as the
 * word-alignment check, so this API mirrors that.
 * ------------------------------------------------------------------ */

/* Configure the byte range the allocator owns. Defaults cover the full
 * VDP2 byte address space if not called. Called once at boot, before
 * any alloc; subsequent calls reset the free list and discard the
 * existing allocations (no free tracking is preserved). */
sat_result_t sat_vdp2_vram_alloc_init(uint32_t start_byte,
                                       uint32_t end_byte);

/* Reset the free list so every byte in the configured range is again
 * available. Discards existing allocations (the caller is responsible
 * for any live VDP2 references — they will point at released memory). */
void sat_vdp2_vram_alloc_reset(void);

/* Allocate `bytes` bytes with a minimum alignment of
 * `alignment_words` *word* boundaries (1 = no alignment, 2 = 4-byte
 * word pairs, 8 = 32-byte quad-word, ...). Returns the byte base on
 * success. On failure (SAT_ERR_INVALID_ARG or SAT_ERR_CAPACITY)
 * returns SAT_VDP2_VRAM_NO_BASE (0xFFFFFFFFu) so the caller can
 * distinguish a successful base of 0 (legal but rare: the configured
 * range starts there) from an exhausted allocator. The base is
 * automatically aligned: if the first fit is below the alignment,
 * the allocator rounds up and may split a free block. */
#define SAT_VDP2_VRAM_NO_BASE 0xFFFFFFFFu
uint32_t sat_vdp2_vram_alloc(uint32_t bytes, uint16_t alignment_words);

/* Free a previously allocated block. Adjacent freed blocks coalesce so
 * a 0x10000 / 0x11000 pair becomes one 0x20000 byte free block. No
 * state per allocation is exposed; the same base returned by alloc
 * must be passed back to free. Returns SAT_OK or SAT_ERR_NOT_FOUND. */
sat_result_t sat_vdp2_vram_free(uint32_t base_byte);

/* Largest single allocation that the current configuration can satisfy
 * with a 1-word alignment. Useful as a capacity probe in tests. */
uint32_t sat_vdp2_vram_largest_free(void);

/* ------------------------------------------------------------------
 * VDP2 CRAM bank / colour-code allocator
 *
 * CRAM is 8 banks of 256 colour codes. Bank 0 is the default VDP1
 * palette and is not part of this allocator; the four bytes are a
 * bank-rotation ring over banks 1..7 (a hand count for a small demo).
 *
 * The wrapper that examples call is `sat_vdp2_palette_reserve(count)`
 * returning the bank and starting code so the caller can pass it to
 * `sat_vdp2_palette_upload(palette, count, base)`. The matching free
 * returns the whole count back to the ring in one call.
 * ------------------------------------------------------------------ */
typedef struct sat_vdp2_palette_range {
    uint16_t bank;     /* 1..7 */
    uint16_t code;     /* 0..255 inside the bank */
    uint16_t count;    /* number of codes in the range */
} sat_vdp2_palette_range_t;

/* Reserve `count` consecutive CRAM codes. Returns the bank and code on
 * success; on failure (count > 256, no bank has room) bank=0 is
 * returned alongside SAT_ERR_CAPACITY. */
sat_result_t sat_vdp2_palette_reserve(uint16_t count,
                                       sat_vdp2_palette_range_t* out);

/* Release a previously reserved range. The whole range becomes free
 * at once; partial frees are not supported because every caller
 * uploads the whole block in one sat_vdp2_palette_upload() call. */
sat_result_t sat_vdp2_palette_free(sat_vdp2_palette_range_t range);

/* Reset every bank back to "empty" so the next test call starts clean. */
void sat_vdp2_palette_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VDP2_ALLOC_H */
