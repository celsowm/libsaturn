/* A-Bus cartridge slot demo: identifies whatever is plugged in from its ID
 * byte, then exercises exactly what the library allows for that kind.
 * g_abus_demo holds what harness/tests/test_abus.py checks. */
#include <stdint.h>
#include "saturn/abus.h"
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/fmt.h"
#include "saturn/font.h"

#define ABUS_DEMO_MAGIC 0x41425531u /* "ABU1" */

typedef struct abus_demo_results {
    uint32_t magic;
    uint32_t detect_status;
    uint32_t kind;
    uint32_t id;
    uint32_t backup_bytes;
    uint32_t read_status;          /* CS0 read, as -result */
    uint32_t write_status;         /* write into the first DRAM bank, as -result */
    uint32_t write_readback_ok;
    uint32_t write_outside_status; /* write below the DRAM banks: must be refused */
    uint32_t write_cs1_status;     /* CS1 write: always refused */
    uint32_t frames;
} abus_demo_results_t;

volatile abus_demo_results_t g_abus_demo;

static sat_ascii_font_t font;

static void line(const char* label, uint32_t value, int y) {
    (void)sat_ascii_font_draw_label_u32(&font, label, value, 8, y, 8, 0u, 0u);
}

int main(void) {
    static const uint8_t pattern[8] = {0x19, 0x28, 0x37, 0x46, 0x55, 0x64, 0x73, 0x82};
    uint8_t back[8] = {0};
    uint8_t scratch[8] = {0};
    sat_abus_info_t info = {SAT_ABUS_NONE, 0u, 0u, 0u, 0u};

    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;

    g_abus_demo.magic = ABUS_DEMO_MAGIC;
    g_abus_demo.detect_status = (uint32_t)(-sat_abus_detect(&info));
    g_abus_demo.kind = (uint32_t)info.kind;
    g_abus_demo.id = info.id;
    g_abus_demo.backup_bytes = info.backup_bytes;
    g_abus_demo.read_status = (uint32_t)(-sat_abus_read(SAT_ABUS_CS0, 0x400000u, scratch, 8u));
    g_abus_demo.write_status = (uint32_t)(-sat_abus_write(SAT_ABUS_CS0, 0x400000u, pattern, 8u));
    if (g_abus_demo.write_status == 0u) {
        uint32_t same = sat_abus_read(SAT_ABUS_CS0, 0x400000u, back, 8u) == SAT_OK;
        for (uint32_t i = 0u; i < 8u; ++i) same = same && back[i] == pattern[i];
        g_abus_demo.write_readback_ok = same;
    }
    g_abus_demo.write_outside_status = (uint32_t)(-sat_abus_write(SAT_ABUS_CS0, 0x1000u, pattern, 8u));
    g_abus_demo.write_cs1_status = (uint32_t)(-sat_abus_write(SAT_ABUS_CS1, 0x0u, pattern, 8u));

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_GREEN, &pad) != SAT_OK) break;
        ++g_abus_demo.frames;
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "A-BUS SLOT", 8, 8, 8, 0u, 0u);
        line("KIND ", g_abus_demo.kind, 32);
        line("ID ", g_abus_demo.id, 48);
        line("BACKUP BYTES ", g_abus_demo.backup_bytes, 64);
        line("READ ", g_abus_demo.read_status, 88);
        line("WRITE ", g_abus_demo.write_status, 104);
        line("READBACK OK ", g_abus_demo.write_readback_ok, 120);
        line("WRITE LOW ", g_abus_demo.write_outside_status, 136);
        line("WRITE CS1 ", g_abus_demo.write_cs1_status, 152);
        (void)sat_app_frame_end();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
