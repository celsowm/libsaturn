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

#include "src/core/runtime/internal.hpp"
#include "src/core/runtime/state.hpp"

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

extern "C" sat_result_t sat_ascii_font_draw_label_u32(
    const sat_ascii_font_t* font,
    const char* label,
    uint32_t value,
    int screen_x,
    int screen_y,
    int char_spacing,
    uint16_t palette_override,
    uint16_t flags
) {
    /* Sized once here instead of in every caller's hand-picked
     * char[32]/[36]/[40]: the label plus a 32-bit decimal always fits. */
    char line[64];
    const sat_result_t formatted = sat_fmt_label_u32(
        label, value, line, sizeof(line), nullptr);
    if (formatted != SAT_OK) {
        return formatted;
    }
    return sat_ascii_font_draw_text_screen_indexed8(
        font, line, screen_x, screen_y, char_spacing,
        palette_override, flags);
}

extern "C" sat_result_t sat_ascii_font_draw_fields(
    const sat_ascii_font_t* font,
    const sat_debug_field_t* fields,
    uint16_t count,
    int screen_x,
    int screen_y,
    uint16_t width,
    int char_spacing,
    uint16_t palette_override,
    uint16_t flags
) {
    if (font == nullptr || fields == nullptr || count == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    const int cell = static_cast<int>(width) / static_cast<int>(count);
    for (uint16_t i = 0; i < count; ++i) {
        const sat_debug_field_t& f = fields[i];
        if (f.label == nullptr) {
            return SAT_ERR_INVALID_ARG;
        }
        char line[80];
        uint16_t len = 0;
        sat_result_t st = sat_fmt_label_u32(
            f.label, f.value, line, sizeof(line), &len);
        if (st != SAT_OK) {
            return st;
        }
        if (f.limit != 0u) {
            uint16_t tail = 0;
            line[len] = '/';
            st = sat_fmt_u32(f.limit, line + len + 1u,
                             static_cast<uint16_t>(sizeof(line) - len - 1u),
                             &tail);
            if (st != SAT_OK) {
                return st;
            }
            len = static_cast<uint16_t>(len + 1u + tail);
        }
        /* Drop a cell that would run past its neighbour instead of drawing a
         * row that reads as the wrong numbers. */
        if (sat_ascii_font_measure_text_indexed8(line, char_spacing) > cell) {
            continue;
        }
        st = sat_ascii_font_draw_text_screen_indexed8(
            font, line, screen_x + static_cast<int>(i) * cell, screen_y,
            char_spacing, palette_override, flags);
        if (st != SAT_OK) {
            return st;
        }
    }
    return SAT_OK;
}
