/* city_walk: first-person walk through a low-poly city that does not fit in
 * memory. CITY.BIN streams from the CD into a 4 MB RAM cart once at boot, then
 * pages from the cart into work RAM as the player walks.
 *
 * main.c owns start-up order and the frame loop; see city_walk.h for the rest.
 * City model: "Low Poly City Game-Ready" by costoWRLD, CC-BY-4.0 (see
 * assets/LICENSE.txt).
 */
#include <stdint.h>

#include "saturn/color.h"
#include "saturn/example_util.h"
#include "saturn/font.h"
#include "saturn/input.h"
#include "saturn/render2d.h"
#include "saturn/resource_plan.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"
#include "saturn/video.h"

#include "city_walk.h"
#include "city_walk/city_data.h"

#define PLAYER_EYE_HEIGHT_FX (2 * 65536)

static sat_ascii_font_t g_font;
static city_player_t g_player;
static sat_resource_plan_t g_resources;
static sat_resource_plan_entry_t g_resource_entries[8];

/* Declares every fixed reservation up front, against the limits of the memory
 * it lands in, before anything is uploaded or allocated: a size change that no
 * longer fits fails here with a code instead of corrupting something later.
 * The RAM cart limit is the 4 MiB the archive needs; whether one is actually
 * fitted is the loader's check, which owns the refusal screen. */
static sat_result_t plan_resources(void) {
    sat_result_t st = sat_resource_plan_init(
        &g_resources, g_resource_entries,
        (uint16_t)(sizeof(g_resource_entries) / sizeof(g_resource_entries[0])));
    if (st != SAT_OK) return st;
    if ((st = sat_resource_plan_set_limit(&g_resources, SAT_RESOURCE_RAM_CART, 4u * 1024u * 1024u)) != SAT_OK ||
        (st = sat_resource_plan_set_limit(&g_resources, SAT_RESOURCE_VDP2_VRAM, 512u * 1024u)) != SAT_OK ||
        (st = sat_resource_plan_set_limit(&g_resources, SAT_RESOURCE_CRAM, 4096u)) != SAT_OK ||
        (st = sat_resource_plan_set_limit(&g_resources, SAT_RESOURCE_VDP1_COMMANDS, 2048u)) != SAT_OK ||
        (st = sat_resource_plan_set_limit(&g_resources, SAT_RESOURCE_WRAM, 512u * 1024u)) != SAT_OK) {
        return st;
    }
    /* The cart holds the blob region and 32 bytes of read slack. */
    if ((st = sat_resource_plan_add(&g_resources, SAT_RESOURCE_RAM_CART,
                                    CITY_BLOB_REGION_BYTES + 32u, 32u, 1u)) != SAT_OK) return st;
    /* VDP2 VRAM: ground bitmap, rotation table, coefficient table, sky pixels and map. */
    if ((st = sat_resource_plan_add(&g_resources, SAT_RESOURCE_VDP2_VRAM,
                                    CITY_GROUND_BYTES + 96u + CITY_H * 4u + 512u * 128u +
                                        SAT_VDP2_NBG0_MAP_CELLS * 2u, 32u, 1u)) != SAT_OK) return st;
    /* CRAM: ground, sky, font and solid-colour banks, 256 entries of 2 bytes each. */
    if ((st = sat_resource_plan_add(&g_resources, SAT_RESOURCE_CRAM, 4u * 512u, 2u, 1u)) != SAT_OK) return st;
    /* VDP1 commands: the face budget, the HUD reserve, the clip slack and the frame's setup. */
    if ((st = sat_resource_plan_add(&g_resources, SAT_RESOURCE_VDP1_COMMANDS,
                                    CITY_FACE_CAP + 96u + 48u + 3u, 1u, 1u)) != SAT_OK) return st;
    /* Work RAM (.wram_l): the residency rings and everything the renderer owns. */
    if ((st = sat_resource_plan_add(&g_resources, SAT_RESOURCE_WRAM,
                                    CITY_POOL_BYTES + render_wram_bytes(), 32u, 1u)) != SAT_OK) return st;
    return sat_resource_plan_finalize(&g_resources);
}

