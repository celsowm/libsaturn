#include <cstdio>
#include "saturn/hud.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

extern "C" sat_result_t sat_ascii_font_draw_text_screen_indexed8(
    const sat_ascii_font_t*, const char*, int, int, int, uint16_t, uint16_t) { return SAT_OK; }
extern "C" sat_result_t sat_ascii_font_draw_text_screen_centered_indexed8(
    const sat_ascii_font_t*, const char*, int, int, int, uint16_t, uint16_t) { return SAT_OK; }
extern "C" sat_result_t sat_ascii_font_draw_label_u32(
    const sat_ascii_font_t*, const char*, uint32_t, int, int, int, uint16_t, uint16_t) { return SAT_OK; }
extern "C" sat_result_t sat_draw_rect_screen(int16_t, int16_t, uint16_t, uint16_t, uint16_t) { return SAT_OK; }

int main() {
    sat_ascii_font_t font{};
    sat_hud_t hud{};
    OK(sat_hud_init(&hud, &font, 2u, 8u) == SAT_OK);
    OK(sat_hud_text(&hud, "OK", 1, 2) == SAT_OK);
    OK(sat_hud_text_centered(&hud, "OK", 160, 2) == SAT_OK);
    OK(sat_hud_value(&hud, "N ", 3u, 1, 2) == SAT_OK);
    OK(sat_hud_bar(&hud, 1, 2, 10, 2, 5u, 10u, 0u, 1u) == SAT_OK);
    OK(sat_hud_init(&hud, &font, 2u, 0u) == SAT_ERR_INVALID_ARG);
    OK(sat_hud_bar(&hud, 1, 2, 10, 2, 11u, 10u, 0u, 1u) == SAT_ERR_INVALID_ARG);
    std::puts("hud: OK");
    return 0;
}
