#include "bh_stream.h"

#include "saturn/time.h"
#include "saturn/vdp2.h"

#define SECTOR 2048u
#define SEGMENT_BYTES (BH_STREAM_SEGMENT_SECTORS * SECTOR)
#define BG_W 512u
#define BG_H 256u
#define BG_PALETTE_BYTES 512u

static sat_result_t note(bh_stream_t* s, sat_result_t r) {
    if (r != SAT_OK && s->stats.last_error == SAT_OK) s->stats.last_error = r;
    return r;
}

static void drop_job(bh_stream_t* s) {
    s->running = 0u;
    s->head = (uint8_t)((s->head + 1u) % BH_STREAM_QUEUE);
    s->count--;
}

sat_result_t bh_stream_init(bh_stream_t* s, sat_cd_block_t* block, sat_cdfs_volume_t* volume,
                            sat_sprite_atlas_t* atlas, const char* atlas_path,
                            const char* const* bg_paths, uint8_t bg_count) {
    if (!s || !block || !volume || !atlas || !atlas_path || bg_count > BH_STREAM_MAX_BACKGROUNDS) return SAT_ERR_INVALID_ARG;
    __builtin_memset(s, 0, sizeof(*s));
    s->block = block;
    s->atlas = atlas;
    sat_result_t r = sat_cdfs_lookup(volume, atlas_path, &s->atlas_file);
    if (r != SAT_OK) return r;
    for (uint8_t i = 0; i < bg_count; ++i) {
        r = sat_cdfs_lookup(volume, bg_paths[i], &s->bg_file[i]);
        if (r != SAT_OK) return r;
        if (s->bg_file[i].size != 2048u + BG_W * BG_H) return SAT_ERR_INVALID_ARG;
    }
    s->bg_files = bg_count;
    s->stats.music_min_buffered = BH_STREAM_MUSIC_RING_FRAMES;
    return SAT_OK;
}

sat_result_t bh_stream_queue_group(bh_stream_t* s, uint16_t group, uint8_t slot) {
    if (s->count >= BH_STREAM_QUEUE) return SAT_ERR_CAPACITY;
    if (group >= BH_STREAM_MAX_GROUPS) return SAT_ERR_INVALID_ARG;
    bh_stream_job_t* j = &s->queue[(s->head + s->count++) % BH_STREAM_QUEUE];
    j->kind = 1; j->slot = slot; j->target = group; j->vram_bytes = 0;
    return SAT_OK;
}

sat_result_t bh_stream_queue_background(bh_stream_t* s, uint16_t index, uint8_t cram_bank, uint32_t vram_bytes) {
    if (s->count >= BH_STREAM_QUEUE) return SAT_ERR_CAPACITY;
    if (index >= s->bg_files || (vram_bytes & 0x1FFFFu) || vram_bytes + BG_W * BG_H > 0x80000u) return SAT_ERR_INVALID_ARG;
    bh_stream_job_t* j = &s->queue[(s->head + s->count++) % BH_STREAM_QUEUE];
    j->kind = 2; j->slot = cram_bank; j->target = index; j->vram_bytes = vram_bytes;
    return SAT_OK;
}

uint8_t bh_stream_busy(const bh_stream_t* s) { return (uint8_t)(s->running || s->count); }

/* ---- music ------------------------------------------------------------------------------ */

static uint32_t ring_room(const bh_stream_t* s) {
    const uint32_t l = sat_audio_stream_available(s->music_left);
    const uint32_t r = sat_audio_stream_available(s->music_right);
    return l < r ? l : r;
}

static uint32_t music_frames(const bh_stream_t* s, uint32_t bytes) { return bytes / (2u * s->music_sample_bytes); }

/* Bytes of the next music segment: a whole segment, or what is left of the file. */
static uint32_t music_segment_bytes(const bh_stream_t* s) {
    const uint32_t left = s->music_bytes - s->music_pos;
    return left < SEGMENT_BYTES ? left : SEGMENT_BYTES;
}

