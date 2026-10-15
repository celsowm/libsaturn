/* sat_music streaming through the disc scheduler (sat_music_use_disc_stream): the real music.cpp,
 * disc_stream.cpp and SCSP stream rings against a fake drive that delivers a segment after a
 * settable number of polls. The rings are drained the way the SCSP drains them, and what they
 * played is compared with the track decoded straight from its disc image.
 *
 * What it pins: bytes are exact for every format across segment boundaries (a 16 KiB segment
 * never lines up with a frame count or an ADPCM block) and across the loop; in steady state
 * nothing waits for the drive (no stream_finish, no asset read); seek, stop and close drop the
 * segment in flight; one disc track at a time; non-disc tracks keep the old path. */
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "saturn/asset.h"
#include "saturn/audio.h"
#include "saturn/disc_stream.h"
#include "src/audio/playback/ima_adpcm.hpp"
#include "src/audio/streaming/runtime.hpp"
#include "src/hal/scsp/scsp.hpp"

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

namespace saturn::core { void music_runtime_reset(); }

namespace {

constexpr uint32_t kSector = 2048u;
namespace codec = saturn::core::audio::ima_adpcm;

/* ---- fake tracks on a fake disc ----------------------------------------------------------------- */

struct Track {
    const char* path;           /* logical asset path */
    const char* source;         /* VFS path, nullptr: not a file the disc scheduler can read */
    uint8_t format;             /* SAT_AUDIO_* */
    uint8_t channels;
    uint32_t sample_count;
    uint32_t lba;
    std::vector<uint8_t> image;   /* the file's bytes */
    sat_cdfs_file_t file{};
    uint32_t frame_bytes() const {
        if (format == SAT_AUDIO_IMA_ADPCM) return 0u;
        return (format == SAT_AUDIO_PCM_S8 ? 1u : 2u) * channels;
    }
};

std::vector<Track> g_tracks;

uint8_t pattern(uint64_t i) { return static_cast<uint8_t>(i * 131u + (i >> 7) * 29u + (i >> 15) * 3u + 5u); }

void add_pcm(const char* path, const char* source, uint8_t format, uint8_t channels, uint32_t frames, uint32_t lba,
             uint32_t extra_bytes = 0u) {
    Track t{};
    t.path = path;
    t.source = source;
    t.format = format;
    t.channels = channels;
    t.sample_count = frames;
    t.lba = lba;
    const uint32_t bytes = frames * t.frame_bytes() + extra_bytes;
    for (uint32_t i = 0; i < bytes; ++i) t.image.push_back(pattern(i + lba * 7u));
    t.file.extent_lba = lba;
    t.file.size = bytes;
    g_tracks.push_back(t);
}

void add_adpcm(const char* path, const char* source, uint8_t channels, uint32_t frames, uint32_t lba) {
    Track t{};
    t.path = path;
    t.source = source;
    t.format = SAT_AUDIO_IMA_ADPCM;
    t.channels = channels;
    t.sample_count = frames;
    t.lba = lba;
    const uint32_t bb = codec::block_bytes(channels);
    const uint32_t blocks = (frames + codec::kBlockFrames - 1u) / codec::kBlockFrames;
    for (uint32_t b = 0; b < blocks; ++b) {
        for (uint32_t i = 0; i < bb; ++i) {
            uint8_t v = pattern(b * 977u + i + lba);
            /* the step index byte of each channel header must stay a valid index */
            if (i < 4u * channels && (i % 4u) == 2u) v = static_cast<uint8_t>(v % 80u);
            t.image.push_back(v);
        }
    }
    t.file.extent_lba = lba;
    t.file.size = static_cast<uint32_t>(t.image.size());
    g_tracks.push_back(t);
}

Track* find_track(const char* path) {
    for (Track& t : g_tracks) if (std::strcmp(t.path, path) == 0) return &t;
    return nullptr;
}

/* Expected per-channel bytes of frames [first, first + n) of the looping track. */
std::vector<uint8_t> expected(const Track& t, uint32_t channel, uint64_t first, uint64_t n) {
    std::vector<uint8_t> out;
    if (t.format == SAT_AUDIO_IMA_ADPCM) {
        const uint32_t bb = codec::block_bytes(t.channels);
        const uint32_t blocks = (t.sample_count + codec::kBlockFrames - 1u) / codec::kBlockFrames;
        std::vector<uint8_t> planar(blocks * codec::kChannelBytes);
        uint8_t tmp[2u * codec::kChannelBytes];
        for (uint32_t b = 0; b < blocks; ++b) {
            CHECK(codec::decode_block(t.image.data() + b * bb, t.channels, tmp));
            std::memcpy(planar.data() + b * codec::kChannelBytes, tmp + channel * codec::kChannelBytes,
                        codec::kChannelBytes);
        }
        for (uint64_t f = first; f < first + n; ++f) {
            const uint32_t frame = static_cast<uint32_t>(f % t.sample_count);
            out.push_back(planar[(frame / codec::kBlockFrames) * codec::kChannelBytes + (frame % codec::kBlockFrames) * 2u]);
            out.push_back(planar[(frame / codec::kBlockFrames) * codec::kChannelBytes + (frame % codec::kBlockFrames) * 2u + 1u]);
        }
        return out;
    }
    const uint32_t sample_bytes = t.format == SAT_AUDIO_PCM_S8 ? 1u : 2u;
    for (uint64_t f = first; f < first + n; ++f) {
        const uint32_t frame = static_cast<uint32_t>(f % t.sample_count);
        for (uint32_t b = 0; b < sample_bytes; ++b) {
            out.push_back(t.image[(frame * t.channels + channel) * sample_bytes + b]);
        }
    }
    return out;
}

/* ---- fake CD Block stream ----------------------------------------------------------------------- */

struct Drive {
    bool open = false;
    uint32_t lba = 0, count = 0;
    uint8_t* dest = nullptr;
    uint32_t latency = 3;
    uint32_t polls_left = 0;
    uint32_t moved = 0;
    uint32_t max_poll_sectors = 0;
    uint32_t begins = 0, finishes = 0, polls = 0;
    int fail_poll_at = -1;
    std::vector<uint32_t> begin_lbas;
};
Drive g_drive;
uint32_t g_asset_reads = 0;
uint16_t g_frc = 0;

uint32_t deliver_sectors(uint32_t n = 0u) {
    const uint32_t left = g_drive.count - g_drive.moved;
    const uint32_t take = n == 0u || n > left ? left : n;
    for (uint32_t i = g_drive.moved * kSector; i < (g_drive.moved + take) * kSector; ++i) {
        const uint64_t abs = static_cast<uint64_t>(g_drive.lba) * kSector + i;
        uint8_t v = 0;
        for (const Track& t : g_tracks) {
            const uint64_t base = static_cast<uint64_t>(t.lba) * kSector;
            if (abs >= base && abs < base + t.image.size()) { v = t.image[abs - base]; break; }
        }
        g_drive.dest[i] = v;
    }
    g_drive.moved += take;
    return take;
}

}  // namespace

