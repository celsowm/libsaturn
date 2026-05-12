/* vdp2_rbg0_mode7.c - Mode 7 style infinite plane via RBG0
 *
 * Uses direct register writes (like working vdp2_nbg0_image) for RBG0 bitmap.
 * Generates a simple ground pattern in VRAM - no external assets needed.
 *
 * D-pad: scroll forward/back/left/right
 * START: exit
 */
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 224

#define BITMAP_WIDTH  512
#define BITMAP_HEIGHT 256

#define BITMAP_BASE_WORD  0x00000u
#define ROT_PARAM_BASE    0x20000u  /* After bitmap (512x256 = 0x10000 words) */

#define FX16_SHIFT 16

static sat_fx16_t scroll_x = 0;
static sat_fx16_t scroll_y = 0;

/* Build a 256-color palette: checkerboard-friendly with red/green/blue/white */
static void build_palette(uint16_t palette[256]) {
    for (int i = 0; i < 256; i++) {
        uint16_t r = 0, g = 0, b = 0;
        if (i < 64) {
            /* Red ramp: black->red */
            r = (uint16_t)((i * 31) / 63);
        } else if (i < 128) {
            /* Green ramp */
            g = (uint16_t)(((i - 64) * 31) / 63);
        } else if (i < 192) {
            /* Blue ramp */
            b = (uint16_t)(((i - 128) * 31) / 63);
        } else {
            /* White ramp */
            uint16_t v = (uint16_t)(((i - 192) * 31) / 63);
            r = v; g = v; b = v;
        }
        palette[i] = (uint16_t)((b << 10) | (g << 5) | r);
    }
}

/* Fill bitmap with a ground-grid pattern:
 * Horizontal lines every 16px, vertical lines every 32px
 * Uses a checker pattern for the ground */
static void fill_ground_pattern(uint8_t pixels[BITMAP_WIDTH * BITMAP_HEIGHT]) {
    for (int y = 0; y < BITMAP_HEIGHT; y++) {
        for (int x = 0; x < BITMAP_WIDTH; x++) {
            uint8_t col;
            /* Checkerboard: 32x32 blocks */
            int bx = (x >> 5) & 1;
            int by = (y >> 5) & 1;
            if (bx ^ by) {
                col = 0x80u; /* Mid-range color (greenish) */
            } else {
                col = 0x20u; /* Dark color */
            }
            /* Grid lines: horizontal every 16px, vertical every 32px */
            if ((y & 0x0F) == 0) col = 0xE0u; /* bright horizontal line */
            if ((x & 0x1F) == 0) col = 0xE0u; /* bright vertical line */
            /* Horizon gradient: darker at top (far), brighter at bottom (near) */
            if (y < 64) col = (uint8_t)(col >> 2);
            pixels[y * BITMAP_WIDTH + x] = col;
        }
    }
}

/* Upload bitmap to VRAM */
static void upload_bitmap(uint8_t pixels[BITMAP_WIDTH * BITMAP_HEIGHT]) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    uint32_t off = BITMAP_BASE_WORD;
    for (int i = 0; i < BITMAP_WIDTH * BITMAP_HEIGHT; i += 2) {
        vram[off++] = (uint16_t)((pixels[i] << 8) | pixels[i + 1]);
    }
}

