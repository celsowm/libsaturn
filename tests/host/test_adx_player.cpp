/* Compact ADX player integration test: fake CD feed, real ADX decoder,
 * user-owned ring buffers and stereo waveform matching across sector splits,
 * sample-count truncation, looping, pause and stop. No Saturn BIOS required. */
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <vector>

#include "saturn/adx.h"
#include "src/audio/playback/adx_adpcm.hpp"

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)
namespace codec = saturn::core::audio::adx_adpcm;

namespace {
struct Stream {
    std::deque<uint8_t> data;
    uint32_t capacity_frames = 0;
    uint32_t underruns = 0;
    bool opened = false;
    bool paused = true;
};
Stream g_streams[2];
std::vector<uint8_t> g_file;
std::vector<uint8_t> g_expected[2];
std::vector<uint8_t> g_played[2];
struct Flight { bool active = false; uint32_t offset = 0; uint32_t bytes = 0; uint32_t done = 0; };
Flight g_flight;
uint32_t g_service_calls = 0;
uint32_t g_decode_calls = 0;
}
extern "C" uint8_t sat_audio_is_initialized(void) { return 1u; }
extern "C" sat_result_t sat_audio_stream_open(sat_audio_stream_t* out,
    const sat_audio_spec_t* spec, void*, uint32_t bytes) {
    for (uint16_t i = 0u; i < 2u; ++i) {
        if (g_streams[i].opened) continue;
        CHECK(spec->format == SAT_AUDIO_PCM_S16 && spec->channels == 1u);
        CHECK(bytes >= spec->buffer_frames * 2u);
        g_streams[i] = Stream{};
        g_streams[i].opened = true;
        g_streams[i].capacity_frames = spec->buffer_frames;
        *out = {i,1u};
        return SAT_OK;
    }
    return SAT_ERR_CAPACITY;
}
extern "C" sat_result_t sat_audio_stream_write(sat_audio_stream_t stream,
    const void* frames, uint32_t frame_count) {
    Stream& s = g_streams[stream.slot];
    if (s.data.size() / 2u + frame_count > s.capacity_frames)
        return SAT_ERR_CAPACITY;
    const uint8_t* bytes = static_cast<const uint8_t*>(frames);
    for (uint32_t i = 0u; i < frame_count * 2u; ++i) s.data.push_back(bytes[i]);
    ++g_decode_calls;
    return SAT_OK;
}
extern "C" uint32_t sat_audio_stream_available(sat_audio_stream_t stream) {
    const Stream& s = g_streams[stream.slot];
    return s.capacity_frames - static_cast<uint32_t>(s.data.size()) / 2u;
}
extern "C" uint32_t sat_audio_stream_buffered(sat_audio_stream_t stream) {
    return static_cast<uint32_t>(g_streams[stream.slot].data.size()) / 2u;
}
extern "C" sat_result_t sat_audio_stream_pause(sat_audio_stream_t stream) {
    g_streams[stream.slot].paused = true; return SAT_OK;
}
extern "C" sat_result_t sat_audio_stream_resume(sat_audio_stream_t stream) {
    g_streams[stream.slot].paused = false; return SAT_OK;
}
extern "C" sat_result_t sat_audio_stream_set_pan(sat_audio_stream_t, int16_t) { return SAT_OK; }
extern "C" sat_result_t sat_audio_stream_flush(sat_audio_stream_t stream) {
    g_streams[stream.slot].data.clear(); return SAT_OK;
}
extern "C" sat_result_t sat_audio_stream_close(sat_audio_stream_t stream) {
    g_streams[stream.slot].opened = false; return SAT_OK;
}
extern "C" sat_result_t sat_audio_stream_stats(sat_audio_stream_t stream,
        sat_audio_stream_stats_t* out) {
    *out = {};
    out->underrun_count = g_streams[stream.slot].underruns;
    out->buffered_frames = sat_audio_stream_buffered(stream);
    return SAT_OK;
}
extern "C" sat_result_t sat_disc_stream_music_attach(sat_disc_stream_t* disc,
    const sat_disc_music_feed_ops_t* ops, void* user) {
    if (disc->music_ops) return SAT_ERR_BUSY;
    disc->music_ops = ops;
    disc->music_user = user;
    return SAT_OK;
}
extern "C" void sat_disc_stream_music_detach(sat_disc_stream_t* disc) {
    disc->music_ops = nullptr;
    disc->music_user = nullptr;
    g_flight = {};
}
extern "C" void sat_disc_stream_music_abort(sat_disc_stream_t*) {
    g_flight = {};
}
extern "C" void sat_disc_stream_music_set_starting(sat_disc_stream_t* disc, uint8_t v) {
    disc->music_starting = v;
}
extern "C" sat_result_t sat_disc_stream_service(sat_disc_stream_t* disc) {
    ++g_service_calls;
    const auto* ops = disc->music_ops;
    if (!ops) return SAT_ERR_INVALID_ARG;
    if (!g_flight.active) {
        const sat_cdfs_file_t* file = nullptr;
        uint32_t offset = 0u, bytes = 0u;
        if (!ops->next(disc->music_user, &file, &offset, &bytes)) return SAT_OK;
        CHECK(file != nullptr && bytes != 0u && offset % 2048u == 0u);
        CHECK(offset + bytes <= g_file.size());
        g_flight = {true, offset, bytes, 0u};
        ops->started(disc->music_user);
    }
    const uint32_t remain = g_flight.bytes - g_flight.done;
    const uint32_t slice = remain < 700u ? remain : 700u;
    uint32_t used = 0u;
    const sat_result_t st = ops->deliver(disc->music_user,
        g_file.data() + g_flight.offset + g_flight.done, slice, &used);
    CHECK(st == SAT_OK && used > 0u && used <= slice);
    g_flight.done += used;
    if (g_flight.done == g_flight.bytes) g_flight = {};
    return SAT_OK;
}