extern "C" uint16_t sat_time_frc(void) { return g_frc += 3u; }
extern "C" uint8_t sat_audio_is_initialized(void) { return 1u; }

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
    if (g_drive.open) return SAT_ERR_BUSY;
    CHECK(sector_count != 0u && sector_count <= SAT_CD_STREAM_MAX_SECTORS);
    CHECK((reinterpret_cast<uintptr_t>(destination) & 1u) == 0u);
    g_drive.open = true;
    g_drive.lba = lba;
    g_drive.count = sector_count;
    g_drive.dest = static_cast<uint8_t*>(destination);
    g_drive.polls_left = g_drive.latency;
    g_drive.moved = 0u;
    ++g_drive.begins;
    g_drive.begin_lbas.push_back(lba);
    return SAT_OK;
}

extern "C" sat_result_t sat_cd_stream_poll(sat_cd_block_t*, uint32_t max_sectors, uint32_t* out_remaining) {
    ++g_drive.polls;
    if (!g_drive.open) {
        if (out_remaining) *out_remaining = 0u;
        return SAT_OK;
    }
    if (g_drive.fail_poll_at > 0 && g_drive.polls == static_cast<uint32_t>(g_drive.fail_poll_at)) {
        g_drive.open = false;
        return SAT_ERR_TIMEOUT;
    }
    if (g_drive.polls_left > 0u) {
        --g_drive.polls_left;
        if (out_remaining) *out_remaining = g_drive.count;
        return SAT_OK;
    }
    const uint32_t took = deliver_sectors(max_sectors);
    if (took > g_drive.max_poll_sectors) g_drive.max_poll_sectors = took;
    if (g_drive.moved < g_drive.count) {
        if (out_remaining) *out_remaining = g_drive.count - g_drive.moved;
        return SAT_OK;
    }
    g_drive.open = false;
    if (out_remaining) *out_remaining = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_cd_stream_finish(sat_cd_block_t*) {
    ++g_drive.finishes;
    if (!g_drive.open) return SAT_OK;
    deliver_sectors();
    g_drive.open = false;
    return SAT_OK;
}

/* ---- fake asset registry and VFS ---------------------------------------------------------------- */

extern "C" sat_result_t sat_asset_open(const char* path, sat_asset_t* out_asset) {
    for (size_t i = 0; i < g_tracks.size(); ++i) {
        if (std::strcmp(g_tracks[i].path, path) == 0) {
            out_asset->slot = static_cast<uint16_t>(i);
            out_asset->generation = 1u;
            return SAT_OK;
        }
    }
    return SAT_ERR_NOT_FOUND;
}

extern "C" sat_result_t sat_asset_info(sat_asset_t asset, sat_asset_info_t* out_info) {
    const Track& t = g_tracks[asset.slot];
    *out_info = {};
    out_info->kind = SAT_ASSET_STREAM;
    out_info->source_path = t.source;
    out_info->size = t.format == SAT_AUDIO_IMA_ADPCM ? static_cast<uint32_t>(t.image.size()) : t.sample_count * t.frame_bytes();
    out_info->sample_rate = 22050u;
    out_info->sample_count = t.sample_count;
    out_info->channels = t.channels;
    out_info->format = t.format;
    return SAT_OK;
}

extern "C" sat_result_t sat_asset_read_at(const char* path, uint32_t offset, void* destination, uint32_t bytes,
                                          uint32_t* out_read) {
    ++g_asset_reads;
    const Track* t = find_track(path);
    CHECK(t != nullptr);
    const uint32_t size = static_cast<uint32_t>(t->image.size());
    const uint32_t count = offset >= size ? 0u : (bytes < size - offset ? bytes : size - offset);
    std::memcpy(destination, t->image.data() + offset, count);
    *out_read = count;
    return SAT_OK;
}

extern "C" sat_result_t sat_cdfs_source_file(const char* source_path, const sat_cdfs_file_t** out_file) {
    for (const Track& t : g_tracks) {
        if (t.source != nullptr && std::strcmp(t.source, source_path) == 0) {
            if (t.source[0] == 'x') return SAT_ERR_UNSUPPORTED;     /* registered with another reader */
            if (t.source[0] == 'r') return SAT_ERR_NOT_FOUND;       /* not on the disc */
            *out_file = &t.file;
            return SAT_OK;
        }
    }
    return SAT_ERR_NOT_FOUND;
}

namespace saturn::hal::scsp {

bool upload(uint32_t, const void*, uint32_t) { return true; }
bool configure_slot(uint8_t, const SlotConfig&) { return true; }
void key_on(uint8_t) {}
void arm_key_on(uint8_t) {}
void execute_key_transitions() {}
void key_off(uint8_t) {}
bool read_current_sample_block(uint8_t, uint8_t* out_block) {
    if (out_block == nullptr) return false;
    *out_block = 0u;
    return true;
}
uint8_t encode_pan(int16_t) { return 0u; }
void set_slot_level_pan(uint8_t, uint8_t, uint8_t, uint8_t) {}

}  // namespace saturn::hal::scsp

namespace {

/* ---- the SCSP: drains the rings of the open music, recording what it played --------------------- */

struct Played {
    std::vector<uint8_t> ch[2];
    uint32_t underruns = 0;
    void clear() { ch[0].clear(); ch[1].clear(); }
};
Played g_played;

/* Streams of the music in registry order: left, then right. */
uint32_t used_streams(saturn::core::AudioStreamSlot** out) {
    uint32_t n = 0;
    for (auto& s : saturn::core::g_audio_streams.slots) if (s.used != 0u) out[n++] = &s;
    return n;
}

void scsp_consume(uint32_t frames) {
    saturn::core::AudioStreamSlot* streams[4];
    const uint32_t n = used_streams(streams);
    for (uint32_t c = 0; c < n; ++c) {
        saturn::core::AudioStreamRing& ring = streams[c]->ring;
        if (ring.paused != 0u) continue;
        uint32_t take = frames;
        if (take > ring.buffered_frames) {
            take = ring.buffered_frames;
            ++g_played.underruns;
        }
        std::vector<uint8_t> tmp(take * ring.frame_bytes);
        saturn::core::audio_stream_peek(ring, 0u, tmp.data(), take);
        saturn::core::audio_stream_consume(ring, take);
        g_played.ch[c].insert(g_played.ch[c].end(), tmp.begin(), tmp.end());
    }
}

uint32_t g_max_update_frames = 0;       /* most frames one sat_music_update put in a ring */
bool g_overlap = false;                 /* a segment was read while another was being delivered */

sat_disc_stream_t g_disc;
sat_music_t g_music;

void fresh(uint32_t latency = 3u) {
    saturn::core::audio_stream_registry_reset(saturn::core::g_audio_streams);
    saturn::core::music_runtime_reset();
    g_drive = Drive{};
    g_drive.latency = latency;
    g_max_update_frames = 0;
    g_overlap = false;
    g_asset_reads = 0;
    g_played = Played{};
    CHECK(sat_disc_stream_init(&g_disc, reinterpret_cast<sat_cd_block_t*>(&g_disc)) == SAT_OK);
    CHECK(sat_music_use_disc_stream(&g_disc) == SAT_OK);
}

uint32_t stream_buffered(uint32_t c);

/* One 60 Hz field: the SCSP drains 22.05 kHz / 60, then the game updates. */
sat_result_t field(const Track& t) {
    (void)t;
    scsp_consume(367u);
    const uint32_t before = stream_buffered(0);
    const sat_result_t r = sat_music_update(g_music);
    const uint32_t after = stream_buffered(0);
    if (after > before && after - before > g_max_update_frames) g_max_update_frames = after - before;
    if (g_disc.deliv_active != 0u && g_disc.in_flight != 0u) g_overlap = true;
    return r;
}

/* The played history of every channel equals the track from `first_frame` on. */
void check_played(const Track& t, uint64_t first_frame) {
    const uint32_t sample_bytes = t.format == SAT_AUDIO_PCM_S8 ? 1u : 2u;
    for (uint32_t c = 0; c < t.channels; ++c) {
        const uint64_t frames = g_played.ch[c].size() / sample_bytes;
        CHECK(frames > 0u);
        CHECK(g_played.ch[c] == expected(t, c, first_frame, frames));
    }
}

uint32_t stream_buffered(uint32_t c) {
    saturn::core::AudioStreamSlot* streams[4];
    used_streams(streams);
    return streams[c]->ring.buffered_frames;
}

/* ---- tests -------------------------------------------------------------------------------------- */

void steady_state(const char* path, uint32_t fields, uint32_t latency = 3u) {
    fresh(latency);
    Track* t = find_track(path);
    CHECK(sat_music_open(&g_music, path) == SAT_OK);
    sat_music_info_t info{};
    CHECK(sat_music_info(g_music, &info) == SAT_OK && info.disc_streamed == 1u);
    CHECK(sat_music_play(g_music) == SAT_OK);
    /* play reads nothing and waits for nothing: the rings fill over the next fields and the streams
     * are released together once they are full */
    CHECK(g_asset_reads == 0u && g_drive.begins == 0u && stream_buffered(0) == 0u);
    CHECK(sat_music_info(g_music, &info) == SAT_OK && info.starting == 1u && info.playing == 1u);
    uint32_t start_fields = 0u;
    while (info.starting != 0u) {
        CHECK(start_fields++ < 400u);
        CHECK(field(*t) == SAT_OK);
        CHECK(sat_music_info(g_music, &info) == SAT_OK);
    }
    CHECK(g_played.underruns == 0u && g_played.ch[0].empty());     /* nothing was played while filling */
    CHECK(stream_buffered(0) > 24000u);                             /* started from a full ring */
    CHECK(g_drive.finishes == 0u);                                  /* not even the start waited for the drive */
    CHECK(g_asset_reads == 0u);                         /* the legacy path is never taken */
    const uint32_t finishes_after_prime = g_drive.finishes;
    const uint32_t begins_after_prime = g_drive.begins;
    uint32_t min_fill = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < fields; ++i) {
        CHECK(field(*t) == SAT_OK);
        if (stream_buffered(0) < min_fill) min_fill = stream_buffered(0);
    }
    if (g_played.underruns) std::fprintf(stderr, "underruns in %s: %u (min fill %u)\n", path, g_played.underruns, min_fill);
    CHECK(g_played.underruns == 0u);
    CHECK(g_drive.finishes == finishes_after_prime);    /* nothing waited for the drive */
    CHECK(g_drive.begins > begins_after_prime);         /* but segments did keep arriving */
    CHECK(g_asset_reads == 0u);
    CHECK(min_fill >= 4096u);                           /* never down to the SCSP half */
    /* a frame pays for a slice, not a segment: 4 KiB of PCM, or one ADPCM block */
    const uint32_t slice_frames = t->format == SAT_AUDIO_IMA_ADPCM ? codec::kBlockFrames : 4096u / t->frame_bytes();
    CHECK(g_max_update_frames > 0u && g_max_update_frames <= slice_frames);
    CHECK(g_drive.max_poll_sectors <= SAT_DISC_STREAM_SEGMENT_SECTORS);
    if (t->format != SAT_AUDIO_IMA_ADPCM && t->frame_bytes() >= 4u) CHECK(g_overlap);
    sat_audio_stream_stats_t st{};
    CHECK(sat_music_stats(g_music, &st) == SAT_OK && st.underrun_count == 0u && st.rejected_write_count == 0u);
    /* run past a loop at least */
    const uint64_t played = g_played.ch[0].size() / (t->format == SAT_AUDIO_PCM_S8 ? 1u : 2u);
    CHECK(played > t->sample_count);
    check_played(*t, 0u);
    CHECK(sat_music_close(g_music) == SAT_OK);
    CHECK(g_disc.music_ops == nullptr && g_disc.music_wanted == 0u);
}

void test_seek(const char* path, uint32_t frame) {
    fresh();
    Track* t = find_track(path);
    CHECK(sat_music_open(&g_music, path) == SAT_OK);
    CHECK(sat_music_play(g_music) == SAT_OK);
    for (int i = 0; i < 30; ++i) CHECK(field(*t) == SAT_OK);
    /* mid-delivery: a segment is half in the rings, the next in flight or waiting */
    for (int i = 0; i < 400 && g_disc.deliv_active == 0u; ++i) CHECK(field(*t) == SAT_OK);
    CHECK(g_disc.deliv_active != 0u);
    CHECK(sat_music_seek(g_music, frame) == SAT_OK && sat_music_is_playing(g_music) != 0u);
    CHECK(g_disc.ready == 0u);
    g_played.clear();
    for (int i = 0; i < 400; ++i) CHECK(field(*t) == SAT_OK);
    CHECK(g_played.underruns == 0u);
    check_played(*t, frame);
    /* past the end of the track is refused */
    CHECK(sat_music_seek(g_music, t->sample_count) == SAT_ERR_INVALID_ARG);
    /* a paused track only moves its cursor: no drive traffic until play */
    CHECK(sat_music_pause(g_music) == SAT_OK);
    const uint32_t begins = g_drive.begins;
    CHECK(sat_music_seek(g_music, 5u) == SAT_OK && sat_music_is_playing(g_music) == 0u);
    CHECK(g_drive.begins == begins);
    g_played.clear();
    CHECK(sat_music_play(g_music) == SAT_OK);
    for (int i = 0; i < 100; ++i) CHECK(field(*t) == SAT_OK);
    check_played(*t, 5u);
    /* stop rewinds to the start */
    CHECK(sat_music_stop(g_music) == SAT_OK && sat_music_is_playing(g_music) == 0u);
    g_played.clear();
    CHECK(sat_music_play(g_music) == SAT_OK);
    for (int i = 0; i < 100; ++i) CHECK(field(*t) == SAT_OK);
    check_played(*t, 0u);
    CHECK(sat_music_close(g_music) == SAT_OK);
}

/* A seek while a segment is in flight must neither deliver it nor leave the drive wedged. */
void test_seek_aborts_flight() {
    fresh(1000u);                                     /* a drive too slow to ever finish by polling */
    Track* t = find_track("music/s16-stereo");
    CHECK(sat_music_open(&g_music, t->path) == SAT_OK);
    CHECK(sat_music_play(g_music) == SAT_OK);
    for (int i = 0; i < 400; ++i) CHECK(field(*t) == SAT_OK);   /* drains the rings, one segment in flight */
    CHECK(g_disc.in_flight == 2u);
    const uint32_t finishes = g_drive.finishes;
    CHECK(sat_music_seek(g_music, 3000u) == SAT_OK);
    CHECK(g_drive.finishes > finishes);               /* the abort waited for the flight, once */
    CHECK(!g_drive.open || g_disc.in_flight == 0u);
    g_played.clear();
    for (int i = 0; i < 200; ++i) CHECK(sat_music_update(g_music) == SAT_OK);
    CHECK(sat_music_close(g_music) == SAT_OK);
}

void test_pause_resume() {
    fresh();
    Track* t = find_track("music/s16-stereo");
    CHECK(sat_music_open(&g_music, t->path) == SAT_OK);
    CHECK(sat_music_play(g_music) == SAT_OK);
    sat_music_info_t info{};
    for (int i = 0; i < 400 && (info.starting != 0u || i == 0); ++i) {
        CHECK(field(*t) == SAT_OK);
        CHECK(sat_music_info(g_music, &info) == SAT_OK);
    }
    CHECK(info.starting == 0u);
    for (int i = 0; i < 50; ++i) CHECK(field(*t) == SAT_OK);
    CHECK(sat_music_pause(g_music) == SAT_OK);
    /* a paused track consumes nothing and asks the drive for nothing more once its rings are topped up */
    const uint32_t begins_pause = g_drive.begins;
    for (int i = 0; i < 30; ++i) CHECK(field(*t) == SAT_OK);
    CHECK(g_drive.begins - begins_pause <= 1u);       /* at most the segment that was already in flight */
    CHECK(sat_music_resume(g_music) == SAT_OK && sat_music_is_playing(g_music) != 0u);
    for (int i = 0; i < 100; ++i) CHECK(field(*t) == SAT_OK);
    CHECK(g_played.underruns == 0u);
    check_played(*t, 0u);                             /* the pause left no gap or repeat */
    CHECK(sat_music_close(g_music) == SAT_OK);
}

void test_ownership() {
    fresh();
    CHECK(sat_music_open(&g_music, "music/s16-stereo") == SAT_OK);
    sat_music_t second{};
    CHECK(sat_music_open(&second, "music/s8-stereo") == SAT_ERR_BUSY);      /* one disc track at a time */
    /* a track the scheduler cannot read keeps the old path, beside the disc one */
    sat_music_t ram{};
    CHECK(sat_music_open(&ram, "music/ram") == SAT_OK);
    sat_music_info_t info{};
    CHECK(sat_music_info(ram, &info) == SAT_OK && info.disc_streamed == 0u);
    CHECK(sat_music_play(ram) == SAT_OK);
    CHECK(g_asset_reads == 0u);                                              /* play itself reads nothing */
    for (int i = 0; i < 20; ++i) CHECK(sat_music_update(ram) == SAT_OK);
    CHECK(g_asset_reads > 0u);
    CHECK(sat_music_close(ram) == SAT_OK);
    sat_music_t other{};
    CHECK(sat_music_open(&other, "music/other-reader") == SAT_OK);          /* UNSUPPORTED source: legacy */
    CHECK(sat_music_info(other, &info) == SAT_OK && info.disc_streamed == 0u);
    CHECK(sat_music_close(other) == SAT_OK);
    CHECK(sat_music_use_disc_stream(nullptr) == SAT_ERR_BUSY);              /* a disc track is open */
    CHECK(sat_music_close(g_music) == SAT_OK);
    /* the slot is free again, and so is the feed */
    CHECK(sat_music_open(&second, "music/s8-stereo") == SAT_OK);
    CHECK(sat_music_close(second) == SAT_OK);
    CHECK(sat_music_use_disc_stream(nullptr) == SAT_OK);
    CHECK(sat_music_open(&second, "music/s16-stereo") == SAT_OK);          /* unbound: old path */
    CHECK(sat_music_info(second, &info) == SAT_OK && info.disc_streamed == 0u);
    CHECK(sat_music_close(second) == SAT_OK);
}

/* Data jobs share the drive: the music keeps its rings, the job completes. */
struct Job {
    std::vector<uint8_t> got;
    uint32_t done = 0;
    sat_cdfs_file_t file{};
};
Job g_job;
sat_result_t job_chunk(void*, uint32_t, const uint8_t* data, uint32_t bytes) {
    g_job.got.insert(g_job.got.end(), data, data + bytes);
    return SAT_OK;
}
void job_done(void*, uint32_t, sat_result_t) { ++g_job.done; }
const sat_disc_job_ops_t kJobOps = {nullptr, job_chunk, job_done};

void test_data_job_beside_music() {
    fresh();
    Track* t = find_track("music/s16-stereo");
    g_job = Job{};
    g_job.file.extent_lba = 4000u;
    g_job.file.size = 150000u;
    Track data{};
    data.path = "data";
    data.lba = 4000u;
    for (uint32_t i = 0; i < g_job.file.size; ++i) data.image.push_back(pattern(i + 99u));
    g_tracks.push_back(data);
    CHECK(sat_music_open(&g_music, t->path) == SAT_OK);
    CHECK(sat_music_play(g_music) == SAT_OK);
    sat_disc_job_t job{};
    job.ops = &kJobOps;
    job.extent = sat_disc_extent_t{&g_job.file, 0u, 0u};
    CHECK(sat_disc_stream_queue(&g_disc, &job) == SAT_OK);
    const uint32_t finishes = g_drive.finishes;
    for (int i = 0; i < 1500 && g_job.done == 0u; ++i) CHECK(field(*t) == SAT_OK);
    CHECK(g_job.done == 1u && g_job.got.size() == g_job.file.size);
    for (uint32_t i = 0; i < g_job.file.size; ++i) CHECK(g_job.got[i] == pattern(i + 99u));
    CHECK(g_played.underruns == 0u);
    CHECK(g_drive.finishes == finishes);
    check_played(*t, 0u);
    g_tracks.pop_back();
    CHECK(sat_music_close(g_music) == SAT_OK);
}

void test_transport_error() {
    fresh();
    Track* t = find_track("music/s16-stereo");
    CHECK(sat_music_open(&g_music, t->path) == SAT_OK);
    CHECK(sat_music_play(g_music) == SAT_OK);
    for (int i = 0; i < 20; ++i) CHECK(field(*t) == SAT_OK);
    g_drive.fail_poll_at = static_cast<int>(g_drive.polls) + 3;
    bool saw = false;
    for (int i = 0; i < 60; ++i) {
        if (field(*t) != SAT_OK) saw = true;
    }
    CHECK(saw);                                      /* reported to the caller */
    CHECK(g_disc.stats.last_error != SAT_OK);
    for (int i = 0; i < 100; ++i) (void)field(*t);   /* and the feed carries on afterwards */
    CHECK(g_drive.begins > 5u);
    CHECK(sat_music_close(g_music) == SAT_OK);
}

/* The scheduler's own service call starts the streams (the header says either call may be used),
 * and the start cannot be missed whichever one runs. */
void test_start_through_scheduler_service() {
    fresh();
    Track* t = find_track("music/s16-stereo");
    CHECK(sat_music_open(&g_music, t->path) == SAT_OK);
    CHECK(sat_music_play(g_music) == SAT_OK);
    sat_music_info_t info{};
    int polls = 0;
    do {
        CHECK(polls++ < 600);
        CHECK(sat_disc_stream_service(&g_disc) == SAT_OK);
        CHECK(sat_music_info(g_music, &info) == SAT_OK);
    } while (info.starting != 0u);
    sat_audio_stream_stats_t st{};
    CHECK(sat_music_stats(g_music, &st) == SAT_OK && st.paused == 0u && st.buffered_frames > 24000u);
    CHECK(g_drive.finishes == 0u);
    CHECK(sat_music_close(g_music) == SAT_OK);
}

/* A stopped (or never started) track asks the drive for nothing, so a data job queued beside it
 * must not wait for the track's empty rings to reach the reserve. */
void test_stopped_music_does_not_hold_data() {
    fresh();
    Track* t = find_track("music/s16-stereo");
    g_job = Job{};
    g_job.file.extent_lba = 4000u;
    g_job.file.size = 60000u;
    Track data{};
    data.path = "data2";
    data.lba = 4000u;
    for (uint32_t i = 0; i < g_job.file.size; ++i) data.image.push_back(pattern(i + 99u));
    g_tracks.push_back(data);
    CHECK(sat_music_open(&g_music, t->path) == SAT_OK);        /* open, never played: rings empty */
    sat_disc_job_t job{};
    job.ops = &kJobOps;
    job.extent = sat_disc_extent_t{&g_job.file, 0u, 0u};
    CHECK(sat_disc_stream_queue(&g_disc, &job) == SAT_OK);
    for (int i = 0; i < 400 && g_job.done == 0u; ++i) CHECK(sat_music_update(g_music) == SAT_OK);
    CHECK(g_job.done == 1u && g_job.got.size() == g_job.file.size);
    CHECK(g_disc.stats.data_deferred == 0u);
    /* ... and while it is starting (playing, rings filling) the music keeps the drive first */
    g_job = Job{};
    g_job.file.size = 60000u;
    g_job.file.extent_lba = 4000u;
    CHECK(sat_music_play(g_music) == SAT_OK);
    CHECK(sat_disc_stream_queue(&g_disc, &job) == SAT_OK);
    sat_music_info_t info{};
    for (int i = 0; i < 1500 && g_job.done == 0u; ++i) CHECK(sat_music_update(g_music) == SAT_OK);
    CHECK(g_job.done == 1u);
    CHECK(sat_music_info(g_music, &info) == SAT_OK && info.starting == 0u);     /* the music started before the job finished */
    CHECK(g_disc.stats.music_segments >= 6u);
    g_tracks.pop_back();
    CHECK(sat_music_close(g_music) == SAT_OK);
}

}  // namespace

