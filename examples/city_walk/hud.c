/* city_walk diagnostic HUD. Every glyph is one VDP1 command, so the scene
 * reserves CITY_HUD_COMMANDS explicitly instead of letting diagnostics steal
 * world commands. Values are intentionally compact and allocation-free. */
#include <stdint.h>

#include "saturn/font.h"
#include "saturn/hud.h"

#include "city_walk.h"

static sat_hud_t g_hud;

static void note(sat_result_t st) {
    if (st != SAT_OK && g_city.hud_status == 0u) g_city.hud_status = (uint32_t)(-st);
}

static uint32_t kib(uint32_t bytes) {
    return bytes / 1024u;
}

void hud_init(const sat_ascii_font_t* font) {
    note(sat_hud_init(&g_hud, font, CITY_PALETTE_FONT, SAT_ASCII_FONT_GLYPH_WIDTH));
}

void hud_draw(const city_view_t* view, uint32_t vblanks_per_frame, uint32_t frame) {
    const uint32_t cart_percent = g_city.cart_capacity != 0u
        ? (uint32_t)(((uint64_t)g_city.cart_used_bytes * 100u) / g_city.cart_capacity)
        : 0u;

    /* Frame/render pressure. */
    note(sat_hud_value(&g_hud, "VBL ", vblanks_per_frame, 8, 8));
    note(sat_hud_value(&g_hud, "FACE ", g_city.max_world_faces, 88, 8));
    note(sat_hud_value(&g_hud, "CMD ", g_city.max_vdp1_commands, 208, 8));

    /* Physical 4 MB RAM-cart allocator usage, not CITY.BIN file size. */
    note(sat_hud_value(&g_hud, "CARTK ", kib(g_city.cart_used_bytes), 8, 20));
    note(sat_hud_value(&g_hud, "FREEK ", kib(g_city.cart_free_bytes), 112, 20));
    note(sat_hud_value(&g_hud, "CART% ", cart_percent, 224, 20));

    /* Streaming residency. */
    note(sat_hud_value(&g_hud, "RES ", g_city.resident_slots, 8, 32));
    note(sat_hud_value(&g_hud, "LOAD ", g_city.chunks_loaded, 104, 32));
    note(sat_hud_value(&g_hud, "EVICT ", g_city.evictions, 208, 32));

    /* Texturing and parallel renderer. UPK is cumulative cart -> VDP1 traffic. */
    note(sat_hud_value(&g_hud, "TEXF ", g_city.max_textured_faces, 8, 44));
    note(sat_hud_value(&g_hud, "UPK ", kib(g_city.texture_bytes_uploaded), 104, 44));
    note(sat_hud_value(&g_hud, "SLV ", g_city.parallel_backend, 224, 44));

    /* Position/orientation. */
    note(sat_hud_value(&g_hud, "X ", (uint32_t)view->pos.chunk_x, 8, 56));
    note(sat_hud_value(&g_hud, "Z ", (uint32_t)view->pos.chunk_z, 72, 56));
    note(sat_hud_value(&g_hud, "YAW ", (uint32_t)(view->yaw >> 16), 136, 56));

    if (frame < 400u) {
        note(sat_hud_text(&g_hud, "CITY BY COSTOWRLD CC-BY", 8, 210));
    }
}
