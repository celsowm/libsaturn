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
    note(sat_hud_value(&g_hud, "CMD ", g_city.actual_world_commands, 208, 8));

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

    /* Planner diagnostics: desired and selected distributions, streaming
     * misses, conservative culls, drops and the next-frame governor budget. */
    note(sat_hud_value(&g_hud, "W0 ", g_city.plan_wanted_lod0, 8, 68));
    note(sat_hud_value(&g_hud, "W1 ", g_city.plan_wanted_lod1, 112, 68));
    note(sat_hud_value(&g_hud, "W2 ", g_city.plan_wanted_lod2, 216, 68));
    note(sat_hud_value(&g_hud, "S0 ", g_city.plan_selected_lod0, 8, 80));
    note(sat_hud_value(&g_hud, "S1 ", g_city.plan_selected_lod1, 112, 80));
    note(sat_hud_value(&g_hud, "S2 ", g_city.plan_selected_lod2, 216, 80));
    note(sat_hud_value(&g_hud, "VIS ", g_city.plan_frustum_rejected, 8, 92));
    note(sat_hud_value(&g_hud, "MISS ", g_city.plan_no_resident, 112, 92));
    note(sat_hud_value(&g_hud, "STALE ", g_city.plan_stale_residency, 216, 92));
    note(sat_hud_value(&g_hud, "DROP ", g_city.plan_optional_dropped, 8, 104));
    note(sat_hud_value(&g_hud, "DEG ", g_city.plan_budget_degraded, 112, 104));
    note(sat_hud_value(&g_hud, "BUD ", g_city.plan_face_budget, 216, 104));
    note(sat_hud_value(&g_hud, "GOV ", g_city.governor_enabled, 8, 116));
    note(sat_hud_value(&g_hud, "GCMD ", g_city.plan_command_budget, 112, 116));
    note(sat_hud_value(&g_hud, "FADE ", g_city.fade_enabled, 216, 116));

    if (frame < 400u) {
        note(sat_hud_text(&g_hud, "CITY BY COSTOWRLD CC-BY", 8, 210));
    }
}
