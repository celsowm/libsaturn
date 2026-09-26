/* save_cartridge_demo - Backup Memory cartridge write/read/verify/persistence demo. Internal Backup RAM is never touched. */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/hud.h"
#include "saturn/save.h"
#include "saturn/save_schema.h"

#define SAVE_MAGIC 0x4C435254u /* "LCRT" */
#define SAVE_VERSION 1u

typedef struct demo_payload {
    uint32_t boot_count;
    uint32_t checksum_seed;
} demo_payload_t;

typedef struct demo_wire {
    sat_save_schema_header_t schema;
    demo_payload_t payload;
} demo_wire_t;

/* What harness/tests/test_save_cartridge.py reads from Work RAM. */
typedef struct cart_demo_results {
    uint32_t magic;            /* "CRT1" */
    uint32_t init_status;      /* sat_save_init, as -result */
    uint32_t status;           /* the write/verify/read run, as -result */
    uint32_t cart_connected;
    uint32_t cart_partitions;
    uint32_t total_size;
    uint32_t block_size;
    uint32_t free_size;
    uint32_t boot_count;
    uint32_t entries;
    uint32_t frames;
} cart_demo_results_t;
volatile cart_demo_results_t g_cart_demo;

static sat_ascii_font_t g_font;
static sat_hud_t g_hud;
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
    (void)sat_hud_text(&g_hud, text, 8, y);
}

static void draw_value(const char* label, uint32_t value, int y) {
    (void)sat_hud_value(&g_hud, label, value, 8, y);
}

