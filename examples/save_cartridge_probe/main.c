/* save_cartridge_probe - read-only BUP_Init device configuration display.
 * No BUP_Stat on an unverified cart selector; no writes, deletes, or format. */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/save.h"

static sat_ascii_font_t g_font;

static void draw_line(const char* text, int y) {
    (void)sat_ascii_font_draw_text_screen_indexed8(
        &g_font, text, 8, y, 8, SAT_COLOR_WHITE, 0u);
}

static void draw_value(const char* label, uint32_t value, int y) {
    char line[36];
    if (sat_fmt_label_u32(label, value, line, sizeof(line), 0u) == SAT_OK) {
        draw_line(line, y);
    }
}

int main(void) {
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(
            &g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;

    const sat_result_t init = sat_save_init();
    sat_save_device_info_t internal = {0};
    sat_save_device_info_t cartridge = {0};
    sat_result_t internal_query = init;
    sat_result_t cartridge_query = init;

    if (init == SAT_OK) {
        internal_query = sat_save_device_info(SAT_SAVE_INTERNAL, &internal);
        cartridge_query = sat_save_device_info(
            SAT_SAVE_BACKUP_CARTRIDGE, &cartridge);
    }

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_app_frame_begin(
                SAT_COLOR_BLACK,
                init == SAT_OK ? SAT_COLOR_GREEN : SAT_COLOR_RED,
                &pad) != SAT_OK) break;

        draw_line("BACKUP MEMORY CART PROBE", 8);
        draw_line("READ-ONLY BUP_INIT CONFIG", 32);
        draw_value("INIT CODE ", (uint32_t)(-init), 48);
        if (init == SAT_OK &&
            internal_query == SAT_OK && cartridge_query == SAT_OK) {
            draw_value("INTERNAL DETECTED ", internal.connected, 72);
            draw_value("INTERNAL PARTITIONS ", internal.partition_count, 88);
            draw_value("CART DETECTED ", cartridge.connected, 112);
            draw_value("CART PARTITIONS ", cartridge.partition_count, 128);
            draw_line("NO CART READ/WRITE/FORMAT", 160);
            draw_line("NO CAPACITY CLAIM YET", 176);
        } else if (init == SAT_OK) {
            draw_value("INT QUERY CODE ", (uint32_t)(-internal_query), 72);
            draw_value("CART QUERY CODE ", (uint32_t)(-cartridge_query), 88);
        }

        draw_line("START = EXIT", 208);
        (void)sat_app_frame_end();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }

    (void)sat_shutdown();
    return init == SAT_OK &&
        internal_query == SAT_OK && cartridge_query == SAT_OK ? 0 : 1;
}
