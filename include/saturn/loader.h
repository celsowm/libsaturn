#ifndef SATURN_LOADER_H
#define SATURN_LOADER_H

#include <stdint.h>

#include "saturn/cdfs.h"
#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Boot-time loading: copy disc files into RAM (usually the RAM cartridge)
 * while a progress callback runs between CD reads. One list of items, one
 * call, one progress total, instead of every program hand-rolling its own
 * lookup / size check / chunked read loop. */

/* `done` and `total` are bytes. Called once with done == 0 before the first
 * read and after every chunk, so a screen can be drawn immediately. */
typedef void (*sat_load_progress_fn)(void* context, uint32_t done, uint32_t total);

typedef struct sat_load_item {
    const char* disc_path;   /* ISO 9660 path of the file */
    uint32_t expected_size;  /* whole file size; 0 accepts any size */
    uint32_t offset;         /* first byte to read (lets one file span two banks) */
    uint32_t bytes;          /* bytes to read; 0 = from offset to the end */
    void* destination;       /* receives `bytes` bytes */
} sat_load_item_t;

#define SAT_LOAD_DEFAULT_CHUNK_BYTES (256u * 1024u)

/* Resolves and validates every item first (SAT_ERR_NOT_FOUND / SAT_ERR_VERSION
 * for a size mismatch / SAT_ERR_INVALID_ARG for a range outside the file), so
 * a bad disc fails before anything is copied. Then reads each item in whole
 * sectors, `chunk_bytes` at a time (0 = SAT_LOAD_DEFAULT_CHUNK_BYTES; rounded
 * down to a sector multiple), calling `progress` (may be NULL) after each
 * chunk. Very small chunks restart the drive for every read and slow the boot
 * down, so keep the default unless the screen needs finer steps. */
sat_result_t sat_load_files(
    sat_cdfs_volume_t* volume,
    const sat_load_item_t* items, uint16_t count,
    uint32_t chunk_bytes,
    sat_load_progress_fn progress, void* context);

/* Loads an SLZ1 file (saturn/lz.h, made by tools/satlz.py) into
 * `destination` and decodes it there, in place: the file is read to the end
 * of the `capacity` bytes (at a 4-byte boundary) and decoded from the start,
 * so nothing beyond `capacity` is needed. Each batch of sectors is decoded
 * as it arrives, while the drive reads the next (sat_cd_set_arrival_observer;
 * the observer is taken for the duration of the call). `destination` must be
 * 4-byte aligned. `expected_decoded` 0 accepts any size. SAT_ERR_CAPACITY
 * when the file is larger than `capacity` (before any read) or capacity is
 * below the header's decoded + margin; SAT_ERR_VERSION when it is not SLZ1
 * or the decoded size differs; SAT_ERR_IO when the payload is corrupt. Those
 * last three are found once the data is in, so the destination is then
 * garbage. `progress` (may be NULL) sees the compressed bytes read. */
sat_result_t sat_load_lz(
    sat_cdfs_volume_t* volume,
    const char* disc_path,
    void* destination, uint32_t capacity,
    uint32_t expected_decoded,
    sat_load_progress_fn progress, void* context,
    uint32_t* out_bytes);

/* Ready-made progress callback: a bar on a flat background, drawn as one
 * VDP1 frame per call (begin, rects, end, wait for VBlank). Pass
 * sat_loading_screen_progress as the callback and the screen as its context.
 * Colours are RGB555. Needs the video system initialised, and no frame open. */
typedef struct sat_loading_screen {
    uint16_t background;
    uint16_t frame;
    uint16_t track;
    uint16_t fill;
    int16_t x, y;            /* top-left of the bar, screen pixels */
    uint16_t width, height;  /* bar size */
    /* Optional (NULL from sat_loading_screen_default): called with
     * `decorate_context` inside each progress frame, after the bar, so a
     * program can add a logo or a caption to the same VDP1 list. */
    void (*decorate)(void* context);
    void* decorate_context;
} sat_loading_screen_t;

/* A 200x8 bar centred low on a 320x224 screen, dark blue with a white fill,
 * and no decorate hook. */
void sat_loading_screen_default(sat_loading_screen_t* screen);

/* Draws the bar (frame + track + filled portion) described by `screen` into
 * the currently open VDP1 frame, using `percent` (clamped to [0, 100]) as the
 * fill ratio. Does not open or close a frame: the caller composes the bar
 * with other draws (a backdrop, a title, percent text) and must already be
 * between sat_begin_frame() and sat_end_frame(). Reused by the example
 * helpers in examples/common/example_util.h so the bar math only lives here.
 * Returns SAT_ERR_INVALID_ARG for a NULL screen. */
sat_result_t sat_loading_screen_draw_bar(const sat_loading_screen_t* screen, uint16_t percent);

void sat_loading_screen_progress(void* context, uint32_t done, uint32_t total);

#ifdef __cplusplus
}
#endif
#endif
