/* vdp2_rbg0_ground.c - Infinite floor plane using VDP2 RBG0.
 *
 * Sky in top half (RBG0 transparent -> BACK screen color). Floor in bottom
 * half rendered through a per-scanline coefficient table with KMD=0
 * (k applied to both kx and ky), giving classic Mode-7 depth.
 *
 * D-Pad UP/DOWN walks forward/back, LEFT/RIGHT strafes. START exits.
 * Uses the same floor.tga texture as vdp2_nbg0_image, tiled into the RBG0
 * bitmap so the plane repeats while walking.
 *
 * VRAM layout:
 *   A0 (0x00000..0x0FFFF words): RBG0 bitmap, 512x256 8bpp
 *   A1 (0x10000..0x1002F words): rotation parameter A table (0x60 bytes = 48 words)
 *   A1 (0x12000..0x121BF words): coefficient table (224 lines x 2 words)
 *   A1 (0x3FFFF): BACK screen color
 */
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"
#include "saturn/font.h"
#include "saturn/color.h"
#include "vdp2_rbg0_ground/bg.h"
#include "rbg0_math.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 224

#define BITMAP_WIDTH  512
#define BITMAP_HEIGHT 256

/* Mode-7 layout */
#define HORIZON       96u                               /* y row of vanishing point */
#define CX            (SCREEN_WIDTH / 2)                /* 160 */
#define CY            HORIZON
#define FOCAL         96u
#define MIN_DEPTH     8u

/* Screen-space distance projected onto the ground plane, i.e. the effective
 * focal length of the Y axis. MUST be non-zero: the hardware computes
 * Ysp = E * (Yst - Py), so Yst == Py collapses every scanline onto a single
 * texture row (only X gets scaled by k, producing radial streaks).
 * Equal to FOCAL keeps the plane isotropic: one world unit sideways covers the
 * same screen area as one unit forward. Lower values stretch the ground in
 * depth (a "longer" look), higher values flatten it.
 */
#define GROUND_FORWARD 96u

#define BM_BASE_WORD   0x00000u
#define RP_BASE_WORD   0x10000u
#define COEF_BASE_WORD 0x12000u
#define BACK_COLOR_WORD 0x3FFFFu

#define SKY_COLOR     0x7F45u

#define FX16_SHIFT 16

/* Palette index 0 is the RBG0 ground bitmap; the pad-debug text uses its own
 * bank so uploading the font never disturbs bg_asset's palette.
 */
#define PAD_DEBUG_PALETTE 1u

static sat_fx16_t cam_x = 0;
static sat_fx16_t cam_y = 0;
static sat_ascii_font_t g_pad_debug_font;
static uint32_t g_frame_count = 0;

/* Renders pad.held as "PAD:xxxx U D L R S A B C" — hex bitmask plus a letter
 * per button that lights up ('.' when not held) — to visually confirm any
 * input at all is reaching the program, independent of camera movement.
 */
static void format_pad_debug(uint16_t held, char* out) {
    static const char kHex[16] = "0123456789ABCDEF";
    out[0] = 'P'; out[1] = 'A'; out[2] = 'D'; out[3] = ':'; out[4] = ' ';
    out[5] = kHex[(held >> 12) & 0xFu];
    out[6] = kHex[(held >> 8) & 0xFu];
    out[7] = kHex[(held >> 4) & 0xFu];
    out[8] = kHex[held & 0xFu];
    out[9] = ' ';
    out[10] = (held & SAT_PAD_UP) ? 'U' : '.';
    out[11] = (held & SAT_PAD_DOWN) ? 'D' : '.';
    out[12] = (held & SAT_PAD_LEFT) ? 'L' : '.';
    out[13] = (held & SAT_PAD_RIGHT) ? 'R' : '.';
    out[14] = (held & SAT_PAD_START) ? 'S' : '.';
    out[15] = (held & SAT_PAD_A) ? 'A' : '.';
    out[16] = (held & SAT_PAD_B) ? 'B' : '.';
    out[17] = (held & SAT_PAD_C) ? 'C' : '.';
    out[18] = '\0';
}

/* Renders a free-running frame counter as "FRM:xxxxxxxx" (hex). If this
 * value is frozen on screen (never counts up), the main loop itself never
 * got past whatever call precedes the increment — proves/disproves a full
 * hang independent of the PAD: line, which only tells us about input.
 */
static void format_frame_debug(uint32_t count, char* out) {
    static const char kHex[16] = "0123456789ABCDEF";
    out[0] = 'F'; out[1] = 'R'; out[2] = 'M'; out[3] = ':'; out[4] = ' ';
    for (int i = 0; i < 8; i++) {
        out[5 + i] = kHex[(count >> ((7 - i) * 4)) & 0xFu];
    }
    out[13] = '\0';
}