int main() {
    g_tracks.reserve(32);     /* tests hold Track pointers: the vector must never move */
    /* Sizes are chosen so no segment boundary lines up with a frame count or a block: 16384 bytes
     * is 4096 S16 stereo frames, and the tracks are 3.x segments and an odd tail long. */
    add_pcm("music/s16-stereo", "disc/s16-stereo", SAT_AUDIO_PCM_S16, 2u, 13038u, 100u, 5000u);   /* file longer than the track */
    add_pcm("music/s8-stereo", "disc/s8-stereo", SAT_AUDIO_PCM_S8, 2u, 26077u, 700u);
    add_pcm("music/s16-mono", "disc/s16-mono", SAT_AUDIO_PCM_S16, 1u, 21003u, 1300u);
    add_pcm("music/s8-mono", "disc/s8-mono", SAT_AUDIO_PCM_S8, 1u, 40001u, 2000u);
    add_pcm("music/short", "disc/short", SAT_AUDIO_PCM_S16, 2u, 4100u, 2900u);              /* a bit over one segment */
    add_adpcm("music/adpcm-stereo", "disc/adpcm-stereo", 2u, 9500u, 3200u);                 /* partial last block */
    add_adpcm("music/adpcm-mono", "disc/adpcm-mono", 1u, 17000u, 3600u);
    add_pcm("music/ram", "disc/ram", SAT_AUDIO_PCM_S16, 2u, 40000u, 5000u);
    g_tracks.back().source = "ram/only";                       /* not on the disc: sat_cdfs_source_file says NOT_FOUND */
    add_pcm("music/other-reader", "xother", SAT_AUDIO_PCM_S16, 2u, 40000u, 5200u);

    steady_state("music/s16-stereo", 1500);
    steady_state("music/s8-stereo", 1500);
    steady_state("music/s16-mono", 1500);
    steady_state("music/s8-mono", 1500);
    steady_state("music/short", 1500, 0u);              /* loops every 11 fields: the drive must be quick */
    steady_state("music/adpcm-stereo", 1500);
    steady_state("music/adpcm-mono", 1500);

    test_seek("music/s16-stereo", 1000u);
    test_seek("music/s8-mono", 12345u);
    test_seek("music/adpcm-stereo", 4097u);
    test_seek("music/adpcm-mono", 9999u);
    test_seek_aborts_flight();
    test_pause_resume();
    test_ownership();
    test_data_job_beside_music();
    test_transport_error();
    test_start_through_scheduler_service();
    test_stopped_music_does_not_hold_data();
    std::puts("music disc: OK");
    return 0;
}
