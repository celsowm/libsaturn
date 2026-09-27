/* city_load_probe: times the two-tier load city_walk depends on (plan R3).
 *
 *   CD -> WRAM-L staging      (sat_cd_block_read_sectors, word-at-a-time DTR)
 *   WRAM-L staging -> cart    (CPU 16-bit / 32-bit stores across the A-Bus)
 *   cart -> WRAM-L            (the residency ring page-in path)
 *
 * Timings are raw FRT ticks around each operation (sat_frame_count floors per
 * call, so summing short operations gives 0). ticks_per_10_vblanks calibrates
 * them against real VBlank edges; a single window must stay under the 16-bit
 * FRC wrap.
 * The payload is PROBE.BIN, 1 MiB, word k = (k*40503 + (k>>7)) & 0xFFFF, so
 * every stage is also verified, not just timed. g_load is what the harness
 * reads out of WRAM-H.
 */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/cd_block.h"
#include "saturn/cdfs.h"
#include "saturn/color.h"
#include "saturn/time.h"
#include "saturn/core.h"
#include "saturn/ram_cart.h"
#include "saturn/video.h"

#define LOAD_MAGIC 0x4C4F4431u /* "LOD1" */
#define PAYLOAD_BYTES (1024u * 1024u)
#define STAGING_BYTES (128u * 1024u)
#define SECTOR_BYTES 2048u
#define SPAN_EXPERIMENTS 8u
/* Each experiment reads its own disjoint 128 KiB of the payload, so no run can
 * be served by (or pay for) the sectors the previous one touched. */
#define EXPERIMENT_BYTES (128u * 1024u)
#define COPY32_BYTES (256u * 1024u)
#define PAGE_BYTES 4096u
#define PAGE_ITERATIONS 256u
#define PAGE_BATCH 32u

#define PROBE_WRAM_L __attribute__((section(".wram_l")))

typedef struct load_results {
    uint32_t magic;
    uint32_t status_cd_init;
    uint32_t status_mount;
    uint32_t status_lookup;
    uint32_t extent_lba;
    uint32_t file_size;
    uint32_t status_cart_init;
    uint32_t cart_type;
    uint32_t cart_capacity;
    uint32_t cart_alloc_ok;
    uint32_t frt_tcr;
    uint32_t ticks_per_10_vblanks;
    /* CD -> staging: 128 KiB per experiment, span size varies and repeats in
     * different positions to expose order effects. */
    uint32_t span_sectors[SPAN_EXPERIMENTS];
    uint32_t span_count[SPAN_EXPERIMENTS];
    uint32_t span_ticks[SPAN_EXPERIMENTS];
    uint32_t span_max_ticks[SPAN_EXPERIMENTS];
    uint32_t span_status[SPAN_EXPERIMENTS];
    uint32_t span_mismatch[SPAN_EXPERIMENTS];
    /* Full 1 MiB load, 32-sector spans, split into its two stages. */
    uint32_t load_cd_ticks;
    uint32_t load_copy16_ticks;
    uint32_t load_status;
    uint32_t load_mismatch_staging;
    uint32_t load_mismatch_cart;
    /* 256 KiB written to the cart with 32-bit stores instead. */
    uint32_t copy32_ticks;
    uint32_t copy32_mismatch;
    /* cart -> WRAM-L, PAGE_ITERATIONS pages of PAGE_BYTES. */
    uint32_t page16_ticks;
    uint32_t page32_ticks;
    uint32_t page_mismatch;
    uint32_t done;
} load_results_t;

volatile load_results_t g_load;

static PROBE_WRAM_L uint8_t g_staging[STAGING_BYTES] __attribute__((aligned(32)));

static sat_cd_block_t g_block;
static sat_cd_device_t g_device;
static sat_cdfs_volume_t g_volume;

static uint16_t expected_word(uint32_t word_index) {
    return (uint16_t)(word_index * 40503u + (word_index >> 7));
}

/* Big-endian word compare of `bytes` at `data`, which starts `file_offset`
 * into the payload. Returns the number of bad 16-bit words. */
static uint32_t count_mismatch(const uint8_t* data, uint32_t file_offset, uint32_t bytes) {
    uint32_t bad = 0u;
    for (uint32_t i = 0u; i + 1u < bytes; i += 2u) {
        uint16_t got = (uint16_t)(((uint16_t)data[i] << 8) | data[i + 1u]);
        if (got != expected_word((file_offset + i) >> 1)) ++bad;
    }
    return bad;
}

static void copy16(volatile uint16_t* dst, const uint16_t* src, uint32_t bytes) {
    for (uint32_t i = 0u; i < bytes / 2u; ++i) dst[i] = src[i];
}