/* Starts the next music segment unless the drive is busy; 1 when it started. */
static uint8_t begin_music(bh_stream_t* s) {
    const uint32_t bytes = music_segment_bytes(s);
    uint32_t lba = 0u, sectors = 0u;
    if (note(s, sat_cdfs_file_sectors(&s->music_file, s->music_pos, bytes, &lba, &sectors)) != SAT_OK) return 0u;
    const sat_result_t r = sat_cd_stream_begin(s->block, lba, sectors, s->buffer);
    if (r == SAT_ERR_BUSY) return 0u;
    if (note(s, r) != SAT_OK) return 0u;
    s->seg_music_bytes = bytes;
    s->seg_sectors = sectors;
    s->in_flight = BH_STREAM_FLIGHT_MUSIC;
    s->stats.segments++;
    return 1u;
}

/* The music segment has landed in s->buffer: split it into the two rings. */
static sat_result_t deliver_music(bh_stream_t* s) {
    const uint32_t frames = music_frames(s, s->seg_music_bytes);
    const uint16_t* in = (const uint16_t*)(const void*)s->buffer;
    const void* left;
    const void* right;
    if (s->music_sample_bytes == 1u) {
        /* S8 stereo: one 16-bit load is a left/right pair (big-endian: left in the high byte). */
        uint8_t* l = s->split;
        uint8_t* rr = l + frames;
        for (uint32_t i = 0; i < frames; ++i) {
            const uint16_t v = in[i];
            l[i] = (uint8_t)(v >> 8);
            rr[i] = (uint8_t)v;
        }
        left = l;
        right = rr;
    } else {
        uint16_t* l = (uint16_t*)(void*)s->split;
        uint16_t* rr = l + frames;
        for (uint32_t i = 0; i < frames; ++i) {
            l[i] = in[i * 2u];
            rr[i] = in[i * 2u + 1u];
        }
        left = l;
        right = rr;
    }
    sat_result_t r = sat_audio_stream_write(s->music_left, left, frames);
    if (r == SAT_OK) r = sat_audio_stream_write(s->music_right, right, frames);
    note(s, r);
    s->music_pos += s->seg_sectors * SECTOR;
    if (s->music_pos >= s->music_bytes) {
        s->music_pos = 0u;
        s->stats.music_loops++;
    }
    s->stats.music_segments++;
    s->stats.music_bytes += s->seg_music_bytes;
    return r;
}

sat_result_t bh_stream_music_start(bh_stream_t* s, bh_music_rings_t* rings, sat_cdfs_volume_t* volume,
                                   const char* path, uint32_t bytes, uint32_t sample_rate, uint32_t sample_bytes) {
    if (!s || !rings || !volume || !path || bytes < SEGMENT_BYTES || (sample_bytes != 1u && sample_bytes != 2u) ||
        (bytes % (2u * sample_bytes)) || s->in_flight) return SAT_ERR_INVALID_ARG;
    s->music_sample_bytes = (uint8_t)sample_bytes;
    sat_result_t r = sat_cdfs_lookup(volume, path, &s->music_file);
    if (r != SAT_OK) return r;
    if (s->music_file.size < bytes) return SAT_ERR_INVALID_ARG;
    const sat_audio_spec_t spec = {sample_rate, BH_STREAM_MUSIC_RING_FRAMES, 1u,
                                   sample_bytes == 1u ? SAT_AUDIO_PCM_S8 : SAT_AUDIO_PCM_S16, 0u};
    const uint32_t ring_bytes = BH_STREAM_MUSIC_RING_FRAMES * sample_bytes;
    r = sat_audio_stream_open(&s->music_left, &spec, rings->left, ring_bytes);
    if (r != SAT_OK) return r;
    r = sat_audio_stream_open(&s->music_right, &spec, rings->right, ring_bytes);
    if (r != SAT_OK) { (void)sat_audio_stream_close(s->music_left); return r; }
    /* Silent until both rings are primed, then they start together (as sat_music does). */
    (void)sat_audio_stream_pause(s->music_left);
    (void)sat_audio_stream_pause(s->music_right);
    (void)sat_audio_stream_set_pan(s->music_left, SAT_AUDIO_PAN_LEFT);
    (void)sat_audio_stream_set_pan(s->music_right, SAT_AUDIO_PAN_RIGHT);
    s->music_bytes = bytes;
    s->music_pos = 0u;
    s->music_open = 1u;
    while (ring_room(s) >= (music_frames(s, music_segment_bytes(s)))) {
        if (!begin_music(s)) return s->stats.last_error ? (sat_result_t)s->stats.last_error : SAT_ERR_BUSY;
        r = sat_cd_stream_finish(s->block);
        s->in_flight = BH_STREAM_FLIGHT_NONE;
        if (r != SAT_OK) return note(s, r);
        r = deliver_music(s);
        if (r != SAT_OK) return r;
    }
    r = sat_audio_stream_resume(s->music_left);
    if (r == SAT_OK) r = sat_audio_stream_resume(s->music_right);
    s->music_wanted = 1u;
    return r;
}

