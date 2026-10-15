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

/* The built-in feed: a looping raw interleaved stereo file split into the scheduler's own two rings. */

uint8_t builtin_next(void* user, const sat_cdfs_file_t** file, uint32_t* offset, uint32_t* bytes) {
    const sat_disc_stream_t* s = static_cast<const sat_disc_stream_t*>(user);
    const uint32_t seg = music_segment_bytes(s);
    if (ring_room(s) < music_frames(s, seg)) return 0u;
    *file = s->music_file;
    *offset = s->music_pos;
    *bytes = seg;
    return 1u;
}

/* The music segment has landed: split it into the two rings (whole, in one call). */
sat_result_t builtin_deliver(void* user, const uint8_t* data, uint32_t bytes, uint32_t* consumed) {
    sat_disc_stream_t* s = static_cast<sat_disc_stream_t*>(user);
    const uint32_t frames = music_frames(s, bytes);
    const uint16_t* in = reinterpret_cast<const uint16_t*>(data);
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
        for (uint32_t i = 0; i < frames; ++i) {
            l[i] = data[i * 2u];
            rr[i] = data[i * 2u + 1u];
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
    s->music_pos += s->seg_sectors * kSector;
    if (s->music_pos >= s->music_bytes) {
        s->music_pos = 0u;
        s->stats.music_loops++;
    }
    *consumed = bytes;
    return r;
}

uint32_t builtin_buffered(void* user) {
    return SAT_DISC_MUSIC_RING_FRAMES - ring_room(static_cast<const sat_disc_stream_t*>(user));
}

/* The rings are full when no segment is wanted and none is on its way: both streams then start
 * together (they were opened paused). */
void builtin_tick(void* user);

const sat_disc_music_feed_ops_t kBuiltinFeed = {builtin_next, nullptr, builtin_deliver, builtin_buffered, 0u, builtin_tick};

enum MusicBegin { kMusicStarted, kMusicNotWanted, kMusicBlocked, kMusicNoBuffer };

uint8_t* segment_buffer(sat_disc_stream_t* s, uint8_t index) {
    return index == 0u ? s->buffer : s->split;
}

const sat_disc_music_feed_ops_t* feed_ops(const sat_disc_stream_t* s, uint8_t owner) {
    return owner != 0u ? s->standby_ops : s->music_ops;
}

void* feed_user(const sat_disc_stream_t* s, uint8_t owner) {
    return owner != 0u ? s->standby_user : s->music_user;
}

/* Whether a feed that delivers from the segment buffer itself is installed (a data segment may then use the second buffer). */
bool any_pipelined(const sat_disc_stream_t* s) {
    return (s->music_ops != nullptr && s->music_ops->pipelined != 0u) ||
           (s->standby_ops != nullptr && s->standby_ops->pipelined != 0u);
}

/* A segment buffer nothing holds (-1: none). The built-in feed splits into `split`, so only a
 * pipelined feed (which delivers from the buffer itself) may land there. */
int free_buffer(const sat_disc_stream_t* s, bool two) {
    const uint8_t used = static_cast<uint8_t>((s->deliv_active != 0u ? 1u << s->deliv_buf : 0u) |
                                              (s->ready != 0u ? 1u << s->ready_buf : 0u) |
                                              (s->in_flight != kFlightNone ? 1u << s->fly_buf : 0u));
    if ((used & 1u) == 0u) return 0;
    if (two && (used & 2u) == 0u) return 1;
    return -1;
}

/* Starts the next segment of feed `owner` (0 primary, 1 standby) unless the drive is busy or the
 * feed wants none. */
MusicBegin begin_music(sat_disc_stream_t* s, uint8_t owner) {
    const sat_disc_music_feed_ops_t* ops = feed_ops(s, owner);
    void* user = feed_user(s, owner);
    const int buf = free_buffer(s, ops->pipelined != 0u);
    if (buf < 0) return kMusicNoBuffer;
    const sat_cdfs_file_t* file = nullptr;
    uint32_t offset = 0u, bytes = 0u;
    if (ops->next(user, &file, &offset, &bytes) == 0u) return kMusicNotWanted;
    if (file == nullptr || bytes == 0u || bytes > SAT_DISC_STREAM_SEGMENT_BYTES) {
        note(s, SAT_ERR_INVALID_ARG);
        return kMusicBlocked;
    }
    uint32_t lba = 0u, sectors = 0u;
    if (note(s, sat_cdfs_file_sectors(file, offset, bytes, &lba, &sectors)) != SAT_OK) return kMusicBlocked;
    const sat_result_t r = sat_cd_stream_begin(s->block, lba, sectors, segment_buffer(s, static_cast<uint8_t>(buf)));
    if (r == SAT_ERR_BUSY) return kMusicBlocked;
    if (note(s, r) != SAT_OK) return kMusicBlocked;
    s->seg_music_bytes = bytes;
    s->seg_sectors = sectors;
    s->fly_buf = static_cast<uint8_t>(buf);
    s->fly_owner = owner;
    s->fly_discard = 0u;
    s->in_flight = kFlightMusic;
    s->stats.segments++;
    if (ops->started != nullptr) ops->started(user);
    return kMusicStarted;
}

/* Hands the next part of the segment being delivered to the feed: one call, whatever it consumes. */
sat_result_t deliver_slice(sat_disc_stream_t* s) {
    const uint16_t t0 = sat_time_frc();
    const uint32_t left = s->deliv_bytes - s->deliv_off;
    uint32_t used = 0u;
    const sat_result_t r = feed_ops(s, s->deliv_owner)->deliver(feed_user(s, s->deliv_owner),
                                                                segment_buffer(s, s->deliv_buf) + s->deliv_off, left, &used);
    note(s, r);
    if (r != SAT_OK || used == 0u || used > left) used = left;     /* a failed or stuck segment is dropped */
    s->deliv_off += used;
    if (s->deliv_off >= s->deliv_bytes) {
        s->stats.music_segments++;
        s->stats.music_bytes += s->deliv_bytes;
        s->deliv_active = s->ready;
        if (s->ready != 0u) {
            s->deliv_buf = s->ready_buf;
            s->deliv_owner = s->ready_owner;
            s->deliv_off = 0u;
            s->deliv_bytes = s->ready_bytes;
            s->ready = 0u;
        }
    }
    const uint16_t dt = static_cast<uint16_t>(sat_time_frc() - t0);
    s->stats.ticks_music += dt;
    if (dt > s->stats.max_ticks_music) s->stats.max_ticks_music = dt;
    return r;
}

/* Runs the deliveries to the end. */
sat_result_t drain_deliveries(sat_disc_stream_t* s) {
    sat_result_t first = SAT_OK;
    while (s->deliv_active != 0u) {
        const sat_result_t r = deliver_slice(s);
        if (first == SAT_OK) first = r;
    }
    return first;
}

void builtin_tick(void* user) {
    sat_disc_stream_t* s = static_cast<sat_disc_stream_t*>(user);
    if (s->music_starting == 0u || s->deliv_active != 0u || s->ready != 0u || s->in_flight != kFlightNone) return;
    if (ring_room(s) >= music_frames(s, music_segment_bytes(s))) return;      // another segment still fits
    s->music_starting = 0u;
    (void)note(s, sat_audio_stream_resume(s->music_left));
    (void)note(s, sat_audio_stream_resume(s->music_right));
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
    const sat_result_t r = job.ops->chunk(job.user, job.arg, segment_buffer(s, s->fly_buf), n);
    s->bytes_left -= n;
    s->stats.bytes += n;
    return r;
}

/* The segment in flight has fully arrived: hand it on. A music segment joins the delivery queue
 * (a pipelined feed is then served one slice per service call; any other is delivered at once).
 * Returns the sink's result (a pipelined music segment: SAT_OK, its result comes with the slices). */
sat_result_t land(sat_disc_stream_t* s) {
    const uint8_t kind = s->in_flight;
    s->in_flight = kFlightNone;
    if (kind == kFlightMusic) {
        if (s->fly_discard != 0u) {         /* read for a feed that has been swapped out */
            s->fly_discard = 0u;
            return SAT_OK;
        }
        if (s->deliv_active == 0u) {
            s->deliv_active = 1u;
            s->deliv_buf = s->fly_buf;
            s->deliv_owner = s->fly_owner;
            s->deliv_off = 0u;
            s->deliv_bytes = s->seg_music_bytes;
        } else {
            s->ready = 1u;
            s->ready_buf = s->fly_buf;
            s->ready_owner = s->fly_owner;
            s->ready_bytes = s->seg_music_bytes;
        }
        return feed_ops(s, s->fly_owner)->pipelined != 0u ? SAT_OK : drain_deliveries(s);
    }
    const uint16_t t0 = sat_time_frc();
    const sat_result_t r = deliver_data(s);
    if (r != SAT_OK) finish_job(s, r);
    else if (s->sectors_left == 0u) finish_job(s, SAT_OK);
    const uint16_t dt = static_cast<uint16_t>(sat_time_frc() - t0);
    s->stats.ticks_data += dt;
    if (dt > s->stats.max_ticks_data) s->stats.max_ticks_data = dt;
    return r;
}

}  // namespace

