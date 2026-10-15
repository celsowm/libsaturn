#ifndef SATURN_DISC_STREAM_H
#define SATURN_DISC_STREAM_H

/* Disc scheduler: background reads of game data and streamed music from one drive.
 *
 * A program that streams stage data from the CD while a track plays needs one owner of the
 * drive. Without one, sat_music reads the disc synchronously (a refill waits for the drive:
 * measured, that alone took a 60 fps game to ~40), and a file read waits for any background
 * stream. This module is that owner. The drive runs in the background (sat_cd_stream_begin), and
 * once a frame sat_disc_stream_service moves the sectors that have arrived into a buffer and hands
 * them on, so the game never waits for the disc.
 *
 *   - One segment (SAT_DISC_STREAM_SEGMENT_SECTORS sectors, 16 KiB, ~55 ms of drive time) is in
 *     flight at a time. Each service call lands the finished one and starts the next; the longest the
 *     drive is held is one segment.
 *   - Music has priority. A looping raw interleaved stereo file (S8 or S16BE) is split into two mono
 *     sat_audio_stream rings (the pair sat_music builds) and a segment is read as soon as both rings
 *     have room for it, so they stay nearly full.
 *   - Data jobs get the gaps, and only while the rings hold at least `music_reserve` frames, enough to
 *     ride out the data segment in flight plus the music segment queued behind it
 *     (docs/SCSP_AUDIO_STREAMING_GUIDE.md: the SCSP half is 4096 samples).
 *   - A data job is a file extent and a sink: the module reads the extent, the job's callbacks do
 *     what the data is for (a VRAM upload, a decompressor, a copy to RAM). The callbacks run in the
 *     caller's service call, one job at a time, in queue order.
 *
 * The caller looks every file up (sat_cdfs_lookup) before the first read is in flight: a lookup
 * reads the disc and would finish a stream in progress.
 *
 * The music side is a pluggable feed (sat_disc_music_feed_ops_t). sat_disc_stream_music_start
 * installs the built-in one (a raw stereo loop). sat_music_use_disc_stream installs sat_music's, so
 * the library's own music API (any track on the disc: S8/S16, mono/stereo, IMA ADPCM, seek, pause,
 * loop) streams without waiting for the drive (saturn/audio.h). sat_music's feed is pipelined: the
 * drive reads the next segment while the last one is delivered to the rings a slice at a time.
 *
 * What a frame costs (Mednafen, 22.05 kHz stereo S16 from the jukebox, no other load): moving a
 * segment's sectors out of the CD Block ~4 ms (an emulator measurement: one sector cost about the
 * same as eight, so splitting the transfer into smaller polls only lowered throughput). The CD
 * Block has only an overview in docs/sega_saturn_hardware; docs/sega_saturn_hardware/hard/cdblock
 * collects it with Sega's CD communication I/F guide (commands are rate limited to 30 per 6.7 ms,
 * continuous command polling is forbidden, the buffer holds 200 sectors, an average seek is 500 ms
 * at 2x) and says which numbers are only emulator measurements. A poll is about three commands
 * plus the data. One delivery slice ~1.8 ms. The audio driver's own SCSP refill (sat_audio_update,
 * ~12 ms for a whole half; the driver writes it a slice per update, see saturn/audio.h) is not part
 * of this module but lands in whichever frame it falls in, including one that also polls.
 *
 * Nothing here allocates. The segment buffers are part of sat_disc_stream_t and the music rings are
 * a caller-owned sat_disc_music_rings_t (put both in .wram_l: 32 KiB and 64 KiB). Not
 * thread-safe; call from the main loop. */

#include <stdint.h>

