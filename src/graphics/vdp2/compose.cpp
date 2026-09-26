#include "saturn/vdp2_compose.h"

#include "saturn/vdp2.h"

#include "src/core/runtime/state.hpp"
#include "src/hal/vdp2/vdp2.hpp"

namespace {

namespace hal = saturn::hal::vdp2;
namespace cmp = saturn::hal::vdp2::compose;

constexpr uint32_t kChunkWords = 64u;

struct LineWindow {
    uint32_t table;
    bool line;
};

LineWindow g_line[2];
uint8_t g_mosaic_w = 1u;
uint8_t g_mosaic_h = 1u;
uint8_t g_mosaic_mask = 0u;

bool valid_screen(uint32_t screen) {
    return screen < cmp::kScreenCount;
}

/* Integer square root for the ellipse edge; arguments stay far below 2^31. */
uint32_t isqrt(uint32_t v) {
    uint32_t r = 0u;
    for (uint32_t bit = 1u << 30u; bit != 0u; bit >>= 2u) {
        const uint32_t t = r + bit;
        if (v >= t) {
            v -= t;
            r = (r >> 1u) + bit;
        } else {
            r >>= 1u;
        }
    }
    return r;
}

uint16_t clamp_x(int32_t x) {
    if (x < 0) return 0u;
    if (x > static_cast<int32_t>(cmp::kMaxX)) return cmp::kMaxX;
    return static_cast<uint16_t>(x);
}

}  // namespace

