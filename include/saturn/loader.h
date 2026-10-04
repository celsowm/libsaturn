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
} sat_loading_screen_t;

/* A 200x8 bar centred low on a 320x224 screen, dark blue with a white fill. */
void sat_loading_screen_default(sat_loading_screen_t* screen);
void sat_loading_screen_progress(void* context, uint32_t done, uint32_t total);

#ifdef __cplusplus
}
#endif
#endif
