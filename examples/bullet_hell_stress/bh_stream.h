#ifndef BH_STREAM_H
#define BH_STREAM_H

/* Disc streaming for stage transitions. The next stage's enemy sprites (an atlas residency group)
 * and its scrolling background (an SBG1 file) are read from the CD while the current stage plays,
 * and so is the stage music. The drive scheduling (one segment in flight, music first, data in the
 * gaps) is the library's sat_disc_stream; this file is what this game's data is for:
 *
 *   group jobs      sector-aligned group data -> sat_sprite_atlas_upload_chunk (VDP1 VRAM slot)
 *   background jobs SBG1 file -> palette to a CRAM bank, pixels to a VDP2 VRAM bank */

#include <stdint.h>

#include "saturn/cdfs.h"
#include "saturn/disc_stream.h"
#include "saturn/sprite_atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BH_STREAM_MAX_GROUPS 16u
#define BH_STREAM_MAX_BACKGROUNDS 4u
#define BH_BG_HEADER_BYTES 16u

typedef struct bh_stream {
    sat_disc_stream_t disc;
    sat_cdfs_file_t atlas_file;
    sat_cdfs_file_t bg_file[BH_STREAM_MAX_BACKGROUNDS];
    sat_cdfs_file_t music_file;
    uint8_t bg_files;
    sat_sprite_atlas_t* atlas;

    /* the data job in progress (the library runs one at a time) */
    uint8_t first_segment;       /* background: the next chunk starts with the header sector */
    uint32_t bytes_left;         /* group: payload bytes not yet uploaded; background: pixel bytes */
    uint32_t vram_word;          /* background: next VDP2 VRAM word to write */

    uint32_t bg_vram[BH_STREAM_MAX_BACKGROUNDS];   /* queued background: VDP2 VRAM byte address */

    uint32_t groups_done;        /* bit g: group g streamed in (cleared by bh_stream_forget_group) */
    uint32_t backgrounds_done;
} bh_stream_t;

/* Looks the files up: the atlas file and `bg_count` background files. Call before the first read
 * from the disc is in flight (a lookup reads the disc). */
sat_result_t bh_stream_init(bh_stream_t* s, sat_cd_block_t* block, sat_cdfs_volume_t* volume,
                            sat_sprite_atlas_t* atlas, const char* atlas_path,
                            const char* const* bg_paths, uint8_t bg_count);

static inline void bh_stream_set_pump(bh_stream_t* s, void (*pump)(void*), void* context) {
    sat_disc_stream_set_pump(&s->disc, pump, context);
}

sat_result_t bh_stream_queue_group(bh_stream_t* s, uint16_t group, uint8_t slot);
sat_result_t bh_stream_queue_background(bh_stream_t* s, uint16_t index, uint8_t cram_bank, uint32_t vram_bytes);

/* Opens the looping stereo music file (raw interleaved S8 or S16BE, `sample_bytes` per channel,
 * `bytes` long, `sample_rate`) and returns at once: the rings fill through bh_stream_service and
 * the streams start together when they are full. Needs sat_audio_init. */
sat_result_t bh_stream_music_start(bh_stream_t* s, sat_disc_music_rings_t* rings, sat_cdfs_volume_t* volume,
                                   const char* path, uint32_t bytes, uint32_t sample_rate,
                                   uint32_t sample_bytes);

static inline void bh_stream_music_stats(const bh_stream_t* s, uint32_t* underruns, uint32_t* refills, uint32_t* buffered) {
    sat_disc_stream_music_stats(&s->disc, underruns, refills, buffered);
}
static inline sat_result_t bh_stream_service(bh_stream_t* s) { return sat_disc_stream_service(&s->disc); }
static inline sat_result_t bh_stream_finish(bh_stream_t* s) { return sat_disc_stream_finish(&s->disc); }
static inline uint8_t bh_stream_busy(const bh_stream_t* s) { return sat_disc_stream_busy(&s->disc); }

static inline uint8_t bh_stream_group_ready(const bh_stream_t* s, uint16_t g) { return (uint8_t)((s->groups_done >> g) & 1u); }
static inline uint8_t bh_stream_background_ready(const bh_stream_t* s, uint16_t i) { return (uint8_t)((s->backgrounds_done >> i) & 1u); }
static inline void bh_stream_forget_group(bh_stream_t* s, uint16_t g) { s->groups_done &= ~(1u << g); }
static inline void bh_stream_forget_background(bh_stream_t* s, uint16_t i) { s->backgrounds_done &= ~(1u << i); }

#ifdef __cplusplus
}
#endif
#endif
