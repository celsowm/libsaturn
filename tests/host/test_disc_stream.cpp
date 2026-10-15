/* sat_disc_stream against a fake drive and fake SCSP rings: the drive starts one segment at a
 * time and delivers it after a settable number of polls, the rings are plain FIFOs the test drains
 * the way the SCSP would. */
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "saturn/disc_stream.h"

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

namespace {

constexpr uint32_t kSector = 2048u;

/* ---- fake disc: the byte at an absolute disc offset is a pure function of it ------------------- */

uint8_t disc_byte(uint64_t idx) { return static_cast<uint8_t>(idx * 31u + (idx >> 8) * 7u + 3u); }

/* ---- fake CD Block stream ---------------------------------------------------------------------- */

struct Drive {
    bool open = false;
    uint32_t lba = 0, count = 0;
    uint8_t* dest = nullptr;
    uint32_t latency = 1;          /* polls that report sectors still to come */
    uint32_t polls_left = 0;
    uint32_t begins = 0, busy_hits = 0;
    int fail_poll_after = -1;      /* poll number (1-based) that returns an error, -1 none */
    uint32_t poll_calls = 0;
    std::vector<uint32_t> begin_lbas;
    uint32_t min_fill_at_data_begin = 0xFFFFFFFFu;   /* ring fill (frames) when a data segment (lba 4000+) started */
};
Drive g_drive;

void deliver_sectors() {
    for (uint32_t i = 0; i < g_drive.count * kSector; ++i) {
        g_drive.dest[i] = disc_byte(static_cast<uint64_t>(g_drive.lba) * kSector + i);
    }
}

/* ---- fake SCSP ring ----------------------------------------------------------------------------- */

struct Ring {
    bool open = false, paused = false, closed = false;
    uint32_t sample_bytes = 0, capacity = 0;
    int16_t pan = 0;
    std::vector<uint8_t> fifo;     /* bytes buffered */
    std::vector<uint8_t> played;   /* bytes the SCSP consumed */
    uint32_t underruns = 0, refills = 0, rejected = 0;
    uint32_t fill_frames() const { return static_cast<uint32_t>(fifo.size()) / sample_bytes; }
};
Ring g_ring[2];
uint32_t g_ring_opens = 0;
uint16_t g_frc = 0;

void reset_fakes() {
    g_drive = Drive{};
    g_ring[0] = Ring{};
    g_ring[1] = Ring{};
    g_ring_opens = 0;
}

/* The SCSP drains `frames` from both rings; a ring that runs dry is an underrun. */
void consume(uint32_t frames) {
    for (Ring& r : g_ring) {
        if (!r.open || r.paused) continue;
        uint32_t take = frames;
        if (take > r.fill_frames()) {
            take = r.fill_frames();
            ++r.underruns;
        }
        const uint32_t n = take * r.sample_bytes;
        r.played.insert(r.played.end(), r.fifo.begin(), r.fifo.begin() + n);
        r.fifo.erase(r.fifo.begin(), r.fifo.begin() + n);
    }
}

/* Where the data jobs' notes go. */
struct Sink {
    uint32_t begin_calls = 0, chunk_calls = 0, done_calls = 0;
    sat_result_t done_result = SAT_OK;
    std::vector<uint8_t> got;
    uint32_t max_chunk = 0;
    uint64_t expect_base = 0;      /* absolute disc offset the first payload byte comes from */
    sat_result_t begin_result = SAT_OK;
    sat_result_t chunk_result = SAT_OK;
    uint32_t chunk_fail_at = 0;    /* 1-based chunk call that fails, 0 never */
    sat_disc_extent_t extent{};
};
Sink g_sink;

}  // namespace

/* ---- link-time fakes of the layers under the module ------------------------------------------- */

extern "C" uint16_t sat_time_frc(void) { return g_frc += 3u; }