static void progress(const char* stage, uint8_t percent) {
    sat_example_loading_frame(&g_font, "CITY WALK", stage, percent, CITY_W, CITY_H);
}

static void centred(const char* text, int y) {
    (void)sat_ascii_font_draw_text_screen_centered_indexed8(
        &g_font, text, (int)(CITY_W / 2u), y, 8, 0u, 0u);
}

/* Refusal and failure screens never return: they redraw until power-off. The
 * background colour differs per cause so a screenshot identifies it. */
static void stop_with_message(uint16_t background, const char* line1, const char* line2,
                              const char* line3, const char* line4, uint32_t code) {
    for (;;) {
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_begin_frame());
        sat_example_must(sat_draw_rect_screen(0, 0, CITY_W, CITY_H, background));
        centred(line1, 60);
        centred(line2, 86);
        centred(line3, 122);
        centred(line4, 138);
        if (code != 0u) {
            (void)sat_ascii_font_draw_label_u32(&g_font, "ERROR CODE ", code, 96, 170, 8, 0u, 0u);
        }
        sat_example_must(sat_end_frame());
    }
}

static void refuse_or_fail(sat_result_t st) {
    g_city.status = (uint32_t)st;
    if (st == SAT_ERR_NOT_CONNECTED) {
        g_city.state = CITY_STATE_NO_CART;
        stop_with_message(SAT_RGB555(14, 2, 2), "4 MB RAM CARTRIDGE REQUIRED",
                          "NO EXPANSION CARTRIDGE FOUND", "POWER OFF, INSERT THE 4 MB",
                          "RAM CARTRIDGE, THEN RESET", 0u);
    }
    if (st == SAT_ERR_UNSUPPORTED) {
        g_city.state = CITY_STATE_CART_TOO_SMALL;
        stop_with_message(SAT_RGB555(14, 2, 2), "4 MB RAM CARTRIDGE REQUIRED",
                          "THIS IS A 1 MB CARTRIDGE", "POWER OFF, INSERT THE 4 MB",
                          "RAM CARTRIDGE, THEN RESET", 0u);
    }
    g_city.state = CITY_STATE_LOAD_FAILED;
    stop_with_message(SAT_RGB555(12, 8, 0), "COULD NOT LOAD THE CITY", "CHECK THE DISC",
                      "AND THE CARTRIDGE", "THEN RESET", (uint32_t)(-st));
}

/* Frame cost histogram, sampled at the top of consecutive iterations: it is
 * the only point that sees the WHOLE frame. Sampling between begin and end
 * frame measures just the render window, which always fits in one VBlank. */
static void record_cost(uint32_t vblanks) {
    uint32_t bucket = vblanks < 1u ? 0u : (vblanks > 8u ? 7u : vblanks - 1u);
    volatile uint32_t* hist = &g_city.hist0;
    ++hist[bucket];
}