#include "saturn/audio.h"
#include "saturn/cd_block.h"
#include "saturn/cdfs.h"
#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAT_DISC_STREAM_SEGMENT_SECTORS 8u
#define SAT_DISC_STREAM_SEGMENT_BYTES (SAT_DISC_STREAM_SEGMENT_SECTORS * 2048u)
#define SAT_DISC_STREAM_QUEUE 8u
#define SAT_DISC_MUSIC_RING_FRAMES 16384u
#define SAT_DISC_MUSIC_RESERVE_FRAMES 8192u    /* default `music_reserve` */
#define SAT_DISC_STANDBY_FLOOR_FRAMES 24576u   /* default `standby_floor` */
#define SAT_DISC_STANDBY_HIGH_FRAMES 28672u    /* default `standby_high` */
#define SAT_DISC_STANDBY_CEIL_FRAMES 31744u    /* `standby_high` never adapts above this: the rings hold no more */

/* What a data job reads: `bytes` of `file` from `offset` (a sector multiple); bytes 0 reads to the
 * end of the file. */
typedef struct sat_disc_extent {
    const sat_cdfs_file_t* file;    /* NULL: nothing to read, the job is complete (already resident) */
    uint32_t offset;
    uint32_t bytes;
} sat_disc_extent_t;

typedef struct sat_disc_job_ops {
    /* The job reached the head of the queue: say what to read. Optional (NULL: the job's `extent`
     * is used as queued). Any failure drops the job: `done` runs with it and it is counted in
     * `last_error`. */
    sat_result_t (*begin)(void* user, uint32_t arg, sat_disc_extent_t* extent);
    /* Payload bytes of the extent, in order, at most one segment per call; the last call is clipped
     * to the extent's byte count. `data` is only valid during the call. */
    sat_result_t (*chunk)(void* user, uint32_t arg, const uint8_t* data, uint32_t bytes);
    /* Optional. The job ended: SAT_OK after the last chunk (or a skip), the error otherwise. */
    void (*done)(void* user, uint32_t arg, sat_result_t result);
} sat_disc_job_ops_t;

typedef struct sat_disc_job {
    const sat_disc_job_ops_t* ops;  /* must outlive the job */
    void* user;
    uint32_t arg;
    sat_disc_extent_t extent;       /* used when ops->begin is NULL */
} sat_disc_job_t;

typedef struct sat_disc_stream_stats {
    uint32_t segments;       /* sat_cd_stream_begin calls */
    uint32_t bytes;          /* data payload bytes handed to chunk callbacks */
    uint32_t polls;          /* service calls while a segment was in flight */
    uint32_t waits;          /* times sat_disc_stream_finish had to wait */
    uint32_t jobs_done;      /* data jobs that completed (not dropped) */
    uint32_t music_segments;
    uint32_t music_bytes;
    uint32_t music_loops;    /* times the track wrapped */
    uint32_t data_deferred;  /* service calls where a data segment waited for the music rings */
    uint32_t music_min_buffered;    /* lowest ring fill seen, frames (the limiting channel) */
    uint32_t standby_visits;        /* visits to the standby's file, and the deepest dip of the playing rings in one */
    uint32_t standby_worst_dip;
    uint32_t ticks_poll, ticks_music, ticks_data;   /* FRT ticks: moving sectors in, splitting music, callbacks */
    uint32_t max_ticks_poll, max_ticks_music, max_ticks_data;   /* the slowest single call of each: what one frame pays at worst */
    int32_t last_error;      /* first failure, SAT_OK none */
} sat_disc_stream_stats_t;

/* What the music wants from the drive. The scheduler asks `next` whenever the drive and a buffer
 * are free; when the feed names a range it reads it (one segment at most) and hands the landed
 * bytes to `deliver`. The feed owns the SCSP rings and everything about the format.
 *
 * With `pipelined` set the scheduler keeps two segment buffers: while one is being delivered
 * (in slices, one `deliver` call per service call, so a frame never pays for a whole segment)
 * the drive reads the next into the other. Otherwise `deliver` runs to completion as soon as the
 * segment lands. */
