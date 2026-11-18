#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "example_util.h"

extern unsigned near_clip_v2_verify(void);
static sat_ascii_font_t font;
int main(void) {
    sat_example_must(sat_app_init_default());
    unsigned errors=near_clip_v2_verify();
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&font,SAT_COLOR_WHITE,0,0));
    for (;;) {
        sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK,SAT_COLOR_BLACK,0));
        sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&font,"NEAR CLIP V2",8,8,8,0,0));
        sat_example_must(sat_ascii_font_draw_label_u32(&font,"ERRORS ",errors,8,32,8,0,0));
        sat_example_must(sat_app_frame_end());
    }
}
