#ifndef SATURN_VIDEO_H
#define SATURN_VIDEO_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Frame lifecycle                                                     */
/* ------------------------------------------------------------------ */
sat_result_t sat_begin_frame(void);
sat_result_t sat_end_frame(void);
sat_result_t sat_wait_vblank(void);

/* ------------------------------------------------------------------ */
/* Clear / backdrop color                                              */
/* ------------------------------------------------------------------ */

/* Sets an OPAQUE VDP1 erase color. The erase clears the sprite framebuffer at
 * the start of each frame. Bit 15 (the RGB code) is added automatically, so the
 * erased framebuffer COVERS the VDP2 backdrop and any VDP2 layer below the
 * sprite layer. Call sat_vdp1_set_erase_transparent() to restore the default.
 */
sat_result_t sat_set_clear_color(uint16_t rgb555);

/* Restores the transparent VDP1 framebuffer erase (the default set by sat_init).
 * The framebuffer is filled with the "no data" code, so the VDP2 backdrop and
 * VDP2 layers below the sprite layer are visible wherever no sprite is drawn.
 */
sat_result_t sat_vdp1_set_erase_transparent(void);

/* Sets VDP2 backdrop color. This color appears when
 * no VDP1/VDP2 layer draws on top. With transparent VDP1 erase
 * (default), this is the visible color on screen.
 */
sat_result_t sat_vdp2_back_color_set(uint16_t rgb555);

/* Enables or disables automatic VDP1 erase for the entire frame.
 * enable=1: VDP1 erases each frame with the current erase color
 *           (transparent by default, opaque after sat_set_clear_color)
 * enable=0: VDP1 does NOT erase (framebuffer keeps previous frame data)
 * Default: enabled.
 */
sat_result_t sat_vdp1_set_erase_enabled(uint8_t enable);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VIDEO_H */
