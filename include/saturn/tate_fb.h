#ifndef SATURN_TATE_FB_H
#define SATURN_TATE_FB_H

#include <stdint.h>
#include "saturn/core.h"
#include "saturn/tate.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Hardware TATE: a vertical game drawn UPRIGHT, turned by the VDP2.
 *
 * The VDP1 is put in its rotation framebuffer mode (TVMR TVM = 011b: 512 x 512 pixels of 8 bits,
 * VDP1 manual 4.1) and draws the logical playfield, e.g. 224 x 448, at the framebuffer's top
 * left. The VDP2 reads that framebuffer back through rotation parameter A (VDP2 manual 6.3),
 * turned a quarter and scaled as the layout says, so no sprite or background needs
 * pre-rotation, a 224-pixel-wide field costs the VDP1 only the pixels it covers, and the
 * 448-line field fits the 320 x 224 screen (5/7: a 320 x 160 field, 32 lines of backdrop above
 * and below). Built and checked in examples/tate_fb_rotation and the TATE_FB variant of
 * examples/bullet_hell_stress; the table maths is tested on the host (sat_tate_fb_rotation_table).
 *
 * What it costs, and what it rules out:
 *   - single density only, the 320 x 224 sat_init mode (hi-res needs the software path of saturn/tate.h);
 *   - the framebuffer is 8 bits/pixel, exactly the hi-res constraints: palette codes only, one
 *     shared sprite palette (set it with sat_vdp2_sprite_palette_bank_set / the atlas), no
 *     Gouraud, no colour calculation, no tint, no LUT4 textures;
 *   - the rotation scaling resamples nearest-neighbour, so a 5/7 field decimates small sprites
 *     (a 1:1 field, e.g. 224 wide shown on a 224-line screen, would not);
 *   - RBG0 is used as a dummy layer (the VDP2 only evaluates rotation tables while one is on)
 *     and so are 128 KiB of VDP2 VRAM for its bitmap, and the table's 96 bytes;
 *   - the VDP1 cannot erase a rotated frame buffer while the picture is shown (VDP1 manual 4.1),
 *     and Mednafen erases only part of a 512-wide window, so the library switches the hardware
 *     erase off and starts every list with a clearing polygon over the whole field (the same
 *     path as ON_SUBMIT). That is about 100,000 pixels of VDP1 fill per frame (~22% of the budget):
 *     measured on Mednafen the bullet-hell stress keeps 60 fps up to ~384 bullets and falls to 28 fps
 *     at 448 and above. A hardware erase window of exactly the field was tried and left stale
 *     pixels outside it on Mednafen, so it is not offered.
 *
 * sat_tate_fb_init needs sat_init at 320 x 224 and a layout from sat_tate_layout_init
 * for that screen. Afterwards sat_video_width()/sat_video_height() and every 2D call (sprite
 * batch, clip, HUD) work in the DRAWING SURFACE, logical_w x logical_h, with the origin at its
 * centre as everywhere in the library. Call sat_vdp2_layers_commit() after sat_wait_vblank()
 * each frame, as for any RBG0. The sprite priority is 7 and RBG0's is 1.
 *
 * bitmap_word: VDP2 VRAM word address (bank aligned, 128 KiB = 0x10000 words are cleared) for
 * the dummy RBG0 bitmap; table_word: word address of the 96 byte table, in another bank. */
sat_result_t sat_tate_fb_init(const sat_tate_layout_t* layout, uint32_t bitmap_word, uint32_t table_word);

/* Non-zero once sat_tate_fb_init has run. */
uint8_t sat_tate_fb_active(void);

#ifdef __cplusplus
}
#endif
#endif
