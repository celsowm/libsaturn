#ifndef BH_STREAM_H
#define BH_STREAM_H

/* Disc streaming for stage transitions. The next stage's enemy sprites (an atlas residency group)
 * and its scrolling background (an SBG1 file) are read from the CD while the current stage plays:
 * the drive is started in the background (sat_cd_stream_begin), and once a frame the program
 * moves the sectors that have arrived into a small buffer and from there into VRAM, so the game
 * never waits for the disc and nothing that is on screen is overwritten.
 *
 *   group jobs      sector-aligned group data -> sat_sprite_atlas_upload_chunk (VDP1 VRAM slot)
 *   background jobs SBG1 file -> palette to a CRAM bank, pixels to a VDP2 VRAM bank
 *
 * The stage music comes from the same drive, so this module is also the disc scheduler. sat_music
 * reads the disc synchronously (a refill waits for the drive: measured, that alone took the frame
 * rate from 60 to ~40), so the music is fed here instead, through two sat_audio_stream rings
 * (left and right, the pair sat_music itself builds) from the same background segments:
 *
 *   - one segment (8 sectors, 16 KiB: 4096 S16 or 8192 S8 stereo frames, ~55 ms of drive time) is in flight at a
 *     time; each service call lands the finished one and starts the next;
 *   - the music has priority: a segment is read as soon as both rings have room for it, so the
 *     rings stay nearly full (16384 frames = 0.74 s);
 *   - sprite and background data only get the gaps, and only while the rings hold at least
 *     BH_STREAM_MUSIC_RESERVE frames, enough to ride out the data segment in flight plus the music
 *     segment queued behind it (docs/SCSP_AUDIO_STREAMING_GUIDE.md: the SCSP half is 4096 samples);
 *   - segments are small so the longest the drive is held is one segment. */

#include <stdint.h>

#include "saturn/audio.h"
#include "saturn/cd_block.h"
#include "saturn/cdfs.h"
#include "saturn/core.h"
#include "saturn/sprite_atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BH_STREAM_SEGMENT_SECTORS 8u
#define BH_STREAM_QUEUE 4u
#define BH_STREAM_MAX_GROUPS 16u
#define BH_STREAM_MAX_BACKGROUNDS 4u
#define BH_BG_HEADER_BYTES 16u
#define BH_STREAM_MUSIC_RING_FRAMES 16384u
#define BH_STREAM_MUSIC_RESERVE 8192u       /* frames both rings must hold before a data segment starts */
#define BH_STREAM_FLIGHT_NONE 0u
#define BH_STREAM_FLIGHT_DATA 1u
#define BH_STREAM_FLIGHT_MUSIC 2u

typedef struct bh_stream_stats {
    uint32_t segments;       /* sat_cd_stream_begin calls */
    uint32_t bytes;          /* payload bytes delivered to VRAM */
    uint32_t polls;          /* service calls while a segment was in flight */
    uint32_t waits;          /* times a caller had to wait for a job (finish) */
    uint32_t jobs_done;
    uint32_t music_segments; /* music segments read */
    uint32_t music_bytes;
    uint32_t music_loops;    /* times the track wrapped */
    uint32_t data_deferred;  /* service calls where a data segment waited for the music rings */
    uint32_t music_min_buffered; /* lowest ring fill seen, frames (the limiting channel) */
    uint32_t ticks_poll, ticks_music, ticks_data;   /* FRT ticks: moving sectors in, splitting music, writing VRAM */
    int32_t last_error;      /* first failure, SAT_OK none */
} bh_stream_stats_t;

typedef struct bh_stream_job {
    uint8_t kind;            /* 0 none, 1 group, 2 background */
    uint8_t slot;            /* group: atlas slot; background: CRAM bank */
    uint16_t target;         /* group index / background index */
    uint32_t vram_bytes;     /* background: byte address of the bitmap bank in VDP2 VRAM */
} bh_stream_job_t;