extern "C" sat_result_t sat_vdp2_window_set_rect(uint8_t window, const sat_vdp2_window_rect_t* rect) {
    SAT_TRY(saturn::core::require_initialized());
    if (window > 1u || rect == nullptr) return SAT_ERR_INVALID_ARG;
    cmp::Window w{};
    w.rect = {rect->x0, rect->y0, rect->x1, rect->y1};
    if (!hal::set_window(window, w)) return SAT_ERR_INVALID_ARG;
    g_line[window].line = false;
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_window_set_line_table(uint8_t window, uint32_t table_address,
                                                       uint16_t y0, uint16_t y1) {
    SAT_TRY(saturn::core::require_initialized());
    if (window > 1u || !cmp::line_table_valid(table_address)) return SAT_ERR_INVALID_ARG;
    cmp::Window w{};
    /* The horizontal extent comes from the table; the registers must still
     * hold start <= end. */
    w.rect = {0u, y0, cmp::kMaxX, y1};
    w.line = true;
    w.line_table = table_address;
    if (!hal::set_window(window, w)) return SAT_ERR_INVALID_ARG;
    g_line[window] = {table_address, true};
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_window_line_write(uint8_t window, uint32_t first_line,
                                                   const sat_vdp2_window_span_t* spans, uint32_t count) {
    SAT_TRY(saturn::core::require_initialized());
    if (window > 1u || spans == nullptr || !g_line[window].line ||
        first_line + count > cmp::kMaxY + 1u) {
        return SAT_ERR_INVALID_ARG;
    }
    uint16_t chunk[kChunkWords];
    uint32_t address = g_line[window].table + first_line * 4u;
    for (uint32_t done = 0u; done < count;) {
        const uint32_t n = count - done < kChunkWords / 2u ? count - done : kChunkWords / 2u;
        for (uint32_t i = 0u; i < n; ++i) {
            chunk[2u * i] = cmp::encode_span_word(spans[done + i].x0, hal::hires());
            chunk[2u * i + 1u] = cmp::encode_span_word(spans[done + i].x1, hal::hires());
        }
        SAT_TRY(sat_vdp2_vram_write_words(address / 2u, chunk, n * 2u));
        address += n * 4u;
        done += n;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_window_set_ellipse(uint8_t window, uint32_t table_address,
                                                    int32_t cx, int32_t cy, int32_t rx, int32_t ry) {
    SAT_TRY(saturn::core::require_initialized());
    if (window > 1u || rx < 0 || ry <= 0 || rx > 512 || ry > 256) return SAT_ERR_INVALID_ARG;
    int32_t top = cy - ry;
    const int32_t bottom = cy + ry;
    if (bottom < 0 || bottom > static_cast<int32_t>(cmp::kMaxY)) return SAT_ERR_INVALID_ARG;
    if (top < 0) top = 0;
    SAT_TRY(sat_vdp2_window_set_line_table(window, table_address, static_cast<uint16_t>(top),
                                           static_cast<uint16_t>(bottom)));
    sat_vdp2_window_span_t spans[32];
    const uint32_t rows = static_cast<uint32_t>(bottom - top + 1);
    const uint32_t r2 = static_cast<uint32_t>(ry) * static_cast<uint32_t>(ry);
    for (uint32_t done = 0u; done < rows;) {
        const uint32_t n = rows - done < 32u ? rows - done : 32u;
        for (uint32_t i = 0u; i < n; ++i) {
            const int32_t dy = (top + static_cast<int32_t>(done + i)) - cy;
            const uint32_t ady = static_cast<uint32_t>(dy < 0 ? -dy : dy);
            /* dx = rx * sqrt(1 - dy^2 / ry^2) */
            const uint32_t inner = r2 > ady * ady ? r2 - ady * ady : 0u;
            const uint32_t dx = static_cast<uint32_t>(rx) * isqrt(inner) / static_cast<uint32_t>(ry);
            spans[i].x0 = clamp_x(cx - static_cast<int32_t>(dx));
            spans[i].x1 = clamp_x(cx + static_cast<int32_t>(dx));
        }
        SAT_TRY(sat_vdp2_window_line_write(window, static_cast<uint32_t>(top) + done, spans, n));
        done += n;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_window_clear(uint8_t window) {
    SAT_TRY(saturn::core::require_initialized());
    if (window > 1u) return SAT_ERR_INVALID_ARG;
    hal::clear_window(window);
    g_line[window].line = false;
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_screen_window_set(const sat_vdp2_screen_window_t* config) {
    SAT_TRY(saturn::core::require_initialized());
    if (config == nullptr || !valid_screen(config->screen)) return SAT_ERR_INVALID_ARG;
    cmp::ScreenWindow s{};
    s.w0 = config->w0;
    s.w1 = config->w1;
    s.logic_and = config->logic_and != 0u;
    return hal::set_screen_window(config->screen, s) ? SAT_OK : SAT_ERR_INVALID_ARG;
}

extern "C" sat_result_t sat_vdp2_screen_window_clear(sat_vdp2_screen_t screen) {
    SAT_TRY(saturn::core::require_initialized());
    if (!valid_screen(static_cast<uint32_t>(screen))) return SAT_ERR_INVALID_ARG;
    return hal::set_screen_window(static_cast<uint8_t>(screen), cmp::ScreenWindow{}) ? SAT_OK
                                                                                       : SAT_ERR_INVALID_ARG;
}

extern "C" sat_result_t sat_vdp2_mosaic_set_size(uint8_t width, uint8_t height) {
    SAT_TRY(saturn::core::require_initialized());
    if (!cmp::mosaic_valid(width, height)) return SAT_ERR_INVALID_ARG;
    if (!hal::set_mosaic(width, height, g_mosaic_mask)) return SAT_ERR_UNSUPPORTED;
    g_mosaic_w = width;
    g_mosaic_h = height;
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_screen_set_mosaic(sat_vdp2_screen_t screen, uint8_t enabled) {
    SAT_TRY(saturn::core::require_initialized());
    /* Mosaic covers the scroll screens; the sprite and colour-calculation
     * entries of the enum are for windows only. */
    if (static_cast<uint32_t>(screen) > SAT_VDP2_SCREEN_RBG0) return SAT_ERR_INVALID_ARG;
    const uint8_t bit = static_cast<uint8_t>(1u << static_cast<uint32_t>(screen));
    const uint8_t mask = enabled != 0u ? static_cast<uint8_t>(g_mosaic_mask | bit)
                                       : static_cast<uint8_t>(g_mosaic_mask & ~bit);
    if (!hal::set_mosaic(g_mosaic_w, g_mosaic_h, mask)) return SAT_ERR_UNSUPPORTED;
    g_mosaic_mask = mask;
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_screen_color_calc_set(sat_vdp2_screen_t screen, uint8_t enabled,
                                                       uint8_t ratio) {
    SAT_TRY(saturn::core::require_initialized());
    if (static_cast<uint32_t>(screen) > SAT_VDP2_SCREEN_RBG0 || ratio > cmp::kMaxRatio) {
        return SAT_ERR_INVALID_ARG;
    }
    return hal::set_screen_color_calc(static_cast<uint8_t>(screen), enabled != 0u, ratio)
               ? SAT_OK
               : SAT_ERR_INVALID_ARG;
}

extern "C" uint8_t sat_vdp2_color_calc_ratio_for_below(uint8_t below_32nds) {
    return cmp::ratio_for_second(below_32nds);
}
