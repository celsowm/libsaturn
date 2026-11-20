#ifndef SATURN_FX_COMPOSITOR_H
#define SATURN_FX_COMPOSITOR_H

#include "saturn/vdp1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Experimental one-layer VDP1/VDP2 compositing at 320x224. Retain this
 * caller-owned workspace until release. Zero-initialize it before reserve. */
#define SAT_FX_COMPOSITOR_PIXELS (160u * 112u)
typedef struct sat_fx_compositor {
    uint32_t presented_serial;
    uint32_t frames, draw_ticks, readback_ticks, upload_ticks, recovery_fields;
    uint32_t bitmap[2];
    uint16_t palette_bank;
    uint8_t active_bitmap, visible, prepared;
    uint8_t reserved[3];
    uint32_t readback_bytes, upload_bytes;
    uint16_t rgb[SAT_FX_COMPOSITOR_PIXELS];
    uint16_t indexed[SAT_FX_COMPOSITOR_PIXELS / 2u];
    uint8_t lookup[65536];
    uint8_t first_row, end_row;
    uint8_t bitmap_first_row[2], bitmap_end_row[2];
} sat_fx_compositor_t;

/* Screen bounds are half-open. Protection expands to whole 2x2 pixels. */
typedef struct sat_fx_mask {
    uint16_t x, y, width, height;
} sat_fx_mask_t;

sat_result_t sat_fx_compositor_reserve(sat_fx_compositor_t* fx);

/* Pass the checkpoints immediately before and after the scene flush.
 * Call before or after emitting the HUD. Solid RGB half-transparency draws
 * enter the layer. Opaque RGB and fully opaque INDEX8 faces can occlude it.
 * Refusal leaves the original native commands untouched. */
sat_result_t sat_fx_compositor_prepare(sat_fx_compositor_t* fx,
    const sat_vdp1_command_checkpoint_t* first,
    const sat_vdp1_command_checkpoint_t* last, const sat_fx_mask_t* masks, uint16_t mask_count);

/* Submits the current frame. With no prepared effect it presents a native
 * frame and hides the previous bitmap at the same boundary. Settle any
 * pending presentation before beginning another VDP1 frame. */
sat_result_t sat_fx_compositor_present(sat_fx_compositor_t* fx);
sat_result_t sat_fx_compositor_release(sat_fx_compositor_t* fx);

#ifdef __cplusplus
}
#endif
#endif