extern "C" sat_result_t sat_cdfs_file_sectors(const sat_cdfs_file_t* file, uint32_t offset, uint32_t bytes,
                                              uint32_t* out_lba, uint32_t* out_sectors) {
    if (file == nullptr || out_lba == nullptr || out_sectors == nullptr || (offset % kSector) != 0u) return SAT_ERR_INVALID_ARG;
    if (offset > file->size) return SAT_ERR_INVALID_ARG;
    if (bytes == 0u) bytes = file->size - offset;
    if (bytes > file->size - offset) return SAT_ERR_INVALID_ARG;
    *out_lba = file->extent_lba + offset / kSector;
    *out_sectors = (bytes + kSector - 1u) / kSector;
    return SAT_OK;
}

extern "C" sat_result_t sat_cd_stream_begin(sat_cd_block_t*, uint32_t lba, uint32_t sector_count, void* destination) {
    if (g_drive.open) {
        ++g_drive.busy_hits;
        return SAT_ERR_BUSY;
    }
    if (sector_count == 0u || sector_count > SAT_CD_STREAM_MAX_SECTORS || (reinterpret_cast<uintptr_t>(destination) & 1u)) {
        return SAT_ERR_INVALID_ARG;
    }
    if (lba >= 4000u && lba < 5000u) {
        for (const Ring& r : g_ring) {
            if (r.open && r.fill_frames() < g_drive.min_fill_at_data_begin) g_drive.min_fill_at_data_begin = r.fill_frames();
        }
    }
    g_drive.open = true;
    g_drive.lba = lba;
    g_drive.count = sector_count;
    g_drive.dest = static_cast<uint8_t*>(destination);
    g_drive.polls_left = g_drive.latency;
    ++g_drive.begins;
    g_drive.begin_lbas.push_back(lba);
    return SAT_OK;
}

