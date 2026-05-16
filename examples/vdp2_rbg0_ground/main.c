/* vdp2_rbg0_mode7.c - Panzer Dragoon style infinite ground plane
 *
 * RBG0 bitmap mode with SEPARATE VRAM banks for bitmap and rotation params.
 * This ensures correct cycle patterns: bitmap fetch (9) for A0, param fetch (8) for A1.
 *
 * NO external assets. D-pad UP/DOWN scrolls, START exits.
 */
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 224

#define BITMAP_WIDTH  512
#define BITMAP_HEIGHT 256

/* Bitmap in VRAM-A0 (words 0x00000..0x0FFFF) */
#define BM_BASE_WORD  0x00000u
/* Rotation params also in VRAM-A0, after bitmap (word 0x10000) */
#define RP_BASE_WORD  0x10000u
/* Coefficient table in VRAM-A1 (word 0x12000) */
#define COEF_BASE_WORD 0x12000u

#define FX16_SHIFT 16

static sat_fx16_t scroll_x = 0;
static sat_fx16_t scroll_y = 0;

/* Build earth-tone palette */
static void build_palette(uint16_t palette[256]) {
    for (int i = 0; i < 256; i++) {
        uint16_t r, g, b;
        if (i < 64) {
            /* Sky gradient (blue-cyan range) */
            r = (uint16_t)(i / 24);
            g = (uint16_t)(8 + (i * 16) / 63);
            b = (uint16_t)(16 + (i * 15) / 63);
        } else if (i < 128) {
            /* Dark/medium earth */
            int j = i - 64;
            r = (uint16_t)(10 + (j * 16) / 63);
            g = (uint16_t)(4 + (j * 10) / 63);
            b = 0;
        } else if (i < 192) {
            /* Warm sand */
            int j = i - 128;
            r = (uint16_t)(20 + (j * 10) / 63);
            g = (uint16_t)(10 + (j * 12) / 63);
            b = (uint16_t)(j / 21);
        } else {
            /* Highlight */
            int j = i - 192;
            uint16_t v = (uint16_t)(26 + (j * 5) / 63);
            r = v; g = (uint16_t)(v - 5); b = (uint16_t)(v / 4);
        }
        palette[i] = (uint16_t)((b << 10) | (g << 5) | r);
    }
}

/* Fill bitmap with ground grid pattern */
static void fill_ground_pattern(uint8_t pixels[BITMAP_WIDTH * BITMAP_HEIGHT]) {
    for (int y = 0; y < BITMAP_HEIGHT; y++) {
        for (int x = 0; x < BITMAP_WIDTH; x++) {
            int bx = (x >> 4) & 1;
            int by = (y >> 4) & 1;
            uint8_t col = (uint8_t)((bx ^ by) ? 130 : 90);
            if ((y & 0x0F) == 0) col = 210;
            if ((x & 0x1F) == 0) col = 210;
            pixels[y * BITMAP_WIDTH + x] = col;
        }
    }
}

/* Upload bitmap to VRAM-A0 */
static void upload_bitmap(uint8_t pixels[BITMAP_WIDTH * BITMAP_HEIGHT]) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    uint32_t off = BM_BASE_WORD;
    for (int i = 0; i < BITMAP_WIDTH * BITMAP_HEIGHT; i += 2) {
        vram[off++] = (uint16_t)((pixels[i] << 8) | pixels[i + 1]);
    }
}

/* 2-word coefficient (mode 0/1/2):
 * word0: bit15=transparent, bits14..8 line-color(ignored here),
 *        bit7=sign, bits6..0 integer(7)
 * word1: fraction(16). */
static void encode_coef_2word(float k, int transparent, uint16_t* out_w0, uint16_t* out_w1) {
    if (k < 0.0f) {
        k = 0.0f;
    }
    if (k > 127.999f) {
        k = 127.999f;
    }

    int integer = (int)k;
    float frac_f = k - (float)integer;
    int frac = (int)(frac_f * 65536.0f + 0.5f);
    if (frac > 65535) {
        frac = 65535;
    }

    *out_w0 = (uint16_t)(integer & 0x007F);
    if (transparent) {
        *out_w0 |= 0x8000u;
    }
    *out_w1 = (uint16_t)(frac & 0xFFFF);
}