void make_track(uint32_t sample_count) {
    g_file.clear();
    g_file.insert(g_file.end(), {0x19,0xA9,0xF5,0xB6});
    codec::State state{};
    codec::init(state, g_file.data());
    for (auto& c : g_expected) c.clear();
    const uint32_t blocks = (sample_count + 31u) / 32u;
    for (uint32_t block = 0u; block < blocks; ++block) {
        uint8_t encoded[36] = {};
        for (uint32_t c = 0u; c < 2u; ++c) {
            encoded[c * 18u] = 0u;
            encoded[c * 18u + 1u] = 8u;
            for (uint32_t n = 0u; n < 16u; ++n)
                encoded[c * 18u + 2u + n] = static_cast<uint8_t>(
                    (block * 37u + n * 19u + c * 29u) & 0xFFu);
        }
        uint8_t decoded[128] = {};
        CHECK(codec::decode_block(encoded, 2u, state, decoded));
        g_file.insert(g_file.end(), encoded, encoded + 36u);
        const uint32_t take = std::min(32u, sample_count - block * 32u);
        for (uint32_t c = 0u; c < 2u; ++c)
            g_expected[c].insert(g_expected[c].end(),
                decoded + 64u * c, decoded + 64u * c + take * 2u);
    }
}

void consume(uint32_t frames) {
    for (uint32_t c = 0u; c < 2u; ++c) {
        Stream& s = g_streams[c];
        if (s.paused) continue;
        CHECK(s.data.size() / 2u >= frames);
        for (uint32_t i = 0u; i < frames * 2u; ++i) {
            g_played[c].push_back(s.data.front());
            s.data.pop_front();
        }
    }
}
void verify(uint32_t first_frame, uint32_t sample_count) {
    for (uint32_t c = 0u; c < 2u; ++c)
        for (size_t i = 0; i < g_played[c].size(); ++i) {
            const uint32_t at = (first_frame + static_cast<uint32_t>(i / 2u)) % sample_count;
            CHECK(g_played[c][i] == g_expected[c][at * 2u + (i & 1u)]);
        }
}
} // namespace

int main() {
    constexpr uint32_t kFrames = 15013u; // partial ADX final block and 5 CD segments
    make_track(kFrames);
    sat_disc_stream_t disc{};
    sat_cdfs_file_t file{};
    file.size = static_cast<uint32_t>(g_file.size());
    static sat_adx_pcm_rings_t rings{};
    static sat_adx_player_t player{};
    static_assert(sizeof(sat_adx_pcm_rings_t) == 65536u);
    static_assert(sizeof(sat_adx_player_t) < 10000u);
    const sat_adx_config_t config{22050u, kFrames, 6569, -2634, 2u, {0,0,0}};
    CHECK(sat_adx_player_open(&player, &disc, &file, &config, &rings) == SAT_OK);
    sat_adx_player_info_t info{};
    CHECK(sat_adx_player_info(&player, &info) == SAT_OK && info.channels == 2u);
    CHECK(sat_adx_player_play(&player) == SAT_OK);
    uint32_t primed = 0u;
    do {
        CHECK(++primed < 200u);
        CHECK(sat_adx_player_update(&player) == SAT_OK);
        CHECK(sat_adx_player_info(&player, &info) == SAT_OK);
    } while (info.starting);
    CHECK(info.buffered_frames >= 12288u);
    for (uint32_t i = 0u; i < 450u; ++i) {
        consume(367u);
        CHECK(sat_adx_player_update(&player) == SAT_OK);
    }
    CHECK(sat_adx_player_info(&player, &info) == SAT_OK);
    CHECK(info.failed_deliveries == 0u && info.max_decode_frames <= 2048u);
    CHECK(info.decoded_frames > 100000u && info.underrun_count == 0u);
    verify(0u,kFrames);
    CHECK(sat_adx_player_pause(&player) == SAT_OK);
    const uint32_t played_before = static_cast<uint32_t>(g_played[0].size());
    for (uint32_t i = 0u; i < 7u; ++i)
        CHECK(sat_adx_player_update(&player) == SAT_OK);
    consume(367u);
    CHECK(g_played[0].size() == played_before);
    CHECK(sat_adx_player_play(&player) == SAT_OK);
    for (uint32_t i = 0u; i < 75u; ++i) {
        consume(367u);
        CHECK(sat_adx_player_update(&player) == SAT_OK);
    }
    verify(0u,kFrames);
    CHECK(sat_adx_player_stop(&player) == SAT_OK);
    g_played[0].clear();
    g_played[1].clear();
    CHECK(sat_adx_player_play(&player) == SAT_OK);
    uint32_t retry = 0u;
    do {
        CHECK(++retry < 200u);
        CHECK(sat_adx_player_update(&player) == SAT_OK);
        CHECK(sat_adx_player_info(&player, &info) == SAT_OK);
    } while (info.starting);
    for (uint32_t i = 0u; i < 70u; ++i) {
        consume(367u);
        CHECK(sat_adx_player_update(&player) == SAT_OK);
    }
    verify(0u,kFrames);
    CHECK(sat_adx_player_close(&player) == SAT_OK);
    CHECK(disc.music_ops == nullptr && !g_streams[0].opened && !g_streams[1].opened);
    std::puts("compact ADX CD player: OK");
    return 0;
}
