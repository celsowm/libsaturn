/* city_walk HUD: a few lines of numbers. Each glyph is a VDP1 command, and the
 * scene reserves CITY_HUD_COMMANDS for all of them, so the text is short. */
#include <stdint.h>

#include "saturn/font.h"
#include "saturn/hud.h"

#include "city_walk.h"

static sat_hud_t g_hud;

static void note(sat_result_t st) {
    if (st != SAT_OK && g_city.hud_status == 0u) g_city.hud_status = (uint32_t)(-st);
}

void hud_init(const sat_ascii_font_t* font) {
    note(sat_hud_init(&g_hud, font, CITY_PALETTE_FONT, SAT_ASCII_FONT_GLYPH_WIDTH));
}

void hud_draw(const city_view_t* view, uint32_t vblanks_per_frame, uint32_t frame) {
    /* 60 or 50 divided by VBlanks per frame: the frame rate this frame took. */
    note(sat_hud_value(&g_hud, "VBL ", vblanks_per_frame, 8, 8));
    note(sat_hud_value(&g_hud, "FACES ", g_city.max_world_faces, 8, 20));
    note(sat_hud_value(&g_hud, "X ", (uint32_t)view->pos.chunk_x, 8, 32));
    note(sat_hud_value(&g_hud, "Z ", (uint32_t)view->pos.chunk_z, 56, 32));
    if (frame < 400u) {
        note(sat_hud_text(&g_hud, "CITY BY COSTOWRLD CC-BY", 8, 210));
    }
}