typedef struct sat_disc_music_feed_ops {
    /* The next read, or 0 when none is wanted now (rings full, paused). `*offset` is a sector
     * multiple, `*bytes` at most SAT_DISC_STREAM_SEGMENT_BYTES and inside the file. Must not
     * change the feed: `started` commits it. With pipelining, the room it tests must already
     * count the segments started and not yet delivered. */
    uint8_t (*next)(void* user, const sat_cdfs_file_t** file, uint32_t* offset, uint32_t* bytes);
    /* Optional. The read `next` named is in flight: advance the read position, reserve its room. */
    void (*started)(void* user);
    /* Turns the next part of a landed segment into ring writes: `data` is the not yet consumed
     * part (`bytes` long, valid only during the call). Sets `*consumed` (at least 1 on success; a
     * call that consumes nothing drops the rest of the segment) and is called again for the
     * remainder, a later service call when pipelined. */
    sat_result_t (*deliver)(void* user, const uint8_t* data, uint32_t bytes, uint32_t* consumed);
    /* Frames buffered in the feed's rings (the limiting channel): decides when data may use the
     * drive and feeds the minimum statistic. */
    uint32_t (*buffered)(void* user);
    uint8_t pipelined;
    /* Optional. Called at the end of every sat_disc_stream_service, whatever it did: the place for
     * the feed to notice that its rings are full and start playing. */
    void (*tick)(void* user);
} sat_disc_music_feed_ops_t;

/* Rings for the two music channels, SAT_DISC_MUSIC_RING_FRAMES frames each (S16 fills them, S8 half). */
typedef struct sat_disc_music_rings {
    uint8_t left[SAT_DISC_MUSIC_RING_FRAMES * 2u] __attribute__((aligned(4)));
    uint8_t right[SAT_DISC_MUSIC_RING_FRAMES * 2u] __attribute__((aligned(4)));
} sat_disc_music_rings_t;

typedef struct sat_disc_stream {
    sat_cd_block_t* block;

    sat_disc_job_t queue[SAT_DISC_STREAM_QUEUE];
    uint8_t head, count;

    /* music: a feed (the built-in raw stereo loop, or sat_music's) */
    const sat_disc_music_feed_ops_t* music_ops;
    void* music_user;
    /* A second feed, served only when the first wants nothing: it prefills its rings silently so
     * a swap (sat_disc_stream_music_swap) starts it without a refill. */
    const sat_disc_music_feed_ops_t* standby_ops;
    void* standby_user;
    uint8_t fly_owner, deliv_owner, ready_owner;   /* which feed a segment belongs to: 0 primary, 1 standby */
    uint8_t fly_discard;         /* the music segment in flight is not wanted any more: it lands and is dropped */
    const sat_cdfs_file_t* music_file;     /* built-in feed: a looping raw interleaved stereo file */
    uint32_t music_bytes;        /* payload bytes (not a sector multiple) */
    uint32_t music_pos;          /* byte offset of the next segment, a sector multiple */
    sat_audio_stream_t music_left, music_right;
    uint8_t music_open;
    uint8_t music_wanted;        /* the feed may start segments */
    uint8_t music_starting;      /* the feed is filling its rings before the streams play: not counted in music_min_buffered */
    uint8_t music_sample_bytes;  /* 1 (S8) or 2 (S16BE), per channel */
    uint32_t seg_music_bytes;    /* valid payload bytes of the music segment in flight */
    uint32_t music_reserve;      /* frames both rings must hold before a data segment starts */
    uint32_t standby_floor;      /* a standby burst ends when the playing feed holds fewer frames than this */
    uint32_t standby_high;       /* ... and starts once it holds at least this many */
    uint8_t standby_burst;       /* the standby is reading ahead of the playing feed */
    /* The drive's real cost of a visit to the standby's file, read off the playing feed's own rings
     * (their fill is the clock): a visit that took the playing rings under `music_reserve` raises
     * `standby_high` (and the floor) by the shortfall; one that cannot be covered even with the rings full
     * stops the prefetch for good (`standby_disabled`), so the damage is one visit, not a habit. An
     * emulator with free seeks never triggers either. */
    uint8_t standby_visit;       /* 0 idle, 1 reading the standby, 2 back on the playing feed, recovering */
    uint8_t standby_disabled;
    uint32_t standby_visit_start, standby_visit_min;

    /* data job in progress */
    uint8_t running;
    uint8_t in_flight;           /* 0 none, 1 data, 2 music */
    uint8_t fly_buf;             /* which segment buffer the one in flight lands in (0 buffer, 1 split) */
    /* a landed music segment being delivered, and one waiting behind it (pipelined feeds) */
    uint8_t deliv_active, deliv_buf, ready, ready_buf;
    uint32_t deliv_off, deliv_bytes, ready_bytes;
    uint32_t lba;                /* next sector to read */
    uint32_t sectors_left;       /* sectors not yet started */
    uint32_t seg_sectors;        /* sectors of the segment in flight */
    uint32_t bytes_left;         /* payload bytes not yet handed to chunk */

    /* Called between segments while sat_disc_stream_finish waits, so the music keeps being fed
     * (and the audio driver serviced). */
    void (*pump)(void* context);
    void* pump_context;

    sat_disc_stream_stats_t stats;
    uint8_t buffer[SAT_DISC_STREAM_SEGMENT_BYTES] __attribute__((aligned(4)));
    /* The built-in music splits into it (L then R); a pipelined feed lands the second segment here. */
    uint8_t split[SAT_DISC_STREAM_SEGMENT_BYTES] __attribute__((aligned(4)));
} sat_disc_stream_t;

