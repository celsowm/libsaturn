/* input_debug.c - live HUD of digital pad held/pressed/released state */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/example_util.h"

#define BUTTON_COUNT 13u

static const uint16_t BUTTON_MASKS[BUTTON_COUNT] = {
    SAT_PAD_UP, SAT_PAD_DOWN, SAT_PAD_LEFT, SAT_PAD_RIGHT, SAT_PAD_START,
    SAT_PAD_A, SAT_PAD_B, SAT_PAD_C, SAT_PAD_X, SAT_PAD_Y, SAT_PAD_Z,
    SAT_PAD_L, SAT_PAD_R
};

static sat_ascii_font_t g_font;
static uint8_t g_press_count[BUTTON_COUNT];
static uint8_t g_release_count[BUTTON_COUNT];

static void set_flag_char(char* line, uint16_t pos, uint16_t is_set) {
    line[pos] = (is_set != 0u) ? '1' : '0';
}

static void set_digit_char(char* line, uint16_t pos, uint8_t value) {
    line[pos] = (char)('0' + (value % 10u));
}

static void update_counters(const sat_pad_state_t* pad) {
    for (uint16_t i = 0; i < BUTTON_COUNT; ++i) {
        if ((pad->pressed & BUTTON_MASKS[i]) != 0u) {
            g_press_count[i] = (uint8_t)((g_press_count[i] + 1u) % 10u);
        }
        if ((pad->released & BUTTON_MASKS[i]) != 0u) {
            g_release_count[i] = (uint8_t)((g_release_count[i] + 1u) % 10u);
        }
    }
}

static void draw_held_lines(const sat_pad_state_t* pad, int y) {
    char line1[] = "U:0 D:0 L:0 R:0 S:0";
    char line2[] = "A:0 B:0 C:0 X:0 Y:0 Z:0";
    char line3[] = "TL:0 TR:0";

    set_flag_char(line1, 2u, (pad->held & SAT_PAD_UP) != 0u);
    set_flag_char(line1, 6u, (pad->held & SAT_PAD_DOWN) != 0u);
    set_flag_char(line1, 10u, (pad->held & SAT_PAD_LEFT) != 0u);
    set_flag_char(line1, 14u, (pad->held & SAT_PAD_RIGHT) != 0u);
    set_flag_char(line1, 18u, (pad->held & SAT_PAD_START) != 0u);

    set_flag_char(line2, 2u, (pad->held & SAT_PAD_A) != 0u);
    set_flag_char(line2, 6u, (pad->held & SAT_PAD_B) != 0u);
    set_flag_char(line2, 10u, (pad->held & SAT_PAD_C) != 0u);
    set_flag_char(line2, 14u, (pad->held & SAT_PAD_X) != 0u);
    set_flag_char(line2, 18u, (pad->held & SAT_PAD_Y) != 0u);
    set_flag_char(line2, 22u, (pad->held & SAT_PAD_Z) != 0u);

    set_flag_char(line3, 3u, (pad->held & SAT_PAD_L) != 0u);
    set_flag_char(line3, 8u, (pad->held & SAT_PAD_R) != 0u);

    sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, "HELD:", -156, y, 8, 0, 0));
    sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, line1, -156, y + 12, 8, 0, 0));
    sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, line2, -156, y + 24, 8, 0, 0));
    sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, line3, -156, y + 36, 8, 0, 0));
}

static void draw_count_lines(const char* label, const uint8_t* counts, int y) {
    char line1[] = "U:0 D:0 L:0 R:0 S:0";
    char line2[] = "A:0 B:0 C:0 X:0 Y:0 Z:0";
    char line3[] = "TL:0 TR:0";

    set_digit_char(line1, 2u, counts[0]);
    set_digit_char(line1, 6u, counts[1]);
    set_digit_char(line1, 10u, counts[2]);
    set_digit_char(line1, 14u, counts[3]);
    set_digit_char(line1, 18u, counts[4]);

    set_digit_char(line2, 2u, counts[5]);
    set_digit_char(line2, 6u, counts[6]);
    set_digit_char(line2, 10u, counts[7]);
    set_digit_char(line2, 14u, counts[8]);
    set_digit_char(line2, 18u, counts[9]);
    set_digit_char(line2, 22u, counts[10]);

    set_digit_char(line3, 3u, counts[11]);
    set_digit_char(line3, 8u, counts[12]);

    sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, label, -156, y, 8, 0, 0));
    sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, line1, -156, y + 12, 8, 0, 0));
    sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, line2, -156, y + 24, 8, 0, 0));
    sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, line3, -156, y + 36, 8, 0, 0));
}

int main(void) {
    sat_example_must(sat_app_init_default());
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 1));

    while (1) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_BLACK, &pad));

        update_counters(&pad);

        sat_example_must(sat_ascii_font_draw_text_centered_indexed8(&g_font, "INPUT DEBUG", 0, -108, 8, 0, 0));
        draw_held_lines(&pad, -92);
        draw_count_lines("PRESS COUNT (mod 10):", g_press_count, -40);
        draw_count_lines("RELEASE COUNT (mod 10):", g_release_count, 12);

        sat_example_must(sat_app_frame_end());
    }
}
