/* VDP2 dynamic tile/map streaming (E07).
 *
 * A top-down race road scrolls upward for a long run. The map is a 64x64-cell
 * torus (16x16 metatiles of 4x4 cells) generated from an original procedural
 * road tileset. Two pattern-name rings live at 0x00000 and 0x04000; each
 * frame stages the next view into the INACTIVE ring, commits it while VDP2 is
 * displaying the active one, then publishes that ring's base and scroll for
 * the next VBlank. The active ring is never written, so the picture cannot
 * tear; the switch is a map-base register write replayed by
 * sat_vdp2_layers_commit().
 *
 * The writing side is sat_stage_map2: it keeps a bounded resident window
 * (viewport + 2 cells of margin), stages only the cells that entered it, and
 * has no heap. Two independent instances give the double buffer. The
 * sat_vdp2_vram_write_words writer uses SCU DMA for long, aligned runs and
 * CPU stores otherwise; the HUD and telemetry report both paths.
 *
 * Every frame the example reads the active resident window back from VDP2
 * VRAM, compares it with sat_stage_map2_cell() and records two CRCs plus a
 * sentinel count. The harness dumps the same ring from Ymir and recomputes
 * the CRC, so an uninitialised or stale cell is visible without a golden
 * image. The two bases are alternated every frame and watched at MPABN0.
 *
 * The camera advances 2 px per frame. It wraps at 16384 px, a multiple of the
 * 512 px map period, so the visual step across the ring seam is still 2 px;
 * the streamer rebuilds its window (bounded by the storage requirements)
 * without changing the picture. A 36,000-frame run is the 10-minute-class
 * stress requested by the plan.
 *
 * Controls: START exits. The scene runs automatically. */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/dma.h"
#include "saturn/font.h"
#include "saturn/saturn.h"
#include "saturn/stage_map2.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"
#include "saturn/vdp2_layers.h"

#define TILE_STREAM_MAGIC SAT_FOURCC('T', 'S', 'T', '1')

#define VIEW_W 320u
#define VIEW_H 224u
#define MARGIN_CELLS 2u
#define RING_CELLS 64u
#define RING_WORDS (RING_CELLS * RING_CELLS)
#define RING_A_BYTES 0x00000u
#define RING_B_BYTES 0x04000u
#define CHARS_BYTES 0x40000u

#define MAP_METATILES 16u
#define METATILE_SHIFT 2u
#define METATILE_CELLS (1u << (2u * METATILE_SHIFT))
#define METATILE_COUNT (MAP_METATILES * MAP_METATILES)
#define STAGE_WORDS 2970u
#define STAGE_RUNS 264u
#define SENTINEL_WORD 0xFFFFu
#define CAMERA_WRAP 16384u
#define CAMERA_STEP 2u
#define FREEZE_FRAME 33000u

typedef struct tile_stream_results {
    uint32_t magic;
    uint32_t init_ok;
    uint32_t storage_ok;
    uint32_t refused_view;
    uint32_t refused_base;
    uint32_t refused_dma;
    uint32_t capacity_errors;
    uint32_t write_errors;
    uint32_t switches;
    uint32_t frames;
    uint32_t active;
    uint32_t active_base;
    uint32_t win_x0;
    uint32_t win_y0;
    uint32_t win_cols;
    uint32_t win_rows;
    uint32_t crc_expected;
    uint32_t crc_vram;
    uint32_t crc_mismatches;
    uint32_t sentinel_cells;
    uint32_t max_pending_words;
    uint32_t staged_words;
    uint32_t committed_words;
    uint32_t dma_scu;
    uint32_t dma_cpu;
    uint32_t dma_bytes_scu;
    uint32_t dma_bytes_cpu;
    uint32_t dma_timeouts;
    uint32_t dma_illegal;
    uint32_t scroll_y;
    uint32_t frozen;
    uint32_t crc_frame;
} tile_stream_results_t;

volatile tile_stream_results_t g_tile_stream;

static sat_ascii_font_t g_font;
static uint16_t g_tileset_cells[METATILE_COUNT * METATILE_CELLS];
static uint16_t g_map[METATILE_COUNT];
static sat_stage_map2_tileset_t g_tileset;
static sat_stage_map2_layer_desc_t g_desc[2];
static sat_stage_map2_config_t g_config[2];
static sat_stage_map2_t g_stream[2];
static sat_stage_map2_storage_t g_storage[2];
static uint16_t g_stage_words[2][STAGE_WORDS];
static sat_stage_map2_run_t g_stage_runs[2][STAGE_RUNS];
static uint16_t g_sentinel[RING_WORDS];
static uint16_t g_row[64];

