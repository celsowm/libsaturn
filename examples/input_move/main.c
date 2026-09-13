/* input_move - held movement versus per-press movement, side by side.
 *
 * examples/red_square already shows the D-pad moving something continuously
 * while a direction is held. This one exists for the distinction that catches
 * people out: sat_pad_poll fills in `held`, `pressed` and `released`, and the
 * choice between them is the difference between a character that walks and a
 * cursor that steps one cell per button push.
 *
 * Two markers share the same input:
 *   - the filled square moves every frame a direction is HELD;
 *   - the hollow square moves one cell per PRESS, and ignores the hold.
 * Tap a direction and both move once. Hold it and only the filled one keeps
 * going.
 *
 * A is a speed modifier on the held marker; START recentres both.
 */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/input.h"
#include "saturn/vdp1.h"
#include "saturn/example_util.h"

#define SCREEN_W 320
#define SCREEN_H 224

#define CELL 16
#define MARKER 12

/* Playfield in cells, leaving the top two text rows clear. */
#define FIELD_X 16
#define FIELD_Y 40
#define FIELD_COLS 18
#define FIELD_ROWS 10

#define HELD_SPEED 2
#define HELD_SPEED_FAST 6

#define HUD_PALETTE 1u

#define COLOR_BG SAT_RGB555(1, 2, 6)
#define COLOR_GRID SAT_RGB555(4, 6, 12)
#define COLOR_HELD SAT_RGB555(31, 26, 4)
#define COLOR_STEP SAT_RGB555(6, 28, 31)

static sat_ascii_font_t g_font;

/* The held marker moves in pixels; the stepped one in whole cells. Keeping
 * them in their natural units is the point -- converting both to one unit
 * would hide exactly the difference this example is about. */
static int g_held_x;
static int g_held_y;
static int g_step_col;
static int g_step_row;
static uint32_t g_presses;

static void recenter(void) {
    g_held_x = FIELD_X + ((FIELD_COLS / 2) * CELL);
    g_held_y = FIELD_Y + ((FIELD_ROWS / 2) * CELL);
    g_step_col = FIELD_COLS / 2;
    g_step_row = FIELD_ROWS / 2;
}

static int clamp(int v, int lo, int hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static void draw_rect(int x, int y, int w, int h, uint16_t color) {
    sat_example_must(sat_draw_rect_screen(
        (int16_t)x, (int16_t)y, (uint16_t)w, (uint16_t)h, color));
}

/* Four thin rectangles rather than a filled one, so the two markers stay
 * telling apart even while they overlap. */
static void draw_outline(int x, int y, int w, int h, uint16_t color) {
    draw_rect(x, y, w, 2, color);
    draw_rect(x, y + h - 2, w, 2, color);
    draw_rect(x, y, 2, h, color);
    draw_rect(x + w - 2, y, 2, h, color);
}

static void update_held(const sat_pad_state_t* pad) {
    const int speed = ((pad->held & SAT_PAD_A) != 0u) ? HELD_SPEED_FAST : HELD_SPEED;

    if ((pad->held & SAT_PAD_LEFT) != 0u) {
        g_held_x -= speed;
    }
    if ((pad->held & SAT_PAD_RIGHT) != 0u) {
        g_held_x += speed;
    }
    if ((pad->held & SAT_PAD_UP) != 0u) {
        g_held_y -= speed;
    }
    if ((pad->held & SAT_PAD_DOWN) != 0u) {
        g_held_y += speed;
    }
    g_held_x = clamp(g_held_x, FIELD_X, FIELD_X + ((FIELD_COLS - 1) * CELL));
    g_held_y = clamp(g_held_y, FIELD_Y, FIELD_Y + ((FIELD_ROWS - 1) * CELL));
}

static void update_stepped(const sat_pad_state_t* pad) {
    const uint16_t edges = pad->pressed & (SAT_PAD_LEFT | SAT_PAD_RIGHT | SAT_PAD_UP | SAT_PAD_DOWN);

    if (edges == 0u) {
        return;
    }
    if ((edges & SAT_PAD_LEFT) != 0u) {
        --g_step_col;
    }
    if ((edges & SAT_PAD_RIGHT) != 0u) {
        ++g_step_col;
    }
    if ((edges & SAT_PAD_UP) != 0u) {
        --g_step_row;
    }
    if ((edges & SAT_PAD_DOWN) != 0u) {
        ++g_step_row;
    }
    g_step_col = clamp(g_step_col, 0, FIELD_COLS - 1);
    g_step_row = clamp(g_step_row, 0, FIELD_ROWS - 1);
    ++g_presses;
}

static void draw_field(void) {
    int i;
    for (i = 0; i <= FIELD_COLS; ++i) {
        draw_rect(FIELD_X - 2 + (i * CELL), FIELD_Y - 2, 1, (FIELD_ROWS * CELL) + 2, COLOR_GRID);
    }
    for (i = 0; i <= FIELD_ROWS; ++i) {
        draw_rect(FIELD_X - 2, FIELD_Y - 2 + (i * CELL), (FIELD_COLS * CELL) + 2, 1, COLOR_GRID);
    }
}

static void draw_hud(const sat_pad_state_t* pad) {
    char text[32];

    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font, "HELD MOVES EVERY FRAME", 8, 4, 0, HUD_PALETTE, 0));
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font, "PRESS MOVES ONE CELL", 8, 14, 0, HUD_PALETTE, 0));

    sat_example_must(sat_fmt_label_u32("PRESSES ", g_presses, text, sizeof(text), NULL));
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font, text, 200, 4, 0, HUD_PALETTE, 0));

    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font,
        ((pad->held & SAT_PAD_A) != 0u) ? "A FAST" : "A SPEED  START RESET",
        8,
        SCREEN_H - 12,
        0,
        HUD_PALETTE,
        0));
}

int main(void) {
    sat_example_must(sat_app_init_default());
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, 0x0000u, HUD_PALETTE));

    recenter();

    while (1) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_app_frame_begin(COLOR_BG, COLOR_BG, &pad));

        if ((pad.pressed & SAT_PAD_START) != 0u) {
            recenter();
            g_presses = 0u;
        }
        update_held(&pad);
        update_stepped(&pad);

        draw_field();
        draw_rect(g_held_x, g_held_y, MARKER, MARKER, COLOR_HELD);
        draw_outline(
            FIELD_X + (g_step_col * CELL),
            FIELD_Y + (g_step_row * CELL),
            MARKER,
            MARKER,
            COLOR_STEP);
        draw_hud(&pad);

        sat_example_must(sat_app_frame_end());
    }

    return 0;
}
