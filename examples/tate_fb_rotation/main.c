/* tate_fb_rotation - hardware-rotated TATE through sat_tate_fb_init (saturn/tate_fb.h).
 *
 * The VDP1 draws an upright 224 x 448 playfield into a 512 x 512 8 bits/pixel frame buffer
 * (TVMR TVM = 011b, "rotation 8", VDP1 manual 4.1) and the VDP2 reads that frame buffer back
 * through rotation parameter A (VDP2 manual 6.3: Xst, Yst, DXst, DYst, DX, DY), turned a quarter
 * and scaled by 5/7, so sprites need no pre-rotation and the 448-line field fits 320 pixels. RBG0
 * is enabled but empty: the VDP2 only evaluates the rotation tables while a rotation background is
 * on.
 *
 * The playfield holds a test image (corner markers, an asymmetric "F") drawn upright with the
 * ordinary 2D calls, every frame, in the 224 x 448 surface that sat_tate_fb_init makes of the
 * 320 x 224 screen. */
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/tate.h"
#include "saturn/tate_fb.h"
#include "example_util.h"

#define SCREEN_W 320
#define SCREEN_H 224
#define FIELD_W 224
#define FIELD_H 448

enum { C_CLEAR, C_WHITE, C_RED, C_GREEN, C_BLUE, C_YELLOW, C_GRAY, C_MAGENTA, C_COUNT };

/* The drawing surface is the 224 x 448 field, origin at its centre (the library's convention). */
static void rect(int x, int y, int w, int h, uint16_t palette_code) {
    const int16_t x0 = (int16_t)(x - FIELD_W / 2), y0 = (int16_t)(y - FIELD_H / 2);
    const int16_t x1 = (int16_t)(x0 + w - 1), y1 = (int16_t)(y0 + h - 1);
    const sat_quad2_t q = {{x0, x1, x1, x0}, {y0, y0, y1, y1}};
    sat_example_must(sat_draw_quad2_polygon(&q, palette_code));   /* 8 bpp frame buffer: a palette code */
}

static void draw_field(void) {
    rect(0, 0, FIELD_W, FIELD_H, C_GRAY);                  /* the playfield */
    rect(0, 0, FIELD_W, 3, C_WHITE);                       /* border */
    rect(0, FIELD_H - 3, FIELD_W, 3, C_WHITE);
    rect(0, 0, 3, FIELD_H, C_WHITE);
    rect(FIELD_W - 3, 0, 3, FIELD_H, C_WHITE);
    rect(0, 0, 24, 24, C_RED);                             /* logical top left */
    rect(FIELD_W - 24, 0, 24, 24, C_GREEN);                /* top right */
    rect(0, FIELD_H - 24, 24, 24, C_BLUE);                 /* bottom left */
    rect(FIELD_W - 24, FIELD_H - 24, 24, 24, C_YELLOW);    /* bottom right */
    rect(0, FIELD_H / 2 - 1, FIELD_W, 2, C_MAGENTA);       /* halfway line */
    rect(40, 40, 10, 80, C_WHITE);                         /* an F: asymmetric, so mirroring shows */
    rect(40, 40, 40, 10, C_WHITE);
    rect(40, 72, 28, 10, C_WHITE);
}

int main(void) {
    sat_video_config_t cfg = {SCREEN_W, SCREEN_H, 1, 0};
    sat_example_must(sat_init(&cfg));

    static const uint16_t palette[C_COUNT] = {
        0x0000u, 0x7FFFu, 0x001Fu, 0x03E0u, 0x7C00u, 0x03FFu, 0x2108u, 0x7C1Fu};
    sat_example_must(sat_vdp2_palette_upload(palette, C_COUNT, 0u));

    /* The library does the rest: VDP1 rotation framebuffer, the VDP2 table, a dummy RBG0, sprite type C. */
    sat_tate_layout_t layout;
    sat_example_must(sat_tate_layout_init(&layout, SCREEN_W, SCREEN_H, FIELD_W, FIELD_H, SAT_TATE_ROT_CW));
    sat_example_must(sat_vdp2_set_backdrop_color(0x4000u));
    sat_example_must(sat_tate_fb_init(&layout, 0u, 0x10000u));

    while (1) {
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_vdp2_layers_commit());
        sat_example_must(sat_begin_frame());
        draw_field();
        sat_example_must(sat_end_frame());
    }
    return 0;
}
