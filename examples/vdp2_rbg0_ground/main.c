/* vdp2_rbg0_ground.c - Infinite Mode-7 ground using VDP2 RBG0.
 *
 * Sky in top half (RBG0 transparent -> BACK screen color).
 * Floor in bottom half rendered through a per-scanline coefficient table
 * with KMD=0 (k applied to both kx and ky), giving classic Mode-7 depth.
 *
 * D-Pad UP/DOWN walks forward/back, LEFT/RIGHT strafes. START exits.
 * NO external assets - palette and bitmap are generated procedurally.
 *
 * VRAM layout:
 *   A0 (0x00000..0x0FFFF words): RBG0 bitmap, 512x256 8bpp
 *   A1 (0x10000..0x10017 words): rotation parameter A table
 *   A1 (0x12000..0x121BF words): coefficient table (224 lines x 2 words)
 */
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 224

#define BITMAP_WIDTH  512
#define BITMAP_HEIGHT 256

/* Mode-7 layout */
#define HORIZON       112u                              /* y row of vanishing point */
#define CX            (SCREEN_WIDTH / 2)                /* 160 */
#define CY            HORIZON                           /* 112 */
#define FOCAL         (SCREEN_HEIGHT - 1u - HORIZON)    /* 111 */

#define BM_BASE_WORD   0x00000u
#define RP_BASE_WORD   0x10000u
#define COEF_BASE_WORD 0x12000u

#define FX16_SHIFT 16

static sat_fx16_t cam_x = 0;
static sat_fx16_t cam_y = 0;

/* Build a simple ground/sky palette.
 * 0..63   : sky gradient (used by BACK screen visually too, optional)
 * 64..255 : earth-tone grid colors
 */
static void build_palette(uint16_t palette[256]) {
    for (int i = 0; i < 256; i++) {
        uint16_t r, g, b;
        if (i < 64) {
            /* Sky gradient: dark blue -> cyan (unused by floor pixels) */
            r = (uint16_t)(i / 24);
            g = (uint16_t)(8 + (i * 16) / 63);
            b = (uint16_t)(16 + (i * 15) / 63);
        } else if (i < 128) {
            int j = i - 64;
            r = (uint16_t)(10 + (j * 16) / 63);
            g = (uint16_t)(4 + (j * 10) / 63);
            b = 0;
        } else if (i < 192) {
            int j = i - 128;
            r = (uint16_t)(20 + (j * 10) / 63);
            g = (uint16_t)(10 + (j * 12) / 63);
            b = (uint16_t)(j / 21);
        } else {
            int j = i - 192;
            uint16_t v = (uint16_t)(26 + (j * 5) / 63);
            r = v; g = (uint16_t)(v - 5); b = (uint16_t)(v / 4);
        }
        palette[i] = (uint16_t)((b << 10) | (g << 5) | r);
    }
}

/* Fill bitmap with a tiled ground grid. 16x16 cells, 32-pixel major
 * gridlines, slightly varied earth tones for depth cueing.
 */
static void fill_ground_pattern(uint8_t pixels[BITMAP_WIDTH * BITMAP_HEIGHT]) {
    for (int y = 0; y < BITMAP_HEIGHT; y++) {
        for (int x = 0; x < BITMAP_WIDTH; x++) {
            int bx = (x >> 4) & 1;
            int by = (y >> 4) & 1;
            uint8_t col = (uint8_t)((bx ^ by) ? 130 : 90);
            if ((y & 0x1F) == 0) col = 210;
            if ((x & 0x1F) == 0) col = 210;
            pixels[y * BITMAP_WIDTH + x] = col;
        }
    }
}

static void upload_bitmap(uint8_t pixels[BITMAP_WIDTH * BITMAP_HEIGHT]) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    uint32_t off = BM_BASE_WORD;
    for (int i = 0; i < BITMAP_WIDTH * BITMAP_HEIGHT; i += 2) {
        vram[off++] = (uint16_t)((pixels[i] << 8) | pixels[i + 1]);
    }
}

/* Coefficient table (2-word, mode 0):
 * word0: bit15 = transparent, bit7 = sign, bits6..0 = integer
 * word1: 16-bit fraction
 *
 * k(y) = FOCAL / (y - HORIZON) for y > HORIZON  (smaller k -> closer / larger texture)
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
            uint32_t d = y - HORIZON;                /* 1..111 */
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

    /* Wrap camera into the bitmap so "repeat overflow" tiles seamlessly. */
    int32_t sx = (int32_t)((uint32_t)cam_xi & (uint32_t)(BITMAP_WIDTH  - 1));
    int32_t sy = (int32_t)((uint32_t)cam_yi & (uint32_t)(BITMAP_HEIGHT - 1));
    int32_t mx = sx - (int32_t)CX;
    int32_t my = sy - (int32_t)CY;

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

    /* Cycle patterns: keep bitmap reads on A0, param/coef reads on A1. */
    r[0x010 >> 1] = 0x9E9Eu; /* CYCA0L */
    r[0x012 >> 1] = 0x9E9Eu; /* CYCA0U */
    r[0x014 >> 1] = 0x8E8Eu; /* CYCA1L */
    r[0x016 >> 1] = 0x8E8Eu; /* CYCA1U */
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

    /* BGON: bit 4 = R0ON (enable RBG0). All NBG layers off. */
    r[0x020 >> 1] = 0x0010u;

    /* TVMD fixed: DISP=1, BDCLMD=1, 320x224 non-interlaced NTSC. */
    r[0x000 >> 1] = 0x8100u;
}

int main(void) {
    uint16_t palette[256];
    uint8_t pixels[BITMAP_WIDTH * BITMAP_HEIGHT];

    build_palette(palette);
    fill_ground_pattern(pixels);

    sat_video_config_t cfg = {SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0};
    sat_example_must(sat_init(&cfg));

    /* Upload palette + bitmap + coefficient table before enabling display. */
    sat_example_must(sat_vdp2_palette_upload(palette, 256, 0));
    upload_bitmap(pixels);
    write_coefficient_table();
    write_rotation_params(0, 0);

    /* Bring up RBG0 with our Mode-7 setup. */
    init_rbg0_mode7();

    /* Sky color (BACK screen) configured AFTER RBG0 init so nothing
     * else can disturb BKTAU/BKTAL afterwards.
     * White (0x7FFF) is used here as a diagnostic stark sky color.
     */
    sat_example_must(sat_vdp2_back_color_set(0x7FFFu));

    int32_t last_x = -1, last_y = -1;

    while (1) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_pad_poll(&pad));
        if ((pad.pressed & SAT_PAD_START) != 0) break;

        /* 4 texels / frame walking speed. */
        const sat_fx16_t spd = 4 << FX16_SHIFT;

        if ((pad.held & SAT_PAD_LEFT))  cam_x -= spd;
        if ((pad.held & SAT_PAD_RIGHT)) cam_x += spd;
        if ((pad.held & SAT_PAD_UP))    cam_y -= spd; /* forward = camera world-Y decreases */
        if ((pad.held & SAT_PAD_DOWN))  cam_y += spd;

        int32_t xi = (int32_t)(cam_x >> FX16_SHIFT);
        int32_t yi = (int32_t)(cam_y >> FX16_SHIFT);
        if (xi != last_x || yi != last_y) {
            write_rotation_params(xi, yi);
            last_x = xi;
            last_y = yi;
        }
    }
    return 0;
}
