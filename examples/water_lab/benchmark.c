#include "lab.h"
#include "example_util.h"
#include "saturn/color.h"
#include "saturn/irq.h"
#include "saturn/time.h"

static sat_ascii_font_t font;
static uint32_t rate_start_fields, rate_start_lists, display_fps, surface_ticks;

/* Big-endian uint32_t words, read through the Ymir harness using the ELF symbol. */
volatile struct {
    uint32_t submitted_tick;
    uint32_t list_serial;
    uint32_t requested_swap_serial;
    uint32_t workload;
    uint32_t world_commands;
    uint32_t skipped_faces;
    uint32_t late_swaps;
    uint32_t draw_timeouts;
    uint32_t frame_commands;
    uint32_t mode;
    uint32_t subdivisions;
    uint32_t unsupported_crossings;
    uint32_t player_immersion;
    uint32_t camera_immersed;
    uint32_t display_serial;
    uint32_t displayed_lists;
    uint32_t display_fields;
    uint32_t preparation_ticks;
    uint32_t flush_ticks;
    uint32_t tick_frequency;
} water_lab_telemetry;

static void swap_requested(uint32_t serial, void* context) {
    (void)context;
    water_lab_telemetry.requested_swap_serial = serial;
}

static void vblank_out(void* context) {
    (void)context;
    ++water_lab_telemetry.display_fields;
    const uint32_t requested = water_lab_telemetry.requested_swap_serial;
    if (requested != water_lab_telemetry.display_serial) {
        water_lab_telemetry.display_serial = requested;
        ++water_lab_telemetry.displayed_lists;
    }
}

void lab_benchmark_scene_timing(uint32_t preparation, uint32_t surface, uint32_t flush) {
    water_lab_telemetry.preparation_ticks = preparation;
    surface_ticks = surface;
    water_lab_telemetry.flush_ticks = flush;
}

void lab_benchmark_init(void) {
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &font, SAT_RGB555(30, 29, 24), SAT_COLOR_BLACK, 0));
    sat_example_must(sat_vdp1_set_swap_handler(swap_requested, 0));
    water_lab_telemetry.tick_frequency = sat_time_frequency_hz();
    if (sat_irq_active()) sat_example_must(sat_irq_set_handler(SAT_IRQ_VBLANK_OUT, vblank_out, 0));
}

static void label(const char* text, uint16_t y) {
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &font, text, 8, y, 8, 0, 0));
}

static void compact(const char* text, uint16_t x, uint16_t y) {
    for (; *text; ++text, x += 6) {
        const unsigned code = (unsigned char)*text;
        if (code > ' ' && code <= '~') {
            sat_example_must(sat_draw_sprite_scaled_screen(
                &font.glyphs[code - 32u], x, y, 6, 7, 0));
        }
    }
}

static void number(const char* title, uint32_t value, uint16_t x, uint16_t y) {
    char text[36];
    sat_example_must(sat_fmt_label_u32(title, value, text, sizeof(text), 0));
    compact(text, x, y);
}

static void fixed(const char* title, sat_fx16_t value, uint16_t x, uint16_t y) {
    char text[SAT_FMT_FX16_MAX];
    uint16_t title_length = 0;
    while (title[title_length]) ++title_length;
    sat_example_must(sat_fmt_fx16(value, 1, text, sizeof(text), 0));
    compact(title, x, y);
    compact(text, x + title_length * 6, y);
}

static void sample_presentation_rate(void) {
    const uint32_t fields = water_lab_telemetry.display_fields;
    const uint32_t lists = water_lab_telemetry.displayed_lists;
    const uint32_t refresh = sat_video_refresh_hz();
    const uint32_t elapsed = fields - rate_start_fields;
    if (elapsed < refresh / 2) return;
    display_fps = ((lists - rate_start_lists) * refresh + elapsed / 2) / elapsed;
    rate_start_fields = fields;
    rate_start_lists = lists;
}

static uint32_t microseconds(uint32_t ticks) {
    const uint32_t frequency = water_lab_telemetry.tick_frequency;
    return frequency ? (uint32_t)((uint64_t)ticks * 1000000u / frequency) : 0;
}

