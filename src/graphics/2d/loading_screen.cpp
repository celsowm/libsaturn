#include "saturn/loader.h"
#include "saturn/vdp1.h"
#include "saturn/video.h"

namespace {

constexpr uint16_t rgb555(unsigned r, unsigned g, unsigned b) {
    return static_cast<uint16_t>(0x8000u | (b << 10) | (g << 5) | r);
}

constexpr int kBorder = 2;

}  // namespace

extern "C" void sat_loading_screen_default(sat_loading_screen_t* screen) {
    if (screen == nullptr) return;
    screen->background = rgb555(0u, 0u, 2u);
    screen->frame = rgb555(31u, 31u, 31u);
    screen->track = rgb555(2u, 2u, 10u);
    screen->fill = rgb555(31u, 31u, 31u);
    screen->width = 200u;
    screen->height = 8u;
    screen->x = 60;
    screen->y = 170;
    screen->decorate = nullptr;
    screen->decorate_context = nullptr;
}

extern "C" sat_result_t sat_loading_screen_draw_bar(
    const sat_loading_screen_t* screen, uint16_t percent) {
    if (screen == nullptr) return SAT_ERR_INVALID_ARG;
    if (percent > 100u) percent = 100u;
    const uint16_t filled = static_cast<uint16_t>(
        (static_cast<uint32_t>(percent) * screen->width) / 100u);

    (void)sat_draw_rect_screen(
        static_cast<int16_t>(screen->x - kBorder),
        static_cast<int16_t>(screen->y - kBorder),
        static_cast<uint16_t>(screen->width + 2u * kBorder),
        static_cast<uint16_t>(screen->height + 2u * kBorder),
        screen->frame);
    (void)sat_draw_rect_screen(screen->x, screen->y, screen->width, screen->height, screen->track);
    if (filled > 0u) {
        (void)sat_draw_rect_screen(
            screen->x, screen->y, filled, screen->height, screen->fill);
    }
    return SAT_OK;
}

extern "C" void sat_loading_screen_progress(void* context, uint32_t done, uint32_t total) {
    const sat_loading_screen_t* screen = static_cast<const sat_loading_screen_t*>(context);
    if (screen == nullptr || total == 0u) return;
    if (done > total) done = total;
    const uint16_t percent = static_cast<uint16_t>((done * 100u) / total);

    // A failed frame only costs one bar update; the load itself must not stop.
    (void)sat_vdp2_back_color_set(screen->background);
    if (sat_begin_frame() != SAT_OK) return;
    (void)sat_loading_screen_draw_bar(screen, percent);
    if (screen->decorate != nullptr) screen->decorate(screen->decorate_context);
    (void)sat_end_frame();
    (void)sat_wait_vblank();
}
