/* The board floats in space, so the sky is a VDP2 background rather than
 * more VDP1 geometry: the VDP1 command list is the scarce resource here
 * (around 400 commands is already a whole frame of SH-2 time) and a VDP2
 * layer costs exactly none of it, whatever it shows.
 *
 * NBG0 specifically, of the four normal backgrounds. NBG2 and NBG3 are
 * 16-colour only, and NBG1 has no API in this library yet.
 *
 * The one setting that matters is priority. sat_vdp2_nbg0_init leaves NBG0
 * at 7 -- in FRONT of the sprite layer -- which puts the starfield over the
 * whole maze and looks like the 3D scene failed to draw. It has to be below
 * the sprite priority, and below rather than equal, because ties are broken
 * by a fixed hardware order instead of by setup order. */

#include "sky.h"

#include "saturn/vdp2.h"
#include "saturn/video.h"
#include "saturn/example_util.h"

#include "p3d_config.h"
#include "pacman_3d/stars.h"

#define SKY_PALETTE 1u
#define SKY_PRIORITY 1u
#define SPRITE_PRIORITY 6u

/* Scratch for the pattern-name map; the library allocates nothing itself.
 * 8KB is too much to leave in the fast work RAM for something used once at
 * startup, so it goes to Work RAM Low. */
static uint16_t g_sky_map[SAT_VDP2_NBG0_MAP_CELLS] __attribute__((section(".wram_l")));

void p3d_sky_init(void) {
    sat_vdp2_nbg0_config_t config;
    sat_vdp2_scroll_t scroll = {0u, 0u, 0u, 0u};

    config.char_size = SAT_VDP2_CHAR_SIZE_1X1;
    config.color_mode = SAT_VDP2_COLOR_MODE_256;
    config.map_plane_index = 0x003Bu;
    config.transparent_code_enabled = 0u;
    config.reserved = 0u;

    sat_example_must(sat_vdp2_nbg0_init(&config));
    sat_example_must(sat_vdp2_palette_upload(
        stars_asset.palette, 256u, (uint16_t)(SKY_PALETTE * 256u)));
    sat_example_must(sat_vdp2_nbg0_upload_indexed8(
        stars_asset.pixels,
        stars_asset.width,
        stars_asset.height,
        SKY_PALETTE,
        g_sky_map));

    /* sat_vdp2_nbg0_init re-enables the display on its way out, and VDP2
     * register writes are dropped while the display is active -- so the
     * priorities have to wait for the next VBlank to stick. */
    sat_example_must(sat_wait_vblank());
    sat_example_must(sat_vdp2_sprite_set_priority(SPRITE_PRIORITY));
    sat_example_must(sat_vdp2_nbg0_set_priority(SKY_PRIORITY));
    sat_example_must(sat_vdp2_nbg0_set_scroll(&scroll));
}

/* Without this the stars stay nailed to the screen while the board pivots
 * under them, which reads as the BOARD turning rather than the camera going
 * round it -- the sky is the only thing on screen far enough away to say
 * which of the two is happening. A full revolution scrolls the 512-pixel
 * plane exactly once, so the stars come back to where they started. */
void p3d_sky_set_angle(uint16_t angle) {
    sat_vdp2_scroll_t scroll = {0u, 0u, 0u, 0u};
    scroll.x_integer = (uint16_t)(((uint32_t)angle * 512u) / P3D_CAM_ANGLES);
    sat_example_must(sat_vdp2_nbg0_set_scroll(&scroll));
}