int main(void) {
    const sat_video_config_t video = {CITY_W, CITY_H, SAT_VIDEO_AUTO, 0u};
    sat_pad_state_t pad = {0};
    uint32_t previous;
    uint32_t warmup = 0u;

    telemetry_init();
    sat_example_must(sat_init(&video));
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, CITY_PALETTE_FONT));
    sat_example_must(sat_vdp1_set_erase_transparent());

    /* Ticks per VBlank, so a harness can turn FRT ticks into seconds. */
    {
        uint16_t begin;
        sat_example_must(sat_wait_vblank());
        begin = city_frt_now();
        for (uint32_t i = 0u; i < 10u; ++i) sat_example_must(sat_wait_vblank());
        g_city.ticks_per_vblank = (uint16_t)(city_frt_now() - begin) / 10u;
    }

    g_city.state = CITY_STATE_LOADING;
    progress("CHECKING CARTRIDGE", 0u);
    {
        sat_result_t planned = plan_resources();
        if (planned != SAT_OK) refuse_or_fail(planned);
    }
    {
        /* The residency pool is empty until the loader is done, so its first
         * 64 KiB is the CD staging buffer. */
        sat_result_t st = city_loader_run(g_ring_pool, 64u * 1024u, progress);
        if (st != SAT_OK) refuse_or_fail(st);
    }

    progress("BUILDING MATERIALS", 97u);
    materials_init();
    render_init();
    hud_init(&g_font);
    player_init(&g_player);
    g_player.view.eye_y = g_archive.header.ground_y_fx + PLAYER_EYE_HEIGHT_FX;
    (void)sat_vdp2_back_color_set(SAT_RGB555(12, 20, 28));
    /* The pool is still empty: borrow it for the sky's pixels and map. */
    background_init(g_ring_pool, sizeof(g_ring_pool));
    ground_init();

    progress("PAGING THE CITY", 99u);
    residency_init();
    residency_prime(&g_player.view.pos);
    progress("READY", 100u);

    g_city.state = CITY_STATE_RUNNING;
    previous = sat_frame_count();
    for (;;) {
        const uint32_t top = sat_frame_count();
        const uint32_t cost = top - previous;
        const uint16_t frame_begin = city_frt_now();
        uint16_t mark;
        previous = top;
        if (warmup >= 8u) record_cost(cost);
        else ++warmup;

        sat_example_must(sat_wait_vblank());
        mark = city_frt_now();
        /* VDP2 latches its registers at VBlank, and the VDP1 frame now on screen
         * was drawn from last iteration's view, so the layers use that view too;
         * they are committed before the slow SMPC poll. */
        ground_update(&g_player.view);
        background_update(g_player.view.yaw);
        sat_example_must(sat_vdp2_layers_commit());
        if (warmup >= 8u) g_city.t_vdp2 += (uint16_t)(city_frt_now() - mark);
        mark = city_frt_now();
        sat_example_must(sat_pad_poll(&pad));
        if (pad.pressed & SAT_PAD_X) {
            /* Teleport: refill every ring at once (~30 ms) instead of paging for
             * several seconds through a city that is not there yet. */
            player_next_viewpoint(&g_player);
            residency_prime(&g_player.view.pos);
        } else if (player_update(&g_player, &pad, cost)) {
            residency_recenter(&g_player.view.pos);
        }
        /* One page-in per frame: a 4 KiB copy is 0.34 ms, and a chunk crossing
         * queues at most 21, so a walker sees them all within ~1 s. */
        (void)residency_service_one(&g_player.view.pos);
        if (warmup >= 8u) g_city.t_input += (uint16_t)(city_frt_now() - mark);

        sat_example_must(sat_begin_frame());
        render_frame(&g_player.view);
        mark = city_frt_now();
        sat_example_must(sat_vdp1_overlay_begin());
        hud_draw(&g_player.view, cost, g_city.frames);
        /* The page-in of this frame gets its textures now, when sat_end_frame
         * is about to wait for the VDP1 anyway. */
        residency_upload_textures();
        if (warmup >= 8u) g_city.t_hud += (uint16_t)(city_frt_now() - mark);
        sat_example_must(sat_end_frame());
        if (warmup >= 8u) {
            g_city.t_frame += (uint16_t)(city_frt_now() - frame_begin);
            ++g_city.measured_frames;
        }

        ++g_city.frames;
        if ((g_city.frames & 15u) == 0u) {
            uint32_t resident = 0u;
            for (uint32_t i = 0u; i < CITY_SLOTS_TOTAL; ++i) {
                resident += g_res.slots[i].state == CITY_SLOT_READY || g_res.slots[i].state == CITY_SLOT_EMPTY;
            }
            /* One snapshot, so loads - evictions == resident holds exactly: a
             * slot completes when its textures land, after the page-in. */
            g_city.resident_slots = resident;
            g_city.chunks_loaded = g_res.loads;
            g_city.evictions = g_res.evictions;
        }
        g_city.chunk_x = (uint32_t)g_player.view.pos.chunk_x;
        g_city.chunk_z = (uint32_t)g_player.view.pos.chunk_z;
        g_city.local_x_fx = (uint32_t)g_player.view.pos.local_x;
        g_city.local_z_fx = (uint32_t)g_player.view.pos.local_z;
        g_city.yaw_degrees = (uint32_t)(g_player.view.yaw >> 16);
        g_city.done = 1u;
    }
}
