#include <cstdio>
#include <cstdlib>

#include "saturn/audio.h"
#include "src/core/audio_stream_runtime.hpp"
#include "src/hal/scsp.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

extern "C" uint8_t sat_audio_is_initialized(void) {
    return 1u;
}

namespace saturn::hal::scsp {

static SlotConfig g_last_config{};
static uint8_t g_configured_slot = 0xffu;
static uint32_t g_upload_count = 0u;
static uint32_t g_key_on_count = 0u;
static uint32_t g_key_off_count = 0u;

bool upload(uint32_t, const void*, uint32_t) {
    ++g_upload_count;
    return true;
}
bool configure_slot(uint8_t slot, const SlotConfig& config) {
    g_configured_slot = slot;
    g_last_config = config;
    return true;
}
void key_on(uint8_t) { ++g_key_on_count; }
void key_off(uint8_t) { ++g_key_off_count; }
uint8_t encode_pan(int16_t) { return 0u; }

}  // namespace saturn::hal::scsp

int main() {
    saturn::core::audio_stream_registry_reset(saturn::core::g_audio_streams);
    sat_audio_spec_t spec = {22050u, 4u, 1u, SAT_AUDIO_PCM_S16, 0u};
    uint8_t storage[8u] = {};
    sat_audio_stream_t streams[4] = {};
    for (uint16_t i = 0u; i < 4u; ++i) {
        OK(sat_audio_stream_open(&streams[i], &spec, storage, sizeof(storage)) == SAT_OK);
    }
    sat_audio_stream_t overflow{};
    OK(sat_audio_stream_open(&overflow, &spec, storage, sizeof(storage)) == SAT_ERR_CAPACITY);

    const uint8_t frames[8u] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    OK(sat_audio_stream_write(streams[0], frames, 4u) == SAT_OK);
    OK(sat_audio_stream_buffered(streams[0]) == 4u);
    OK(sat_audio_stream_available(streams[0]) == 0u);
    OK(sat_audio_stream_write(streams[0], frames, 1u) == SAT_ERR_CAPACITY);

    saturn::core::audio_stream_service(saturn::core::g_audio_streams, 0u);

    sat_audio_stream_stats_t stats{};
    OK(sat_audio_stream_stats(streams[0], &stats) == SAT_OK);
    OK(stats.rejected_write_count == 1u && stats.maximum_fill == 4u &&
       stats.consumed_frames == 4u && stats.refill_count == 1u && stats.playing == 1u);
    OK(saturn::hal::scsp::g_configured_slot == 28u &&
       saturn::hal::scsp::g_last_config.sample_count == 4u &&
       saturn::hal::scsp::g_last_config.loop == 0u &&
       saturn::hal::scsp::g_last_config.loop_start == 0u &&
       saturn::hal::scsp::g_last_config.loop_end == 3u);
    OK(sat_audio_stream_pause(streams[0]) == SAT_OK);
    OK(sat_audio_stream_stats(streams[0], &stats) == SAT_OK &&
       stats.paused == 1u && stats.playing == 0u);
    OK(sat_audio_stream_resume(streams[0]) == SAT_OK);
    OK(sat_audio_stream_flush(streams[0]) == SAT_OK);
    OK(sat_audio_stream_buffered(streams[0]) == 0u);
    saturn::core::audio_stream_service(saturn::core::g_audio_streams, 100u);
    OK(sat_audio_stream_stats(streams[0], &stats) == SAT_OK &&
       stats.underrun_count == 1u && stats.playing == 0u);

    const sat_audio_stream_t stale = streams[0];
    OK(sat_audio_stream_close(streams[0]) == SAT_OK);
    OK(sat_audio_stream_stats(stale, &stats) == SAT_ERR_INVALID_ARG);
    sat_audio_stream_t recycled{};
    OK(sat_audio_stream_open(&recycled, &spec, storage, sizeof(storage)) == SAT_OK);
    OK(recycled.slot == stale.slot && recycled.generation != stale.generation);
    for (uint16_t i = 1u; i < 4u; ++i) (void)sat_audio_stream_close(streams[i]);
    (void)sat_audio_stream_close(recycled);

    saturn::core::audio_stream_registry_reset(saturn::core::g_audio_streams);
    sat_audio_spec_t seamless_spec = {11025u, 4096u, 1u, SAT_AUDIO_PCM_S16, 0u};
    uint8_t seamless_storage[8192u] = {};
    uint8_t seamless_frames[4096u] = {};
    sat_audio_stream_t seamless{};
    OK(sat_audio_stream_open(
        &seamless, &seamless_spec, seamless_storage, sizeof(seamless_storage)) == SAT_OK);
    OK(sat_audio_stream_write(seamless, seamless_frames, 2048u) == SAT_OK);

    saturn::hal::scsp::g_upload_count = 0u;
    saturn::hal::scsp::g_key_on_count = 0u;
    saturn::hal::scsp::g_key_off_count = 0u;
    saturn::core::audio_stream_service(saturn::core::g_audio_streams, 0u, 60u);
    OK(sat_audio_stream_stats(seamless, &stats) == SAT_OK);
    OK(stats.consumed_frames == 2048u && stats.refill_count == 2u && stats.playing == 1u);
    OK(saturn::hal::scsp::g_upload_count == 2u);
    OK(saturn::hal::scsp::g_key_on_count == 1u);
    OK(saturn::hal::scsp::g_key_off_count == 0u);
    OK(saturn::hal::scsp::g_last_config.sample_count == 2048u &&
       saturn::hal::scsp::g_last_config.loop == 1u &&
       saturn::hal::scsp::g_last_config.loop_start == 0u &&
       saturn::hal::scsp::g_last_config.loop_end == 2047u);

    OK(sat_audio_stream_write(seamless, seamless_frames, 1024u) == SAT_OK);
    saturn::core::audio_stream_service(saturn::core::g_audio_streams, 5u, 60u);
    OK(sat_audio_stream_stats(seamless, &stats) == SAT_OK &&
       stats.consumed_frames == 2048u && stats.refill_count == 2u);
    saturn::core::audio_stream_service(saturn::core::g_audio_streams, 6u, 60u);
    OK(sat_audio_stream_stats(seamless, &stats) == SAT_OK &&
       stats.consumed_frames == 3072u && stats.refill_count == 3u &&
       stats.underrun_count == 0u);
    OK(saturn::hal::scsp::g_upload_count == 3u);
    OK(saturn::hal::scsp::g_key_on_count == 1u);
    OK(saturn::hal::scsp::g_key_off_count == 0u);
    OK(sat_audio_stream_close(seamless) == SAT_OK);
    OK(saturn::hal::scsp::g_key_off_count == 1u);

    sat_audio_spec_t stereo = {22050u, 4u, 2u, SAT_AUDIO_PCM_S16, 0u};
    OK(sat_audio_stream_open(&overflow, &stereo, storage, sizeof(storage)) == SAT_ERR_INVALID_ARG);
    std::puts("audio stream api: OK");
    return 0;
}