extern "C" sat_result_t sat_disc_stream_init(sat_disc_stream_t* s, sat_cd_block_t* block) {
    if (s == nullptr || block == nullptr) return SAT_ERR_INVALID_ARG;
    __builtin_memset(s, 0, sizeof(*s));
    s->block = block;
    s->music_reserve = SAT_DISC_MUSIC_RESERVE_FRAMES;
    s->standby_floor = SAT_DISC_STANDBY_FLOOR_FRAMES;
    s->standby_high = SAT_DISC_STANDBY_HIGH_FRAMES;
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
        s->in_flight != kFlightNone || s->music_open != 0u || s->music_ops != nullptr || s->standby_ops != nullptr) {
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
    s->music_ops = &kBuiltinFeed;
    s->music_user = s;
    s->music_starting = 1u;
    s->music_wanted = 1u;
    return SAT_OK;
}

extern "C" void sat_disc_stream_music_set_starting(sat_disc_stream_t* s, uint8_t starting) {
    if (s != nullptr) s->music_starting = starting != 0u ? 1u : 0u;
}

extern "C" uint8_t sat_disc_stream_music_playing(const sat_disc_stream_t* s) {
    return static_cast<uint8_t>(s != nullptr && s->music_open != 0u && s->music_starting == 0u);
}

