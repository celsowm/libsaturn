#include <cstdio>
#include <cstdlib>

#include "saturn/asset.h"
#include "saturn/audio.h"
#include "src/core/audio_stream_runtime.hpp"
#include "src/hal/scsp.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace {
int16_t g_samples[4096] = {};
}

extern "C" uint8_t sat_audio_is_initialized(void) {
    return 1u;
}

extern "C" sat_result_t sat_asset_open(const char*, sat_asset_t* out_asset) {
    if (out_asset == nullptr) return SAT_ERR_INVALID_ARG;
    out_asset->slot = 0u;
    out_asset->generation = 1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_asset_info(sat_asset_t, sat_asset_info_t* out_info) {
    if (out_info == nullptr) return SAT_ERR_INVALID_ARG;
    *out_info = {};
    out_info->kind = SAT_ASSET_STREAM;
    out_info->data = g_samples;
    out_info->size = sizeof(g_samples);
    out_info->sample_rate = 22050u;
    out_info->sample_count = 4096u;
    out_info->channels = 1u;
    out_info->format = SAT_AUDIO_PCM_S16;
    return SAT_OK;
}

namespace saturn::hal::scsp {

bool upload(uint32_t, const void*, uint32_t) { return true; }
bool configure_slot(uint8_t, const SlotConfig&) { return true; }
void key_on(uint8_t) {}
void key_off(uint8_t) {}
uint8_t encode_pan(int16_t) { return 0u; }

}  // namespace saturn::hal::scsp

int main() {
    saturn::core::audio_stream_registry_reset(saturn::core::g_audio_streams);
    sat_music_t music{};
    OK(sat_music_open(&music, "music/theme.satstream") == SAT_OK);
    sat_music_info_t info{};
    OK(sat_music_info(music, &info) == SAT_OK && info.sample_rate == 22050u &&
       info.sample_count == 4096u && info.looping != 0u);
    OK(sat_music_play(music) == SAT_OK && sat_music_is_playing(music) != 0u);
    sat_audio_stream_stats_t stats{};
    OK(sat_music_stats(music, &stats) == SAT_OK && stats.buffered_frames == 2048u);
    saturn::core::audio_stream_service(saturn::core::g_audio_streams, 0u);
    OK(sat_music_stats(music, &stats) == SAT_OK && stats.consumed_frames == 1024u &&
       stats.refill_count == 1u);
    OK(sat_music_update(music) == SAT_OK);
    OK(sat_music_pause(music) == SAT_OK && sat_music_is_playing(music) == 0u);
    OK(sat_music_resume(music) == SAT_OK && sat_music_is_playing(music) != 0u);
    OK(sat_music_stop(music) == SAT_OK && sat_music_is_playing(music) == 0u);
    const sat_music_t stale = music;
    OK(sat_music_close(music) == SAT_OK);
    OK(sat_music_info(stale, &info) == SAT_ERR_INVALID_ARG);
    std::puts("music api: OK");
    return 0;
}
