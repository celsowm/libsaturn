/* RAM expansion demo: safely distinguish absent/1 MiB/4 MiB carts. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/ram_cart.h"

static sat_ascii_font_t font;

static void line(const char* text, int y) {
    (void)sat_ascii_font_draw_text_screen_indexed8(
        &font, text, 8, y, 8, SAT_COLOR_WHITE, 0u);
}
static void number(const char* label, uint32_t value, int y) {
    char output[40];
    if (sat_fmt_label_u32(label, value, output, sizeof(output), 0u) == SAT_OK) line(output, y);
}
int main(void) {
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(
            &font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;

    sat_result_t status = sat_ram_cart_init();
    sat_ram_cart_info_t info = {0};
    uint8_t passed = 0u;
    if (status == SAT_OK) status = sat_ram_cart_info(&info);
    if (status == SAT_OK) {
        /* Probe both banks with unique patterns, including both sides of the
         * 2 MiB boundary on a 4 MiB cart. No saved data is ever touched. */
        sat_ram_cart_buffer_t buffer = {0};
        status = sat_ram_cart_buffer_alloc(info.capacity, 32u, &buffer);
        if (status == SAT_OK) {
            uint8_t expected[8] = {0x19, 0x28, 0x37, 0x46, 0x55, 0x64, 0x73, 0x82};
            uint8_t actual[8] = {0};
            status = sat_ram_cart_buffer_write_at(
                &buffer, buffer.bank_bytes[0] - 4u, expected, sizeof(expected));
            if (status == SAT_OK) status = sat_ram_cart_buffer_read_at(
                &buffer, buffer.bank_bytes[0] - 4u, actual, sizeof(actual));
            passed = status == SAT_OK;
            for (uint32_t i = 0u; i < sizeof(expected); ++i) {
                if (actual[i] != expected[i]) passed = 0u;
            }
            if (!passed && status == SAT_OK) status = SAT_ERR_VERIFY_FAILED;
        }
    }
    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_app_frame_begin(SAT_COLOR_BLACK,
                status == SAT_OK ? SAT_COLOR_GREEN : SAT_COLOR_RED, &pad) != SAT_OK) break;
        line("RAM EXPANSION CARTRIDGE", 8);
        if (status == SAT_ERR_NOT_CONNECTED) {
            line("NO EXPANSION INSTALLED", 40);
        } else {
            number("CART TYPE MB ", (uint32_t)info.type, 40);
            number("CAPACITY BYTES ", info.capacity, 64);
            number("BANKS ", info.bank_count, 88);
            number("STATUS CODE ", (uint32_t)(-status), 112);
            line(passed ? "BANK CROSSING: PASS" : "BANK CROSSING: FAIL", 144);
        }
        line("START = EXIT", 200);
        (void)sat_app_frame_end();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return status == SAT_OK || status == SAT_ERR_NOT_CONNECTED ? 0 : 1;
}