typedef struct bh_stream {
    sat_cd_block_t* block;
    sat_cdfs_file_t atlas_file;
    sat_cdfs_file_t bg_file[BH_STREAM_MAX_BACKGROUNDS];
    uint8_t bg_files;
    sat_sprite_atlas_t* atlas;

    bh_stream_job_t queue[BH_STREAM_QUEUE];
    uint8_t head, count;
    /* music: a looping raw interleaved stereo file, fed to two mono rings */
    sat_cdfs_file_t music_file;
    uint32_t music_bytes;        /* payload bytes (not a sector multiple) */
    uint32_t music_pos;          /* byte offset of the next segment, a sector multiple */
    sat_audio_stream_t music_left, music_right;
    uint8_t music_open;
    uint8_t music_wanted;        /* once the rings are primed */
    uint8_t music_sample_bytes;  /* 1 (S8) or 2 (S16BE), per channel */
    uint32_t seg_music_bytes;    /* valid payload bytes of the music segment in flight */

    /* job in progress */
    uint8_t running;
    uint8_t in_flight;           /* BH_STREAM_FLIGHT_* */
    uint32_t lba;            /* next sector to read */
    uint32_t sectors_left;   /* sectors not yet started */
    uint32_t seg_sectors;    /* sectors of the segment in flight */
    uint32_t bytes_left;     /* group: payload bytes not yet delivered; background: pixel bytes */
    uint32_t vram_word;      /* background: next VDP2 VRAM word to write */
    uint8_t first_segment;
    uint8_t skip;            /* group already resident: nothing to read */

    /* Called between segments while bh_stream_finish waits, so the music keeps being fed. */
    void (*pump)(void* context);
    void* pump_context;

    uint32_t groups_done;    /* bit g: group g streamed in (cleared by bh_stream_forget_group) */
    uint32_t backgrounds_done;
    bh_stream_stats_t stats;
    uint8_t buffer[BH_STREAM_SEGMENT_SECTORS * 2048u] __attribute__((aligned(4)));
    uint8_t split[BH_STREAM_SEGMENT_SECTORS * 2048u] __attribute__((aligned(4)));   /* L then R */
} bh_stream_t;

/* Rings for the two music channels, BH_STREAM_MUSIC_RING_FRAMES frames each (S16 fills them, S8 half); the caller owns
 * them (put them in .wram_l: 64 KiB). */
typedef struct bh_music_rings {
    uint8_t left[BH_STREAM_MUSIC_RING_FRAMES * 2u] __attribute__((aligned(4)));
    uint8_t right[BH_STREAM_MUSIC_RING_FRAMES * 2u] __attribute__((aligned(4)));
} bh_music_rings_t;

/* Looks the files up: the atlas file and `bg_count` background files. Call before the first read
 * from the disc is in flight (a lookup reads the disc). */
sat_result_t bh_stream_init(bh_stream_t* s, sat_cd_block_t* block, sat_cdfs_volume_t* volume,
                            sat_sprite_atlas_t* atlas, const char* atlas_path,
                            const char* const* bg_paths, uint8_t bg_count);

static inline void bh_stream_set_pump(bh_stream_t* s, void (*pump)(void*), void* context) {
    s->pump = pump;
    s->pump_context = context;
}

sat_result_t bh_stream_queue_group(bh_stream_t* s, uint16_t group, uint8_t slot);
sat_result_t bh_stream_queue_background(bh_stream_t* s, uint16_t index, uint8_t cram_bank, uint32_t vram_bytes);

/* Opens the looping stereo music file (raw interleaved S8 or S16BE, `sample_bytes` per channel,
 * `bytes` long, `sample_rate`), fills both rings with blocking reads (the screen is still dark) and
 * starts it. Needs sat_audio_init. S8 halves the bytes the CPU moves through the CD Block data port. */
sat_result_t bh_stream_music_start(bh_stream_t* s, bh_music_rings_t* rings, sat_cdfs_volume_t* volume,
                                   const char* path, uint32_t bytes, uint32_t sample_rate,
                                   uint32_t sample_bytes);
/* Lowest ring fill (frames), underruns and refills of the music, for telemetry. */
void bh_stream_music_stats(const bh_stream_t* s, uint32_t* underruns, uint32_t* refills, uint32_t* buffered);

/* Advances the job in progress without waiting for the drive: moves what has arrived, starts the
 * next segment. Call once per frame. */
sat_result_t bh_stream_service(bh_stream_t* s);
/* Runs every queued job to completion (blocking, calling `pump` between segments). Used at boot,
 * and at a stage transition when the prefetch has not finished (counted in stats.waits). */
sat_result_t bh_stream_finish(bh_stream_t* s);
uint8_t bh_stream_busy(const bh_stream_t* s);

static inline uint8_t bh_stream_group_ready(const bh_stream_t* s, uint16_t g) { return (uint8_t)((s->groups_done >> g) & 1u); }
static inline uint8_t bh_stream_background_ready(const bh_stream_t* s, uint16_t i) { return (uint8_t)((s->backgrounds_done >> i) & 1u); }
static inline void bh_stream_forget_group(bh_stream_t* s, uint16_t g) { s->groups_done &= ~(1u << g); }
static inline void bh_stream_forget_background(bh_stream_t* s, uint16_t i) { s->backgrounds_done &= ~(1u << i); }

#ifdef __cplusplus
}
#endif
#endif