/* Init RBG0 with direct register writes (like working vdp2_nbg0_image) */
static void init_rbg0_mode7(void) {
    volatile uint16_t* r = (volatile uint16_t*)0x25F80000u;

    r[0x000 >> 1] = 0x0000u;  /* TVMD off */

    /* RAMCTL: 512K mode, bank A0 = bitmap + param */
    r[0x00E >> 1] = 0x1103u;

    /* Cycle patterns: bank A0 = RBG0 bitmap fetch (9) + CPU (E) */
    r[0x010 >> 1] = 0x9E9Eu;
    r[0x012 >> 1] = 0x9E9Eu;
    r[0x014 >> 1] = 0xEEEEu;
    r[0x016 >> 1] = 0xEEEEu;
    r[0x018 >> 1] = 0xEEEEu;
    r[0x01A >> 1] = 0xEEEEu;
    r[0x01C >> 1] = 0xEEEEu;
    r[0x01E >> 1] = 0xEEEEu;

    r[0x020 >> 1] = 0x0000u;  /* BGON off */

    /* CHCTLB: 256-color (0x1 << 12), 512x256 (0 << 10), bitmap on (1 << 9) */
    r[0x02A >> 1] = 0x1200u;

    /* MPOFR: bitmap bank = 0 (A0) */
    r[0x03E >> 1] = 0x0000u;

    /* RPTA: rotation table at 0x10000 */
    r[0x0B8 >> 1] = 0x0001u;
    r[0x0BA >> 1] = 0x0000u;

    r[0x0B0 >> 1] = 0x0000u;  /* RPMD = A */
    r[0x0B2 >> 1] = 0x0000u;  /* RPRCTL */
    r[0x0B4 >> 1] = 0x0000u;  /* KTCTL */
    r[0x0FC >> 1] = 0x0007u;  /* PRIR = 7 */
    r[0x02E >> 1] = 0x0000u;  /* BMPNB = 0 */
    r[0x03A >> 1] = 0x0000u;  /* PLSZ: repeat overflow */

    r[0x020 >> 1] = 0x0010u;  /* BGON: RBG0 on */
    r[0x000 >> 1] = 0x8100u;  /* TVMD on */
}

/* Write rotation params directly to VRAM for flat Mode 7 ground */
static void write_mode7_params(int32_t scroll_int_x, int32_t scroll_int_y) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    uint32_t b = ROT_PARAM_BASE;

    /* Zero entire table */
    for (int i = 0; i < 48; i++) vram[b + i] = 0;

    /* For Mode 7 ground plane:
     * - Identity rotation (no tilt)
     * - DeltaYst = 1.0 (Y advances 1 per scanline)
     * - DeltaX = 1.0 (X advances 1 per dot)
     * - Scaling = 1.0
     *
     * fx16 format: int in upper word, frac in lower word
     * So 1.0 = 0x0001 (int=1, frac=0)
     * frac=0x0100 means 1.0 in upper 8 bits of frac field
     */

    /* Xst, Yst = scroll position */
    vram[b + 0] = (uint16_t)(scroll_int_x & 0x07FF);  /* Xst integer */
    vram[b + 1] = 0x0000;  /* Xst frac */
    vram[b + 2] = (uint16_t)(scroll_int_y & 0x07FF);  /* Yst integer */
    vram[b + 3] = 0x0000;  /* Yst frac */

    /* DeltaYst: Y advances 1 per scanline */
    vram[b + 9] = 0x0100;  /* frac = 0x0100 = 1.0 in upper bits */

    /* DeltaX: X advances 1 per dot */
    vram[b + 10] = 0x0001;  /* int = 1 */

    /* Identity rotation matrix */
    vram[b + 14] = 0x0001;  /* A = 1.0 */
    vram[b + 22] = 0x0001;  /* E = 1.0 */

    /* Scaling = 1.0 */
    vram[b + 38] = 0x0001;  /* Kx int */
    vram[b + 40] = 0x0001;  /* Ky int */
}

int main(void) {
    uint16_t palette[256];
    uint8_t pixels[BITMAP_WIDTH * BITMAP_HEIGHT];

    build_palette(palette);
    fill_ground_pattern(pixels);

    sat_video_config_t cfg = {SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0};
    sat_example_must(sat_init(&cfg));

    /* Upload palette and bitmap */
    sat_example_must(sat_vdp2_palette_upload(palette, 256, 0));
    upload_bitmap(pixels);

    /* Write rotation params BEFORE init (VDP2 latches params at init time) */
    write_mode7_params(0, 0);

    /* Init RBG0 (enables display and latches rotation params) */
    init_rbg0_mode7();

    sat_example_must(sat_vdp2_back_color_set(0x0000));

    int32_t last_scroll_x = -1;
    int32_t last_scroll_y = -1;

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

        /* Write rotation params every frame */
        int32_t xi = (int32_t)(scroll_x >> FX16_SHIFT);
        int32_t yi = (int32_t)(scroll_y >> FX16_SHIFT);
        if (xi != last_scroll_x || yi != last_scroll_y) {
            write_mode7_params(xi, yi);
            last_scroll_x = xi;
            last_scroll_y = yi;
        }

        /* Flash backdrop to prove VDP2 is running */
        volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
        vram[0x3FFFF] = (yi & 0x10) ? 0x001F : 0x03E0;
    }
    return 0;
}
