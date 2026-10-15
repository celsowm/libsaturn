#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "saturn/asset.h"
#include "saturn/audio.h"
#include "saturn/disc_stream.h"
#include "src/audio/streaming/runtime.hpp"
#include "src/hal/scsp/scsp.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace {
int16_t g_samples[4096] = {};
bool g_physical = false;
bool g_stereo = false;
/* Nonzero: the Nth CD read plays the left channel ahead, as a service run
 * from the CD progress callback does when only the left CA changed half. */
uint32_t g_skew_left_on_read = 0u;
uint32_t g_reads = 0u;
/* Offset of the first CD read since the test last cleared it. */
int64_t g_first_offset = -1;
}

extern "C" uint8_t sat_audio_is_initialized(void) {
    return 1u;
}

extern "C" sat_result_t sat_asset_open(const char*, sat_asset_t* out_asset) {
    if (out_asset == nullptr) return SAT_ERR_INVALID_ARG;
    out_asset->slot = g_physical ? 1u : 0u;
    out_asset->generation = 1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_asset_info(sat_asset_t asset, sat_asset_info_t* out_info) {
    if (out_info == nullptr) return SAT_ERR_INVALID_ARG;
    *out_info = {};
    out_info->kind = SAT_ASSET_STREAM;
    out_info->data = asset.slot == 1u ? nullptr : g_samples;
    out_info->source_path = asset.slot == 1u ? "MUSIC/THEME.S16" : nullptr;
    out_info->size = sizeof(g_samples);
    out_info->sample_rate = 22050u;
    out_info->sample_count = g_stereo ? 2048u : 4096u;
    out_info->channels = g_stereo ? 2u : 1u;
    out_info->format = SAT_AUDIO_PCM_S16;
    return SAT_OK;
}

extern "C" sat_result_t sat_asset_read_at(
    const char*, uint32_t offset, void* destination, uint32_t bytes, uint32_t* out_read) {
    if (destination == nullptr || out_read == nullptr || offset > sizeof(g_samples)) {
        return SAT_ERR_INVALID_ARG;
    }
    if (g_skew_left_on_read != 0u && ++g_reads == g_skew_left_on_read) {
        saturn::core::AudioStreamSlot& left = saturn::core::g_audio_streams.slots[0];
        OK(left.used != 0u && left.ring.buffered_frames >= 4096u);
        saturn::core::audio_stream_consume(left.ring, 4096u);
    }
    if (g_first_offset < 0) g_first_offset = offset;
    const uint32_t available = static_cast<uint32_t>(sizeof(g_samples)) - offset;
    const uint32_t count = bytes < available ? bytes : available;
    std::memcpy(destination, reinterpret_cast<const uint8_t*>(g_samples) + offset, count);
    *out_read = count;
    return SAT_OK;
}

/* music.cpp links the disc scheduler hooks; this test never binds one, so none is reached. */
extern "C" sat_result_t sat_cdfs_source_file(const char*, const sat_cdfs_file_t**) { return SAT_ERR_NOT_FOUND; }
extern "C" sat_result_t sat_disc_stream_music_attach(sat_disc_stream_t*, const sat_disc_music_feed_ops_t*, void*) { OK(false); return SAT_ERR_INVALID_ARG; }
extern "C" void sat_disc_stream_music_detach(sat_disc_stream_t*) { OK(false); }
extern "C" void sat_disc_stream_music_abort(sat_disc_stream_t*) { OK(false); }
extern "C" sat_result_t sat_disc_stream_music_attach_standby(sat_disc_stream_t*, const sat_disc_music_feed_ops_t*, void*) { OK(false); return SAT_ERR_INVALID_ARG; }
extern "C" void sat_disc_stream_music_detach_standby(sat_disc_stream_t*) { OK(false); }
extern "C" void sat_disc_stream_music_abort_standby(sat_disc_stream_t*) { OK(false); }
extern "C" sat_result_t sat_disc_stream_music_swap(sat_disc_stream_t*) { OK(false); return SAT_ERR_INVALID_ARG; }
extern "C" void sat_disc_stream_music_standby_gate(sat_disc_stream_t*, uint8_t) {}
extern "C" void sat_disc_stream_music_set_starting(sat_disc_stream_t*, uint8_t) { OK(false); }
extern "C" sat_result_t sat_disc_stream_service(sat_disc_stream_t*) { OK(false); return SAT_ERR_INVALID_ARG; }

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

/* Updates until the rings are full and the streams are released; returns the updates it took. */
uint32_t pump(sat_music_t music) {
    uint32_t updates = 0u;
    for (;;) {
        sat_music_info_t info{};
        OK(sat_music_info(music, &info) == SAT_OK);
        if (info.starting == 0u) return updates;
        OK(updates < 40u);
        OK(sat_music_update(music) == SAT_OK);
        ++updates;
    }
}

/* Services display frames until every open stream is keyed on. */
uint32_t service_started(sat_music_t music, uint32_t first_frame) {
    sat_audio_stream_stats_t stats{};
    uint32_t frame = first_frame;
    for (;; ++frame) {
        saturn::core::audio_stream_service(saturn::core::g_audio_streams, frame);
        OK(sat_music_stats(music, &stats) == SAT_OK);
        if (stats.playing != 0u) return frame;
        OK(frame - first_frame < 40u);
    }
}

int main() {
    saturn::core::audio_stream_registry_reset(saturn::core::g_audio_streams);
    sat_music_t music{};
    OK(sat_music_open(&music, "music/theme.satstream") == SAT_OK);
    sat_music_info_t info{};
    OK(sat_music_info(music, &info) == SAT_OK && info.sample_rate == 22050u &&
       info.sample_count == 4096u && info.looping != 0u && info.starting == 0u);
    // play only marks the track wanted: nothing is read, nothing sounds.
    OK(sat_music_play(music) == SAT_OK && sat_music_is_playing(music) != 0u);
    OK(sat_music_info(music, &info) == SAT_OK && info.starting == 1u && info.playing == 1u);
    sat_audio_stream_stats_t stats{};
    OK(sat_music_stats(music, &stats) == SAT_OK && stats.buffered_frames == 0u && stats.paused == 1u);
    saturn::core::audio_stream_service(saturn::core::g_audio_streams, 0u);
    OK(sat_music_stats(music, &stats) == SAT_OK && stats.playing == 0u);       // still silent
    // Four updates (8192 frames each) fill the 32768-frame ring; the streams are released then.
    OK(pump(music) == 4u);
    OK(sat_music_stats(music, &stats) == SAT_OK && stats.buffered_frames == 32768u && stats.paused == 0u);
    OK(sat_music_info(music, &info) == SAT_OK && info.starting == 0u && info.playing == 1u);
    service_started(music, 1u);
    OK(sat_music_stats(music, &stats) == SAT_OK && stats.consumed_frames == 8192u &&
       stats.refill_count == 2u && stats.playing == 1u);
    OK(sat_music_update(music) == SAT_OK);
    OK(sat_music_pause(music) == SAT_OK && sat_music_is_playing(music) == 0u);
    OK(sat_music_resume(music) == SAT_OK && sat_music_is_playing(music) != 0u);
    OK(sat_music_info(music, &info) == SAT_OK && info.starting == 0u);         // the ring is still full: the streams restart at once
    OK(sat_music_stats(music, &stats) == SAT_OK && stats.paused == 0u);
    OK(pump(music) <= 1u);
    OK(sat_music_stop(music) == SAT_OK && sat_music_is_playing(music) == 0u);
    OK(sat_music_info(music, &info) == SAT_OK && info.starting == 0u);
    const sat_music_t stale = music;
    OK(sat_music_close(music) == SAT_OK);
    OK(sat_music_info(stale, &info) == SAT_ERR_INVALID_ARG);
    g_physical = true;
    sat_music_t physical{};
    OK(sat_music_open(&physical, "music/cd-theme.satstream") == SAT_OK);
    OK(sat_music_play(physical) == SAT_OK);
    g_reads = 0u;
    OK(sat_music_stats(physical, &stats) == SAT_OK && stats.buffered_frames == 0u);
    pump(physical);
    OK(sat_music_stats(physical, &stats) == SAT_OK && stats.buffered_frames == 32768u);
    OK(sat_music_close(physical) == SAT_OK);
    g_stereo = true;
    sat_music_t stereo{};
    OK(sat_music_open(&stereo, "music/cd-stereo.satstream") == SAT_OK);
    OK(sat_music_info(stereo, &info) == SAT_OK && info.channels == 2u &&
       info.sample_count == 2048u);
    OK(sat_music_play(stereo) == SAT_OK && sat_music_is_playing(stereo) != 0u);
    pump(stereo);
    OK(sat_music_stats(stereo, &stats) == SAT_OK && stats.buffered_frames == 32768u);
    service_started(stereo, 200u);
    OK(sat_music_stats(stereo, &stats) == SAT_OK && stats.consumed_frames == 8192u &&
       stats.refill_count == 4u && stats.playing == 1u);
    OK(sat_music_update(stereo) == SAT_OK);
    OK(sat_music_stop(stereo) == SAT_OK && sat_music_is_playing(stereo) == 0u);
    OK(sat_music_close(stereo) == SAT_OK);

    // The fill must end when one channel is full, even if a service during a
    // CD read left the other channel with room feed() cannot fill alone.
    g_reads = 0u;
    g_skew_left_on_read = 3u;
    sat_music_t skewed{};
    OK(sat_music_open(&skewed, "music/cd-stereo.satstream") == SAT_OK);
    OK(sat_music_play(skewed) == SAT_OK);
    OK(g_reads == 0u);
    pump(skewed);
    OK(g_reads >= 3u);
    const saturn::core::AudioStreamRing& left = saturn::core::g_audio_streams.slots[0].ring;
    const saturn::core::AudioStreamRing& right = saturn::core::g_audio_streams.slots[1].ring;
    OK(left.capacity_frames - left.buffered_frames == 4096u);
    OK(right.buffered_frames == right.capacity_frames);
    OK(sat_music_stats(skewed, &stats) == SAT_OK && stats.buffered_frames == 32768u - 4096u);
    g_skew_left_on_read = 0u;
    OK(sat_music_close(skewed) == SAT_OK);

    // Seek restarts a playing stereo track at the requested frame: both
    // channels refill from the new source offset and start together.
    sat_music_t seeking{};
    OK(sat_music_open(&seeking, "music/cd-stereo.satstream") == SAT_OK);
    OK(sat_music_play(seeking) == SAT_OK);
    pump(seeking);
    g_first_offset = -1;
    OK(sat_music_seek(seeking, 1000u) == SAT_OK && sat_music_is_playing(seeking) != 0u);
    OK(g_first_offset == -1 && left.buffered_frames == 0u && right.buffered_frames == 0u);   // nothing read by the seek
    OK(sat_music_info(seeking, &info) == SAT_OK && info.starting == 1u);
    pump(seeking);
    OK(g_first_offset == 1000 * 4);
    OK(left.buffered_frames == left.capacity_frames &&
       right.buffered_frames == right.capacity_frames);
    OK(sat_music_seek(seeking, 2048u) == SAT_ERR_INVALID_ARG);
    // A paused track only moves its cursor; the read happens after play.
    OK(sat_music_pause(seeking) == SAT_OK);
    g_first_offset = -1;
    OK(sat_music_seek(seeking, 10u) == SAT_OK && sat_music_is_playing(seeking) == 0u);
    OK(g_first_offset == -1 && left.buffered_frames == 0u && right.buffered_frames == 0u);
    OK(sat_music_play(seeking) == SAT_OK && g_first_offset == -1);
    OK(sat_music_update(seeking) == SAT_OK && g_first_offset == 10 * 4);
    OK(sat_music_close(seeking) == SAT_OK);
    std::puts("music api: OK");
    return 0;
}
