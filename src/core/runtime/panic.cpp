#include "saturn/color.h"
#include "saturn/core.h"
#include "saturn/vdp1.h"
#include "saturn/video.h"

#include "src/core/runtime/panic_logic.hpp"
#include "src/core/runtime/state.hpp"

using namespace saturn::core::panic;

extern "C" {

volatile sat_panic_info_t g_sat_last_panic;

const char* sat_result_name(sat_result_t st) {
    return result_name(st);
}

}  /* extern "C" */

namespace {

constexpr uint16_t kBackground = SAT_RGB555(14, 0, 0);
constexpr uint16_t kText = SAT_RGB555(31, 31, 31);
constexpr uint16_t kTitle = SAT_RGB555(31, 24, 8);
constexpr int kScale = 2;
constexpr int kLeft = 8;
constexpr int kTop = 40;
constexpr int kLinePitch = 5 * kScale + 2 * kScale * 2;  /* glyph + gap */

struct Painter {
    sat_result_t st;
    uint16_t color;
};

void emit_rect(void* ctx, int x, int y, int w, int h) {
    Painter* p = static_cast<Painter*>(ctx);
    if (p->st != SAT_OK) return;
    p->st = sat_draw_rect_screen(
        static_cast<int16_t>(x), static_cast<int16_t>(y),
        static_cast<uint16_t>(w), static_cast<uint16_t>(h), p->color);
}

sat_result_t draw_report(
    char lines[kLineCount][kLineChars + 1u], uint16_t width, uint16_t height
) {
    sat_result_t st = sat_begin_frame();
    if (st != SAT_OK) return st;
    st = sat_draw_rect_screen(0, 0, width, height, kBackground);
    if (st != SAT_OK) return st;

    Painter painter = {SAT_OK, kText};
    for (uint8_t i = 0u; i < kLineCount; ++i) {
        painter.color = (i == 0u) ? kTitle : kText;
        for_each_run(
            lines[i], kLeft, kTop + static_cast<int>(i) * kLinePitch, kScale,
            [&painter](int x, int y, int w, int h) {
                emit_rect(&painter, x, y, w, h);
            });
        if (painter.st != SAT_OK) return painter.st;
    }
    return sat_end_frame();
}

}  /* namespace */

extern "C" void sat_panic(sat_result_t st, const char* file, int line) {
    static volatile uint8_t entered = 0u;

    g_sat_last_panic.magic = SAT_PANIC_MAGIC;
    g_sat_last_panic.code = static_cast<int32_t>(st);
    g_sat_last_panic.line = static_cast<int32_t>(line);
    const char* base = basename(file);
    uint32_t i = 0u;
    for (; i + 1u < SAT_PANIC_FILE_MAX && base[i] != '\0'; ++i) {
        g_sat_last_panic.file[i] = base[i];
    }
    g_sat_last_panic.file[i] = '\0';

    /* A failure while drawing the report must not re-enter and loop forever
     * on the same bad state: the second entry goes straight to the spin. */
    if (entered == 0u) {
        entered = 1u;
        char lines[kLineCount][kLineChars + 1u];
        build_report(lines, st, file, line);
        /* Dark backdrop and an opaque VDP1 erase so VDP2 layers do not hide
         * the report. Errors here are ignored: they only mean "not
         * initialised", and then there is nothing to draw on anyway. */
        (void)sat_vdp2_back_color_set(SAT_COLOR_BLACK);
        (void)sat_set_clear_color(kBackground);
        const sat_video_config_t& cfg = saturn::core::g_state.config;
        for (;;) {
            if (sat_wait_vblank() != SAT_OK) break;
            if (draw_report(lines, cfg.width, cfg.height) != SAT_OK) break;
        }
    }
    for (;;) {
    }
}
