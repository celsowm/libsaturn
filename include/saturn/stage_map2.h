#ifndef SATURN_STAGE_MAP2_H
#define SATURN_STAGE_MAP2_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/vdp2.h"
#include "saturn/vdp2_layers.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Large-map streaming to VDP2 scroll layers. The map is a grid of metatile indices, a
 * metatile is a small square of cells (VDP2 pattern names), and a layer shows the part of
 * the map around the camera. Nothing here replaces the low-level VDP2 calls: the streamer
 * only decides which pattern names have to be written and where, and a commit step hands
 * them to a writer.
 *
 *   map entries (per layer)  ->  metatile table (shared)  ->  cells  ->  VDP2 pattern names
 *
 * A VDP2 scroll layer is a ring: the cell at map column x, row y lives at ring column
 * x mod W, row y mod H, and scrolling just moves the window over it. A layer keeps the
 * cells it needs (the visible window plus a margin) resident, so a camera move writes only
 * the columns and rows that entered the window, a camera that stands still writes nothing,
 * and a jump too big to be incremental rebuilds just the window, never the whole ring.
 *
 * Staging is the caller's: update calls only produce write runs into a caller-owned buffer,
 * and sat_stage_map2_commit sends them (the pattern name table is VRAM, which is best
 * written in VBlank). A commit is the only thing that touches hardware, and only through
 * the writer function you give it, so the same code runs against a plain array on the host.
 *
 * Cell words use the VDP2 one-word pattern name layout, so a metatile table can be built
 * straight from a tile sheet: bits 0-9 character number, bit 10 horizontal flip, bit 11
 * vertical flip, bits 12-15 palette. Map entries are metatile indices (bits 0-13) with
 * SAT_STAGE_MAP2_FLIP_X / FLIP_Y mirroring the whole metatile. Cells are 8 x 8 pixels with
 * 1x1 characters or 16 x 16 with 2x2 characters. Two-word pattern names are not supported.
 *
 * Layers are generic: there are up to four, none is called foreground or background. Each
 * has its own map, its own VDP2 ring, and a scroll ratio against the camera (SAT_FX16_ONE
 * is locked to it; less is parallax). Resident windows are kept per layer.
 *
 * The module depends on core.h and the VDP2 layer configuration type only. */

#define SAT_STAGE_MAP2_MAX_LAYERS 4u
#define SAT_STAGE_MAP2_MAX_RING_PAGES 4u /* per axis */
#define SAT_STAGE_MAP2_FLIP_X 0x4000u
#define SAT_STAGE_MAP2_FLIP_Y 0x8000u
#define SAT_STAGE_MAP2_INDEX_MASK 0x3FFFu
#define SAT_STAGE_MAP2_CELL_HFLIP 0x0400u
#define SAT_STAGE_MAP2_CELL_VFLIP 0x0800u

typedef enum sat_stage_map2_outside {
    SAT_STAGE_MAP2_OUTSIDE_EMPTY = 0, /* the layer's fill_word */
    SAT_STAGE_MAP2_OUTSIDE_CLAMP = 1, /* the nearest cell on the map's edge */
    SAT_STAGE_MAP2_OUTSIDE_WRAP = 2   /* the map repeats */
} sat_stage_map2_outside_t;

#define SAT_STAGE_MAP2_PALETTE_OVERRIDE 0x01u /* replace the palette bits of every cell with `palette` */

/* The metatile table, shared by every layer. */
typedef struct sat_stage_map2_tileset {
    const uint16_t* cells;   /* metatile_count * (1 << 2*shift) cell words, row-major inside a metatile */
    uint16_t metatile_count; /* 1..16384 */
    uint8_t shift;           /* log2 of the metatile edge in cells, 0..4 */
    uint8_t reserved;
} sat_stage_map2_tileset_t;

/* Where a layer's cells live in VDP2 VRAM: a grid of pages_x * pages_y pattern name pages,
 * row-major, each a page of (1 << page_shift) cells per edge. This is the ring that cells
 * wrap around. */