/* 16-colour palette 0: 1 asphalt, 2 white curb, 3 grass, 4 lane, 5 edge,
 * 6 red curb, 7 dark grass, 8 dirt, 9 light asphalt. Palette 15 is the
 * uninitialised-cell canary (magenta), reached by the 0xFFFF sentinel. */
static uint16_t map_cell(int32_t cx, int32_t cy) {
    cx &= 63;
    cy &= 63;
    if (cx < 6 || cx > 37) return (((cx * 7 + cy * 3) & 15) == 0) ? 7u : 3u;
    if (cx == 6 || cx == 37) return ((cy & 3) < 2) ? 2u : 6u;
    if (cx == 7 || cx == 36) return 5u;
    if ((cx == 14 || cx == 22 || cx == 30) && ((cy & 3) < 2)) return 4u;
    return 1u;
}

static void build_map(void) {
    for (uint32_t my = 0; my < MAP_METATILES; ++my) {
        for (uint32_t mx = 0; mx < MAP_METATILES; ++mx) {
            const uint32_t index = my * MAP_METATILES + mx;
            g_map[index] = (uint16_t)index;
            for (uint32_t ly = 0; ly < 4u; ++ly) {
                for (uint32_t lx = 0; lx < 4u; ++lx) {
                    g_tileset_cells[index * METATILE_CELLS + ly * 4u + lx] =
                        map_cell((int32_t)(mx * 4u + lx), (int32_t)(my * 4u + ly));
                }
            }
        }
    }
    g_tileset.cells = g_tileset_cells;
    g_tileset.metatile_count = (uint16_t)METATILE_COUNT;
    g_tileset.shift = METATILE_SHIFT;
    g_tileset.reserved = 0u;
}

static void setup_ring(uint32_t index, uint32_t base_bytes) {
    sat_stage_map2_layer_desc_t* d = &g_desc[index];
    *d = (sat_stage_map2_layer_desc_t){0};
    d->map = g_map;
    d->map_w = MAP_METATILES;
    d->map_h = MAP_METATILES;
    d->fill_word = SENTINEL_WORD;
    d->outside = SAT_STAGE_MAP2_OUTSIDE_WRAP;
    d->flags = 0u;
    d->palette = 0u;
    d->vdp2_layer = SAT_VDP2_NBG0;
    d->char_bias = 0;
    d->ratio_x = SAT_FX16_ONE;
    d->ratio_y = SAT_FX16_ONE;
    d->ring.cell_shift = 3u;
    d->ring.page_shift = 6u;
    d->ring.pages_x = 1u;
    d->ring.pages_y = 1u;
    d->ring.page_word[0] = base_bytes / 2u;
    g_config[index].tileset = &g_tileset;
    g_config[index].layers = d;
    g_config[index].layer_count = 1u;
    g_config[index].margin_cells = MARGIN_CELLS;
    g_config[index].viewport_w = VIEW_W;
    g_config[index].viewport_h = VIEW_H;
    g_storage[index].words = g_stage_words[index];
    g_storage[index].word_capacity = STAGE_WORDS;
    g_storage[index].runs = g_stage_runs[index];
    g_storage[index].run_capacity = STAGE_RUNS;
}

static sat_result_t setup_layer(void) {
    sat_vdp2_layer_config_t c;
    sat_vdp2_layer_config_default(SAT_VDP2_NBG0, &c);
    c.color_mode = SAT_VDP2_COLOR_MODE_16;
    c.char_size = SAT_VDP2_CHAR_SIZE_1X1;
    c.pattern_name_words = 1u;
    c.plane_pages_x = 1u;
    c.plane_pages_y = 1u;
    c.priority = 4u;
    c.transparent = 1u;
    c.char_bank_mask = SAT_VDP2_BANK_B0;
    c.char_base_address = CHARS_BYTES;
    for (uint32_t p = 0u; p < 4u; ++p) c.plane_address[p] = RING_A_BYTES;
    return sat_vdp2_layer_configure(&c);
}