/* Build line-based coefficient table (224 lines), 2-word entries.
 * Use mode1(kx only) with a narrow range to avoid lateral bowing.
 */
static void write_coefficient_table(void) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    const int horizon = SCREEN_HEIGHT / 2;
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        float k;
        int transparent = 0;
        if (y < horizon) {
            k = 1.0f;
            transparent = 1;
        } else {
            const float t = (float)(y - horizon) / (float)(SCREEN_HEIGHT - 1 - horizon);
            /* Mild perspective without half-pipe curvature. */
            k = 1.25f - (t * 0.35f);
        }
        uint16_t w0, w1;
        encode_coef_2word(k, transparent, &w0, &w1);
        const uint32_t base = COEF_BASE_WORD + ((uint32_t)y * 2u);
        vram[base + 0u] = w0;
        vram[base + 1u] = w1;
    }
}

/* Write rotation params directly to VRAM-A1 */
static void write_rotation_params(int32_t sx, int32_t sy) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    uint32_t b = RP_BASE_WORD;

    /* Zero entire 48-word table */
    for (int i = 0; i < 48; i++) vram[b + i] = 0;

    /* Keep origin in world space; projection center is handled by Px/Cx. */
    const int32_t xst = sx;
    /* Start from the bottom of the texture and walk upward per scanline.
     * This flips "ceiling-like" projection into floor-like depth.
     */
    const int32_t yst = sy + (BITMAP_HEIGHT - 1);

    /* Xst, Yst, Zst */
    vram[b + 0] = (uint16_t)(xst & 0x1FFF);
    vram[b + 1] = 0x0000;
    vram[b + 2] = (uint16_t)(yst & 0x1FFF);
    vram[b + 3] = 0x0000;
    vram[b + 4] = 0x0000;
    vram[b + 5] = 0x0000;

    /* DeltaXst/DeltaYst */
    vram[b + 6] = 0x0000;
    vram[b + 7] = 0x0000;
    vram[b + 8] = 0xFFFF; /* -1.0 per scanline (16.16 integer part) */
    vram[b + 9] = 0x0000;

    /* DeltaX/DeltaY */
    vram[b + 10] = 0x0001;
    vram[b + 11] = 0x0000;
    vram[b + 12] = 0x0000;
    vram[b + 13] = 0x0000;

    /* Identity matrix: A=1.0, E=1.0, others 0 */
    vram[b + 14] = 0x0001;
    vram[b + 15] = 0x0000;
    vram[b + 22] = 0x0001;
    vram[b + 23] = 0x0000;

    /* Scaling = 1.0 */
    vram[b + 38] = 0x0001;
    vram[b + 39] = 0x0000;
    vram[b + 40] = 0x0001;
    vram[b + 41] = 0x0000;

    /* Align perspective center with screen center in X/Y. */
    vram[b + 26] = (uint16_t)(SCREEN_WIDTH / 2); /* Px */
    vram[b + 27] = (uint16_t)(SCREEN_HEIGHT / 2); /* Py */
    vram[b + 28] = 0x0380; /* Pz = 896: balanced depth */
    vram[b + 30] = (uint16_t)(SCREEN_WIDTH / 2); /* Cx */
    vram[b + 31] = (uint16_t)(SCREEN_HEIGHT / 2); /* Cy */
    vram[b + 32] = 0x0000; /* Cz */

    /* Coefficient table addressing (2-word mode):
     * start = (RAKTAOS_low2 * 0x40000) + (KAst * 4)
     * Use RAKTAOS=0 and KAst=0x9000 => byte 0x24000 => word 0x12000.
     */
    vram[b + 42] = 0x9000; /* KAst integer */
    vram[b + 43] = 0x0000; /* KAst fraction */
    vram[b + 44] = 0x0001; /* DeltaKAst integer: next line */
    vram[b + 45] = 0x0000; /* DeltaKAst fraction */
    vram[b + 46] = 0x0000; /* DeltaKAx integer: no per-dot advance */
    vram[b + 47] = 0x0000; /* DeltaKAx fraction */
}

