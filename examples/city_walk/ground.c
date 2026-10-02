/* city_walk ground: the streets and pavement are not VDP1 faces. The chunker
 * rasterised every flat, street-level triangle into one 512x512 bitmap (one
 * unit per dot, fine enough for lane dashes and crosswalk stripes), the loader
 * streamed it into VDP2 VRAM, and here it is shown as an RBG0 rotation plane
 * -- Mode 7 -- under the VDP1 geometry.
 *
 * Perspective. For a camera at height h looking level, the ground at screen
 * row y (below the horizon row) is at forward distance h * F / (y - horizon),
 * F being the projection's focal length in pixels (215 for 55 degrees over 224
 * rows). The coefficient table encodes k(y) = focal / (y - horizon), and the
 * rotation table's forward offset is F, so h == focal: the eye is exactly
 * `focal` units high. That is why the eye height is 2.0 and focal is 2.
 *
 * Yaw and position live in the rotation matrix and Mx/My, rewritten every
 * frame. Mx/My carry a 10-bit fraction, which matters: an integer-only camera
 * would snap the ground in whole texels (1-2 units!) against buildings that
 * move smoothly.
 *
 * Distance limit. Rows within a few pixels of the horizon map to hundreds of
 * units, and the bitmap repeats every 512, so those rows are made transparent:
 * the ground fades into the sky at ~130 units, just past the last LOD ring.
 */
#include <stdint.h>

#include "example_util.h"
#include "saturn/vdp2.h"
#include "saturn/vdp2_color_calc.h"
#include "saturn/vdp2_environment.h"

#include "city_walk.h"

#define GROUND_HORIZON_ROW 112u  /* the camera looks level: horizon at mid-screen */
#define GROUND_FOCAL 2u          /* == eye height in units */
#define GROUND_FORWARD 215u      /* projection focal length, pixels */
#define GROUND_FAR_ROWS 3u       /* rows below the horizon left transparent */

/* The bitmap height comes from the archive (256 for the old 1 x 2 unit dots,
 * 512 for one unit per dot); ground_init fills it in. */
static sat_vdp2_rbg0_ground_config_t g_ground_cfg = {
    512u, 256u, CITY_W / 2u, GROUND_HORIZON_ROW, GROUND_FOCAL, 0u, GROUND_FORWARD,
    CITY_COEFFICIENT_WORD
};
static sat_vdp2_ground_environment_t g_environment;
static uint16_t g_coefficients[CITY_H * 2u];
static uint16_t g_params[48];

static void put_fx(uint16_t* table, uint32_t index, int32_t value) {
    table[index] = (uint16_t)((uint32_t)value >> 16);
    table[index + 1u] = (uint16_t)value;
}

void ground_init(void) {
    const int tall = g_archive.header.ground_height == 512u;
    const sat_vdp2_rbg0_mode7_config_t mode7 = {
        tall ? SAT_VDP2_RBG0_BITMAP_512x512 : SAT_VDP2_RBG0_BITMAP_512x256,
        SAT_VDP2_COLOR_MODE_256, CITY_GROUND_BITMAP_WORD,
        CITY_ROTATION_WORD, SAT_COLOR_BLACK, 5u, 7u
    };
    g_ground_cfg.bitmap_height = tall ? 512u : 256u;
    sat_example_must(sat_vdp2_ground_environment_validate_layout(
        &g_ground_cfg, &mode7, CITY_H, SAT_VDP2_VRAM_WORD_CAPACITY, 0u, 512u, 2u));
    sat_example_must(sat_vdp2_ground_environment_init(
        &g_environment, &g_ground_cfg, &mode7, g_coefficients, CITY_H * 2u, g_params, CITY_H));
    /* The far rows are transparent so the sky shows through instead of a
     * repeat of the bitmap. Coefficient words: bit 15 of the first = transparent. */
    for (uint32_t row = GROUND_HORIZON_ROW; row <= GROUND_HORIZON_ROW + GROUND_FAR_ROWS; ++row) {
        g_coefficients[row * 2u] = 0x8000u;
        g_coefficients[row * 2u + 1u] = 0u;
    }
    sat_example_must(sat_vdp2_ground_environment_upload_coefficients(&g_environment));
    /* Dot 0 of the bitmap is "no ground": transparent, so the area outside the
     * city footprint shows the sky/backdrop. */
    sat_example_must(sat_vdp2_rbg0_set_transparent_code_enabled(1u));
    /* Distance fade: eight sprite colour-calc slots. Ordinary sprites keep
     * priority 7, faded ones take 6, and RBG0 (5) and NBG0 (2) stay below both.
     * Configured once; the layer commit replays it every frame. */
    sat_example_must(sat_vdp2_sprite_color_calc_configure_alpha(7u));
}

/* Rewrites the rotation table for this view. Call right after the VBlank wait
 * (VRAM writes are safe any time, but the table must be complete before the
 * scanlines that read it). */
void ground_update(const city_view_t* view) {
    const sat_fx16_t s = sat_sin_deg(view->yaw);
    const sat_fx16_t c = sat_cos_deg(view->yaw);
    const int32_t units_per_dot_z = g_archive.header.ground_units_per_dot_z;
    const int32_t units_per_dot_x = g_archive.header.ground_units_per_dot_x;
    /* Position in dots, 16.16. The bitmap origin is the grid origin, so the
     * chunk index times 32 plus the local offset is already relative to it. */
    int32_t tex_x = (view->pos.chunk_x * CITY_CHUNK_UNITS * 65536 + view->pos.local_x) / units_per_dot_x;
    int32_t tex_y = (view->pos.chunk_z * CITY_CHUNK_UNITS * 65536 + view->pos.local_z) / units_per_dot_z;
    const int32_t mask_x = (int32_t)(g_ground_cfg.bitmap_width << 16) - 1;
    const int32_t mask_y = (int32_t)(g_ground_cfg.bitmap_height << 16) - 1;
    int32_t mx = (tex_x & mask_x) - (int32_t)(g_ground_cfg.cx << 16);
    int32_t my = (tex_y & mask_y) - (int32_t)(g_ground_cfg.horizon << 16);
    uint16_t* p = g_params;

    /* Defaults (Xst, Yst, Px, Py, Cx, Cy, deltas, coefficient address). */
    sat_example_must(sat_vdp2_ground_environment_build_params(&g_environment, 0, 0));
    /* Rotation: tex = M * (screen offset), scaled per axis to dots.
     *   tex_x = -cos * sx + sin * fwd     (right = (-cos, 0, sin))
     *   tex_y = ( sin * sx + cos * fwd) / units_per_dot_z
     * sx is the pixel's horizontal offset from the centre, fwd the ground
     * forward distance the coefficient table scales. */
    put_fx(p, 14u, -c / units_per_dot_x);
    put_fx(p, 16u, s / units_per_dot_x);
    put_fx(p, 20u, s / units_per_dot_z);
    put_fx(p, 22u, c / units_per_dot_z);
    /* Mx / My: 13-bit integer word, then a 10-bit fraction in the top of the next. */
    p[34] = (uint16_t)((uint32_t)(mx >> 16) & 0x1FFFu);
    p[35] = (uint16_t)((uint32_t)mx & 0xFFC0u);
    p[36] = (uint16_t)((uint32_t)(my >> 16) & 0x1FFFu);
    p[37] = (uint16_t)((uint32_t)my & 0xFFC0u);
    sat_example_must(sat_vdp2_ground_environment_commit_params(&g_environment));
}
