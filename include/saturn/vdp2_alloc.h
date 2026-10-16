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

/* ------------------------------------------------------------------
 * Transactional resource plan
 *
 * The free list above owns the bytes; this plan is the semantic ledger on
 * top of it: what each range is FOR, which banks it may live in, and an
 * all-or-nothing reserve/commit/rollback transaction so a half-built
 * scene configuration never survives a failure (VDP2 manual 3.3, 6.2 and
 * 6.4, vendored under docs/sega_saturn_hardware/hard/vdp2/hon/, govern
 * the bank rules; the pure policy is src/hal/vdp2/resource_plan_logic.hpp).
 *
 * One plan per process, like the free list. Examples call
 * sat_vdp2_plan_begin(), reserve every table the scene needs, then
 * sat_vdp2_plan_commit(); a failure at any point (misalignment, an
 * overlap, a rotation-claimed bank, exhaustion) returns an error, changes
 * nothing and sat_vdp2_plan_rollback() restores the exact previous free
 * space. The old raw sat_vdp2_vram_alloc pair keeps working beside it:
 * the free list stays the single byte owner, but ranges that never went
 * through the plan carry no kind and appear in telemetry only as free
 * space that is gone.
 *
 * Bank masks are SAT_VDP2_BANK_A0/A1/B0/B1 from vdp2_layers.h. A whole
 * bank claim (sat_vdp2_plan_claim_banks) mirrors the RAMCTL rotation data
 * bank bits: RBG0 pattern names, characters/bitmap and per-dot
 * coefficient tables each occupy a bank's whole access cycle, so that
 * bank's cycle pattern is ignored and pattern-name, character, bitmap,
 * rotation-parameter and coefficient reservations are refused there with
 * SAT_ERR_BUSY. Line scroll, window, back screen, line colour and scratch
 * tables may still live in a claimed bank (manual 5.3, 7.1, 7.2, 8.2).
 * The inverse rule holds for RBG0's OWN image data: rotation name tables
 * and character windows are fetched through the rotation data bank bits,
 * so they are refused with SAT_ERR_BUSY anywhere outside banks claimed
 * for rotation, and sat_vdp2_plan_release_banks is refused while such a
 * reservation is live.
 * ------------------------------------------------------------------ */

typedef enum sat_vdp2_reserve_kind {
    SAT_VDP2_RESERVE_PATTERN_NAMES = 0,  /* plane alignment is the caller's */
    SAT_VDP2_RESERVE_CHARACTERS     = 1,
    SAT_VDP2_RESERVE_BITMAP         = 2,  /* 0x20000-aligned, one per bank */
    SAT_VDP2_RESERVE_ROTATION_PARAMS = 3, /* the A+B parameter region: 4-byte aligned */
    SAT_VDP2_RESERVE_COEFFICIENTS    = 4, /* per-line tables: 4-byte aligned */
    SAT_VDP2_RESERVE_LINE_SCROLL    = 5,  /* even address */
    SAT_VDP2_RESERVE_WINDOW_TABLE   = 6,  /* 4-byte aligned */
    SAT_VDP2_RESERVE_BACK_SCREEN    = 7,  /* even address */
    SAT_VDP2_RESERVE_LINE_COLOR     = 8,  /* even address */
    SAT_VDP2_RESERVE_SCRATCH        = 9,  /* CPU-written, no VDP2 read */
    SAT_VDP2_RESERVE_ROTATION_NAMES = 10, /* RBG0's own pattern names: only
                                           * inside banks claimed for rotation */
    SAT_VDP2_RESERVE_ROTATION_CHARS = 11  /* RBG0's character windows,
                                           * 0x8000-aligned: only inside claimed banks */
} sat_vdp2_reserve_kind_t;

/* Opens a transaction. SAT_ERR_BUSY if one is already open. */
sat_result_t sat_vdp2_plan_begin(void);

/* Reserves `bytes` for `kind`, placed by the free list (first fit over the
 * allowed banks), with a minimum alignment of `alignment_bytes` bytes
 * (a 2-byte multiple; never below the kind's own minimum; 0 selects the
 * kind's minimum). Writes the byte base on success. On failure returns
 * SAT_ERR_INVALID_ARG / SAT_ERR_CAPACITY / SAT_ERR_BUSY and stages
 * nothing: the previous state is intact.
 *
 * Pattern names, characters, bitmaps, rotation parameters and
 * coefficient tables are placed inside one maximal run of banks not
 * claimed for rotation: they may span bank edges inside the run (the
 * cycle patterns time a mask of banks, manual 3.3), but never reach into
 * a claimed bank. Tables that need no cycle pattern (line scroll,
 * window, back screen, line colour, scratch) use the whole space. */