static void copy32(volatile uint32_t* dst, const uint32_t* src, uint32_t bytes) {
    for (uint32_t i = 0u; i < bytes / 4u; ++i) dst[i] = src[i];
}

static void read16(uint16_t* dst, const volatile uint16_t* src, uint32_t bytes) {
    for (uint32_t i = 0u; i < bytes / 2u; ++i) dst[i] = src[i];
}

static void read32(uint32_t* dst, const volatile uint32_t* src, uint32_t bytes) {
    for (uint32_t i = 0u; i < bytes / 4u; ++i) dst[i] = src[i];
}

static uint16_t frt_now(void) {
    return sat_time_frc();
}

static uint32_t elapsed(uint16_t since) {
    return (uint16_t)(frt_now() - since);
}

static void run_span_experiments(const sat_cdfs_file_t* file) {
    static const uint32_t sectors[SPAN_EXPERIMENTS] = {8u, 32u, 64u, 64u, 32u, 8u, 4u, 16u};
    for (uint32_t e = 0u; e < SPAN_EXPERIMENTS; ++e) {
        uint32_t span_bytes = sectors[e] * SECTOR_BYTES;
        uint32_t spans = EXPERIMENT_BYTES / span_bytes;
        uint32_t total = 0u;
        uint32_t worst = 0u;
        uint32_t bad = 0u;
        sat_result_t status = SAT_OK;
        g_load.span_sectors[e] = sectors[e];
        g_load.span_count[e] = spans;
        for (uint32_t s = 0u; s < spans && status == SAT_OK; ++s) {
            uint16_t begin = frt_now();
            status = sat_cd_block_read_sectors(
                &g_block, file->extent_lba + (e * EXPERIMENT_BYTES) / SECTOR_BYTES + s * sectors[e],
                sectors[e], g_staging);
            uint32_t cost = elapsed(begin);
            total += cost;
            if (cost > worst) worst = cost;
            if (status == SAT_OK) bad += count_mismatch(g_staging, e * EXPERIMENT_BYTES + s * span_bytes, span_bytes);
        }
        g_load.span_ticks[e] = total;
        g_load.span_max_ticks[e] = worst;
        g_load.span_status[e] = (uint32_t)status;
        g_load.span_mismatch[e] = bad;
    }
}

static sat_result_t run_full_load(const sat_cdfs_file_t* file, uint8_t* cart) {
    const uint32_t span_sectors = 32u;
    const uint32_t span_bytes = span_sectors * SECTOR_BYTES;
    uint32_t cd_ticks = 0u;
    uint32_t copy_ticks = 0u;
    uint32_t bad_staging = 0u;
    for (uint32_t offset = 0u; offset < PAYLOAD_BYTES; offset += span_bytes) {
        uint16_t begin = frt_now();
        sat_result_t status = sat_cd_block_read_sectors(
            &g_block, file->extent_lba + offset / SECTOR_BYTES, span_sectors, g_staging);
        cd_ticks += elapsed(begin);
        if (status != SAT_OK) return status;
        bad_staging += count_mismatch(g_staging, offset, span_bytes);
        begin = frt_now();
        copy16((volatile uint16_t*)(cart + offset), (const uint16_t*)g_staging, span_bytes);
        copy_ticks += elapsed(begin);
    }
    g_load.load_cd_ticks = cd_ticks;
    g_load.load_copy16_ticks = copy_ticks;
    g_load.load_mismatch_staging = bad_staging;
    return SAT_OK;
}

static void verify_cart(const uint8_t* cart) {
    uint32_t bad = 0u;
    for (uint32_t offset = 0u; offset < PAYLOAD_BYTES; offset += STAGING_BYTES) {
        read16((uint16_t*)g_staging, (const volatile uint16_t*)(cart + offset), STAGING_BYTES);
        bad += count_mismatch(g_staging, offset, STAGING_BYTES);
    }
    g_load.load_mismatch_cart = bad;
}

static void run_copy32(uint8_t* cart) {
    /* Re-write the first 256 KiB with 32-bit stores from a staging buffer
     * refilled from the already verified cart, then verify it again. */
    uint32_t ticks = 0u;
    for (uint32_t offset = 0u; offset < COPY32_BYTES; offset += STAGING_BYTES) {
        read16((uint16_t*)g_staging, (const volatile uint16_t*)(cart + offset), STAGING_BYTES);
        uint16_t begin = frt_now();
        copy32((volatile uint32_t*)(cart + offset), (const uint32_t*)g_staging, STAGING_BYTES);
        ticks += elapsed(begin);
    }
    g_load.copy32_ticks = ticks;
    uint32_t bad = 0u;
    for (uint32_t offset = 0u; offset < COPY32_BYTES; offset += STAGING_BYTES) {
        read16((uint16_t*)g_staging, (const volatile uint16_t*)(cart + offset), STAGING_BYTES);
        bad += count_mismatch(g_staging, offset, STAGING_BYTES);
    }
    g_load.copy32_mismatch = bad;
}