void bh_stream_music_stats(const bh_stream_t* s, uint32_t* underruns, uint32_t* refills, uint32_t* buffered) {
    sat_audio_stream_stats_t l = {0}, r = {0};
    if (!s->music_open) { *underruns = *refills = *buffered = 0u; return; }
    (void)sat_audio_stream_stats(s->music_left, &l);
    (void)sat_audio_stream_stats(s->music_right, &r);
    *underruns = l.underrun_count > r.underrun_count ? l.underrun_count : r.underrun_count;
    *refills = l.refill_count;
    *buffered = l.buffered_frames < r.buffered_frames ? l.buffered_frames : r.buffered_frames;
}

/* ---- data jobs -------------------------------------------------------------------------- */

/* Takes the next queued job; groups that are already resident finish at once. */
static void start_job(bh_stream_t* s) {
    while (!s->running && s->count) {
        const bh_stream_job_t* j = &s->queue[s->head];
        sat_result_t r;
        if (j->kind == 1) {
            sat_sprite_atlas_group_info_t gi;
            uint8_t skipped = 0;
            r = sat_sprite_atlas_group_info(s->atlas, j->target, &gi);
            if (r == SAT_OK) r = sat_sprite_atlas_upload_begin(s->atlas, j->slot, j->target, &skipped);
            if (r == SAT_OK && skipped) {
                s->groups_done |= 1u << j->target;
                s->stats.jobs_done++;
                drop_job(s);
                continue;
            }
            if (r == SAT_OK) r = sat_cdfs_file_sectors(&s->atlas_file, gi.data_offset, gi.data_bytes, &s->lba, &s->sectors_left);
            s->bytes_left = gi.data_bytes;
        } else {
            r = sat_cdfs_file_sectors(&s->bg_file[j->target], 0, 0, &s->lba, &s->sectors_left);
            s->first_segment = 1;
            s->bytes_left = 0;
            s->vram_word = j->vram_bytes >> 1;
        }
        if (r != SAT_OK) {
            /* A bad job is dropped so the queue cannot jam; the error stays in the stats. */
            note(s, r);
            drop_job(s);
            continue;
        }
        s->running = 1u;
    }
}

static sat_result_t deliver_data(bh_stream_t* s, const bh_stream_job_t* j) {
    const uint8_t* p = s->buffer;
    uint32_t avail = s->seg_sectors * SECTOR;
    sat_result_t r = SAT_OK;
    if (j->kind == 1) {
        const uint32_t n = s->bytes_left < avail ? s->bytes_left : avail;
        r = sat_sprite_atlas_upload_chunk(s->atlas, p, n);
        s->bytes_left -= n;
        s->stats.bytes += n;
        if (r == SAT_OK && s->bytes_left == 0) r = sat_sprite_atlas_upload_end(s->atlas);
        return r;
    }
    if (s->first_segment) {
        /* Sector 0: 16-byte header, then the 256-entry CRAM palette; pixels start at the next sector. */
        uint16_t w, h, flags;
        __builtin_memcpy(&w, p + 4, 2); __builtin_memcpy(&h, p + 6, 2); __builtin_memcpy(&flags, p + 10, 2);
        if (!(p[0] == 'S' && p[1] == 'B' && p[2] == 'G' && p[3] == '1') || w != BG_W || h != BG_H || flags != 0) return SAT_ERR_INVALID_ARG;
        r = sat_vdp2_palette_upload((const uint16_t*)(const void*)(p + BH_BG_HEADER_BYTES), BG_PALETTE_BYTES / 2u,
                                    (uint16_t)(j->slot * 256u));
        if (r != SAT_OK) return r;
        s->first_segment = 0;
        s->bytes_left = BG_W * BG_H;
        p += SECTOR;
        avail -= SECTOR;
    }
    const uint32_t n = s->bytes_left < avail ? s->bytes_left : avail;
    if (n) {
        r = sat_vdp2_vram_write_words(s->vram_word, (const uint16_t*)(const void*)p, n >> 1);
        s->vram_word += n >> 1;
        s->bytes_left -= n;
        s->stats.bytes += n;
    }
    return r;
}

