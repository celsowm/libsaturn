#ifndef SATURN_HUD_H
#define SATURN_HUD_H

#include <stdint.h>
#include "saturn/core.h"
#include "saturn/font.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tiny immediate screen-space overlay. The font and command reservation stay
 * caller-owned; this type contains no retained widget tree or heap state. */
typedef struct sat_hud {
    const sat_ascii_font_t* font;
    uint16_t palette;
    uint16_t char_spacing;
    uint16_t flags;
} sat_hud_t;

sat_result_t sat_hud_init(sat_hud_t* hud, const sat_ascii_font_t* font,
                          uint16_t palette, uint16_t char_spacing);
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