static void upload_tiles(void) {
    uint16_t tile[16];
    for (uint32_t code = 0u; code < 16u; ++code) {
        const uint16_t word = (uint16_t)(code | (code << 4u) | (code << 8u) | (code << 12u));
        for (uint32_t i = 0u; i < 16u; ++i) tile[i] = word;
        sat_vdp2_vram_write_words((CHARS_BYTES + code * 32u) / 2u, tile, 16u);
    }
    /* Character 1023 is the sentinel's code-15 magenta tile. */
    for (uint32_t i = 0u; i < 16u; ++i) tile[i] = 0xFFFFu;
    sat_vdp2_vram_write_words((CHARS_BYTES + 1023u * 32u) / 2u, tile, 16u);
}

static void upload_palette(void) {
    uint16_t palette[256] = {0};
    palette[1] = SAT_RGB555_RAW(16, 16, 18);
    palette[2] = SAT_RGB555_RAW(31, 31, 31);
    palette[3] = SAT_RGB555_RAW(4, 24, 4);
    palette[4] = SAT_RGB555_RAW(31, 28, 0);
    palette[5] = SAT_RGB555_RAW(31, 31, 31);
    palette[6] = SAT_RGB555_RAW(28, 4, 4);
    palette[7] = SAT_RGB555_RAW(2, 16, 2);
    palette[8] = SAT_RGB555_RAW(20, 14, 6);
    palette[9] = SAT_RGB555_RAW(24, 24, 26);
    for (uint32_t i = 0u; i < 16u; ++i) palette[15u * 16u + i] = SAT_RGB555_RAW(31, 0, 31);
    (void)sat_vdp2_palette_upload(palette, 256u, 0u);
}