sat_result_t bh_stream_service(bh_stream_t* s) {
    /* 1. land the segment in flight, if it has arrived */
    if (s->in_flight) {
        uint32_t remaining = 0;
        s->stats.polls++;
        const uint16_t tp0 = sat_time_frc();
        const sat_result_t r = sat_cd_stream_poll(s->block, 0, &remaining);
        s->stats.ticks_poll += (uint16_t)(sat_time_frc() - tp0);
        if (r != SAT_OK) {
            note(s, r);
            if (s->in_flight == BH_STREAM_FLIGHT_DATA) drop_job(s);
            s->in_flight = BH_STREAM_FLIGHT_NONE;
            return r;
        }
        if (remaining) return SAT_OK;
        const uint8_t kind = s->in_flight;
        s->in_flight = BH_STREAM_FLIGHT_NONE;
        const uint16_t td0 = sat_time_frc();
        if (kind == BH_STREAM_FLIGHT_MUSIC) {
            deliver_music(s);
            s->stats.ticks_music += (uint16_t)(sat_time_frc() - td0);
        } else {
            const bh_stream_job_t* j = &s->queue[s->head];
            const sat_result_t d = deliver_data(s, j);
            if (d != SAT_OK) {
                note(s, d);
                drop_job(s);
            } else if (s->sectors_left == 0) {
                if (j->kind == 1) s->groups_done |= 1u << j->target;
                else s->backgrounds_done |= 1u << j->target;
                s->stats.jobs_done++;
                drop_job(s);
            }
            s->stats.ticks_data += (uint16_t)(sat_time_frc() - td0);
        }
    }

    /* 2. start the next segment: the music first, then the data in the gaps */
    if (s->music_wanted) {
        const uint32_t room = ring_room(s);
        const uint32_t buffered = BH_STREAM_MUSIC_RING_FRAMES - room;
        if (buffered < s->stats.music_min_buffered) s->stats.music_min_buffered = buffered;
        if (room >= (music_frames(s, music_segment_bytes(s))) && begin_music(s)) return SAT_OK;
        if (s->in_flight) return SAT_OK;
        if (buffered < BH_STREAM_MUSIC_RESERVE && (s->running || s->count)) {
            s->stats.data_deferred++;
            return SAT_OK;
        }
    }
    if (!s->running) start_job(s);
    if (s->running && s->sectors_left) {
        const uint32_t seg = s->sectors_left < BH_STREAM_SEGMENT_SECTORS ? s->sectors_left : BH_STREAM_SEGMENT_SECTORS;
        const sat_result_t r = sat_cd_stream_begin(s->block, s->lba, seg, s->buffer);
        if (r == SAT_ERR_BUSY) return SAT_OK;             /* the drive is in use: try next frame */
        if (r != SAT_OK) { note(s, r); drop_job(s); return r; }
        s->lba += seg;
        s->sectors_left -= seg;
        s->seg_sectors = seg;
        s->in_flight = BH_STREAM_FLIGHT_DATA;
        s->stats.segments++;
    }
    return SAT_OK;
}

sat_result_t bh_stream_finish(bh_stream_t* s) {
    if (!bh_stream_busy(s)) return SAT_OK;
    s->stats.waits++;
    while (bh_stream_busy(s)) {
        if (s->in_flight) note(s, sat_cd_stream_finish(s->block));   /* at most one segment of waiting */
        bh_stream_service(s);
        if (s->pump) s->pump(s->pump_context);
    }
    return s->stats.last_error == SAT_OK ? SAT_OK : (sat_result_t)s->stats.last_error;
}
