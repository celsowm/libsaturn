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

/* Sets VDP1 erase color. The erase clears the sprite framebuffer
 * at the start of each frame. The erase uses TRANSPARENT color (end code) to
 * allow the VDP2 backdrop to show in areas without sprites.
 * Bit 15 (transparency) is added automatically.
 */
sat_result_t sat_set_clear_color(uint16_t rgb555);

/* Sets VDP2 backdrop color. This color appears when
 * no VDP1/VDP2 layer draws on top. With transparent VDP1 erase
 * (default), this is the visible color on screen.
 */
sat_result_t sat_vdp2_back_color_set(uint16_t rgb555);

/* Enables or disables automatic VDP1 erase for the entire frame.
 * enable=1: VDP1 erases with transparent color (backdrop visible)
 * enable=0: VDP1 does NOT erase (framebuffer keeps previous frame data)
 * Default: enabled with transparent color
 */
sat_result_t sat_vdp1_set_erase_enabled(uint8_t enable);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VIDEO_H */
