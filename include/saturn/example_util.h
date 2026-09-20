#ifndef SATURN_EXAMPLE_UTIL_H
#define SATURN_EXAMPLE_UTIL_H

#include "saturn/core.h"
#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/render2d.h"
#include "saturn/video.h"

static inline void sat_example_must(sat_result_t st) {
    if (st != SAT_OK) {
        while (1) {
        }
    }
}

/* A whole boot progress frame: title, current stage and a percentage bar.
 *
 * Procedural startup work (generating an ocean bitmap, importing a model,
 * synthesising audio) takes long enough that a black screen looks like a
 * hang, so examples that do it end up writing the same panel. This draws
 * VDP1-only, so it works before any VDP2 layer is configured, and it is a
 * report of work already finished rather than a timed fake.
 *
 * Call it between units of startup work; it waits for VBlank and presents. */
static inline void sat_example_loading_frame(
    const sat_ascii_font_t* font, const char* title, const char* stage,
    uint8_t percent, uint16_t width, uint16_t height) {
    const uint16_t bar_x = 34u;
    const uint16_t bar_w = 252u;
    const int centre = (int)(width / 2u);
    char progress[SAT_FMT_U32_MAX + 12u];
    uint16_t filled;
    if (percent > 100u) percent = 100u;
    filled = (uint16_t)(((uint32_t)percent * bar_w) / 100u);
    sat_example_must(sat_wait_vblank());
    sat_example_must(sat_begin_frame());
    sat_example_must(sat_draw_rect_screen(
        0, 0, width, height, SAT_RGB555(2, 7, 14)));
    sat_example_must(sat_draw_rect_screen(
        bar_x, 64, bar_w, 3u, SAT_RGB555(12, 26, 27)));
    sat_example_must(sat_ascii_font_draw_text_screen_centered_indexed8(
        font, title, centre, 77, 8, 0u, 0u));
    sat_example_must(sat_ascii_font_draw_text_screen_centered_indexed8(
        font, stage, centre, 110, 8, 0u, 0u));
    sat_example_must(sat_draw_rect_screen(
        bar_x, 137, bar_w, 9u, SAT_RGB555(6, 13, 17)));
    if (filled > 0u) {
        sat_example_must(sat_draw_rect_screen(
            bar_x, 137, filled, 9u, SAT_RGB555(20, 29, 19)));
    }
    sat_example_must(sat_fmt_label_u32(
        "LOADING ", percent, progress, sizeof(progress), 0));
    sat_example_must(sat_ascii_font_draw_text_screen_centered_indexed8(
        font, progress, centre, 153, 8, 0u, 0u));
    sat_example_must(sat_end_frame());
}

#endif /* SATURN_EXAMPLE_UTIL_H */