typedef struct sat_stage_map2_ring {
    uint8_t cell_shift;  /* log2 pixels per cell: 3 (1x1 characters) or 4 (2x2) */
    uint8_t page_shift;  /* log2 cells per page edge: 6 (1x1 characters) or 5 (2x2) */
    uint8_t pages_x;     /* 1, 2 or 4 */
    uint8_t pages_y;     /* 1, 2 or 4 */
    uint32_t page_word[SAT_STAGE_MAP2_MAX_RING_PAGES * SAT_STAGE_MAP2_MAX_RING_PAGES]; /* VRAM word offset of each page */
} sat_stage_map2_ring_t;

/* The ring a configured VDP2 layer shows: when all four planes point at the same page(s)
 * the ring is one plane, otherwise the whole 2 x 2 plane map. SAT_ERR_INVALID_ARG for a
 * bitmap layer or two-word pattern names. */
sat_result_t sat_stage_map2_ring_from_layer(const sat_vdp2_layer_config_t* config, sat_stage_map2_ring_t* out);

typedef struct sat_stage_map2_layer_desc {
    const uint16_t* map;       /* map_w * map_h entries, row-major */
    uint16_t map_w, map_h;     /* in metatiles, > 0 */
    uint16_t fill_word;        /* the cell shown outside the map (EMPTY) and for an index the table lacks */
    uint8_t outside;           /* sat_stage_map2_outside_t */
    uint8_t flags;             /* SAT_STAGE_MAP2_PALETTE_OVERRIDE */
    uint8_t palette;           /* 0..15 with PALETTE_OVERRIDE */
    uint8_t vdp2_layer;        /* sat_vdp2_layer_t this layer feeds (used by the VDP2 helpers) */
    int16_t char_bias;         /* added to every cell's character number (modulo 1024) */
    uint16_t reserved;
    sat_fx16_t ratio_x, ratio_y; /* scroll against the camera, >= 0; SAT_FX16_ONE locks it to the camera */
    sat_stage_map2_ring_t ring;
} sat_stage_map2_layer_desc_t;

typedef struct sat_stage_map2_run {
    uint32_t word_offset;   /* VDP2 VRAM word offset of the first cell */
    uint32_t staging_index; /* the first word in the staging buffer */
    uint16_t count;
    uint16_t layer;
} sat_stage_map2_run_t;

/* Caller-owned staging: the cell words and the runs that place them. */
typedef struct sat_stage_map2_storage {
    uint16_t* words;
    uint32_t word_capacity;
    sat_stage_map2_run_t* runs;
    uint32_t run_capacity;
} sat_stage_map2_storage_t;

typedef struct sat_stage_map2_config {
    const sat_stage_map2_tileset_t* tileset;
    const sat_stage_map2_layer_desc_t* layers;
    uint8_t layer_count;      /* 1..SAT_STAGE_MAP2_MAX_LAYERS */
    uint8_t margin_cells;     /* extra cells kept resident on every side of the view */
    uint16_t viewport_w;      /* pixels, > 0 */
    uint16_t viewport_h;
    uint16_t reserved;
} sat_stage_map2_config_t;

typedef struct sat_stage_map2_stats {
    uint32_t updates;         /* set_view calls that changed what is resident */
    uint32_t rebuilds;        /* of those, how many rebuilt the window instead of sliding it */
    uint32_t cells_staged;    /* cells produced into staging (resident updates and dirty marks) */
    uint32_t cells_committed; /* cells handed to the writer */
    uint32_t runs_committed;
} sat_stage_map2_stats_t;

typedef struct sat_stage_map2_layer_state {
    sat_stage_map2_layer_desc_t desc;
    int32_t x0, y0;           /* resident window, in cells (its size is fixed by the config) */
    int32_t scroll_x, scroll_y; /* 16.16 pixels, wrapped to the ring */
    uint16_t cols, rows;      /* resident window size */
    uint8_t valid;            /* the resident window holds written cells */
    uint8_t reserved[3];
} sat_stage_map2_layer_state_t;