static void run_paging(const uint8_t* cart) {
    uint32_t ticks16 = 0u;
    uint32_t ticks32 = 0u;
    for (uint32_t i = 0u; i < PAGE_ITERATIONS; i += PAGE_BATCH) {
        uint16_t begin = frt_now();
        for (uint32_t j = 0u; j < PAGE_BATCH; ++j) {
            read16((uint16_t*)g_staging,
                   (const volatile uint16_t*)(cart + (i + j) * PAGE_BYTES), PAGE_BYTES);
        }
        ticks16 += elapsed(begin);
    }
    uint32_t bad = count_mismatch(g_staging, (PAGE_ITERATIONS - 1u) * PAGE_BYTES, PAGE_BYTES);
    for (uint32_t i = 0u; i < PAGE_ITERATIONS; i += PAGE_BATCH) {
        uint16_t begin = frt_now();
        for (uint32_t j = 0u; j < PAGE_BATCH; ++j) {
            read32((uint32_t*)g_staging,
                   (const volatile uint32_t*)(cart + (i + j) * PAGE_BYTES), PAGE_BYTES);
        }
        ticks32 += elapsed(begin);
    }
    bad += count_mismatch(g_staging, (PAGE_ITERATIONS - 1u) * PAGE_BYTES, PAGE_BYTES);
    g_load.page16_ticks = ticks16;
    g_load.page32_ticks = ticks32;
    g_load.page_mismatch = bad;
}

int main(void) {
    if (sat_app_init_default() != SAT_OK) return 1;
    g_load.magic = LOAD_MAGIC;

    sat_result_t status = sat_cd_block_init(&g_block, SAT_CD_BLOCK_DEFAULT_TIMEOUT);
    g_load.status_cd_init = (uint32_t)status;
    if (status == SAT_OK) status = sat_cd_block_bind_device(&g_block, &g_device, 0u);
    if (status == SAT_OK) {
        status = sat_cdfs_mount(&g_volume, &g_device);
        g_load.status_mount = (uint32_t)status;
    }
    sat_cdfs_file_t file = {0};
    if (status == SAT_OK) {
        status = sat_cdfs_lookup(&g_volume, "PROBE.BIN", &file);
        g_load.status_lookup = (uint32_t)status;
        g_load.extent_lba = file.extent_lba;
        g_load.file_size = file.size;
    }
    if (status == SAT_OK && file.size != PAYLOAD_BYTES) status = SAT_ERR_IO;

    sat_result_t cart_status = sat_ram_cart_init();
    sat_ram_cart_info_t info = {0};
    g_load.status_cart_init = (uint32_t)cart_status;
    if (cart_status == SAT_OK) (void)sat_ram_cart_info(&info);
    g_load.cart_type = (uint32_t)info.type;
    g_load.cart_capacity = info.capacity;

    g_load.frt_tcr = *(volatile uint8_t*)0xFFFFFE16u;
    (void)sat_wait_vblank();
    uint16_t calib = frt_now();
    for (uint32_t v = 0u; v < 10u; ++v) (void)sat_wait_vblank();
    g_load.ticks_per_10_vblanks = elapsed(calib);

    if (status == SAT_OK) run_span_experiments(&file);

    uint8_t* cart = 0;
    if (status == SAT_OK && cart_status == SAT_OK) {
        cart = (uint8_t*)sat_ram_cart_alloc(PAYLOAD_BYTES, 32u);
        g_load.cart_alloc_ok = cart != 0;
    }
    if (status == SAT_OK && cart != 0) {
        sat_result_t load = run_full_load(&file, cart);
        g_load.load_status = (uint32_t)load;
        if (load == SAT_OK) {
            verify_cart(cart);
            run_copy32(cart);
            run_paging(cart);
        }
    }
    g_load.done = 1u;

    uint16_t color = SAT_COLOR_RED;
    if (status == SAT_OK && cart != 0 && g_load.load_status == 0u &&
        g_load.load_mismatch_cart == 0u && g_load.load_mismatch_staging == 0u) {
        color = SAT_COLOR_GREEN;
    }
    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_app_frame_begin(SAT_COLOR_BLACK, color, &pad) != SAT_OK) break;
        (void)sat_app_frame_end();
    }
    return 0;
}
