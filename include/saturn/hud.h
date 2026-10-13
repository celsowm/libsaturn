#ifndef SATURN_HUD_H
#define SATURN_HUD_H

#include <stdint.h>
#include "saturn/core.h"
#include "saturn/font.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tiny immediate screen-space overlay. The font and command reservation stay
 * caller-owned; this type contains no retained widget tree or heap state.
 *
 * The cursor (`cursor_x`, `cursor_y`, `line_height`) is shared state: every
 * sat_hud_line call updates it, so a HUD-owning function can simply call
 * sat_hud_line in order without tracking the row itself. The cursor resets
 * with sat_hud_reset_cursor. */
typedef struct sat_hud {
    const sat_ascii_font_t* font;
    uint16_t palette;
    uint16_t char_spacing;
    uint16_t flags;
    int16_t  cursor_x;
    int16_t  cursor_y;
    uint16_t line_height;
} sat_hud_t;

sat_result_t sat_hud_init(sat_hud_t* hud, const sat_ascii_font_t* font,
                          uint16_t palette, uint16_t char_spacing);
/* All-in-one constructor: initialises `font` as a white-on-black 8x8
 * indexed font in hi-res-aware spacing and binds it to `hud`. Use this
 * instead of the sat_ascii_font_init_8x8_indexed8 + sat_hud_init pair
 * that almost every example still open-codes. The `font` storage must
 * outlive the HUD. */
sat_result_t sat_hud_init_default(sat_hud_t* hud, sat_ascii_font_t* font);
/* Reset the cursor to (cursor_x, cursor_y) -- the next sat_hud_line starts
 * a fresh stack. The default after sat_hud_init* is (4, 4). */
void sat_hud_reset_cursor(sat_hud_t* hud, int x, int y);
/* Format "label: value" at the cursor with line_height spacing, then
 * advance the cursor by line_height pixels. Value is rendered unsigned; use
 * sat_hud_text_signed for negative counts. */
sat_result_t sat_hud_line(sat_hud_t* hud, const char* label, uint32_t value);
sat_result_t sat_hud_text(const sat_hud_t* hud, const char* text,
                          int x, int y);
sat_result_t sat_hud_text_centered(const sat_hud_t* hud, const char* text,
                                   int center_x, int y);
sat_result_t sat_hud_value(const sat_hud_t* hud, const char* label,
                           uint32_t value, int x, int y);
sat_result_t sat_hud_bar(const sat_hud_t* hud, int x, int y, int width,
                         int height, uint32_t value, uint32_t maximum,
                         uint16_t background, uint16_t foreground);

#ifdef __cplusplus
}
#endif
#endif