/* Single source of truth for the Mode-7 layout, shared verbatim with
 * tests/host/test_rbg0_ground.cpp via rbg0_math.h.
 */
static const rbg0_ground_config_t rbg0_cfg = {
    BITMAP_WIDTH,
    BITMAP_HEIGHT,
    CX,
    HORIZON, /* == CY */
    FOCAL,
    MIN_DEPTH,
    GROUND_FORWARD,
    COEF_BASE_WORD,
};

static void upload_tiled_bitmap(const sat_indexed8_asset_t* asset) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    uint32_t off = BM_BASE_WORD;

    for (uint32_t y = 0; y < BITMAP_HEIGHT; y++) {
        const uint32_t src_y = y % asset->height;
        for (uint32_t x = 0; x < BITMAP_WIDTH; x += 2u) {
            const uint32_t src_x0 = x % asset->width;
            const uint32_t src_x1 = (x + 1u) % asset->width;
            const uint8_t p0 = asset->pixels[(src_y * asset->width) + src_x0];
            const uint8_t p1 = asset->pixels[(src_y * asset->width) + src_x1];
            vram[off++] = (uint16_t)(((uint16_t)p0 << 8u) | p1);
        }
    }
}

/* Coefficient table (2-word, mode 0) — see rbg0_ground_encode_coefficient()
 * in rbg0_math.h for the encoding and the k(y) = FOCAL / depth derivation.
 */
static void write_coefficient_table(void) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    for (uint32_t y = 0; y < (uint32_t)SCREEN_HEIGHT; y++) {
        uint16_t w0, w1;
        rbg0_ground_encode_coefficient(&rbg0_cfg, y, &w0, &w1);
        uint32_t base = COEF_BASE_WORD + (y * 2u);
        vram[base + 0u] = w0;
        vram[base + 1u] = w1;
    }
}

static void write_back_screen_sky(void) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    volatile uint16_t* r = (volatile uint16_t*)0x25F80000u;

    vram[BACK_COLOR_WORD] = SKY_COLOR;
    r[0x0AC >> 1] = (uint16_t)((BACK_COLOR_WORD >> 16u) & 0x0007u);
    r[0x0AE >> 1] = (uint16_t)(BACK_COLOR_WORD & 0xFFFFu);
}

/* Rotation parameter A table at word 0x10000. Built by
 * rbg0_ground_build_params() (rbg0_math.h) — see that function's comment for
 * the Xst/Yst/Px/Py derivation and why Yst must differ from Py.
 * Camera position lives in Mx/My (parallel-translation) so the per-line
 * coefficient does NOT scale the camera, only the per-pixel deltas.
 */
static void write_rotation_params(int32_t cam_xi, int32_t cam_yi) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    uint16_t table[48];
    int i;

    rbg0_ground_build_params(&rbg0_cfg, cam_xi, cam_yi, table);
    for (i = 0; i < 48; i++) {
        vram[RP_BASE_WORD + (uint32_t)i] = table[i];
    }
}

static void write_camera_translation(int32_t cam_xi, int32_t cam_yi) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    int32_t mx = rbg0_ground_wrap_translation(cam_xi, rbg0_cfg.bitmap_width, rbg0_cfg.cx);
    int32_t my = rbg0_ground_wrap_translation(cam_yi, rbg0_cfg.bitmap_height, rbg0_cfg.horizon);

    vram[RP_BASE_WORD + 34u] = (uint16_t)((uint32_t)mx & 0x1FFFu);
    vram[RP_BASE_WORD + 35u] = 0x0000;
    vram[RP_BASE_WORD + 36u] = (uint16_t)((uint32_t)my & 0x1FFFu);
    vram[RP_BASE_WORD + 37u] = 0x0000;
}