extern "C" sat_result_t sat_disc_stream_music_attach(sat_disc_stream_t* s, const sat_disc_music_feed_ops_t* ops, void* user) {
    if (s == nullptr || ops == nullptr || ops->next == nullptr || ops->deliver == nullptr || ops->buffered == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (s->music_ops != nullptr) return SAT_ERR_BUSY;
    s->music_ops = ops;
    s->music_user = user;
    s->music_wanted = 1u;
    s->stats.music_min_buffered = 0xFFFFFFFFu;
    return SAT_OK;
}

namespace {

/* Drops what feed `owner` has in flight or queued for delivery. The other feed's segments stay:
 * if the one being delivered is dropped, the one waiting behind it moves up. */
void abort_owner(sat_disc_stream_t* s, uint8_t owner) {
    if (s->in_flight == kFlightMusic && s->fly_owner == owner && s->fly_discard == 0u) {
        note(s, sat_cd_stream_finish(s->block));
        s->in_flight = kFlightNone;
    }
    if (s->ready != 0u && s->ready_owner == owner) s->ready = 0u;
    if (s->deliv_active != 0u && s->deliv_owner == owner) {
        s->deliv_active = s->ready;
        if (s->ready != 0u) {
            s->deliv_buf = s->ready_buf;
            s->deliv_owner = s->ready_owner;
            s->deliv_off = 0u;
            s->deliv_bytes = s->ready_bytes;
            s->ready = 0u;
        }
    }
}

}  // namespace

extern "C" void sat_disc_stream_music_abort(sat_disc_stream_t* s) {
    if (s != nullptr) abort_owner(s, 0u);
}

extern "C" sat_result_t sat_disc_stream_music_attach_standby(sat_disc_stream_t* s, const sat_disc_music_feed_ops_t* ops, void* user) {
    if (s == nullptr || ops == nullptr || ops->next == nullptr || ops->deliver == nullptr || ops->buffered == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (s->standby_ops != nullptr || s->music_open != 0u) return SAT_ERR_BUSY;
    s->standby_ops = ops;
    s->standby_user = user;
    return SAT_OK;
}

extern "C" void sat_disc_stream_music_abort_standby(sat_disc_stream_t* s) {
    if (s == nullptr) return;
    abort_owner(s, 1u);
    s->standby_burst = 0u;
    s->standby_visit = 0u;
}

extern "C" void sat_disc_stream_music_detach_standby(sat_disc_stream_t* s) {
    if (s == nullptr) return;
    abort_owner(s, 1u);
    s->standby_ops = nullptr;
    s->standby_user = nullptr;
    s->standby_burst = 0u;
    s->standby_visit = 0u;
}

extern "C" sat_result_t sat_disc_stream_music_swap(sat_disc_stream_t* s) {
    if (s == nullptr || s->standby_ops == nullptr) return SAT_ERR_INVALID_ARG;
    if (s->music_open != 0u) return SAT_ERR_BUSY;
    /* What the old primary has delivering or waiting goes at once; a read in flight is let finish
     * (waiting would stall this call) and dropped when it lands. */
    if (s->ready != 0u && s->ready_owner == 0u) s->ready = 0u;
    if (s->deliv_active != 0u && s->deliv_owner == 0u) {
        s->deliv_active = s->ready;
        if (s->ready != 0u) {
            s->deliv_buf = s->ready_buf;
            s->deliv_owner = s->ready_owner;
            s->deliv_off = 0u;
            s->deliv_bytes = s->ready_bytes;
            s->ready = 0u;
        }
    }
    if (s->in_flight == kFlightMusic && s->fly_owner == 0u) s->fly_discard = 1u;
    /* The roles trade places, and so do the segments' owners. */
    s->fly_owner ^= 1u;
    s->deliv_owner ^= 1u;
    s->ready_owner ^= 1u;
    const sat_disc_music_feed_ops_t* ops = s->music_ops;
    void* user = s->music_user;
    s->music_ops = s->standby_ops;
    s->music_user = s->standby_user;
    s->standby_ops = ops;
    s->standby_user = user;
    s->music_wanted = 1u;
    s->music_starting = 0u;
    s->standby_burst = 0u;
    s->standby_visit = 0u;
    s->stats.music_min_buffered = 0xFFFFFFFFu;
    return SAT_OK;
}

extern "C" void sat_disc_stream_music_detach(sat_disc_stream_t* s) {
    if (s == nullptr || s->music_open != 0u) return;     /* the built-in feed has no detach */
    sat_disc_stream_music_abort(s);
    s->music_ops = nullptr;
    s->music_user = nullptr;
    s->music_wanted = 0u;
    s->music_starting = 0u;
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

namespace {

/* Follows one visit to the standby's file through the playing feed's fill. When the playing rings
 * ended up under the reserve, the next visit is started from deeper rings; when no ring depth would
 * cover it, the prefetch stops. */
void standby_visit_track(sat_disc_stream_t* s, uint32_t buffered) {
    if (s->standby_visit == 0u) {
        if (s->standby_burst != 0u) {
            s->standby_visit = 1u;
            s->standby_visit_start = buffered;
            s->standby_visit_min = buffered;
            s->stats.standby_visits++;
        }
        return;
    }
    if (buffered < s->standby_visit_min) s->standby_visit_min = buffered;
    if (s->standby_visit == 1u) {
        if (s->standby_burst == 0u) s->standby_visit = 2u;
        return;
    }
    if (buffered < s->standby_visit_min + 2048u) return;          /* still falling or flat: not back yet */
    s->standby_visit = 0u;
    const uint32_t dip = s->standby_visit_start - s->standby_visit_min;
    if (dip > s->stats.standby_worst_dip) s->stats.standby_worst_dip = dip;
    if (s->standby_visit_min >= s->music_reserve) return;
    const uint32_t shortfall = s->music_reserve - s->standby_visit_min;
    if (s->standby_high + shortfall > SAT_DISC_STANDBY_CEIL_FRAMES) {
        s->standby_disabled = 1u;
        s->standby_burst = 0u;
        return;
    }
    s->standby_high += shortfall;
    s->standby_floor += shortfall;
}

sat_result_t service_once(sat_disc_stream_t* s) {
    /* the playing rings are the clock of a visit to the standby: watch them while a segment is in flight too */
    if (s->music_wanted != 0u && s->music_starting == 0u && s->standby_ops != nullptr && s->standby_disabled == 0u)
        standby_visit_track(s, s->music_ops->buffered(s->music_user));
    /* 1. move what has arrived of the segment in flight, and land it when it is whole */
    if (s->in_flight != kFlightNone) {
        uint32_t remaining = 0;
        s->stats.polls++;
        const uint16_t tp0 = sat_time_frc();
        const sat_result_t r = sat_cd_stream_poll(s->block, 0, &remaining);
        const uint16_t dp = static_cast<uint16_t>(sat_time_frc() - tp0);
        s->stats.ticks_poll += dp;
        if (dp > s->stats.max_ticks_poll) s->stats.max_ticks_poll = dp;
        if (r != SAT_OK) {
            note(s, r);
            const bool data = s->in_flight == kFlightData;
            s->in_flight = kFlightNone;
            if (data) finish_job(s, r);
            return r;
        }
        if (remaining == 0u) (void)land(s);
    }

    /* 2. one slice of the music segment being delivered */
    if (s->deliv_active != 0u) (void)deliver_slice(s);
    if (s->in_flight != kFlightNone) return SAT_OK;

    /* 3. start the next segment: the music first, then the data in the gaps */
    MusicBegin mb = kMusicNotWanted;
    if (s->music_wanted != 0u) {
        const uint32_t buffered = s->music_ops->buffered(s->music_user);
        if (s->music_starting == 0u && buffered < s->stats.music_min_buffered) s->stats.music_min_buffered = buffered;
        /* A burst: while the primary is comfortably ahead the standby reads its segments one after
         * another, instead of the drive alternating between two files. */
        if (s->standby_ops != nullptr && s->standby_disabled == 0u) {
            if (s->standby_burst == 0u && buffered >= s->standby_high) s->standby_burst = 1u;
            else if (s->standby_burst != 0u && buffered < s->standby_floor) s->standby_burst = 0u;
            if (s->standby_burst != 0u) {
                const MusicBegin sb = begin_music(s, 1u);
                if (sb == kMusicStarted || sb == kMusicNoBuffer) return SAT_OK;
                if (sb == kMusicNotWanted) s->standby_burst = 0u;
            }
        }
        mb = begin_music(s, 0u);
        if (mb == kMusicStarted || mb == kMusicNoBuffer) return SAT_OK;
        if (buffered < s->music_reserve && (s->running != 0u || s->count != 0u)) {
            s->stats.data_deferred++;
            return SAT_OK;
        }
    }
    /* The playing feed wants nothing: the standby may fill its rings, ahead of the data jobs. */
    if (s->standby_ops != nullptr && s->standby_disabled == 0u && mb == kMusicNotWanted) {
        const MusicBegin sb = begin_music(s, 1u);
        if (sb == kMusicStarted || sb == kMusicNoBuffer) return SAT_OK;
    }
    if (s->running == 0u) start_job(s);
    if (s->running != 0u && s->sectors_left != 0u) {
        const int buf = free_buffer(s, any_pipelined(s));
        if (buf < 0) return SAT_OK;
        const uint32_t seg = s->sectors_left < SAT_DISC_STREAM_SEGMENT_SECTORS ? s->sectors_left : SAT_DISC_STREAM_SEGMENT_SECTORS;
        const sat_result_t r = sat_cd_stream_begin(s->block, s->lba, seg, segment_buffer(s, static_cast<uint8_t>(buf)));
        if (r == SAT_ERR_BUSY) return SAT_OK;             /* the drive is in use: try next frame */
        if (r != SAT_OK) {
            finish_job(s, r);
            return r;
        }
        s->lba += seg;
        s->sectors_left -= seg;
        s->seg_sectors = seg;
        s->fly_buf = static_cast<uint8_t>(buf);
        s->in_flight = kFlightData;
        s->stats.segments++;
    }
    return SAT_OK;
}

}  // namespace

extern "C" sat_result_t sat_disc_stream_service(sat_disc_stream_t* s) {
    const sat_result_t r = service_once(s);
    if (s->music_ops != nullptr && s->music_ops->tick != nullptr) s->music_ops->tick(s->music_user);
    return r;
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