typedef struct sat_stage_map2 {
    sat_stage_map2_tileset_t tileset;
    sat_stage_map2_layer_state_t layer[SAT_STAGE_MAP2_MAX_LAYERS];
    sat_stage_map2_storage_t storage;
    uint32_t pending_words;
    uint32_t pending_runs;
    sat_stage_map2_stats_t stats;
    uint16_t viewport_w, viewport_h;
    uint8_t layer_count;
    uint8_t margin_cells;
    uint8_t reserved[2];
} sat_stage_map2_t;

/* Writes `count` words at VDP2 VRAM word offset `word_offset`. */
typedef sat_result_t (*sat_stage_map2_write_fn)(void* user, uint32_t word_offset, const uint16_t* words, uint32_t count);

/* SAT_ERR_INVALID_ARG for a missing pointer, an empty or oversized tileset or layer list,
 * a map with a zero side, a ring that is not 1/2/4 pages, a resident window larger than
 * a ring, a negative ratio or an unknown outside policy. Nothing is written. */
sat_result_t sat_stage_map2_init(sat_stage_map2_t* map, const sat_stage_map2_config_t* config,
    const sat_stage_map2_storage_t* storage);

/* Staging that always suffices: enough for a full rebuild of every layer, with room to spare
 * for one more update before a commit. Commit between updates to need less. */
sat_result_t sat_stage_map2_requirements(const sat_stage_map2_config_t* config, uint32_t* out_words,
    uint32_t* out_runs);

/* The same, as the bytes of the two buffers together (0 for an invalid configuration), for a
 * memory plan. The tables the layers read (tileset and maps) are the game's own data. */
uint32_t sat_stage_map2_requirements_bytes(const sat_stage_map2_config_t* config);

/* Moves the view. (x, y) is the top-left pixel of the viewport in the world, 16.16 (for the
 * follow camera, the VIEW range's centre minus its half size). Each layer applies its own
 * ratio, works out the window it needs resident, and stages only the cells that are not
 * already there. SAT_ERR_CAPACITY if the staging cannot hold everything: then nothing at
 * all has changed (commit and call again). A view that did not move stages nothing. */
sat_result_t sat_stage_map2_set_view(sat_stage_map2_t* map, sat_fx16_t x, sat_fx16_t y);

/* The scroll to give the layer's VDP2 layer: the view position wrapped to the ring, 16.16.
 * Valid after set_view. */
sat_result_t sat_stage_map2_scroll(const sat_stage_map2_t* map, uint8_t layer, sat_fx16_t* out_x, sat_fx16_t* out_y);

/* The cell word the layer shows at map cell (cx, cy): the pure function behind every write. */
uint16_t sat_stage_map2_cell(const sat_stage_map2_t* map, uint8_t layer, int32_t cx, int32_t cy);

/* The next set_view rebuilds the layer's window (after the VRAM was reused, say). */
sat_result_t sat_stage_map2_invalidate(sat_stage_map2_t* map, uint8_t layer);

/* A map edit: the cells in [cx, cx + w) x [cy, cy + h) changed, restage those that are
 * resident. Cells outside the resident window cost nothing. SAT_ERR_CAPACITY leaves it unstaged. */
sat_result_t sat_stage_map2_mark_dirty(sat_stage_map2_t* map, uint8_t layer, int32_t cx, int32_t cy, int32_t w,
    int32_t h);

/* What is waiting for a commit. */
void sat_stage_map2_pending(const sat_stage_map2_t* map, uint32_t* out_runs, uint32_t* out_words);

/* Sends every pending run, in the order it was staged, through `write`. Stops at the first
 * error and returns it, keeping the runs not yet written pending. */
sat_result_t sat_stage_map2_commit(sat_stage_map2_t* map, sat_stage_map2_write_fn write, void* user);
/* Drops everything pending without writing it (the layers are invalidated: what they think is
 * resident is no longer true). */
void sat_stage_map2_discard(sat_stage_map2_t* map);

sat_stage_map2_stats_t sat_stage_map2_stats(const sat_stage_map2_t* map);

/* The VDP2 side: sat_vdp2_vram_write_words as a writer, and the scroll applied to the layer. */
sat_result_t sat_stage_map2_commit_vdp2(sat_stage_map2_t* map);
sat_result_t sat_stage_map2_apply_scroll_vdp2(const sat_stage_map2_t* map);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_STAGE_MAP2_H */
