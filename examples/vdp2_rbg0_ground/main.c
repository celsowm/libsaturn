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
 *   A1 (0x10000..0x10017 words): rotation parameter A table
 *   A1 (0x12000..0x121BF words): coefficient table (224 lines x 2 words)
 *   A1 (0x3FFFF): BACK screen color
 */
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"
#include "vdp2_rbg0_ground/bg.h"

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

#define BM_BASE_WORD   0x00000u
#define RP_BASE_WORD   0x10000u
#define COEF_BASE_WORD 0x12000u
#define BACK_COLOR_WORD 0x3FFFFu

#define SKY_COLOR     0x7F45u

#define FX16_SHIFT 16

static sat_fx16_t cam_x = 0;
static sat_fx16_t cam_y = 0;

static void compute_wrapped_camera_translation(int32_t cam_xi, int32_t cam_yi, int32_t* mx, int32_t* my) {
    const int32_t sx = (int32_t)((uint32_t)cam_xi & (uint32_t)(BITMAP_WIDTH  - 1));
    const int32_t sy = (int32_t)((uint32_t)cam_yi & (uint32_t)(BITMAP_HEIGHT - 1));

    *mx = sx - (int32_t)CX;
    *my = sy - (int32_t)CY;
}

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

/* Coefficient table (2-word, mode 0):
 * word0: bit15 = transparent, bit7 = sign, bits6..0 = integer
 * word1: 16-bit fraction
 *
 * k(y) = FOCAL / depth for y > HORIZON.
 * MIN_DEPTH avoids the near-horizon singularity that produced huge stretched
 * streaks in the screenshot.
 * y <= HORIZON => transparent line, BACK screen shows through (sky).
 */
static void write_coefficient_table(void) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    for (uint32_t y = 0; y < (uint32_t)SCREEN_HEIGHT; y++) {
        uint16_t w0, w1;
        if (y <= HORIZON) {
            w0 = 0x8000u;
            w1 = 0x0000u;
        } else {
            uint32_t d = (y - HORIZON) + MIN_DEPTH;
            /* k in 16.16: round((FOCAL << 16) / d) */
            uint32_t k16 = (((uint32_t)FOCAL << 16) + (d / 2u)) / d;
            uint32_t integer = (k16 >> 16) & 0x007Fu;
            uint32_t frac    = k16 & 0xFFFFu;
            w0 = (uint16_t)integer;
            w1 = (uint16_t)frac;
        }
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

/* Rotation parameter A table at word 0x10000.
 * Camera position lives in Mx/My (parallel-translation) so the per-line
 * coefficient does NOT scale the camera, only the per-pixel deltas.
 *
 * Effective sample point (per pixel) for line y:
 *   tex_x = cam_x + k(y) * (screen_x - 160)
 *   tex_y = cam_y + k(y) * 111
 */
static void write_rotation_params(int32_t cam_xi, int32_t cam_yi) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    uint32_t b = RP_BASE_WORD;
    int32_t mx, my;

    compute_wrapped_camera_translation(cam_xi, cam_yi, &mx, &my);

    /* Zero the whole 48-word table first. */
    for (int i = 0; i < 48; i++) vram[b + i] = 0;

    /* Xst = 0, Yst = FOCAL (so that at horizon offset 0 sample lands at camera) */
    vram[b +  0] = 0x0000;          /* Xst integer */
    vram[b +  1] = 0x0000;          /* Xst fraction */
    vram[b +  2] = (uint16_t)FOCAL; /* Yst integer = 111 */
    vram[b +  3] = 0x0000;          /* Yst fraction */
    /* Zst = 0 already zeroed */

    /* DeltaXst/DeltaYst = 0 (per-line stepping is encoded in coefficient k) */
    /* already zero */

    /* DeltaX = +1 per pixel, DeltaY = 0 */
    vram[b + 10] = 0x0001;
    vram[b + 12] = 0x0000;

    /* Identity rotation matrix: A=E=1, others 0 */
    vram[b + 14] = 0x0001; /* A */
    vram[b + 22] = 0x0001; /* E */

    /* Viewpoint Px,Py,Pz */
    vram[b + 26] = (uint16_t)CX; /* Px = 160 */
    vram[b + 27] = (uint16_t)CY; /* Py = 112 */
    vram[b + 28] = 0x0000;       /* Pz */

    /* Center Cx,Cy,Cz */
    vram[b + 30] = (uint16_t)CX;
    vram[b + 31] = (uint16_t)CY;
    vram[b + 32] = 0x0000;

    /* Parallel translation = camera position relative to projection center.
     * Stored as signed 13-bit integer + 10-bit fraction (Mx/My layout).
     */
    vram[b + 34] = (uint16_t)(mx & 0x1FFF); /* Mx integer */
    vram[b + 35] = 0x0000;                  /* Mx fraction */
    vram[b + 36] = (uint16_t)(my & 0x1FFF); /* My integer */
    vram[b + 37] = 0x0000;                  /* My fraction */

    /* kx/ky fallback = 1.0; ignored once coefficient table drives k. */
    vram[b + 38] = 0x0001;
    vram[b + 40] = 0x0001;

    /* Coefficient table addressing (2-word, RAKTAOS=0):
     *   start_byte = (KAst_integer) * 4
     * COEF_BASE_WORD = 0x12000 (word) => byte 0x24000 => KAst = 0x9000.
     */
    vram[b + 42] = 0x9000; /* KAst integer */
    vram[b + 43] = 0x0000; /* KAst fraction */
    vram[b + 44] = 0x0001; /* DeltaKAst: next coefficient per scanline */
    vram[b + 45] = 0x0000;
    vram[b + 46] = 0x0000; /* DeltaKAx = 0 (line-based, not per-dot) */
    vram[b + 47] = 0x0000;
}

static void write_camera_translation(int32_t cam_xi, int32_t cam_yi) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    int32_t mx, my;

    compute_wrapped_camera_translation(cam_xi, cam_yi, &mx, &my);

    vram[RP_BASE_WORD + 34u] = (uint16_t)(mx & 0x1FFF);
    vram[RP_BASE_WORD + 35u] = 0x0000;
    vram[RP_BASE_WORD + 36u] = (uint16_t)(my & 0x1FFF);
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
        sat_example_must(sat_pad_poll(&pad));
        if ((pad.pressed & SAT_PAD_START) != 0) break;

        /* Faster walking speed; only Mx/My are rewritten during VBlank. */
        const sat_fx16_t spd = 16 << FX16_SHIFT;

        if ((pad.held & SAT_PAD_LEFT))  cam_x -= spd;
        if ((pad.held & SAT_PAD_RIGHT)) cam_x += spd;
        if ((pad.held & SAT_PAD_UP))    cam_y -= spd; /* forward = camera world-Y decreases */
        if ((pad.held & SAT_PAD_DOWN))  cam_y += spd;

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