/* `block` is an initialized CD Block (sat_cd_block_init). Clears everything. */
sat_result_t sat_disc_stream_init(sat_disc_stream_t* s, sat_cd_block_t* block);

static inline void sat_disc_stream_set_pump(sat_disc_stream_t* s, void (*pump)(void*), void* context) {
    s->pump = pump;
    s->pump_context = context;
}

/* Appends a data job. SAT_ERR_CAPACITY when SAT_DISC_STREAM_QUEUE are queued, SAT_ERR_INVALID_ARG
 * without ops/chunk or (with no `begin`) a file. */
sat_result_t sat_disc_stream_queue(sat_disc_stream_t* s, const sat_disc_job_t* job);

/* Starts the looping stereo music from `file` (raw interleaved S8 or S16BE, `sample_bytes` per
 * channel, `bytes` long, at least one segment). Opens both rings and returns at once: the rings fill
 * over the following sat_disc_stream_service calls, and the streams start together in the one that
 * finds them full (sat_disc_stream_music_playing says when). Needs sat_audio_init. S8 halves the
 * bytes the CPU moves through the CD Block data port. Call with no segment in flight. */
sat_result_t sat_disc_stream_music_start(sat_disc_stream_t* s, sat_disc_music_rings_t* rings,
                                         const sat_cdfs_file_t* file, uint32_t bytes,
                                         uint32_t sample_rate, uint32_t sample_bytes);
/* Installs an external music feed (SAT_ERR_BUSY while another feed or the built-in music is
 * installed). It starts wanting segments at once. */
sat_result_t sat_disc_stream_music_attach(sat_disc_stream_t* s, const sat_disc_music_feed_ops_t* ops, void* user);
/* Removes the feed, dropping a segment in flight (waits for it: at most one segment of drive time). */
void sat_disc_stream_music_detach(sat_disc_stream_t* s);
/* Drops a music segment in flight, if one is, keeping the feed. For a feed that moves its
 * position (a seek, a stop): the segment that was read for the old position is not delivered. */
void sat_disc_stream_music_abort(sat_disc_stream_t* s);
/* The standby feed: a second feed (SAT_ERR_BUSY while one is installed, or while the built-in music
 * is). The scheduler asks it for a segment when the primary feed wants none. A drive that just keeps
 * a track topped up has no such moments, so the standby also reads in bursts ahead of the primary:
 * a burst starts when the primary's rings hold `standby_high` frames, reads the standby's segments
 * one after another (alternating files would seek for every segment) and ends when the primary is
 * down to `standby_floor`, or the standby wants nothing more. The primary then refills and the next
 * burst waits for it to be full again, so a prefetch costs the playing track about
 * (standby_high - standby_floor) frames of margin and no more. Whatever the standby reads moves the
 * drive away from the primary's position: on a real drive that costs seeks the primary's ring has
 * to ride out (see sat_music_prefetch). */