/* Init RBG0 with SEPARATE banks and cycle patterns */
static void init_rbg0_mode7(void) {
    volatile uint16_t* r = (volatile uint16_t*)0x25F80000u;

    /* TVMD off */
    r[0x000 >> 1] = 0x0000u;

    /* RAMCTL: 512K mode
     * A0 = bitmap (bits 1-0 = 11)
     * A1 = params (bits 3-2 = 01)
     */
    r[0x00E >> 1] = 0x1107u;

    /* Cycle patterns:
     * A0: bitmap fetch (9) + CPU (E)
     * A1: param fetch (8) + CPU (E)
     */
    r[0x010 >> 1] = 0x9E9Eu;  /* CYCA0L */
    r[0x012 >> 1] = 0x9E9Eu;  /* CYCA0U */
    r[0x014 >> 1] = 0x8E8Eu;  /* CYCA1L */
    r[0x016 >> 1] = 0x8E8Eu;  /* CYCA1U */
    r[0x018 >> 1] = 0xEEEEu;  /* CYCB0L */
    r[0x01A >> 1] = 0xEEEEu;  /* CYCB0U */
    r[0x01C >> 1] = 0xEEEEu;  /* CYCB1L */
    r[0x01E >> 1] = 0xEEEEu;  /* CYCB1U */

    /* BGON off during config */
    r[0x020 >> 1] = 0x0000u;

    /* CHCTLB: 256-color (1 << 12), 512x256 (0 << 10), bitmap on (1 << 9) */
    r[0x02A >> 1] = 0x1200u;

    /* MPOFR: bitmap bank = 0 (A0) */
    r[0x03E >> 1] = 0x0000u;

    /* RPTA: rotation table at word 0x10000 (A1 bank start)
     * RPTAU = 0x10000 >> 16 = 1
     * RPTAL = 0x10000 & 0xFFFE = 0x0000
     */
    r[0x0BC >> 1] = 0x0001u;
    r[0x0BE >> 1] = 0x0000u;

    r[0x0B0 >> 1] = 0x0000u;  /* RPMD = A */
    r[0x0B2 >> 1] = 0x0000u;  /* RPRCTL: default reads */
    r[0x0B4 >> 1] = 0x0005u;  /* KTCTL: A coeff enable + 2-word + mode1(kx only) */
    r[0x0B6 >> 1] = 0x0000u;  /* KTAOF: RAKTAOS=0 */
    r[0x0FC >> 1] = 0x0007u;  /* PRIR = 7 */
    r[0x02E >> 1] = 0x0000u;  /* BMPNB = 0 */
    r[0x03A >> 1] = 0x0000u;  /* PLSZ: repeat overflow */

    /* BGON: bit 4 = R0ON (RBG0 enable) */
    r[0x020 >> 1] = 0x0010u;

    /* TVMD: re-enable display, black border (disable back-screen border mode) */
    r[0x000 >> 1] = 0x8000u;
}

int main(void) {
    uint16_t palette[256];
    uint8_t pixels[BITMAP_WIDTH * BITMAP_HEIGHT];

    build_palette(palette);
    fill_ground_pattern(pixels);

    sat_video_config_t cfg = {SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0};
    sat_example_must(sat_init(&cfg));

    /* Upload data */
    sat_example_must(sat_vdp2_palette_upload(palette, 256, 0));
    upload_bitmap(pixels);
    write_coefficient_table();

    /* Write rotation params BEFORE init */
    write_rotation_params(0, 0);

    /* Init RBG0 */
    init_rbg0_mode7();

    sat_example_must(sat_vdp2_back_color_set(0x7C00));

    int32_t last_x = -1, last_y = -1;

    while (1) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_pad_poll(&pad));
        if ((pad.pressed & SAT_PAD_START) != 0) break;

        const sat_fx16_t spd = 2 << FX16_SHIFT;
        if ((pad.held & SAT_PAD_LEFT))  scroll_x -= spd;
        if ((pad.held & SAT_PAD_RIGHT)) scroll_x += spd;
        if ((pad.held & SAT_PAD_UP))    scroll_y -= spd;
        if ((pad.held & SAT_PAD_DOWN))  scroll_y += spd;

        int32_t xi = (int32_t)(scroll_x >> FX16_SHIFT);
        int32_t yi = (int32_t)(scroll_y >> FX16_SHIFT);
        if (xi != last_x || yi != last_y) {
            write_rotation_params(xi, yi);
            last_x = xi;
            last_y = yi;
        }

    }
    return 0;
}
