#ifndef SATURN_CART_OVERLAY_H
#define SATURN_CART_OVERLAY_H

#include <stdint.h>

#include "saturn/cdfs.h"
#include "saturn/core.h"
#include "saturn/loader.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Overlays in the RAM cartridge: one region of the cart (the slot) holds one
 * of several disc files at a time, swapped in when the program needs it (a
 * game's levels, a set of sprites). Data that does not all fit the cart
 * stays on the disc and only the current piece occupies the slot.
 *
 * The usual setup links every overlay's data at the slot's address (an ld
 * OVERLAY, each member extracted into its own disc file), so pointers into
 * and out of the data are resolved by the linker; the program only has to
 * load the right file before touching the data.
 *
 * The program reads the slot through the cached cart address; the load
 * writes the cache-through mirror and then purges the CPU cache, so no line
 * of the previous overlay survives the swap. Only the calling CPU's cache is
 * purged: a Slave SH-2 reading the slot must purge its own. */

#define SAT_CART_OVERLAY_NONE (-1)

typedef struct sat_cart_overlay {
    uint32_t base;      /* cached cart address of the slot */
    uint32_t capacity;  /* bytes */
    int32_t resident;   /* id of the overlay in the slot, or SAT_CART_OVERLAY_NONE */
} sat_cart_overlay_t;

/* `base` is the slot's cart address (0x02400000..0x027FFFFF, or its
 * cache-through mirror), `capacity` its size; the whole slot must lie in the
 * 4 MiB cart window, and `base` must be 4-byte aligned. SAT_ERR_INVALID_ARG
 * otherwise. The slot starts empty. Does not check that a cart is present:
 * reserve the region with sat_ram_cart_* first. */
sat_result_t sat_cart_overlay_init(sat_cart_overlay_t* overlay, uint32_t base, uint32_t capacity);

/* Makes overlay `id` (>= 0) resident: returns SAT_OK at once if it already
 * is, otherwise reads the whole of `disc_path` into the slot (through
 * sat_load_files, so `progress` may be NULL) and purges the CPU cache.
 * `expected_size` 0 accepts any size. A file larger than the slot is
 * SAT_ERR_CAPACITY and a size mismatch SAT_ERR_VERSION, both before anything
 * is written. Once a read has started, a failure leaves the slot empty
 * (resident = SAT_CART_OVERLAY_NONE): its contents are no longer any
 * overlay. */
sat_result_t sat_cart_overlay_load(
    sat_cart_overlay_t* overlay,
    sat_cdfs_volume_t* volume,
    int32_t id,
    const char* disc_path,
    uint32_t expected_size,
    sat_load_progress_fn progress, void* context);

/* Marks the slot empty (for example after the program wrote into it), so
 * the next load reads the file again. */
void sat_cart_overlay_forget(sat_cart_overlay_t* overlay);

#ifdef __cplusplus
}
#endif
#endif
