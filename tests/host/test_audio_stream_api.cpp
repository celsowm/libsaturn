#include <cstdio>
#include <cstdlib>

#include "saturn/audio.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

extern "C" uint8_t sat_audio_is_initialized(void) {
    return 1u;
}

int main() {
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

    sat_audio_stream_stats_t stats{};
    OK(sat_audio_stream_stats(streams[0], &stats) == SAT_OK);
    OK(stats.rejected_write_count == 1u && stats.maximum_fill == 4u);
    OK(sat_audio_stream_pause(streams[0]) == SAT_OK);
    OK(sat_audio_stream_stats(streams[0], &stats) == SAT_OK && stats.paused == 1u);
    OK(sat_audio_stream_resume(streams[0]) == SAT_OK);
    OK(sat_audio_stream_flush(streams[0]) == SAT_OK);
    OK(sat_audio_stream_buffered(streams[0]) == 0u);

    const sat_audio_stream_t stale = streams[0];
    OK(sat_audio_stream_close(streams[0]) == SAT_OK);
    OK(sat_audio_stream_stats(stale, &stats) == SAT_ERR_INVALID_ARG);
    sat_audio_stream_t recycled{};
    OK(sat_audio_stream_open(&recycled, &spec, storage, sizeof(storage)) == SAT_OK);
    OK(recycled.slot == stale.slot && recycled.generation != stale.generation);
    for (uint16_t i = 1u; i < 4u; ++i) (void)sat_audio_stream_close(streams[i]);
    (void)sat_audio_stream_close(recycled);

    sat_audio_spec_t stereo = {22050u, 4u, 2u, SAT_AUDIO_PCM_S16, 0u};
    OK(sat_audio_stream_open(&overflow, &stereo, storage, sizeof(storage)) == SAT_ERR_INVALID_ARG);
    std::puts("audio stream api: OK");
    return 0;
}
