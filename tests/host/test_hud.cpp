#include <cstdio>
#include "saturn/hud.h"
#include "saturn/video.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

extern "C" sat_result_t sat_ascii_font_draw_text_screen_indexed8(
    const sat_ascii_font_t*, const char*, int, int, int, uint16_t, uint16_t) { return SAT_OK; }
extern "C" sat_result_t sat_ascii_font_draw_text_screen_centered_indexed8(
    const sat_ascii_font_t*, const char*, int, int, int, uint16_t, uint16_t) { return SAT_OK; }
extern "C" sat_result_t sat_ascii_font_draw_label_u32(
    const sat_ascii_font_t*, const char*, uint32_t, int, int, int, uint16_t, uint16_t) { return SAT_OK; }
extern "C" sat_result_t sat_draw_rect_screen(int16_t, int16_t, uint16_t, uint16_t, uint16_t) { return SAT_OK; }
extern "C" sat_result_t sat_ascii_font_init_8x8_indexed8(
    sat_ascii_font_t*, uint16_t, uint16_t, uint16_t) { return SAT_OK; }
static uint16_t g_video_width = 320u;
extern "C" uint16_t sat_video_width(void) { return g_video_width; }
extern "C" uint16_t sat_video_height(void) { return 224u; }

int main() {
    sat_ascii_font_t font{};
    sat_hud_t hud{};
    OK(sat_hud_init(&hud, &font, 2u, 8u) == SAT_OK);
    OK(sat_hud_text(&hud, "OK", 1, 2) == SAT_OK);
    OK(sat_hud_text_centered(&hud, "OK", 160, 2) == SAT_OK);
    OK(sat_hud_value(&hud, "N ", 3u, 1, 2) == SAT_OK);
    OK(sat_hud_bar(&hud, 1, 2, 10, 2, 5u, 10u, 0u, 1u) == SAT_OK);
    /* char_spacing 0 means "tight": the font's glyph width is the per-glyph
     * advance, with no extra gap. Two HUDs in the suite (input_move,
     * physics_2d) pass 0 to match the open-coded draw call they replaced. */
    OK(sat_hud_init(&hud, &font, 2u, 0u) == SAT_OK);
    OK(hud.char_spacing == 0u);
    OK(sat_hud_text(&hud, "OK", 1, 2) == SAT_OK);
    OK(sat_hud_bar(&hud, 1, 2, 10, 2, 11u, 10u, 0u, 1u) == SAT_ERR_INVALID_ARG);

    /* The cursor API in 320x224 -- 8-pixel glyph advance, 10-pixel
     * line advance from a (4, 4) anchor. */
    g_video_width = 320u;
    OK(sat_hud_init_default(&hud, &font) == SAT_OK);
    OK(hud.cursor_x == 4 && hud.cursor_y == 4);
    OK(hud.char_spacing == 8u);
    OK(sat_hud_line(&hud, "FPS", 60u) == SAT_OK);
    OK(hud.cursor_y == 14);   /* 4 + 10 */
    OK(sat_hud_line(&hud, "POS", 99u) == SAT_OK);
    OK(hud.cursor_y == 24);
    sat_hud_reset_cursor(&hud, 4, 4);
    OK(hud.cursor_y == 4);

    /* In hi-res the spacing doubles so the glyph keeps the same on-screen
     * proportion. */
    g_video_width = 640u;
    OK(sat_hud_init_default(&hud, &font) == SAT_OK);
    OK(hud.char_spacing == 16u);
    OK(sat_hud_line(&hud, "FPS", 60u) == SAT_OK);
    OK(hud.cursor_y == 14);

    /* Bad inputs to the new functions. */
    OK(sat_hud_init_default(nullptr, &font) == SAT_ERR_INVALID_ARG);
    OK(sat_hud_init_default(&hud, nullptr) == SAT_ERR_INVALID_ARG);
    OK(sat_hud_line(nullptr, "x", 0u) == SAT_ERR_INVALID_ARG);
    OK(sat_hud_line(&hud, nullptr, 0u) == SAT_ERR_INVALID_ARG);

    std::puts("hud: OK");
    return 0;
}
