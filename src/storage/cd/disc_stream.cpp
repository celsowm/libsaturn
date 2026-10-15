#include "saturn/disc_stream.h"

#include "saturn/time.h"

namespace {

constexpr uint32_t kSector = 2048u;
constexpr uint8_t kFlightNone = 0u;
constexpr uint8_t kFlightData = 1u;
constexpr uint8_t kFlightMusic = 2u;

sat_result_t note(sat_disc_stream_t* s, sat_result_t r) {
    if (r != SAT_OK && s->stats.last_error == SAT_OK) s->stats.last_error = r;
    return r;
}

const sat_disc_job_t& head_job(const sat_disc_stream_t* s) {
    return s->queue[s->head];
}

/* Removes the head job and tells its sink how it ended. */
void finish_job(sat_disc_stream_t* s, sat_result_t result) {
    const sat_disc_job_t job = s->queue[s->head];
    s->running = 0u;
    s->head = static_cast<uint8_t>((s->head + 1u) % SAT_DISC_STREAM_QUEUE);
    s->count--;
    if (result == SAT_OK) s->stats.jobs_done++;
    else note(s, result);
    if (job.ops->done != nullptr) job.ops->done(job.user, job.arg, result);
}

/* ---- music ------------------------------------------------------------------------------ */

uint32_t ring_room(const sat_disc_stream_t* s) {
    const uint32_t l = sat_audio_stream_available(s->music_left);
    const uint32_t r = sat_audio_stream_available(s->music_right);
    return l < r ? l : r;
}

uint32_t music_frames(const sat_disc_stream_t* s, uint32_t bytes) {
    return bytes / (2u * s->music_sample_bytes);
}

/* Bytes of the next music segment: a whole segment, or what is left of the file. */
uint32_t music_segment_bytes(const sat_disc_stream_t* s) {
    const uint32_t left = s->music_bytes - s->music_pos;
    return left < SAT_DISC_STREAM_SEGMENT_BYTES ? left : SAT_DISC_STREAM_SEGMENT_BYTES;
}

/* Starts the next music segment unless the drive is busy; true when it started. */
bool begin_music(sat_disc_stream_t* s) {
    const uint32_t bytes = music_segment_bytes(s);
    uint32_t lba = 0u, sectors = 0u;
    if (note(s, sat_cdfs_file_sectors(s->music_file, s->music_pos, bytes, &lba, &sectors)) != SAT_OK) return false;
    const sat_result_t r = sat_cd_stream_begin(s->block, lba, sectors, s->buffer);
    if (r == SAT_ERR_BUSY) return false;
    if (note(s, r) != SAT_OK) return false;
    s->seg_music_bytes = bytes;
    s->seg_sectors = sectors;
    s->in_flight = kFlightMusic;
    s->stats.segments++;
    return true;
}

/* The music segment has landed in s->buffer: split it into the two rings. */
sat_result_t deliver_music(sat_disc_stream_t* s) {
    const uint32_t frames = music_frames(s, s->seg_music_bytes);
    const uint16_t* in = reinterpret_cast<const uint16_t*>(s->buffer);
    const void* left;
    const void* right;
    if (s->music_sample_bytes == 1u) {
        /* S8 stereo: on the SH-2 one 16-bit load is a left/right pair (big-endian: left in the high
         * byte), cheaper than two byte loads. */
        uint8_t* l = s->split;
        uint8_t* rr = l + frames;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        for (uint32_t i = 0; i < frames; ++i) {
            const uint16_t v = in[i];
            l[i] = static_cast<uint8_t>(v >> 8);
            rr[i] = static_cast<uint8_t>(v);
        }
#else
        const uint8_t* const bytes = s->buffer;
        for (uint32_t i = 0; i < frames; ++i) {
            l[i] = bytes[i * 2u];
            rr[i] = bytes[i * 2u + 1u];
        }
#endif
        left = l;
        right = rr;
    } else {
        uint16_t* l = reinterpret_cast<uint16_t*>(s->split);
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
    s->music_pos += s->seg_sectors * kSector;
    if (s->music_pos >= s->music_bytes) {
        s->music_pos = 0u;
        s->stats.music_loops++;
    }
    s->stats.music_segments++;
    s->stats.music_bytes += s->seg_music_bytes;
    return r;
}

/* ---- data jobs -------------------------------------------------------------------------- */

/* Takes the next queued job. A job with nothing to read, or that cannot start, ends at once. */
void start_job(sat_disc_stream_t* s) {
    while (!s->running && s->count != 0u) {
        const sat_disc_job_t& job = head_job(s);
        sat_disc_extent_t extent = job.extent;
        sat_result_t r = SAT_OK;
        if (job.ops->begin != nullptr) {
            extent = sat_disc_extent_t{};
            r = job.ops->begin(job.user, job.arg, &extent);
        }
        if (r != SAT_OK) {
            /* A bad job is dropped so the queue cannot jam; the error stays in the stats. */
            finish_job(s, r);
            continue;
        }
        if (extent.file == nullptr) {
            finish_job(s, SAT_OK);
            continue;
        }
        uint32_t sectors = 0u;
        r = sat_cdfs_file_sectors(extent.file, extent.offset, extent.bytes, &s->lba, &sectors);
        if (r != SAT_OK) {
            finish_job(s, r);
            continue;
        }
        s->sectors_left = sectors;
        s->bytes_left = extent.bytes != 0u ? extent.bytes : extent.file->size - extent.offset;
        if (sectors == 0u || s->bytes_left == 0u) {
            finish_job(s, SAT_OK);
            continue;
        }
        s->running = 1u;
    }
}

sat_result_t deliver_data(sat_disc_stream_t* s) {
    const sat_disc_job_t& job = head_job(s);
    const uint32_t avail = s->seg_sectors * kSector;
    const uint32_t n = s->bytes_left < avail ? s->bytes_left : avail;
    const sat_result_t r = job.ops->chunk(job.user, job.arg, s->buffer, n);
    s->bytes_left -= n;
    s->stats.bytes += n;
    return r;
}

}  // namespace

extern "C" sat_result_t sat_disc_stream_init(sat_disc_stream_t* s, sat_cd_block_t* block) {
    if (s == nullptr || block == nullptr) return SAT_ERR_INVALID_ARG;
    __builtin_memset(s, 0, sizeof(*s));
    s->block = block;
    s->music_reserve = SAT_DISC_MUSIC_RESERVE_FRAMES;
    s->stats.music_min_buffered = SAT_DISC_MUSIC_RING_FRAMES;
    return SAT_OK;
}

extern "C" sat_result_t sat_disc_stream_queue(sat_disc_stream_t* s, const sat_disc_job_t* job) {
    if (s == nullptr || job == nullptr || job->ops == nullptr || job->ops->chunk == nullptr) return SAT_ERR_INVALID_ARG;
    if (job->ops->begin == nullptr && job->extent.file == nullptr) return SAT_ERR_INVALID_ARG;
    if (s->count >= SAT_DISC_STREAM_QUEUE) return SAT_ERR_CAPACITY;
    s->queue[(s->head + s->count++) % SAT_DISC_STREAM_QUEUE] = *job;
    return SAT_OK;
}

extern "C" uint8_t sat_disc_stream_busy(const sat_disc_stream_t* s) {
    return static_cast<uint8_t>(s->running != 0u || s->count != 0u);
}

extern "C" sat_result_t sat_disc_stream_music_start(sat_disc_stream_t* s, sat_disc_music_rings_t* rings,
                                                    const sat_cdfs_file_t* file, uint32_t bytes,
                                                    uint32_t sample_rate, uint32_t sample_bytes) {
    if (s == nullptr || rings == nullptr || file == nullptr || bytes < SAT_DISC_STREAM_SEGMENT_BYTES ||
        (sample_bytes != 1u && sample_bytes != 2u) || (bytes % (2u * sample_bytes)) != 0u ||
        s->in_flight != kFlightNone || s->music_open != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (file->size < bytes) return SAT_ERR_INVALID_ARG;
    s->music_sample_bytes = static_cast<uint8_t>(sample_bytes);
    s->music_file = file;
    const sat_audio_spec_t spec = {sample_rate, SAT_DISC_MUSIC_RING_FRAMES, 1u,
                                   static_cast<uint8_t>(sample_bytes == 1u ? SAT_AUDIO_PCM_S8 : SAT_AUDIO_PCM_S16), 0u};
    const uint32_t ring_bytes = SAT_DISC_MUSIC_RING_FRAMES * sample_bytes;
    sat_result_t r = sat_audio_stream_open(&s->music_left, &spec, rings->left, ring_bytes);
    if (r != SAT_OK) return r;
    r = sat_audio_stream_open(&s->music_right, &spec, rings->right, ring_bytes);
    if (r != SAT_OK) {
        (void)sat_audio_stream_close(s->music_left);
        return r;
    }
    /* Silent until both rings are primed, then they start together (as sat_music does). */
    (void)sat_audio_stream_pause(s->music_left);
    (void)sat_audio_stream_pause(s->music_right);
    (void)sat_audio_stream_set_pan(s->music_left, SAT_AUDIO_PAN_LEFT);
    (void)sat_audio_stream_set_pan(s->music_right, SAT_AUDIO_PAN_RIGHT);
    s->music_bytes = bytes;
    s->music_pos = 0u;
    s->music_open = 1u;
    while (ring_room(s) >= music_frames(s, music_segment_bytes(s))) {
        if (!begin_music(s)) return s->stats.last_error != SAT_OK ? static_cast<sat_result_t>(s->stats.last_error) : SAT_ERR_BUSY;
        r = sat_cd_stream_finish(s->block);
        s->in_flight = kFlightNone;
        if (r != SAT_OK) return note(s, r);
        r = deliver_music(s);
        if (r != SAT_OK) return r;
    }
    r = sat_audio_stream_resume(s->music_left);
    if (r == SAT_OK) r = sat_audio_stream_resume(s->music_right);
    s->music_wanted = 1u;
    return r;
}

extern "C" void sat_disc_stream_music_stats(const sat_disc_stream_t* s, uint32_t* underruns, uint32_t* refills,
                                            uint32_t* buffered) {
    sat_audio_stream_stats_t l = {}, r = {};
    if (s->music_open == 0u) {
        *underruns = *refills = *buffered = 0u;
        return;
    }
    (void)sat_audio_stream_stats(s->music_left, &l);
    (void)sat_audio_stream_stats(s->music_right, &r);
    *underruns = l.underrun_count > r.underrun_count ? l.underrun_count : r.underrun_count;
    *refills = l.refill_count;
    *buffered = l.buffered_frames < r.buffered_frames ? l.buffered_frames : r.buffered_frames;
}

extern "C" sat_result_t sat_disc_stream_service(sat_disc_stream_t* s) {
    /* 1. land the segment in flight, if it has arrived */
    if (s->in_flight != kFlightNone) {
        uint32_t remaining = 0;
        s->stats.polls++;
        const uint16_t tp0 = sat_time_frc();
        const sat_result_t r = sat_cd_stream_poll(s->block, 0, &remaining);
        s->stats.ticks_poll += static_cast<uint16_t>(sat_time_frc() - tp0);
        if (r != SAT_OK) {
            note(s, r);
            const bool data = s->in_flight == kFlightData;
            s->in_flight = kFlightNone;
            if (data) finish_job(s, r);
            return r;
        }
        if (remaining != 0u) return SAT_OK;
        const uint8_t kind = s->in_flight;
        s->in_flight = kFlightNone;
        const uint16_t td0 = sat_time_frc();
        if (kind == kFlightMusic) {
            (void)deliver_music(s);
            s->stats.ticks_music += static_cast<uint16_t>(sat_time_frc() - td0);
        } else {
            const sat_result_t d = deliver_data(s);
            if (d != SAT_OK) finish_job(s, d);
            else if (s->sectors_left == 0u) finish_job(s, SAT_OK);
            s->stats.ticks_data += static_cast<uint16_t>(sat_time_frc() - td0);
        }
    }

    /* 2. start the next segment: the music first, then the data in the gaps */
    if (s->music_wanted != 0u) {
        const uint32_t room = ring_room(s);
        const uint32_t buffered = SAT_DISC_MUSIC_RING_FRAMES - room;
        if (buffered < s->stats.music_min_buffered) s->stats.music_min_buffered = buffered;
        if (room >= music_frames(s, music_segment_bytes(s)) && begin_music(s)) return SAT_OK;
        if (s->in_flight != kFlightNone) return SAT_OK;
        if (buffered < s->music_reserve && (s->running != 0u || s->count != 0u)) {
            s->stats.data_deferred++;
            return SAT_OK;
        }
    }
    if (s->running == 0u) start_job(s);
    if (s->running != 0u && s->sectors_left != 0u) {
        const uint32_t seg = s->sectors_left < SAT_DISC_STREAM_SEGMENT_SECTORS ? s->sectors_left : SAT_DISC_STREAM_SEGMENT_SECTORS;
        const sat_result_t r = sat_cd_stream_begin(s->block, s->lba, seg, s->buffer);
        if (r == SAT_ERR_BUSY) return SAT_OK;             /* the drive is in use: try next frame */
        if (r != SAT_OK) {
            finish_job(s, r);
            return r;
        }
        s->lba += seg;
        s->sectors_left -= seg;
        s->seg_sectors = seg;
        s->in_flight = kFlightData;
        s->stats.segments++;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_disc_stream_finish(sat_disc_stream_t* s) {
    if (sat_disc_stream_busy(s) == 0u) return SAT_OK;
    s->stats.waits++;
    while (sat_disc_stream_busy(s) != 0u) {
        if (s->in_flight != kFlightNone) note(s, sat_cd_stream_finish(s->block));   /* at most one segment of waiting */
        (void)sat_disc_stream_service(s);
        if (s->pump != nullptr) s->pump(s->pump_context);
    }
    return s->stats.last_error == SAT_OK ? SAT_OK : static_cast<sat_result_t>(s->stats.last_error);
}
