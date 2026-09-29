#ifndef SATURN_PALETTE_H
#define SATURN_PALETTE_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Logical palette handle. Content-equal palettes share one VDP1 CRAM
 * bank, so registering the same colours twice is cheap. The handle is
 * validated against the registry's per-bank generation counter: a
 * handle from a previous registration is rejected once its palette has
 * been unregistered and the bank reused. */
typedef struct sat_palette {
    uint16_t bank;
    uint16_t generation;
} sat_palette_t;

/* The zero handle: drawing uses each texture's own palette. */
static inline sat_palette_t sat_palette_none(void) {
    sat_palette_t palette;
    palette.bank = 0u;
    palette.generation = 0u;
    return palette;
}

/* Claims one CRAM bank (deduplicated by content) and uploads the given
 * 256 RGB555 entries when the bank is newly claimed. The caller keeps
 * ownership of the storage; the palette contents are copied. */
sat_result_t sat_palette_register(
    const uint16_t* rgb555,
    sat_palette_t* out_palette
);

/* Releases one registration. The last user of a bank frees it for
 * future textures or palettes; the CRAM contents stay until reused. */
sat_result_t sat_palette_unregister(sat_palette_t palette);

/* Resolves a live handle to its CRAM bank. */
sat_result_t sat_palette_bank(sat_palette_t palette, uint16_t* out_bank);

uint16_t sat_palette_bank_count(void);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_PALETTE_H */
