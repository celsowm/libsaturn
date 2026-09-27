/* city_walk loader: CD -> WRAM-L staging -> (RAM cart | VDP2 VRAM | CRAM).
 *
 * The shape is dictated by measurements (plan section 0, R3): a CD read
 * costs ~27 ms per call plus 6.5 ms per sector, so it is few large reads by
 * LBA (32 sectors = 64 KiB) straight from CDFS's extent, never the 64 KiB
 * asset cache. The cart is written with 32-bit stores from that staging
 * buffer (1.8x the 16-bit rate, verified), never directly from the CD block,
 * whose byte-wise transfer would make a bank boundary inside a read
 * unrecoverable.
 *
 * Every 64 KiB span is routed by file offset: table -> WRAM, ground bitmap ->
 * VDP2 VRAM, ground palette -> CRAM, blobs -> the cart bank they belong to.
 */
#include <stdint.h>

#include "saturn/cd_block.h"
#include "saturn/cdfs.h"
#include "saturn/vdp2.h"
#include "saturn/video.h"

#include "city_walk.h"
#include "city_walk/city_data.h"

#define SPAN_SECTORS 32u
#define SPAN_BYTES (SPAN_SECTORS * 2048u)

city_archive_t g_archive;

static sat_cd_block_t g_block;
static sat_cd_device_t g_device;
static sat_cdfs_volume_t g_volume;

static void copy32(volatile uint32_t* dst, const uint32_t* src, uint32_t bytes) {
    for (uint32_t i = 0u; i < bytes / 4u; ++i) dst[i] = src[i];
}

static uint32_t max_u32(uint32_t a, uint32_t b) { return a > b ? a : b; }
static uint32_t min_u32(uint32_t a, uint32_t b) { return a < b ? a : b; }

/* Intersection of [a0, a1) and [b0, b1); returns 1 when non-empty. */
static int overlap(uint32_t a0, uint32_t a1, uint32_t b0, uint32_t b1,
                   uint32_t* out0, uint32_t* out1) {
    *out0 = max_u32(a0, b0);
    *out1 = min_u32(a1, b1);
    return *out1 > *out0;
}

static sat_result_t route_span(const uint8_t* data, uint32_t span_off, uint32_t bytes) {
    const city_header_t* h = &g_archive.header;
    const uint32_t span_end = span_off + bytes;
    const uint32_t toc_end = h->toc_offset + CITY_TOC_ENTRIES * CITY_TOC_ENTRY_BYTES;
    uint32_t s, e;

    /* Materials + padding + TOC, kept in WRAM for the whole run. */
    if (overlap(span_off, span_end, h->material_offset, toc_end, &s, &e)) {
        uint8_t* dst = g_archive.table + (s - h->material_offset);
        const uint8_t* src = data + (s - span_off);
        for (uint32_t i = 0u; i < e - s; ++i) dst[i] = src[i];
    }
    if (h->ground_bytes != 0u) {
        if (overlap(span_off, span_end, h->ground_offset, h->ground_offset + h->ground_bytes, &s, &e)) {
            sat_result_t st = sat_vdp2_vram_write_words(
                CITY_GROUND_BITMAP_WORD + (s - h->ground_offset) / 2u,
                (const uint16_t*)(data + (s - span_off)), (e - s) / 2u);
            if (st != SAT_OK) return st;
        }
        if (overlap(span_off, span_end, h->ground_palette_offset,
                    h->ground_palette_offset + (uint32_t)h->ground_palette_count * 2u, &s, &e)) {
            uint8_t* dst = (uint8_t*)g_archive.ground_palette + (s - h->ground_palette_offset);
            const uint8_t* src = data + (s - span_off);
            for (uint32_t i = 0u; i < e - s; ++i) dst[i] = src[i];
        }
    }
    if (h->texture_palette_count != 0u &&
        overlap(span_off, span_end, h->texture_palette_offset,
                h->texture_palette_offset + (uint32_t)h->texture_palette_count * 2u, &s, &e)) {
        uint8_t* dst = (uint8_t*)g_archive.texture_palette + (s - h->texture_palette_offset);
        const uint8_t* src = data + (s - span_off);
        for (uint32_t i = 0u; i < e - s; ++i) dst[i] = src[i];
    }
    /* Blobs: never straddle a 2 MiB boundary of this region (the packer pads),
     * but a 64 KiB span can, so split the copy where the bank changes. */
    uint32_t from = max_u32(span_off, h->blob_base);
    while (from < span_end) {
        uint32_t rel = from - h->blob_base;
        uint32_t bank = rel / CITY_BANK_BYTES;
        uint32_t bank_end = h->blob_base + (bank + 1u) * CITY_BANK_BYTES;
        uint32_t to = min_u32(span_end, min_u32(bank_end, h->total_bytes));
        if (to <= from) break;
        if (bank > 1u || g_archive.bank[bank] == 0) return SAT_ERR_CAPACITY;
        copy32((volatile uint32_t*)(g_archive.bank[bank] + (rel - bank * CITY_BANK_BYTES)),
               (const uint32_t*)(data + (from - span_off)), to - from);
        from = to;
    }
    return SAT_OK;
}

