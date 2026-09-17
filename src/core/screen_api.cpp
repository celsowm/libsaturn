/* Screen-coordinate convenience layer.
 *
 * The VDP1 entry points in vdp1_api.cpp take native coordinates with (0,0) at
 * the screen centre, because that is what the hardware draws in. HUDs are
 * naturally written in screen coordinates with (0,0) top-left, and mixing the
 * two silently draws in the wrong place -- text at x = 240 on a 320-wide
 * screen lands 80 pixels past the right edge instead of near it.
 *
 * These wrappers live in their own translation unit so font_api.cpp stays free
 * of any runtime-state dependency and keeps linking into the host font tests.
 */

#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/vdp1.h"

#include "src/core/internal.hpp"
#include "src/core/runtime_state.hpp"

extern "C" sat_result_t sat_draw_rect_screen(
    int16_t x,
    int16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color
) {
    const sat_result_t st = saturn::core::require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (width == 0u || height == 0u) {
        return SAT_OK;
    }

    const sat_video_config_t& cfg = saturn::core::g_state.config;
    const int16_t x0 = saturn::internal::screen_to_native(x, cfg.width);
    const int16_t y0 = saturn::internal::screen_to_native(y, cfg.height);
    const int16_t x1 = static_cast<int16_t>(x0 + static_cast<int16_t>(width));
    const int16_t y1 = static_cast<int16_t>(y0 + static_cast<int16_t>(height));

    sat_polygon_cmd_t cmd = {};
    cmd.x[0] = x0; cmd.y[0] = y0;
    cmd.x[1] = x1; cmd.y[1] = y0;
    cmd.x[2] = x1; cmd.y[2] = y1;
    cmd.x[3] = x0; cmd.y[3] = y1;
    cmd.color = color;
    cmd.flags = 0;
    return sat_vdp1_draw_polygon(&cmd);
}

extern "C" sat_result_t sat_ascii_font_draw_text_screen_indexed8(
    const sat_ascii_font_t* font,
    const char* text,
    int screen_x,
    int screen_y,
    int char_spacing,
    uint16_t palette_override,
    uint16_t flags
) {
    const sat_result_t st = saturn::core::require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    const sat_video_config_t& cfg = saturn::core::g_state.config;
    return sat_ascii_font_draw_text_indexed8(
        font,
        text,
        saturn::internal::screen_to_native(screen_x, cfg.width),
        saturn::internal::screen_to_native(screen_y, cfg.height),
        char_spacing,
        palette_override,
        flags);
}

extern "C" sat_result_t sat_ascii_font_draw_text_screen_centered_indexed8(
    const sat_ascii_font_t* font,
    const char* text,
    int screen_center_x,
    int screen_y,
    int char_spacing,
    uint16_t palette_override,
    uint16_t flags
) {
    if (font == nullptr || text == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    const int width = sat_ascii_font_measure_text_indexed8(text, char_spacing);
    return sat_ascii_font_draw_text_screen_indexed8(
        font,
        text,
        screen_center_x - (width / 2),
        screen_y,
        char_spacing,
        palette_override,
        flags);
}
