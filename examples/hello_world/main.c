/* hello_world.c - "HELLO WORLD" renderizado com sprites bitmap */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/example_util.h"

static sat_ascii_font_t g_font;

int main(void) {
    const char* text = "HELLO WORLD!";
    const uint16_t backdrop_color = SAT_COLOR_BLUE;  // 0x001F - azul vibrante para teste

    sat_example_must(sat_app_init_default());
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 0));

    while (1) {
        sat_example_must(sat_app_frame_begin(backdrop_color, backdrop_color, NULL));
        sat_example_must(sat_ascii_font_draw_text_centered_indexed8(&g_font, text, 0, -4, 10, 0, 0));
        sat_example_must(sat_app_frame_end());
    }
}