extern "C" sat_result_t sat_cd_stream_poll(sat_cd_block_t*, uint32_t, uint32_t* out_remaining) {
    ++g_drive.poll_calls;
    if (!g_drive.open) {
        if (out_remaining) *out_remaining = 0u;
        return SAT_OK;
    }
    if (g_drive.fail_poll_after > 0 && g_drive.poll_calls == static_cast<uint32_t>(g_drive.fail_poll_after)) {
        g_drive.open = false;
        return SAT_ERR_TIMEOUT;
    }
    if (g_drive.polls_left > 0u) {
        --g_drive.polls_left;
        if (out_remaining) *out_remaining = g_drive.count;
        return SAT_OK;
    }
    deliver_sectors();
    g_drive.open = false;
    if (out_remaining) *out_remaining = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_cd_stream_finish(sat_cd_block_t*) {
    if (!g_drive.open) return SAT_OK;
    deliver_sectors();
    g_drive.open = false;
    return SAT_OK;
}

extern "C" sat_result_t sat_audio_stream_open(sat_audio_stream_t* out, const sat_audio_spec_t* spec, void* buffer, uint32_t bytes) {
    CHECK(out != nullptr && spec != nullptr && buffer != nullptr);
    CHECK(spec->channels == 1u);
    CHECK(g_ring_opens < 2u);
    Ring& r = g_ring[g_ring_opens];
    r = Ring{};
    r.open = true;
    r.sample_bytes = spec->format == SAT_AUDIO_PCM_S8 ? 1u : 2u;
    r.capacity = spec->buffer_frames;
    CHECK(bytes == r.capacity * r.sample_bytes);
    out->slot = static_cast<uint16_t>(g_ring_opens);
    out->generation = 1u;
    ++g_ring_opens;
    return SAT_OK;
}
extern "C" sat_result_t sat_audio_stream_write(sat_audio_stream_t s, const void* frames, uint32_t count) {
    Ring& r = g_ring[s.slot];
    if (count > r.capacity - r.fill_frames()) {
        ++r.rejected;
        return SAT_ERR_CAPACITY;
    }
    const uint8_t* p = static_cast<const uint8_t*>(frames);
    r.fifo.insert(r.fifo.end(), p, p + count * r.sample_bytes);
    return SAT_OK;
}
extern "C" uint32_t sat_audio_stream_available(sat_audio_stream_t s) {
    const Ring& r = g_ring[s.slot];
    return r.capacity - r.fill_frames();
}
extern "C" sat_result_t sat_audio_stream_pause(sat_audio_stream_t s) { g_ring[s.slot].paused = true; return SAT_OK; }
extern "C" sat_result_t sat_audio_stream_resume(sat_audio_stream_t s) { g_ring[s.slot].paused = false; return SAT_OK; }
extern "C" sat_result_t sat_audio_stream_set_pan(sat_audio_stream_t s, int16_t pan) { g_ring[s.slot].pan = pan; return SAT_OK; }
extern "C" sat_result_t sat_audio_stream_close(sat_audio_stream_t s) { g_ring[s.slot].closed = true; return SAT_OK; }
extern "C" sat_result_t sat_audio_stream_stats(sat_audio_stream_t s, sat_audio_stream_stats_t* out) {
    const Ring& r = g_ring[s.slot];
    *out = {};
    out->buffered_frames = r.fill_frames();
    out->capacity_frames = r.capacity;
    out->underrun_count = r.underruns;
    out->refill_count = r.refills;
    return SAT_OK;
}

namespace {

/* ---- helpers ------------------------------------------------------------------------------------ */

sat_disc_stream_t g_stream;
sat_disc_music_rings_t g_rings;

sat_cdfs_file_t make_file(uint32_t lba, uint32_t size) {
    sat_cdfs_file_t f{};
    f.extent_lba = lba;
    f.size = size;
    return f;
}

constexpr uint32_t kMusicLba = 100u;
constexpr uint32_t kMusicBytes = 3u * 16384u + 3000u;     /* three segments and a short tail */
constexpr uint32_t kDataLba = 4000u;
constexpr uint32_t kDataBytes = 40000u;                   /* 20 sectors: segments of 8, 8 and 4 */

const sat_cdfs_file_t g_music_file = make_file(kMusicLba, kMusicBytes);
const sat_cdfs_file_t g_data_file = make_file(kDataLba, kDataBytes);

sat_result_t sink_begin(void*, uint32_t, sat_disc_extent_t* extent) {
    ++g_sink.begin_calls;
    if (g_sink.begin_result != SAT_OK) return g_sink.begin_result;
    *extent = g_sink.extent;
    return SAT_OK;
}

sat_result_t sink_chunk(void*, uint32_t, const uint8_t* data, uint32_t bytes) {
    ++g_sink.chunk_calls;
    if (g_sink.chunk_fail_at != 0u && g_sink.chunk_calls == g_sink.chunk_fail_at) return g_sink.chunk_result;
    g_sink.got.insert(g_sink.got.end(), data, data + bytes);
    if (bytes > g_sink.max_chunk) g_sink.max_chunk = bytes;
    return SAT_OK;
}

void sink_done(void*, uint32_t, sat_result_t result) {
    ++g_sink.done_calls;
    g_sink.done_result = result;
}

const sat_disc_job_ops_t k_ops = {sink_begin, sink_chunk, sink_done};
const sat_disc_job_ops_t k_ops_no_begin = {nullptr, sink_chunk, sink_done};

void start_stream() {
    reset_fakes();
    g_sink = Sink{};
    CHECK(sat_disc_stream_init(&g_stream, reinterpret_cast<sat_cd_block_t*>(&g_stream)) == SAT_OK);
}

sat_disc_job_t data_job(uint32_t offset = 0u, uint32_t bytes = 0u) {
    g_sink.extent = sat_disc_extent_t{&g_data_file, offset, bytes};
    sat_disc_job_t j{};
    j.ops = &k_ops;
    return j;
}

/* Expected music payload byte `k` of a track of `bytes` that loops: the byte of frame (k) of the
 * channel, found in the file's interleaved layout. */
uint8_t music_byte(uint32_t off) {
    return disc_byte(static_cast<uint64_t>(kMusicLba) * kSector + (off % kMusicBytes));
}

/* The bytes ring `ch` should have received for frames [first, first + n) of the looping track. */
std::vector<uint8_t> expected_channel(uint32_t sample_bytes, uint32_t ch, uint64_t first, uint64_t n) {
    std::vector<uint8_t> out;
    for (uint64_t f = first; f < first + n; ++f) {
        const uint32_t frame_off = static_cast<uint32_t>((f * 2u * sample_bytes) % kMusicBytes);
        for (uint32_t b = 0; b < sample_bytes; ++b) out.push_back(music_byte(frame_off + ch * sample_bytes + b));
    }
    return out;
}

/* Everything a ring has been given so far, played then still buffered. */
std::vector<uint8_t> ring_history(const Ring& r) {
    std::vector<uint8_t> all = r.played;
    all.insert(all.end(), r.fifo.begin(), r.fifo.end());
    return all;
}

void check_music_stream(uint32_t sample_bytes) {
    for (uint32_t ch = 0; ch < 2u; ++ch) {
        const std::vector<uint8_t> have = ring_history(g_ring[ch]);
        const uint64_t frames = have.size() / sample_bytes;
        CHECK(frames > 0u);
        CHECK(have == expected_channel(sample_bytes, ch, 0u, frames));
    }
}

/* One display field: the SCSP drains `frames_per_tick`, then the game services the drive. */
void tick(uint32_t frames_per_tick) {
    consume(frames_per_tick);
    (void)sat_disc_stream_service(&g_stream);
}

/* ---- tests -------------------------------------------------------------------------------------- */

void test_music_prime(uint32_t sample_bytes) {
    start_stream();
    CHECK(sat_disc_stream_music_start(&g_stream, &g_rings, &g_music_file, kMusicBytes, 22050u, sample_bytes) == SAT_OK);
    CHECK(g_ring_opens == 2u);
    CHECK(g_ring[0].pan == SAT_AUDIO_PAN_LEFT && g_ring[1].pan == SAT_AUDIO_PAN_RIGHT);
    CHECK(!g_ring[0].paused && !g_ring[1].paused);              /* resumed together, once primed */
    CHECK(g_stream.music_wanted == 1u);
    CHECK(g_ring[0].fill_frames() == g_ring[1].fill_frames());
    /* full segments (4096 frames in S16, 8192 in S8) until one more would not fit */
    const uint32_t left = SAT_DISC_MUSIC_RING_FRAMES - g_ring[0].fill_frames();
    const uint32_t seg_frames = kMusicBytes >= 16384u ? 16384u / (2u * sample_bytes) : 0u;
    CHECK(left < seg_frames);
    CHECK(g_ring[0].rejected == 0u && g_ring[1].rejected == 0u);
    check_music_stream(sample_bytes);
    CHECK(g_stream.stats.music_segments == g_drive.begins);
    CHECK(g_drive.busy_hits == 0u);
    uint32_t under = 9u, refills = 9u, buffered = 0u;
    sat_disc_stream_music_stats(&g_stream, &under, &refills, &buffered);
    CHECK(under == 0u && buffered == g_ring[0].fill_frames());
}

void test_music_loops_without_underrun(uint32_t sample_bytes) {
    start_stream();
    g_drive.latency = 3u;
    CHECK(sat_disc_stream_music_start(&g_stream, &g_rings, &g_music_file, kMusicBytes, 22050u, sample_bytes) == SAT_OK);
    const uint32_t start_loops = g_stream.stats.music_loops;
    /* a 22050 Hz track drains 367 frames per 60 Hz field; run well past two loops */
    const uint32_t track_frames = kMusicBytes / (2u * sample_bytes);
    const uint32_t ticks = 3u * track_frames / 367u;
    for (uint32_t i = 0; i < ticks; ++i) tick(367u);
    CHECK(g_stream.stats.music_loops >= start_loops + 2u);
    CHECK(g_ring[0].underruns == 0u && g_ring[1].underruns == 0u);
    CHECK(g_drive.busy_hits == 0u);
    CHECK(g_ring[0].rejected == 0u && g_ring[1].rejected == 0u);
    check_music_stream(sample_bytes);
    CHECK(g_stream.stats.music_min_buffered > 0u);
}

void test_data_alone() {
    start_stream();
    g_drive.latency = 2u;
    sat_disc_job_t job = data_job();
    CHECK(sat_disc_stream_queue(&g_stream, &job) == SAT_OK);
    CHECK(sat_disc_stream_busy(&g_stream) == 1u);
    for (uint32_t i = 0; i < 100u && sat_disc_stream_busy(&g_stream); ++i) (void)sat_disc_stream_service(&g_stream);
    CHECK(sat_disc_stream_busy(&g_stream) == 0u);
    CHECK(g_sink.begin_calls == 1u && g_sink.done_calls == 1u && g_sink.done_result == SAT_OK);
    CHECK(g_sink.got.size() == kDataBytes);                     /* the last chunk is clipped to the file */
    for (uint32_t i = 0; i < kDataBytes; ++i) CHECK(g_sink.got[i] == disc_byte(static_cast<uint64_t>(kDataLba) * kSector + i));
    CHECK(g_sink.max_chunk == SAT_DISC_STREAM_SEGMENT_BYTES);
    CHECK(g_sink.chunk_calls == 3u);                            /* 8 + 8 + 4 sectors */
    CHECK(g_stream.stats.jobs_done == 1u && g_stream.stats.bytes == kDataBytes && g_stream.stats.segments == 3u);
    CHECK(g_stream.stats.last_error == SAT_OK);
    CHECK(g_drive.busy_hits == 0u);
}

void test_data_extent() {
    /* sub-range: 5 sectors from sector 3, the last one short */
    start_stream();
    const uint32_t off = 3u * kSector, bytes = 4u * kSector + 100u;
    sat_disc_job_t job = data_job(off, bytes);
    CHECK(sat_disc_stream_queue(&g_stream, &job) == SAT_OK);
    for (uint32_t i = 0; i < 100u && sat_disc_stream_busy(&g_stream); ++i) (void)sat_disc_stream_service(&g_stream);
    CHECK(g_sink.got.size() == bytes && g_sink.done_result == SAT_OK);
    for (uint32_t i = 0; i < bytes; ++i) CHECK(g_sink.got[i] == disc_byte(static_cast<uint64_t>(kDataLba) * kSector + off + i));
    CHECK(g_drive.begin_lbas.size() == 1u && g_drive.begin_lbas[0] == kDataLba + 3u);
}

void test_job_without_begin_hook() {
    start_stream();
    sat_disc_job_t job{};
    job.ops = &k_ops_no_begin;
    job.extent = sat_disc_extent_t{&g_data_file, 0u, 2u * kSector};
    CHECK(sat_disc_stream_queue(&g_stream, &job) == SAT_OK);
    CHECK(sat_disc_stream_finish(&g_stream) == SAT_OK);
    CHECK(g_sink.begin_calls == 0u && g_sink.got.size() == 2u * kSector && g_sink.done_result == SAT_OK);
    /* no hook and no file is not a job */
    sat_disc_job_t bad{};
    bad.ops = &k_ops_no_begin;
    CHECK(sat_disc_stream_queue(&g_stream, &bad) == SAT_ERR_INVALID_ARG);
}

void test_skip_job() {
    start_stream();
    sat_disc_job_t job = data_job();
    g_sink.extent = sat_disc_extent_t{};                         /* file NULL: already resident */
    CHECK(sat_disc_stream_queue(&g_stream, &job) == SAT_OK);
    (void)sat_disc_stream_service(&g_stream);
    CHECK(sat_disc_stream_busy(&g_stream) == 0u);
    CHECK(g_drive.begins == 0u && g_sink.chunk_calls == 0u);
    CHECK(g_sink.done_calls == 1u && g_sink.done_result == SAT_OK);
    CHECK(g_stream.stats.jobs_done == 1u);
}

void test_bad_job_does_not_jam_queue() {
    start_stream();
    sat_disc_job_t bad = data_job();
    g_sink.begin_result = SAT_ERR_INVALID_ARG;
    CHECK(sat_disc_stream_queue(&g_stream, &bad) == SAT_OK);
    (void)sat_disc_stream_service(&g_stream);                    /* drops it ... */
    CHECK(g_sink.done_calls == 1u && g_sink.done_result == SAT_ERR_INVALID_ARG);
    CHECK(g_stream.stats.last_error == SAT_ERR_INVALID_ARG);
    /* ... and the next one still runs */
    g_sink.begin_result = SAT_OK;
    sat_disc_job_t good = data_job();
    CHECK(sat_disc_stream_queue(&g_stream, &good) == SAT_OK);
    for (uint32_t i = 0; i < 100u && sat_disc_stream_busy(&g_stream); ++i) (void)sat_disc_stream_service(&g_stream);
    CHECK(g_sink.done_calls == 2u && g_sink.done_result == SAT_OK && g_sink.got.size() == kDataBytes);
    CHECK(g_stream.stats.jobs_done == 1u);
    CHECK(sat_disc_stream_finish(&g_stream) == SAT_OK);          /* idle: nothing to wait for */
}

void test_chunk_error_drops_job() {
    start_stream();
    g_sink.chunk_fail_at = 2u;
    g_sink.chunk_result = SAT_ERR_IO;
    sat_disc_job_t job = data_job();
    CHECK(sat_disc_stream_queue(&g_stream, &job) == SAT_OK);
    for (uint32_t i = 0; i < 100u && sat_disc_stream_busy(&g_stream); ++i) (void)sat_disc_stream_service(&g_stream);
    CHECK(sat_disc_stream_busy(&g_stream) == 0u);
    CHECK(g_sink.done_calls == 1u && g_sink.done_result == SAT_ERR_IO);
    CHECK(g_sink.chunk_calls == 2u);                             /* nothing is read for a dropped job */
    CHECK(g_stream.stats.jobs_done == 0u && g_stream.stats.last_error == SAT_ERR_IO);
}

void test_transport_error_drops_job() {
    start_stream();
    g_drive.latency = 1u;
    g_drive.fail_poll_after = 2;                                 /* the second poll fails */
    sat_disc_job_t job = data_job();
    CHECK(sat_disc_stream_queue(&g_stream, &job) == SAT_OK);
    sat_result_t seen = SAT_OK;
    for (uint32_t i = 0; i < 20u && sat_disc_stream_busy(&g_stream); ++i) {
        const sat_result_t r = sat_disc_stream_service(&g_stream);
        if (r != SAT_OK) seen = r;
    }
    CHECK(seen == SAT_ERR_TIMEOUT);
    CHECK(sat_disc_stream_busy(&g_stream) == 0u);
    CHECK(g_sink.done_calls == 1u && g_sink.done_result == SAT_ERR_TIMEOUT);
    CHECK(g_stream.stats.last_error == SAT_ERR_TIMEOUT);
}

void test_queue_capacity_and_order() {
    start_stream();
    sat_disc_job_t job = data_job(0u, kSector);
    for (uint32_t i = 0; i < SAT_DISC_STREAM_QUEUE; ++i) CHECK(sat_disc_stream_queue(&g_stream, &job) == SAT_OK);
    CHECK(sat_disc_stream_queue(&g_stream, &job) == SAT_ERR_CAPACITY);
    sat_disc_job_t none{};
    CHECK(sat_disc_stream_queue(&g_stream, &none) == SAT_ERR_INVALID_ARG);
    CHECK(sat_disc_stream_finish(&g_stream) == SAT_OK);
    CHECK(g_sink.done_calls == SAT_DISC_STREAM_QUEUE && g_stream.stats.jobs_done == SAT_DISC_STREAM_QUEUE);
    CHECK(g_stream.stats.waits == 1u);
    /* the ring buffer of the queue wraps: another round works */
    for (uint32_t i = 0; i < SAT_DISC_STREAM_QUEUE; ++i) CHECK(sat_disc_stream_queue(&g_stream, &job) == SAT_OK);
    CHECK(sat_disc_stream_finish(&g_stream) == SAT_OK);
    CHECK(g_sink.done_calls == 2u * SAT_DISC_STREAM_QUEUE);
}

/* Music and data share the drive: one segment in flight, music first, data only while the rings are
 * above the reserve, and the music never starves. */
void test_data_with_music(uint32_t sample_bytes, uint32_t drain_per_tick, uint32_t latency) {
    start_stream();
    g_drive.latency = latency;
    CHECK(sat_disc_stream_music_start(&g_stream, &g_rings, &g_music_file, kMusicBytes, 22050u, sample_bytes) == SAT_OK);
    sat_disc_job_t job = data_job();
    CHECK(sat_disc_stream_queue(&g_stream, &job) == SAT_OK);
    uint32_t ticks = 0;
    while (sat_disc_stream_busy(&g_stream) && ticks < 4000u) {
        tick(drain_per_tick);
        ++ticks;
    }
    CHECK(sat_disc_stream_busy(&g_stream) == 0u);
    CHECK(g_sink.done_result == SAT_OK && g_sink.got.size() == kDataBytes);
    for (uint32_t i = 0; i < kDataBytes; ++i) CHECK(g_sink.got[i] == disc_byte(static_cast<uint64_t>(kDataLba) * kSector + i));
    CHECK(g_drive.busy_hits == 0u);                              /* never two segments at once */
    CHECK(g_ring[0].underruns == 0u && g_ring[1].underruns == 0u);
    CHECK(g_ring[0].rejected == 0u && g_ring[1].rejected == 0u);
    check_music_stream(sample_bytes);
    /* data segments were only started while the rings held the reserve */
    CHECK(g_drive.min_fill_at_data_begin >= SAT_DISC_MUSIC_RESERVE_FRAMES);
    CHECK(g_stream.stats.music_min_buffered > 0u);
    /* the read order shows the music kept going between the data segments */
    CHECK(g_stream.stats.music_segments >= 1u);
}

void test_data_deferred_below_reserve() {
    start_stream();
    g_drive.latency = 2u;
    CHECK(sat_disc_stream_music_start(&g_stream, &g_rings, &g_music_file, kMusicBytes, 22050u, 2u) == SAT_OK);
    /* drain the rings to well under the reserve without servicing, then queue a data job */
    consume(g_ring[0].fill_frames() - 1000u);
    sat_disc_job_t job = data_job();
    CHECK(sat_disc_stream_queue(&g_stream, &job) == SAT_OK);
    const uint32_t data_begins_before = g_drive.begins;
    (void)sat_disc_stream_service(&g_stream);                    /* the music takes the drive first */
    CHECK(g_drive.begins == data_begins_before + 1u);
    CHECK(g_drive.begin_lbas.back() < kDataLba);                 /* a music segment, not the data */
    CHECK(g_sink.chunk_calls == 0u);
    /* hold the rings under the reserve: the data job waits, and is counted */
    g_stream.music_reserve = SAT_DISC_MUSIC_RING_FRAMES + 1u;    /* unreachable: data never starts */
    for (uint32_t i = 0; i < 40u; ++i) tick(0u);
    CHECK(g_stream.stats.data_deferred > 0u);
    CHECK(g_sink.chunk_calls == 0u && sat_disc_stream_busy(&g_stream) == 1u);
    for (uint32_t b : g_drive.begin_lbas) CHECK(b < kDataLba);
    /* lower the reserve and it goes through */
    g_stream.music_reserve = SAT_DISC_MUSIC_RESERVE_FRAMES;
    for (uint32_t i = 0; i < 400u && sat_disc_stream_busy(&g_stream); ++i) tick(300u);
    CHECK(sat_disc_stream_busy(&g_stream) == 0u && g_sink.got.size() == kDataBytes);
    CHECK(g_ring[0].underruns == 0u);
}

uint32_t g_pump_calls = 0;
void pump_consume(void*) {
    ++g_pump_calls;
    consume(300u);
}

void test_finish_pumps_and_keeps_music_fed() {
    start_stream();
    g_drive.latency = 0u;
    CHECK(sat_disc_stream_music_start(&g_stream, &g_rings, &g_music_file, kMusicBytes, 22050u, 2u) == SAT_OK);
    g_pump_calls = 0;
    sat_disc_stream_set_pump(&g_stream, pump_consume, nullptr);
    sat_disc_job_t job = data_job();
    CHECK(sat_disc_stream_queue(&g_stream, &job) == SAT_OK);
    CHECK(sat_disc_stream_finish(&g_stream) == SAT_OK);
    CHECK(g_pump_calls >= 3u);                                   /* at least once per data segment */
    CHECK(g_sink.got.size() == kDataBytes && g_stream.stats.waits == 1u);
    CHECK(g_ring[0].underruns == 0u && g_ring[1].underruns == 0u);
    check_music_stream(2u);
}

void test_music_argument_checks() {
    start_stream();
    const sat_cdfs_file_t small = make_file(kMusicLba, 4096u);
    CHECK(sat_disc_stream_music_start(&g_stream, &g_rings, &small, 4096u, 22050u, 2u) == SAT_ERR_INVALID_ARG);        /* under a segment */
    CHECK(sat_disc_stream_music_start(&g_stream, &g_rings, &g_music_file, kMusicBytes, 22050u, 3u) == SAT_ERR_INVALID_ARG);
    CHECK(sat_disc_stream_music_start(&g_stream, &g_rings, &g_music_file, kMusicBytes + 1u, 22050u, 2u) == SAT_ERR_INVALID_ARG);  /* odd frame */
    CHECK(sat_disc_stream_music_start(&g_stream, &g_rings, &g_music_file, kMusicBytes + 4u * kSector, 22050u, 2u) == SAT_ERR_INVALID_ARG);  /* past the file */
    CHECK(sat_disc_stream_music_start(&g_stream, nullptr, &g_music_file, kMusicBytes, 22050u, 2u) == SAT_ERR_INVALID_ARG);
    CHECK(g_ring_opens == 0u && g_stream.music_wanted == 0u);
    CHECK(sat_disc_stream_init(nullptr, nullptr) == SAT_ERR_INVALID_ARG);
    uint32_t u = 7u, r = 7u, b = 7u;
    sat_disc_stream_music_stats(&g_stream, &u, &r, &b);          /* no music: zeros */
    CHECK(u == 0u && r == 0u && b == 0u);
}

}  // namespace

int main() {
    test_music_prime(2u);
    test_music_prime(1u);
    test_music_loops_without_underrun(2u);
    test_music_loops_without_underrun(1u);
    test_data_alone();
    test_data_extent();
    test_job_without_begin_hook();
    test_skip_job();
    test_bad_job_does_not_jam_queue();
    test_chunk_error_drops_job();
    test_transport_error_drops_job();
    test_queue_capacity_and_order();
    test_data_with_music(2u, 367u, 2u);
    test_data_with_music(1u, 367u, 4u);
    test_data_with_music(2u, 700u, 1u);
    test_data_deferred_below_reserve();
    test_finish_pumps_and_keeps_music_fed();
    test_music_argument_checks();
    std::puts("test_disc_stream: ok");
    return 0;
}