static sat_result_t write_and_verify_demo(void) {
    sat_save_storage_info_t storage;
    sat_result_t status =
        sat_save_storage_info(SAT_SAVE_BACKUP_CARTRIDGE, sizeof(demo_wire_t), &storage);
    if (status == SAT_ERR_UNFORMATTED) {
        g_unformatted = 1u;
        return status;
    }
    if (status != SAT_OK) return status;

    demo_payload_t payload = {1u, 0x13579BDFu};
    uint32_t existing_size = 0u;
    demo_wire_t existing;
    status = sat_save_read(
        SAT_SAVE_BACKUP_CARTRIDGE, "LIBSAT_CART",
        &existing, sizeof(existing), &existing_size);
    if (status == SAT_OK) {
        uint32_t existing_payload_size = 0u;
        status = sat_save_schema_validate(
            &existing.schema, SAVE_MAGIC, SAVE_VERSION,
            &existing.payload, sizeof(existing.payload), &existing_payload_size);
        if (existing_size != sizeof(existing) ||
            status != SAT_OK || existing_payload_size != sizeof(existing.payload)) {
            return status == SAT_OK ? SAT_ERR_VERIFY_FAILED : status;
        }
        payload.boot_count = existing.payload.boot_count + 1u;
    } else if (status != SAT_ERR_NOT_FOUND) {
        return status;
    }

    demo_wire_t wire;
    status = sat_save_schema_encode(
        &wire.schema, SAVE_MAGIC, SAVE_VERSION,
        &payload, sizeof(payload));
    if (status != SAT_OK) return status;
    wire.payload = payload;

    const sat_save_record_t record = {
        "LIBSAT_CART",
        "LibSaturn",
        SAT_SAVE_ENGLISH,
        0u
    };
    status = sat_save_write(
        SAT_SAVE_BACKUP_CARTRIDGE, &record, &wire, sizeof(wire), 1u);
    if (status != SAT_OK) return status;

    status = sat_save_verify(
        SAT_SAVE_BACKUP_CARTRIDGE, record.name, &wire, sizeof(wire));
    if (status != SAT_OK) return status;

    demo_wire_t roundtrip;
    uint32_t roundtrip_size = 0u;
    status = sat_save_read(
        SAT_SAVE_BACKUP_CARTRIDGE, record.name,
        &roundtrip, sizeof(roundtrip), &roundtrip_size);
    if (status != SAT_OK) return status;
    uint32_t roundtrip_payload_size = 0u;
    status = sat_save_schema_validate(
        &roundtrip.schema, SAVE_MAGIC, SAVE_VERSION,
        &roundtrip.payload, sizeof(roundtrip.payload), &roundtrip_payload_size);
    if (roundtrip_size != sizeof(roundtrip) || status != SAT_OK ||
        roundtrip_payload_size != sizeof(roundtrip.payload) ||
        !payload_equals(&roundtrip.payload, &payload)) {
        return SAT_ERR_VERIFY_FAILED;
    }

    sat_save_entry_t entries[8];
    uint16_t total = 0u;
    status = sat_save_list(
        SAT_SAVE_BACKUP_CARTRIDGE, "*", entries,
        (uint16_t)(sizeof(entries) / sizeof(entries[0])), &total);
    if (status != SAT_OK) return status;

    status = sat_save_storage_info(SAT_SAVE_BACKUP_CARTRIDGE, 0u, &storage);
    if (status != SAT_OK) return status;

    g_cart_demo.total_size = storage.total_size;
    g_cart_demo.block_size = storage.block_size;
    g_cart_demo.free_size = storage.free_size;
    g_cart_demo.entries = total;
    g_cart_demo.boot_count = payload.boot_count;
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
    g_status = sat_hud_init(&g_hud, &g_font, SAT_COLOR_WHITE, 8u);
    if (g_status != SAT_OK) return 1;

    g_status = sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u);
    if (g_status != SAT_OK) return 1;

    g_cart_demo.magic = 0x43525431u;
    g_status = sat_save_init();
    g_cart_demo.init_status = (uint32_t)(-g_status);
    if (g_status == SAT_OK) {
        sat_save_device_info_t info = {0};
        if (sat_save_device_info(SAT_SAVE_BACKUP_CARTRIDGE, &info) == SAT_OK) {
            g_cart_demo.cart_connected = info.connected;
            g_cart_demo.cart_partitions = info.partition_count;
        }
        g_status = write_and_verify_demo();
        g_cart_demo.status = (uint32_t)(-g_status);
    }

    for (;;) {
        sat_pad_state_t pad = {0};
        const uint16_t clear =
            g_status == SAT_OK ? SAT_COLOR_GREEN :
            g_unformatted != 0u ? SAT_COLOR_YELLOW : SAT_COLOR_RED;

        if (sat_app_frame_begin(SAT_COLOR_BLACK, clear, &pad) != SAT_OK) break;
        ++g_cart_demo.frames;

        draw_line("LIBSATURN BACKUP CART", 8);

        if (g_unformatted != 0u) {
            draw_line("CARTRIDGE IS UNFORMATTED", 40);
            draw_line("A = FORMAT + RUN TEST", 56);
            draw_line("FORMAT ERASES THE CARTRIDGE", 72);

            if ((pad.pressed & SAT_PAD_A) != 0u) {
                g_status = sat_save_format(SAT_SAVE_BACKUP_CARTRIDGE);
                if (g_status == SAT_OK) g_status = write_and_verify_demo();
            }
        } else if (g_status == SAT_OK) {
            draw_line("WRITE / VERIFY / READ: PASS", 40);
            draw_line("SAVE: LIBSAT_CART", 56);
            draw_value("BOOT COUNT ", g_boot_count, 80);
            draw_value("FREE BYTES ", g_free_bytes, 96);
            draw_value("FREE BLOCKS ", g_free_blocks, 112);
            draw_value("DIR ENTRIES ", g_entry_count, 128);
            draw_line("RESTART EMULATOR TO TEST", 152);
            draw_line("PERSISTENCE", 168);
            draw_line("X = DELETE DEMO SAVE", 192);

            if ((pad.pressed & SAT_PAD_X) != 0u) {
                g_status = sat_save_delete(SAT_SAVE_BACKUP_CARTRIDGE, "LIBSAT_CART");
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