static uint32_t crc_word(uint32_t crc, uint16_t word) {
    crc ^= word;
    for (uint32_t i = 0u; i < 16u; ++i) {
        crc = (crc >> 1u) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return crc;
}

/* Reads the active resident window from VRAM in row runs and compares it to
 * the staged model. The per-frame path only publishes the cheap window
 * coordinates; the CRC readback runs every 60 frames (and once at the freeze)
 * so the VDP2 display can stay at 60 fps. */
static void update_window_telemetry(uint8_t active, uint8_t compute_crc) {
    const sat_stage_map2_layer_state_t* l = &g_stream[active].layer[0];
    const uint32_t base_word = (active == 0u ? RING_A_BYTES : RING_B_BYTES) / 2u;
    uint32_t ce = 0xFFFFFFFFu, cv = 0xFFFFFFFFu;
    uint32_t sentinel = 0u;
    g_tile_stream.win_x0 = (uint32_t)l->x0 & (RING_CELLS - 1u);
    g_tile_stream.win_y0 = (uint32_t)l->y0 & (RING_CELLS - 1u);
    g_tile_stream.win_cols = l->cols;
    g_tile_stream.win_rows = l->rows;
    g_tile_stream.active_base = active == 0u ? RING_A_BYTES : RING_B_BYTES;
    sat_fx16_t sx = 0, sy = 0;
    if (sat_stage_map2_scroll(&g_stream[active], 0u, &sx, &sy) == SAT_OK) {
        g_tile_stream.scroll_y = (uint32_t)(sy >> 16) & 0x1FFu;
    }
    if (compute_crc == 0u) return;
    for (int32_t r = 0; r < (int32_t)l->rows; ++r) {
        const int32_t cy = l->y0 + r;
        const uint32_t ry = (uint32_t)cy & (RING_CELLS - 1u);
        int32_t done = 0;
        while (done < (int32_t)l->cols) {
            const int32_t cx = l->x0 + done;
            const uint32_t rx = (uint32_t)cx & (RING_CELLS - 1u);
            uint32_t n = RING_CELLS - rx;
            if (n > (uint32_t)((int32_t)l->cols - done)) n = (uint32_t)((int32_t)l->cols - done);
            (void)sat_vdp2_vram_read_words(base_word + ry * RING_CELLS + rx, g_row, n);
            for (uint32_t i = 0u; i < n; ++i) {
                const uint16_t expected = sat_stage_map2_cell(&g_stream[active], 0, cx + (int32_t)i, cy);
                const uint16_t actual = g_row[i];
                ce = crc_word(ce, expected);
                cv = crc_word(cv, actual);
                if (actual == SENTINEL_WORD) ++sentinel;
            }
            done += (int32_t)n;
        }
    }
    g_tile_stream.crc_expected = ce ^ 0xFFFFFFFFu;
    g_tile_stream.crc_vram = cv ^ 0xFFFFFFFFu;
    if (g_tile_stream.crc_expected != g_tile_stream.crc_vram) ++g_tile_stream.crc_mismatches;
    g_tile_stream.sentinel_cells = sentinel;
    g_tile_stream.crc_frame = g_tile_stream.frames;
}

static sat_result_t publish_next(uint8_t active, uint32_t camera_y) {
    const uint8_t next = (uint8_t)(active ^ 1u);
    sat_result_t st = sat_stage_map2_set_view(&g_stream[next], 0,
                                              (sat_fx16_t)(camera_y << 16u));
    if (st != SAT_OK) return st;
    uint32_t runs = 0u, words = 0u;
    sat_stage_map2_pending(&g_stream[next], &runs, &words);
    if (words > g_tile_stream.max_pending_words) g_tile_stream.max_pending_words = words;
    st = sat_stage_map2_commit_vdp2(&g_stream[next]);
    if (st != SAT_OK) return st;
    /* Validate/stage the base first: if it were refused after the scroll was
     * staged, the next VBlank would pair the old ring with the new scroll. */
    st = sat_vdp2_layer_set_pattern_name_base(
        SAT_VDP2_NBG0, next == 0u ? RING_A_BYTES : RING_B_BYTES);
    if (st != SAT_OK) return st;
    return sat_stage_map2_apply_scroll_vdp2(&g_stream[next]);
}

static void draw_hud(void) {
    /* Opaque HUD strip: the font's zero texels are transparent, so white text
     * over the white road edge would otherwise disappear as the map scrolls. */
    (void)sat_draw_rect_screen(0, 0, 232, 64, SAT_RGB555(0, 0, 0));
    (void)sat_ascii_font_draw_text_screen_indexed8(
        &g_font, "VDP2 TILE STREAM", 8, 8, 8, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(
        &g_font, "FRAME ", g_tile_stream.frames, 8, 24, 8, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(
        &g_font, "SWITCH ", g_tile_stream.switches, 8, 40, 8, 0u, 0u);
    (void)sat_ascii_font_draw_text_screen_indexed8(
        &g_font, g_tile_stream.crc_expected == g_tile_stream.crc_vram ? "CRC OK" : "CRC BAD",
        8, 56, 8, 0u, 0u);
    /* A simple original VDP1 car at the bottom, above the scrolling ground. */
    (void)sat_draw_rect_screen(148, 164, 24, 32, SAT_RGB555(28, 4, 4));
    (void)sat_draw_rect_screen(152, 168, 16, 10, SAT_RGB555(8, 16, 28));
    (void)sat_draw_rect_screen(150, 182, 4, 10, SAT_RGB555(4, 4, 4));
    (void)sat_draw_rect_screen(166, 182, 4, 10, SAT_RGB555(4, 4, 4));
}

int main(void) {
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) {
        return 1;
    }
    upload_palette();
    build_map();
    upload_tiles();

    /* Poison both rings, then fill the visible windows from the model. */
    for (uint32_t i = 0u; i < RING_WORDS; ++i) g_sentinel[i] = SENTINEL_WORD;
    (void)sat_vdp2_vram_write_words(RING_A_BYTES / 2u, g_sentinel, RING_WORDS);
    (void)sat_vdp2_vram_write_words(RING_B_BYTES / 2u, g_sentinel, RING_WORDS);
    {
        const uint16_t one = 0u;
        (void)sat_vdp2_vram_write_words(0x7FFFFu, &one, 1u); /* force one CPU transfer */
    }

    g_tile_stream.magic = TILE_STREAM_MAGIC;
    for (uint32_t i = 0u; i < 2u; ++i) setup_ring(i, i == 0u ? RING_A_BYTES : RING_B_BYTES);
    g_tile_stream.storage_ok = 1u;
    for (uint32_t i = 0u; i < 2u; ++i) {
        uint32_t words = 0u, runs = 0u;
        if (sat_stage_map2_requirements(&g_config[i], &words, &runs) != SAT_OK ||
            words > STAGE_WORDS || runs > STAGE_RUNS) {
            g_tile_stream.storage_ok = 0u;
        }
        if (sat_stage_map2_init(&g_stream[i], &g_config[i], &g_storage[i]) != SAT_OK) {
            g_tile_stream.storage_ok = 0u;
        }
    }
    if (g_tile_stream.storage_ok != 0u) {
        for (uint32_t i = 0u; i < 2u; ++i) {
            if (sat_stage_map2_set_view(&g_stream[i], 0, 0) != SAT_OK ||
                sat_stage_map2_commit_vdp2(&g_stream[i]) != SAT_OK) {
                g_tile_stream.storage_ok = 0u;
            }
        }
    }
    if (setup_layer() != SAT_OK) return 1;
    (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG0, 0, 0);
    (void)sat_vdp2_sprite_set_priority(7u);

    /* Capacity and invalid-base refusals, before the scene starts. */
    {
        static uint16_t tiny_words[1];
        static sat_stage_map2_run_t tiny_runs[1];
        sat_stage_map2_t tiny = {0};
        sat_stage_map2_storage_t ts = {tiny_words, 1u, tiny_runs, 1u};
        sat_stage_map2_config_t tc = g_config[0];
        if (sat_stage_map2_init(&tiny, &tc, &ts) == SAT_OK) {
            g_tile_stream.refused_view =
                sat_stage_map2_set_view(&tiny, 0, 0) == SAT_ERR_CAPACITY;
        }
    }
    g_tile_stream.refused_base =
        sat_vdp2_layer_set_pattern_name_base(SAT_VDP2_NBG0, 0x01000u) == SAT_ERR_INVALID_ARG;
    {
        uint32_t a = 0u, b = 0u;
        g_tile_stream.refused_dma = sat_dma_start(3u, &a, &b, 4u) == SAT_ERR_INVALID_ARG;
        (void)a;
        (void)b;
    }
    g_tile_stream.init_ok =
        (g_tile_stream.storage_ok != 0u && g_tile_stream.refused_view != 0u &&
         g_tile_stream.refused_base != 0u && g_tile_stream.refused_dma != 0u);

    uint8_t active = 0u, pending = 0u;
    uint32_t camera_y = 0u;
    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        (void)sat_vdp2_layers_commit();
        if (pending != 0u) {
            active ^= 1u;
            pending = 0u;
            ++g_tile_stream.switches;
        }
        (void)sat_vdp1_set_erase_transparent();
        if (sat_begin_frame() != SAT_OK) break;
        (void)sat_pad_poll(&pad);
        ++g_tile_stream.frames;
        g_tile_stream.active = active;

        if (g_tile_stream.frozen == 0u && g_tile_stream.frames >= FREEZE_FRAME) {
            /* Leave the last published frame on screen so the probe's VRAM
             * dump and the final CRC describe exactly the same picture. */
            g_tile_stream.frozen = 1u;
            update_window_telemetry(active, 1u);
        } else if (g_tile_stream.frozen != 0u) {
            update_window_telemetry(active, 0u);
        } else {
            uint32_t next_camera = camera_y + CAMERA_STEP;
            if (next_camera >= CAMERA_WRAP) next_camera -= CAMERA_WRAP;
            const sat_result_t published = publish_next(active, next_camera);
            if (published == SAT_OK) {
                camera_y = next_camera;
                pending = 1u;
            } else if (published == SAT_ERR_CAPACITY) {
                ++g_tile_stream.capacity_errors;
                sat_stage_map2_discard(&g_stream[(uint8_t)(active ^ 1u)]);
            } else {
                ++g_tile_stream.write_errors;
                sat_stage_map2_discard(&g_stream[(uint8_t)(active ^ 1u)]);
            }
            update_window_telemetry(active, (g_tile_stream.frames % 60u) == 0u);
        }
        g_tile_stream.staged_words = sat_stage_map2_stats(&g_stream[0]).cells_staged +
                                     sat_stage_map2_stats(&g_stream[1]).cells_staged;
        g_tile_stream.committed_words = sat_stage_map2_stats(&g_stream[0]).cells_committed +
                                        sat_stage_map2_stats(&g_stream[1]).cells_committed;
        {
            sat_dma_stats_t ds = {0};
            sat_dma_get_stats(&ds);
            g_tile_stream.dma_scu = ds.scu_transfers;
            g_tile_stream.dma_cpu = ds.cpu_copies;
            g_tile_stream.dma_bytes_scu = ds.bytes_scu;
            g_tile_stream.dma_bytes_cpu = ds.bytes_cpu;
            g_tile_stream.dma_timeouts = ds.timeouts;
            g_tile_stream.dma_illegal = ds.illegal;
        }
        draw_hud();
        (void)sat_end_frame();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }

    (void)sat_shutdown();
    return 0;
}