sat_result_t sat_vdp2_plan_reserve(sat_vdp2_reserve_kind_t kind, uint32_t bytes,
                                   uint32_t alignment_bytes, uint32_t* out_base);

/* Reserves exactly [base, base + bytes) for `kind` (same rules; the
 * alignment check applies to `base`). SAT_ERR_CAPACITY when the range is
 * not free, SAT_ERR_BUSY when its bank is claimed for rotation. */
sat_result_t sat_vdp2_plan_reserve_at(sat_vdp2_reserve_kind_t kind, uint32_t base,
                                      uint32_t bytes);

/* Ends the transaction keeping every staged reservation. */
sat_result_t sat_vdp2_plan_commit(void);

/* Ends the transaction discarding every staged reservation; the free
 * space is exactly what it was before sat_vdp2_plan_begin(). */
sat_result_t sat_vdp2_plan_rollback(void);

/* Releases one committed reservation (deterministic teardown).
 * SAT_ERR_NOT_FOUND when `base` is not a committed plan reservation. */
sat_result_t sat_vdp2_plan_release(uint32_t base);

/* Releases every committed reservation at once. */
sat_result_t sat_vdp2_plan_release_all(void);

/* Claims whole banks for rotation use (RAMCTL RDBS bits; see above) and
 * refuses (SAT_ERR_BUSY, nothing changes) while a pattern-name, character,
 * bitmap, rotation-parameter or coefficient reservation lives in one of
 * them. Re-claiming an already claimed bank is SAT_OK. */
sat_result_t sat_vdp2_plan_claim_banks(uint8_t bank_mask);

/* Reserves whole banks for RBG1's fixed fetch (B0/B1). They are NOT
 * counted when sat_vdp2_plan_reserve_at() checks that parameter or
 * coefficient tables stay out of "claimed" banks: RDBSB0/B1 must be 00B
 * while RBG1 displays (manual p06_20), and the parameter/coefficient
 * tables need exactly that role. RBG1 image data (RotationNames/
 * RotationChars) may be staged into those banks after the claim.
 * Refused (SAT_ERR_BUSY) when an NBG range already lives in one of them
 * because routing an NBG pattern-name or character window through B0/B1
 * is not what B0/B1 are for. */
sat_result_t sat_vdp2_plan_claim_rbg1_banks(uint8_t bank_mask);

/* Gives claimed banks back. SAT_ERR_BUSY while rotation image data
 * (names or character windows) still lives in one of them;
 * SAT_ERR_INVALID_ARG on a bad mask. */
sat_result_t sat_vdp2_plan_release_banks(uint8_t bank_mask);

/* Gives RBG1-reserved banks back. SAT_ERR_BUSY while RBG1 image data
 * still lives in one of them. */
sat_result_t sat_vdp2_plan_release_rbg1_banks(uint8_t bank_mask);

/* Discards the whole ledger (reservations and bank claims) without
 * freeing anything. Tests and example boot use it, paired with
 * sat_vdp2_vram_alloc_reset: any range the plan had handed out is the
 * caller's responsibility exactly as with sat_vdp2_vram_alloc_reset. */
void sat_vdp2_plan_reset(void);

typedef struct sat_vdp2_plan_usage {
    uint32_t committed_count;
    uint32_t staged_count;
    uint32_t reserved_bytes;             /* committed, plan-managed */
    uint32_t reserved_bytes_per_bank[4]; /* whole ranges attributed to every bank they touch */
    uint32_t free_bytes_per_bank[4];     /* from the byte free list */
    uint32_t claimed_banks;              /* SAT_VDP2_BANK_* mask */
    uint32_t refusals;                   /* refused calls since boot */
    int32_t  last_status;                /* sat_result_t of the last refused call, SAT_OK if none */
    uint32_t last_conflict_base;         /* overlap culprit, 0 when not an overlap */
} sat_vdp2_plan_usage_t;

/* Snapshot for HUDs and tests. Never fails; a null pointer writes nothing. */
void sat_vdp2_plan_usage(sat_vdp2_plan_usage_t* out);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VDP2_ALLOC_H */