static sat_result_t check_cart(void) {
    sat_ram_cart_info_t info = {SAT_RAM_CART_NONE, 0u, 0u, 0u};
    sat_result_t st = sat_ram_cart_init();
    if (st == SAT_OK) (void)sat_ram_cart_info(&info);
    g_city.cart_type = (uint32_t)info.type;
    g_city.cart_capacity = info.capacity;
    if (st == SAT_ERR_NOT_CONNECTED) return SAT_ERR_NOT_CONNECTED;
    if (st != SAT_OK) return st;
    /* Not "enough bytes": the archive relies on two 2 MiB banks, because
     * sat_ram_cart_alloc never spans one. A 1 MiB cart is refused, not
     * half-loaded. */
    return info.type == SAT_RAM_CART_4MB ? SAT_OK : SAT_ERR_UNSUPPORTED;
}

sat_result_t city_loader_run(uint8_t* staging, uint32_t staging_bytes,
                             city_progress_fn progress) {
    sat_result_t st;
    sat_cdfs_file_t file = {0u, 0u, 0u, {0u, 0u, 0u}};
    uint32_t cd_ticks = 0u, copy_ticks = 0u, spans = 0u;
    const uint32_t total_begin = sat_frame_count();

    if (staging == 0 || staging_bytes < SPAN_BYTES) return SAT_ERR_INVALID_ARG;
    st = check_cart();
    if (st != SAT_OK) return st;

    progress("OPENING DISC", 2u);
    st = sat_cd_block_init(&g_block, SAT_CD_BLOCK_DEFAULT_TIMEOUT);
    if (st == SAT_OK) st = sat_cd_block_bind_device(&g_block, &g_device, 0u);
    if (st == SAT_OK) st = sat_cdfs_mount(&g_volume, &g_device);
    if (st == SAT_OK) st = sat_cdfs_lookup(&g_volume, "CITY.BIN", &file);
    if (st != SAT_OK) return st;
    if (file.size < CITY_HEADER_BYTES) return SAT_ERR_IO;

    /* The header decides where everything else goes, so read span 0 first. */
    {
        uint32_t sectors = min_u32(SPAN_SECTORS, (file.size + 2047u) / 2048u);
        uint16_t begin = city_frt_now();
        st = sat_cd_block_read_sectors(&g_block, file.extent_lba, sectors, staging);
        cd_ticks += (uint16_t)(city_frt_now() - begin);
        if (st != SAT_OK) return st;
    }
    st = city_header_parse(staging, min_u32(file.size, SPAN_BYTES), &g_archive.header);
    if (st != SAT_OK) return st;
    if (g_archive.header.total_bytes != file.size) return SAT_ERR_IO;
    if (g_archive.header.total_bytes != CITY_ARCHIVE_BYTES) return SAT_ERR_VERSION;
    st = city_header_check_caps(&g_archive.header);
    if (st != SAT_OK) return st;
    {
        const city_header_t* h = &g_archive.header;
        uint32_t table_bytes = h->toc_offset + CITY_TOC_ENTRIES * CITY_TOC_ENTRY_BYTES -
                               h->material_offset;
        if (table_bytes > sizeof(g_archive.table)) return SAT_ERR_CAPACITY;
        g_archive.materials = g_archive.table;
        g_archive.toc = g_archive.table + (h->toc_offset - h->material_offset);
    }

    /* Two allocations, one per physical bank: sat_ram_cart_alloc never spans
     * the gap, and the TOC's bank field says which one a blob is in. */
    {
        uint32_t region = g_archive.header.total_bytes - g_archive.header.blob_base;
        uint32_t first = region < CITY_BANK_BYTES ? region : CITY_BANK_BYTES;
        g_archive.bank[0] = (uint8_t*)sat_ram_cart_alloc(first + 32u > CITY_BANK_BYTES ? CITY_BANK_BYTES : first + 32u, 32u);
        if (g_archive.bank[0] == 0) return SAT_ERR_CAPACITY;
        if (region > CITY_BANK_BYTES) {
            g_archive.bank[1] = (uint8_t*)sat_ram_cart_alloc(region - CITY_BANK_BYTES + 32u, 32u);
            if (g_archive.bank[1] == 0) return SAT_ERR_CAPACITY;
        }
    }

    for (uint32_t off = 0u; off < g_archive.header.total_bytes; off += SPAN_BYTES) {
        uint32_t bytes = min_u32(SPAN_BYTES, g_archive.header.total_bytes - off);
        if (off != 0u) {
            uint16_t begin = city_frt_now();
            st = sat_cd_block_read_sectors(&g_block, file.extent_lba + off / 2048u,
                                           (bytes + 2047u) / 2048u, staging);
            cd_ticks += (uint16_t)(city_frt_now() - begin);
            if (st != SAT_OK) return st;
        }
        {
            uint16_t begin = city_frt_now();
            st = route_span(staging, off, bytes);
            copy_ticks += (uint16_t)(city_frt_now() - begin);
            if (st != SAT_OK) return st;
        }
        ++spans;
        progress("READING DISC", (uint8_t)(4u + (uint32_t)(off + bytes) * 90u /
                                              g_archive.header.total_bytes));
    }

    progress("VERIFYING", 96u);
    {
        const city_header_t* h = &g_archive.header;
        uint32_t table_bytes = h->toc_offset + CITY_TOC_ENTRIES * CITY_TOC_ENTRY_BYTES -
                               h->material_offset;
        g_city.toc_crc_ok = city_crc32(g_archive.table, table_bytes) == h->toc_crc32;
        if (!g_city.toc_crc_ok) return SAT_ERR_VERIFY_FAILED;
        if (h->ground_bytes != 0u) {
            st = sat_vdp2_palette_upload(g_archive.ground_palette, h->ground_palette_count,
                                         CITY_PALETTE_GROUND * 256u);
            if (st != SAT_OK) return st;
            g_city.ground_loaded = 1u;
        }
        if (h->texture_palette_count != 0u) {
            /* Facade texels index this bank. Entries carry the RGB code bit,
             * like the solid pool's, so both kinds of sprite read alike. */
            static uint16_t palette[256];
            for (uint32_t i = 0u; i < 256u; ++i) {
                palette[i] = i < h->texture_palette_count
                    ? (uint16_t)(0x8000u | g_archive.texture_palette[i]) : 0x8000u;
            }
            st = sat_palette_upload_indexed8(palette, CITY_PALETTE_TEXTURE);
            if (st != SAT_OK) return st;
        }
    }
    g_city.archive_bytes = g_archive.header.total_bytes;
    g_city.blob_base = g_archive.header.blob_base;
    g_city.material_count = g_archive.header.material_count;
    g_city.load_cd_ticks = cd_ticks;
    g_city.load_copy_ticks = copy_ticks;
    g_city.load_spans = spans;
    g_city.load_total_vblanks = sat_frame_count() - total_begin;
    return SAT_OK;
}
