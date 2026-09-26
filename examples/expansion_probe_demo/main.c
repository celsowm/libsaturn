/* expansion_probe_demo: the MPEG card probe and a UART open, on a Saturn with neither.
 * Both must say "not there" and leave things alone: the probe reports MPEG version 0 and
 * sat_mpeg_start() returns NOT_CONNECTED without authenticating anything; opening a 16550
 * on the empty A-Bus fails its self-test. g_expansion holds what
 * harness/tests/test_expansion_probe.py checks. */
#include <stdint.h>
#include "saturn/abus.h"
#include "saturn/app.h"
#include "saturn/cd_block.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/mpeg.h"
#include "saturn/uart.h"
#include "saturn/video.h"

#define EXPANSION_MAGIC 0x45585031u /* "EXP1" */
#define EMPTY_SLOT_UART_BASE 0x24000001u  /* A-Bus CS1: written only after the slot reported empty */

typedef struct expansion_results {
    uint32_t magic;
    uint32_t cd_init;
    uint32_t probe_status;
    uint32_t present;
    uint32_t mpeg_version;
    uint32_t hardware_flags;
    uint32_t hardware_version;
    uint32_t drive_version;
    uint32_t drive_revision;
    uint32_t authentication_status;
    uint32_t start_status;
    uint32_t abus_status;
    uint32_t abus_kind;
    uint32_t uart_status;
    uint32_t uart_attempted;
    uint32_t frames;
} expansion_results_t;

volatile expansion_results_t g_expansion;

static sat_ascii_font_t font;
static sat_cd_block_t block;

static void line(const char* label, uint32_t value, int y) {
    (void)sat_ascii_font_draw_label_u32(&font, label, value, 8, y, 8, 0u, 0u);
}

int main(void) {
    sat_mpeg_info_t info = {0};
    sat_abus_info_t abus = {0};
    sat_uart_t uart = {0};
    const sat_uart_config_t config = {1843200u, 9600u, 8u, 1u, 0u, 0u};

    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;
    g_expansion.magic = EXPANSION_MAGIC;

    g_expansion.cd_init = (uint32_t)(-sat_cd_block_init(&block, SAT_CD_BLOCK_DEFAULT_TIMEOUT));
    if (g_expansion.cd_init == 0u) {
        g_expansion.probe_status = (uint32_t)(-sat_mpeg_probe(&block, &info));
        g_expansion.present = info.present;
        g_expansion.mpeg_version = info.mpeg_version;
        g_expansion.hardware_flags = info.hardware_flags;
        g_expansion.hardware_version = info.hardware_version;
        g_expansion.drive_version = info.drive_version;
        g_expansion.drive_revision = info.drive_revision;
        g_expansion.authentication_status = info.authentication_status;
        g_expansion.start_status = (uint32_t)(-sat_mpeg_start(&block));
    }

    g_expansion.abus_status = (uint32_t)(-sat_abus_detect(&abus));
    g_expansion.abus_kind = (uint32_t)abus.kind;
    if (g_expansion.abus_status == 0u && abus.kind == SAT_ABUS_NONE) {
        g_expansion.uart_attempted = 1u;
        g_expansion.uart_status = (uint32_t)(-sat_uart_open_mmio(&uart, EMPTY_SLOT_UART_BASE, 4u, &config));
    }

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_GREEN, &pad) != SAT_OK) break;
        ++g_expansion.frames;
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "EXPANSION PROBE", 8, 8, 8, 0u, 0u);
        line("MPEG PRESENT ", g_expansion.present, 24);
        line("MPEG VERSION ", g_expansion.mpeg_version, 40);
        line("START STATUS ", g_expansion.start_status, 56);
        line("ABUS KIND ", g_expansion.abus_kind, 72);
        line("UART STATUS ", g_expansion.uart_status, 88);
        (void)sat_app_frame_end();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