sat_result_t sat_disc_stream_music_attach_standby(sat_disc_stream_t* s, const sat_disc_music_feed_ops_t* ops, void* user);
/* Removes the standby feed, dropping its segment in flight. */
void sat_disc_stream_music_detach_standby(sat_disc_stream_t* s);
/* Drops the standby feed's segment in flight, keeping the feed (its position moved). */
void sat_disc_stream_music_abort_standby(sat_disc_stream_t* s);
/* Exchanges the two feeds: the standby becomes the primary, and what was primary becomes the
 * standby. The old primary's segments are dropped without waiting: one being delivered at once, one
 * in flight when it lands (the drive is busy with it until then). The standby's segments carry on
 * to the new primary. With no primary the standby just takes its place. The caller owns what the
 * swap means to the feeds (stopping the old one, starting the new one). SAT_ERR_INVALID_ARG without
 * a standby, SAT_ERR_BUSY while the built-in music is open. */
sat_result_t sat_disc_stream_music_swap(sat_disc_stream_t* s);
/* 1 once the built-in music (sat_disc_stream_music_start) has filled its rings and started. */
uint8_t sat_disc_stream_music_playing(const sat_disc_stream_t* s);
/* For an external feed: nonzero while it fills its rings before the streams play (after a start or
 * a seek). The ring fill seen meanwhile is not counted in the minimum statistic, which then means
 * "lowest fill while playing". */
void sat_disc_stream_music_set_starting(sat_disc_stream_t* s, uint8_t starting);
/* Makes sat_music read its disc-backed tracks through this scheduler instead of waiting for the
 * drive (pass NULL to go back). Call before sat_music_open: a track opened while a scheduler
 * is bound and whose source is a disc file (sat_cdfs_register_source_manifest) streams through
 * it, and any other track is read as before (sat_music_info reports which). The drive plays one such
 * track at a time. The first opened is the playing feed; a second one opens as the standby (it can
 * be prefilled, sat_music_prefetch, and sat_music_play on it swaps the two without a refill); a third
 * returns SAT_ERR_BUSY. Refused with SAT_ERR_BUSY while a disc-scheduled track is open.
 *
 * Streaming music then costs a frame what the scheduler costs: sat_music_play only marks the track
 * wanted, sat_music_update (or sat_disc_stream_service, either, once a frame) fills the rings one
 * segment at a time, starts the streams when they are full and from then on lands and starts one
 * segment per frame, and sat_music_seek / stop drop the segment in
 * flight (waiting at most one segment of drive time). Look every file up before the music plays: a
 * lookup or a file read finishes the segment in flight. sat_cd_block progress hooks must not call
 * sat_music_update. */
sat_result_t sat_music_use_disc_stream(sat_disc_stream_t* stream);
/* Underruns (the worse channel), refills and the lowest ring fill (frames) of the music. */
void sat_disc_stream_music_stats(const sat_disc_stream_t* s, uint32_t* underruns, uint32_t* refills,
                                 uint32_t* buffered);

/* Advances without waiting for the drive: moves what has arrived, starts the next segment.
 * Call once per frame. Returns the transport error of the segment just polled, else SAT_OK. */
sat_result_t sat_disc_stream_service(sat_disc_stream_t* s);
/* Runs every queued job to completion (blocking, calling `pump` between segments). Used at boot,
 * and at a stage transition when the prefetch has not finished (counted in stats.waits). Returns
 * the first error recorded since init, if any. */
sat_result_t sat_disc_stream_finish(sat_disc_stream_t* s);
/* Nonzero while a data job is running or queued. */
uint8_t sat_disc_stream_busy(const sat_disc_stream_t* s);

#ifdef __cplusplus
}
#endif
#endif