void lab_benchmark_draw(const sat_scene_t* scene, uint32_t tick, uint16_t workload,
                        const lab_water_t* water, uint8_t diagnostics, uint8_t paused,
                        const lab_player_t* player, sat_fx16_t camera_y, uint8_t camera_immersed,
                        sat_result_t fx_result, const sat_fx_compositor_t* fx) {
    static const char* const modes[] = {
        "DRY REFERENCE", "OPAQUE", "MESH", "RGB HALF",
        "UNDERWATER / RGB HALF", "MULTIPASS / VDP2", "RIPPLING WATER"
    };
    sat_scene_stats_t stats = {0};
    sat_vdp1_frame_stats_t frame = {0};
    sat_example_must(sat_scene_stats(scene, &stats));
    sat_example_must(sat_vdp1_frame_stats(&frame));
    water_lab_telemetry.workload = workload;
    water_lab_telemetry.world_commands = stats.world_commands;
    water_lab_telemetry.skipped_faces = stats.skipped_faces;
    water_lab_telemetry.late_swaps = frame.late_swaps;
    water_lab_telemetry.draw_timeouts = frame.draw_timeouts;

    water_lab_telemetry.mode = water->mode;
    water_lab_telemetry.subdivisions = water->subdivisions;
    water_lab_telemetry.unsupported_crossings = lab_unsupported_crossings();
    water_lab_telemetry.player_immersion = (uint32_t)player->immersed;
    water_lab_telemetry.camera_immersed = camera_immersed;
    sample_presentation_rate();
    sat_example_must(sat_vdp1_overlay_begin());
    label("WATER LAB", 8);
    if (water->mode == LAB_MULTIPASS && fx_result != SAT_OK) {
        label(fx_result == SAT_ERR_CAPACITY ? "RGB HALF / FX CAPACITY" :
              fx_result == SAT_ERR_BUSY ? "RGB HALF / FX BUSY" :
              fx_result == SAT_ERR_NOT_FOUND ? "RGB HALF / NO FX FACES" :
              "RGB HALF / FX UNSUPPORTED", 20);
    } else {
        label(modes[water->mode], 20);
    }
    label("X MODE  Y LEVEL  C CAMERA  Z STATS", 207);
    if (paused) label("PAUSED", 195);
    if (diagnostics) {
        enum { TIMING_X = 8, DRAW_X = 112, WATER_X = 214 };
        sat_example_must(sat_draw_rect_screen(4, 30, 312, 59, SAT_RGB555(2, 4, 7)));
        if (sat_irq_active()) number("DISPLAY FPS ", display_fps, TIMING_X, 32);
        else compact("FPS N/A", TIMING_X, 32);
        number("PREP US ", microseconds(water_lab_telemetry.preparation_ticks), TIMING_X, 40);
        number("SURF US ", microseconds(surface_ticks), TIMING_X, 48);
        number("FLUSH US ", microseconds(water_lab_telemetry.flush_ticks), TIMING_X, 56);
        if (water->mode == LAB_MULTIPASS && fx_result == SAT_OK) {
            number("DRAW US ", microseconds(fx->draw_ticks), TIMING_X, 64);
            number("READ US ", microseconds(fx->readback_ticks), TIMING_X, 72);
            number("UPLOAD US ", microseconds(fx->upload_ticks), TIMING_X, 80);
        } else {
            number("TICK ", tick, TIMING_X, 64);
            number("LIST ID ", water_lab_telemetry.display_serial, TIMING_X, 72);
            number("LATE ", frame.late_swaps, TIMING_X, 80);
        }

        number("DRY LOAD ", workload, DRAW_X, 32);
        number("WORLD CMD ", stats.world_commands, DRAW_X, 40);
        number("LAST CMD ", water_lab_telemetry.frame_commands, DRAW_X, 48);
        number("CLIPPED ", stats.clipped_faces, DRAW_X, 56);
        number("SKIPPED ", stats.skipped_faces, DRAW_X, 64);
        number("FALLBACK ", stats.fallback_faces, DRAW_X, 72);
        number("TIMEOUTS ", frame.draw_timeouts, DRAW_X, 80);

        fixed("LEVEL ", water->height, WATER_X, 32);
        number("GRID ", water->subdivisions, WATER_X, 40);
        compact(water->waves ? "WAVE ON" : "WAVE OFF", WATER_X + 48, 40);
        number("PLAYER % ", 100u * (uint32_t)player->immersed / SAT_FX16_ONE, WATER_X, 48);
        fixed("PLAYER Y ", player->position.y, WATER_X, 56);
        compact(camera_immersed ? "CAM UNDERWATER" : "CAM ABOVE", WATER_X, 64);
        fixed("CAM Y ", camera_y, WATER_X, 72);
        number("TEX CROSS ", lab_unsupported_crossings(), WATER_X, 80);
    }
}

void lab_benchmark_submitted(uint32_t tick) {
    sat_vdp1_command_stats_t frame;
    sat_example_must(sat_vdp1_command_stats(&frame));
    water_lab_telemetry.submitted_tick = tick;
    water_lab_telemetry.list_serial = sat_vdp1_list_serial();
    water_lab_telemetry.frame_commands = frame.used;
}
