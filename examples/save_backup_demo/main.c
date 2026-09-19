/* save_backup_demo - internal Backup RAM write/read/verify/persistence demo. */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/save.h"

#define SAVE_MAGIC 0x4C534156u /* "LSAV" */
#define SAVE_VERSION 1u

typedef struct demo_payload {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t boot_count;
    uint32_t checksum_seed;
} demo_payload_t;

static sat_ascii_font_t g_font;
static sat_result_t g_status = SAT_OK;
static uint8_t g_unformatted = 0u;
static uint8_t g_deleted = 0u;
static uint32_t g_boot_count = 0u;
static uint32_t g_free_bytes = 0u;
static uint32_t g_free_blocks = 0u;
static uint16_t g_entry_count = 0u;

/* The SH-2 freestanding toolchain has no libc string.h. Compare the exact
 * payload bytes locally; do not pull in a hosted C runtime for this demo. */
static uint8_t payload_equals(const demo_payload_t* left, const demo_payload_t* right) {
    const uint8_t* a = (const uint8_t*)left;
    const uint8_t* b = (const uint8_t*)right;
    for (uint32_t i = 0u; i < sizeof(demo_payload_t); ++i) {
        if (a[i] != b[i]) return 0u;
    }
    return 1u;
}

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

static sat_result_t write_and_verify_demo(void) {
    sat_save_storage_info_t storage;
    sat_result_t status =
        sat_save_storage_info(SAT_SAVE_INTERNAL, sizeof(demo_payload_t), &storage);
    if (status == SAT_ERR_UNFORMATTED) {
        g_unformatted = 1u;
        return status;
    }
    if (status != SAT_OK) return status;

    demo_payload_t payload = {
        SAVE_MAGIC,
        SAVE_VERSION,
        (uint16_t)sizeof(demo_payload_t),
        1u,
        0x13579BDFu
    };
    uint32_t existing_size = 0u;
    demo_payload_t existing;
    status = sat_save_read(
        SAT_SAVE_INTERNAL, "LIBSAT_DEMO",
        &existing, sizeof(existing), &existing_size);
    if (status == SAT_OK) {
        if (existing_size != sizeof(existing) ||
            existing.magic != SAVE_MAGIC ||
            existing.version != SAVE_VERSION ||
            existing.size != sizeof(existing)) {
            return SAT_ERR_IO;
        }
        payload.boot_count = existing.boot_count + 1u;
    } else if (status != SAT_ERR_NOT_FOUND) {
        return status;
    }

    const sat_save_record_t record = {
        "LIBSAT_DEMO",
        "LibSaturn",
        SAT_SAVE_ENGLISH,
        0u
    };
    status = sat_save_write(
        SAT_SAVE_INTERNAL, &record, &payload, sizeof(payload), 1u);
    if (status != SAT_OK) return status;

    status = sat_save_verify(
        SAT_SAVE_INTERNAL, record.name, &payload, sizeof(payload));
    if (status != SAT_OK) return status;

    demo_payload_t roundtrip;
    uint32_t roundtrip_size = 0u;
    status = sat_save_read(
        SAT_SAVE_INTERNAL, record.name,
        &roundtrip, sizeof(roundtrip), &roundtrip_size);
    if (status != SAT_OK) return status;
    if (roundtrip_size != sizeof(roundtrip) ||
        !payload_equals(&roundtrip, &payload)) {
        return SAT_ERR_VERIFY_FAILED;
    }

    sat_save_entry_t entries[8];
    uint16_t total = 0u;
    status = sat_save_list(
        SAT_SAVE_INTERNAL, "*", entries,
        (uint16_t)(sizeof(entries) / sizeof(entries[0])), &total);
    if (status != SAT_OK) return status;

    status = sat_save_storage_info(SAT_SAVE_INTERNAL, 0u, &storage);
    if (status != SAT_OK) return status;

    g_boot_count = payload.boot_count;
    g_free_bytes = storage.free_size;
    g_free_blocks = storage.free_blocks;
    g_entry_count = total;
    g_unformatted = 0u;
    g_deleted = 0u;
    return SAT_OK;
}

int main(void) {
    g_status = sat_app_init_default();
    if (g_status != SAT_OK) return 1;

    g_status = sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u);
    if (g_status != SAT_OK) return 1;

    g_status = sat_save_init();
    if (g_status == SAT_OK) {
        g_status = write_and_verify_demo();
    }

    for (;;) {
        sat_pad_state_t pad = {0};
        const uint16_t clear =
            g_status == SAT_OK ? SAT_COLOR_GREEN :
            g_unformatted != 0u ? SAT_COLOR_YELLOW : SAT_COLOR_RED;

        if (sat_app_frame_begin(SAT_COLOR_BLACK, clear, &pad) != SAT_OK) break;

        draw_line("LIBSATURN BACKUP RAM", 8);

        if (g_unformatted != 0u) {
            draw_line("INTERNAL BUP IS UNFORMATTED", 40);
            draw_line("A = FORMAT + RUN TEST", 56);
            draw_line("FORMAT ERASES INTERNAL SAVES", 72);

            if ((pad.pressed & SAT_PAD_A) != 0u) {
                g_status = sat_save_format(SAT_SAVE_INTERNAL);
                if (g_status == SAT_OK) g_status = write_and_verify_demo();
            }
        } else if (g_status == SAT_OK) {
            draw_line("WRITE / VERIFY / READ: PASS", 40);
            draw_line("SAVE: LIBSAT_DEMO", 56);
            draw_value("BOOT COUNT ", g_boot_count, 80);
            draw_value("FREE BYTES ", g_free_bytes, 96);
            draw_value("FREE BLOCKS ", g_free_blocks, 112);
            draw_value("DIR ENTRIES ", g_entry_count, 128);
            draw_line("RESTART EMULATOR TO TEST", 152);
            draw_line("PERSISTENCE", 168);
            draw_line("X = DELETE DEMO SAVE", 192);

            if ((pad.pressed & SAT_PAD_X) != 0u) {
                g_status = sat_save_delete(SAT_SAVE_INTERNAL, "LIBSAT_DEMO");
                if (g_status == SAT_OK) {
                    g_deleted = 1u;
                    g_boot_count = 0u;
                }
            }
            if (g_deleted != 0u) draw_line("DELETED", 208);
        } else {
            draw_line("SAVE TEST FAILED", 40);
            draw_value("STATUS ", (uint32_t)(-g_status), 56);
        }

        (void)sat_app_frame_end();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }

    (void)sat_shutdown();
    return g_status == SAT_OK ? 0 : 1;
}