/* Configure RBG0 in bitmap mode with per-line coefficient (Mode-7 floor). */
static void init_rbg0_mode7(void) {
    volatile uint16_t* r = (volatile uint16_t*)0x25F80000u;

    /* Turn display off while reconfiguring (keep BDCLMD bit). */
    r[0x000 >> 1] = (uint16_t)(r[0x000 >> 1] & (uint16_t)~0x8000u);

    /* RAMCTL:
     *   A0 = RBG0 bitmap/character data (bits 1..0 = 11)
     *   A1 = RBG0 coefficient table     (bits 3..2 = 01)
     */
    r[0x00E >> 1] = 0x1107u;

    /* Cycle patterns: keep bitmap reads on A0 and alternate parameter /
     * coefficient reads on A1.
     */
    r[0x010 >> 1] = 0x9E9Eu; /* CYCA0L */
    r[0x012 >> 1] = 0x9E9Eu; /* CYCA0U */
    r[0x014 >> 1] = 0x9898u; /* CYCA1L */
    r[0x016 >> 1] = 0x9898u; /* CYCA1U */
    r[0x018 >> 1] = 0xEEEEu; /* CYCB0L */
    r[0x01A >> 1] = 0xEEEEu; /* CYCB0U */
    r[0x01C >> 1] = 0xEEEEu; /* CYCB1L */
    r[0x01E >> 1] = 0xEEEEu; /* CYCB1U */

    /* BGON off during config. */
    r[0x020 >> 1] = 0x0000u;

    /* CHCTLB: 256-color, 512x256 bitmap, bitmap mode on. */
    r[0x02A >> 1] = 0x1200u;

    /* MPOFR: bitmap bank = 0 (A0). */
    r[0x03E >> 1] = 0x0000u;

    /* RPTA: rotation table at byte 0x20000 (word 0x10000). */
    r[0x0BC >> 1] = 0x0001u; /* RPTAU */
    r[0x0BE >> 1] = 0x0000u; /* RPTAL */

    r[0x0B0 >> 1] = 0x0000u; /* RPMD = use parameter A only */
    r[0x0B2 >> 1] = 0x0000u; /* RPRCTL */
    r[0x0B4 >> 1] = 0x0001u; /* KTCTL: A enable, 2-word, KMD=0 (k -> kx & ky) */
    r[0x0B6 >> 1] = 0x0000u; /* KTAOF: RAKTAOS=0 */

    r[0x0FC >> 1] = 0x0007u; /* PRIR: highest priority */
    /* PRISA: sprite priority 7, matching RBG0's PRIR above. Saturn resolves
     * equal priority in the sprite's favor, so the VDP1 pad-debug text stays
     * visible over the ground plane instead of being hidden behind it.
     */
    r[0x0F0 >> 1] = 0x0707u;
    r[0x02E >> 1] = 0x0000u; /* BMPNB */
    r[0x03A >> 1] = 0x0000u; /* PLSZ: repeat overflow */

    write_back_screen_sky();

    /* BGON: bit 4 = R0ON, bit 12 = R0TPON (disable transparent color code).
     * floor.tga uses palette index 0 as valid texel data; leaving it
     * transparent creates the huge black perspective stripes.
     */
    r[0x020 >> 1] = 0x1010u;

    /* TVMD fixed: DISP=1, BDCLMD=1, 320x224 non-interlaced NTSC. */
    r[0x000 >> 1] = 0x8100u;
}

int main(void) {
    sat_video_config_t cfg = {SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0};
    sat_example_must(sat_init(&cfg));
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_pad_debug_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, PAD_DEBUG_PALETTE
    ));

    /* Upload palette + bitmap + coefficient table before enabling display. */
    sat_example_must(sat_vdp2_palette_upload(
        bg_asset.palette,
        (uint16_t)bg_asset.palette_count,
        0
    ));
    upload_tiled_bitmap(&bg_asset);
    write_coefficient_table();
    write_rotation_params(0, 0);

    /* Bring up RBG0 with our Mode-7 setup. */
    init_rbg0_mode7();

    int32_t last_x = -1, last_y = -1;

    while (1) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_vdp2_wait_vblank_start());
        g_frame_count++;
        sat_example_must(sat_pad_poll(&pad));
        if ((pad.pressed & SAT_PAD_START) != 0) break;

        char pad_debug_text[19];
        char frame_debug_text[14];
        format_pad_debug(pad.held, pad_debug_text);
        format_frame_debug(g_frame_count, frame_debug_text);
        sat_example_must(sat_begin_frame());
        sat_example_must(sat_ascii_font_draw_text_indexed8(
            &g_pad_debug_font, pad_debug_text, -152, -108, 8, 0, 0
        ));
        sat_example_must(sat_ascii_font_draw_text_indexed8(
            &g_pad_debug_font, frame_debug_text, -152, -96, 8, 0, 0
        ));
        sat_example_must(sat_end_frame());

        /* Walking speed in texels/frame; only Mx/My are rewritten during VBlank. */
        const sat_fx16_t spd = 2 << FX16_SHIFT;

        if ((pad.held & SAT_PAD_LEFT))  cam_x -= spd;
        if ((pad.held & SAT_PAD_RIGHT)) cam_x += spd;
        /* tex_y = cam_y + k(y) * GROUND_FORWARD, so larger tex_y is farther away:
         * walking forward moves distant ground toward the viewer => cam_y grows.
         */
        if ((pad.held & SAT_PAD_UP))    cam_y += spd;
        if ((pad.held & SAT_PAD_DOWN))  cam_y -= spd;

        int32_t xi = (int32_t)(cam_x >> FX16_SHIFT);
        int32_t yi = (int32_t)(cam_y >> FX16_SHIFT);
        if (xi != last_x || yi != last_y) {
            write_camera_translation(xi, yi);
            last_x = xi;
            last_y = yi;
        }
        sat_example_must(sat_vdp2_wait_vblank_end());
    }
    return 0;
}
