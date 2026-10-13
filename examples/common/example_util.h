#ifndef LIBSATURN_EXAMPLES_COMMON_EXAMPLE_UTIL_H
#define LIBSATURN_EXAMPLES_COMMON_EXAMPLE_UTIL_H

#include "saturn/core.h"
#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/loader.h"
#include "saturn/render2d.h"
#include "saturn/video.h"

/* Example-code error check: any non-OK result stops the program on a
 * full-screen report (error name, file, line, code) instead of hanging
 * silently. A macro rather than a function so the report can name the call
 * site; call syntax is unchanged. See sat_panic() in saturn/core.h. */
#define sat_example_must(expr) SAT_PANIC_IF_ERROR(expr)

/* A whole boot progress frame: title, current stage and a percentage bar.
 *
 * Procedural startup work (generating an ocean bitmap, importing a model,
 * synthesising audio) takes long enough that a black screen looks like a
 * hang, so examples that do it end up writing the same panel. This draws
 * VDP1-only, so it works before any VDP2 layer is configured, and it is a
 * report of work already finished rather than a timed fake.
 *
 * The bar itself (track + filled portion) is drawn by the library so the
 * math lives in one place; the caller configures position, size and colours
 * by passing a sat_loading_screen_t, same as sat_loading_screen_progress.
 *
 * Call it between units of startup work; it waits for VBlank and presents. */
static inline void sat_example_loading_frame(
    const sat_ascii_font_t* font, const sat_loading_screen_t* screen,
    const char* title, const char* stage, uint8_t percent) {
    if (percent > 100u) percent = 100u;
    const uint16_t width = sat_video_width();
    const uint16_t height = sat_video_height();
    const int centre = (int)(width / 2u);
    char progress[SAT_FMT_U32_MAX + 12u];
    sat_example_must(sat_wait_vblank());
    sat_example_must(sat_begin_frame());
    sat_example_must(sat_draw_rect_screen(
        0, 0, width, height, SAT_RGB555(2, 7, 14)));
    sat_example_must(sat_draw_rect_screen(
        screen->x, 64, screen->width, 3u, SAT_RGB555(12, 26, 27)));
    sat_example_must(sat_ascii_font_draw_text_screen_centered_indexed8(
        font, title, centre, 77, 8, 0u, 0u));
    sat_example_must(sat_ascii_font_draw_text_screen_centered_indexed8(
        font, stage, centre, 110, 8, 0u, 0u));
    (void)sat_loading_screen_draw_bar(screen, percent);
    sat_example_must(sat_fmt_label_u32(
        "LOADING ", percent, progress, sizeof(progress), 0));
    sat_example_must(sat_ascii_font_draw_text_screen_centered_indexed8(
        font, progress, centre, 153, 8, 0u, 0u));
    sat_example_must(sat_end_frame());
}

#endif /* LIBSATURN_EXAMPLES_COMMON_EXAMPLE_UTIL_H */
