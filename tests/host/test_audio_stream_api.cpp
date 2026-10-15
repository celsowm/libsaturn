#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "saturn/audio.h"
#include "src/audio/playback/claim.hpp"
#include "src/audio/streaming/runtime.hpp"
#include "src/hal/scsp/scsp.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

extern "C" uint8_t sat_audio_is_initialized(void) {
    return 1u;
}

namespace saturn::hal::scsp {

static SlotConfig g_last_config{};
static uint8_t g_configured_slot = 0xffu;
static int16_t g_last_encode_input = 0;
static uint8_t g_pan_calls = 0u;
static uint32_t g_upload_count = 0u;
static uint32_t g_key_on_count = 0u;
static uint32_t g_key_off_count = 0u;
static uint8_t g_current_sample_block = 0u;

/* 1-based upload call to fail (0 = never); the next calls succeed again. */
static uint32_t g_upload_fail_at = 0u;
static uint8_t g_last_upload_head[4] = {};
static uint32_t g_last_upload_offset = 0u;
static uint32_t g_upload_bytes = 0u;
/* What the SCSP would hold: every upload lands here. */
static uint8_t g_sound_ram[0x80000u] = {};

bool upload(uint32_t offset, const void* data, uint32_t byte_count) {
    ++g_upload_count;
    if (g_upload_fail_at != 0u && g_upload_count == g_upload_fail_at) return false;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (uint32_t i = 0u; i < 4u && i < byte_count; ++i) g_last_upload_head[i] = bytes[i];
    g_last_upload_offset = offset;
    g_upload_bytes += byte_count;
    std::memcpy(g_sound_ram + offset, bytes, byte_count);
    return true;
}
bool configure_slot(uint8_t slot, const SlotConfig& config) {
    g_configured_slot = slot;
    g_last_config = config;
    return true;
}
void key_on(uint8_t) { ++g_key_on_count; }
static uint32_t g_armed_pending = 0u;
static uint32_t g_armed_at_execute = 0u;
static uint32_t g_execute_count = 0u;
void arm_key_on(uint8_t) { ++g_key_on_count; ++g_armed_pending; }
void execute_key_transitions() {
    ++g_execute_count;
    g_armed_at_execute = g_armed_pending;
    g_armed_pending = 0u;
}
void key_off(uint8_t) { ++g_key_off_count; }
bool read_current_sample_block(uint8_t, uint8_t* out_block) {
    if (out_block == nullptr) return false;
    *out_block = g_current_sample_block;
    return true;
}
uint8_t encode_pan(int16_t pan) {
    g_last_encode_input = pan;
    return static_cast<uint8_t>(pan + 15);
}
void set_slot_level_pan(uint8_t, uint8_t, uint8_t, uint8_t) { ++g_pan_calls; }

}  // namespace saturn::hal::scsp

namespace {

namespace scsp = saturn::hal::scsp;

void service(uint32_t frame, uint32_t rate = 60u) {
    saturn::core::audio_stream_service(saturn::core::g_audio_streams, frame, rate);
}

/* Services `first`, `first + 1`, ... until the stream is keyed on; returns the last frame serviced
 * (the tests stop after 40 frames: a stream that never starts). */
uint32_t service_until_playing(sat_audio_stream_t stream, uint32_t first, uint32_t rate = 60u) {
    sat_audio_stream_stats_t st{};
    uint32_t frame = first;
    for (;; ++frame) {
        service(frame, rate);
        OK(sat_audio_stream_stats(stream, &st) == SAT_OK);
        if (st.playing != 0u) return frame;
        OK(frame - first < 40u);
    }
}

}  // namespace

int main() {
    saturn::core::audio_stream_registry_reset(saturn::core::g_audio_streams);
    sat_audio_spec_t spec = {22050u, 4u, 1u, SAT_AUDIO_PCM_S16, 0u};
    uint8_t storage[8u] = {};
    sat_audio_stream_t streams[4] = {};

    // slots or buffers the sampler claimed are not opened
    namespace claim = saturn::core::audio::claim;
    claim::g_claim = {0xF0000000u, 0u, 0u};
    OK(sat_audio_stream_open(&streams[0], &spec, storage, sizeof(storage)) == SAT_ERR_BUSY);
    claim::g_claim = {0x0FFFFFFFu, 0x6F000u, 0x70002u}; /* reaches stream 0's buffer */
    OK(sat_audio_stream_open(&streams[0], &spec, storage, sizeof(storage)) == SAT_OK && streams[0].slot == 1u);
    OK(sat_audio_stream_close(streams[0]) == SAT_OK);
    claim::g_claim = {};

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
    OK(scsp::g_configured_slot == 28u &&
       scsp::g_last_config.sample_count == 4u &&
       scsp::g_last_config.loop == 0u &&
       scsp::g_last_config.loop_start == 0u &&
       scsp::g_last_config.loop_end == 3u);
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
    OK(sat_audio_stream_set_pan(stale, 0) == SAT_ERR_INVALID_ARG);
    // Pan placement with clamping; streams[1] is open and idle.
    OK(sat_audio_stream_set_pan(streams[1], -15) == SAT_OK &&
       scsp::g_last_encode_input == -15 &&
       scsp::g_pan_calls == 1u);
    OK(sat_audio_stream_set_pan(streams[1], 100) == SAT_OK &&
       scsp::g_last_encode_input == 15 &&
       scsp::g_pan_calls == 2u);
    /* Placement survives the next chunk configuration rather than being
     * replaced by the default centred value. */
    OK(sat_audio_stream_write(streams[1], frames, 4u) == SAT_OK);
    saturn::core::audio_stream_service(saturn::core::g_audio_streams, 100u);
    OK(scsp::g_last_config.pan == 30u);
    sat_audio_stream_t recycled{};
    OK(sat_audio_stream_open(&recycled, &spec, storage, sizeof(storage)) == SAT_OK);
    OK(recycled.slot == stale.slot && recycled.generation != stale.generation);
    for (uint16_t i = 1u; i < 4u; ++i) (void)sat_audio_stream_close(streams[i]);
    (void)sat_audio_stream_close(recycled);

    // ---- a seamless stream: the two opening halves are written a slice per call, then it keys on
    saturn::core::audio_stream_registry_reset(saturn::core::g_audio_streams);
    sat_audio_spec_t seamless_spec = {11025u, 8192u, 1u, SAT_AUDIO_PCM_S16, 0u};
    static uint8_t seamless_storage[16384u];
    static uint8_t seamless_frames[16384u];
    for (uint32_t i = 0u; i < sizeof(seamless_frames); ++i) seamless_frames[i] = static_cast<uint8_t>(i * 7u + 3u);
    sat_audio_stream_t seamless{};
    OK(sat_audio_stream_open(
        &seamless, &seamless_spec, seamless_storage, sizeof(seamless_storage)) == SAT_OK);
    OK(sat_audio_stream_set_pan(seamless, SAT_AUDIO_PAN_RIGHT) == SAT_OK);
    OK(sat_audio_stream_write(seamless, seamless_frames, 8192u) == SAT_OK);

    scsp::g_upload_count = 0u;
    scsp::g_upload_bytes = 0u;
    scsp::g_key_on_count = 0u;
    scsp::g_key_off_count = 0u;
    std::memset(scsp::g_sound_ram, 0, sizeof(scsp::g_sound_ram));
    service(0u);
    OK(sat_audio_stream_stats(seamless, &stats) == SAT_OK);
    OK(stats.playing == 0u && stats.consumed_frames == 0u);           // first slice only: not keyed on yet
    OK(scsp::g_upload_count == 1u && scsp::g_upload_bytes == 1024u * 2u);
    service(0u);                                                      // a second call in the same frame adds nothing
    OK(scsp::g_upload_count == 1u);
    uint32_t frame = 1u;
    for (; frame < 7u; ++frame) {
        service(frame);
        OK(sat_audio_stream_stats(seamless, &stats) == SAT_OK && stats.playing == 0u && stats.consumed_frames == 0u);
        OK(scsp::g_key_on_count == 0u);
    }
    service(frame);                                                   // the eighth slice completes the opening
    OK(sat_audio_stream_stats(seamless, &stats) == SAT_OK);
    OK(stats.consumed_frames == 8192u && stats.refill_count == 2u && stats.playing == 1u);
    OK(scsp::g_upload_count == 8u && scsp::g_upload_bytes == 8192u * 2u);
    OK(scsp::g_key_on_count == 1u);
    OK(scsp::g_key_off_count == 0u);
    OK(scsp::g_last_config.sample_count == 8192u &&
       scsp::g_last_config.loop == 1u &&
       scsp::g_last_config.loop_start == 0u &&
       scsp::g_last_config.loop_end == 8191u &&
       scsp::g_last_config.pan == 30u);
    // Both halves hold the ring's first 8192 frames in order (the halves are adjacent in Sound RAM).
    {
        const uint32_t ram = saturn::core::g_audio_streams.slots[0].sound_ram_offset;
        OK(std::memcmp(scsp::g_sound_ram + ram, seamless_frames, 8192u * 2u) == 0);
    }

    // The ring is only read until the opening is whole: a failed slice leaves it intact (below).
    OK(sat_audio_stream_write(seamless, seamless_frames, 4096u) == SAT_OK);
    scsp::g_current_sample_block = 0u;
    service(frame + 5u);
    OK(sat_audio_stream_stats(seamless, &stats) == SAT_OK &&
       stats.consumed_frames == 8192u && stats.refill_count == 2u);

    // A huge display-frame jump must not refill anything while CA still says
    // the SCSP is reading the same half.
    service(frame + 23u);
    OK(sat_audio_stream_stats(seamless, &stats) == SAT_OK &&
       stats.consumed_frames == 8192u && stats.refill_count == 2u);

    // The refill is driven by the hardware half transition, not elapsed time, and the half is
    // rewritten a slice per frame, so the ring is consumed only when it is whole.
    scsp::g_current_sample_block = 1u;
    scsp::g_upload_count = 0u;
    scsp::g_upload_bytes = 0u;
    uint32_t refill_frame = frame + 24u;
    service(refill_frame);
    OK(sat_audio_stream_stats(seamless, &stats) == SAT_OK &&
       stats.consumed_frames == 8192u && stats.refill_count == 2u);
    OK(scsp::g_upload_count == 1u && scsp::g_upload_bytes < 4096u * 2u);
    for (++refill_frame; stats.refill_count == 2u; ++refill_frame) {
        OK(refill_frame < frame + 24u + 20u);
        service(refill_frame);
        OK(sat_audio_stream_stats(seamless, &stats) == SAT_OK);
    }
    OK(stats.consumed_frames == 12288u && stats.refill_count == 3u && stats.underrun_count == 0u);
    OK(scsp::g_upload_bytes == 4096u * 2u);                           // the whole half, once
    OK(scsp::g_key_on_count == 1u);
    OK(scsp::g_key_off_count == 0u);
    // Half 0 now holds the frames written after the opening ones.
    {
        const uint32_t ram = saturn::core::g_audio_streams.slots[0].sound_ram_offset;
        OK(std::memcmp(scsp::g_sound_ram + ram, seamless_frames, 4096u * 2u) == 0);
    }
    OK(sat_audio_stream_close(seamless) == SAT_OK);
    OK(scsp::g_key_off_count == 1u);

    // A failed Sound RAM transfer must not consume the ring: the retry
    // uploads the very same samples.
    {
        saturn::core::audio_stream_registry_reset(saturn::core::g_audio_streams);
        static uint8_t data[16384u];
        static uint8_t refill[8192u];
        for (uint32_t i = 0u; i < sizeof(data); ++i) data[i] = static_cast<uint8_t>(i * 7u + 3u);
        for (uint32_t i = 0u; i < sizeof(refill); ++i) refill[i] = static_cast<uint8_t>(0xA0u + (i & 0x0Fu));
        sat_audio_stream_t retry{};
        OK(sat_audio_stream_open(
            &retry, &seamless_spec, seamless_storage, sizeof(seamless_storage)) == SAT_OK);
        OK(sat_audio_stream_write(retry, data, 8192u) == SAT_OK);

        // Seamless start: the second slice fails after the first was sent.
        scsp::g_current_sample_block = 0u;
        scsp::g_upload_count = 0u;
        scsp::g_key_on_count = 0u;
        scsp::g_upload_fail_at = 2u;
        service(0u);
        service(1u);
        OK(sat_audio_stream_stats(retry, &stats) == SAT_OK);
        OK(stats.playing == 0u && stats.consumed_frames == 0u && stats.refill_count == 0u);
        OK(sat_audio_stream_buffered(retry) == 8192u);
        OK(scsp::g_key_on_count == 0u);

        // The opening starts over from the first frame of the ring.
        scsp::g_upload_fail_at = 0u;
        std::memset(scsp::g_sound_ram, 0, sizeof(scsp::g_sound_ram));
        service_until_playing(retry, 2u);
        OK(sat_audio_stream_stats(retry, &stats) == SAT_OK);
        OK(stats.playing == 1u && stats.consumed_frames == 8192u && stats.refill_count == 2u);
        OK(sat_audio_stream_buffered(retry) == 0u);
        {
            const uint32_t ram = saturn::core::g_audio_streams.slots[0].sound_ram_offset;
            OK(std::memcmp(scsp::g_sound_ram + ram, data, 8192u * 2u) == 0);
        }

        // Steady-state refill fails once, then retries the same chunk.
        OK(sat_audio_stream_write(retry, refill, 4096u) == SAT_OK);
        scsp::g_current_sample_block = 1u;
        scsp::g_upload_fail_at = scsp::g_upload_count + 1u;
        service(30u);
        OK(sat_audio_stream_stats(retry, &stats) == SAT_OK);
        OK(stats.consumed_frames == 8192u && stats.refill_count == 2u);
        OK(sat_audio_stream_buffered(retry) == 4096u);
        const uint32_t underruns = stats.underrun_count;

        scsp::g_upload_fail_at = 0u;
        for (uint32_t f = 31u; stats.refill_count == 2u; ++f) {
            OK(f < 60u);
            service(f);
            OK(sat_audio_stream_stats(retry, &stats) == SAT_OK);
        }
        OK(stats.consumed_frames == 12288u && stats.refill_count == 3u);
        OK(stats.underrun_count == underruns);
        OK(sat_audio_stream_buffered(retry) == 0u);
        {
            const uint32_t ram = saturn::core::g_audio_streams.slots[0].sound_ram_offset;
            OK(std::memcmp(scsp::g_sound_ram + ram, refill, 4096u * 2u) == 0);
        }
        OK(sat_audio_stream_close(retry) == SAT_OK);
        scsp::g_current_sample_block = 0u;
    }

    sat_audio_spec_t stereo = {22050u, 4u, 2u, SAT_AUDIO_PCM_S16, 0u};
    OK(sat_audio_stream_open(&overflow, &stereo, storage, sizeof(storage)) == SAT_ERR_INVALID_ARG);
    // Two streams started together (a stereo pair) key on with ONE KYONEX
    // after both openings, not one after the other.
    {
        saturn::core::audio_stream_registry_reset(saturn::core::g_audio_streams);
        static uint8_t left_storage[16384u];
        static uint8_t right_storage[16384u];
        static uint8_t pair_frames[16384u];
        sat_audio_stream_t left{};
        sat_audio_stream_t right{};
        OK(sat_audio_stream_open(&left, &seamless_spec, left_storage, sizeof(left_storage)) == SAT_OK);
        OK(sat_audio_stream_open(&right, &seamless_spec, right_storage, sizeof(right_storage)) == SAT_OK);
        OK(sat_audio_stream_write(left, pair_frames, 8192u) == SAT_OK);
        OK(sat_audio_stream_write(right, pair_frames, 8192u) == SAT_OK);
        scsp::g_execute_count = 0u;
        scsp::g_armed_pending = 0u;
        scsp::g_current_sample_block = 0u;
        const uint32_t last = service_until_playing(left, 0u);
        OK(scsp::g_execute_count == 1u);
        OK(scsp::g_armed_at_execute == 2u);
        sat_audio_stream_stats_t left_stats{};
        sat_audio_stream_stats_t right_stats{};
        OK(sat_audio_stream_stats(left, &left_stats) == SAT_OK && left_stats.playing == 1u);
        OK(sat_audio_stream_stats(right, &right_stats) == SAT_OK && right_stats.playing == 1u);
        // Steady state arms nothing, so it executes nothing.
        service(last + 1u);
        OK(scsp::g_execute_count == 1u);

        // Pause during the opening drops it; resume starts again from the first frame.
        saturn::core::audio_stream_registry_reset(saturn::core::g_audio_streams);
        OK(sat_audio_stream_open(&left, &seamless_spec, left_storage, sizeof(left_storage)) == SAT_OK);
        OK(sat_audio_stream_write(left, pair_frames, 8192u) == SAT_OK);
        service(0u);
        service(1u);
        OK(sat_audio_stream_pause(left) == SAT_OK);
        service(2u);                                                   // paused: no slice
        const uint32_t before = scsp::g_upload_count;
        service(3u);
        OK(scsp::g_upload_count == before);
        OK(sat_audio_stream_resume(left) == SAT_OK);
        scsp::g_upload_count = 0u;
        service_until_playing(left, 4u);
        OK(scsp::g_upload_count == 8u);                                // eight fresh slices, none reused
        OK(sat_audio_stream_close(left) == SAT_OK);
    }

    // ---- steady-state refill: the inactive half is rewritten over several service frames, the ring is
    // consumed only when the half is whole, and the half is never rewritten out of turn.
    {
        saturn::core::audio_stream_registry_reset(saturn::core::g_audio_streams);
        static uint8_t spread_storage[32768u];
        static uint8_t spread_frames[2u * 4096u * 3u];
        for (uint32_t i = 0u; i < sizeof(spread_frames); ++i) spread_frames[i] = static_cast<uint8_t>(i * 7u + 3u);
        sat_audio_spec_t spec11 = {11025u, 16384u, 1u, SAT_AUDIO_PCM_S16, 0u};
        sat_audio_stream_t spread{};
        OK(sat_audio_stream_open(&spread, &spec11, spread_storage, sizeof(spread_storage)) == SAT_OK);
        OK(sat_audio_stream_write(spread, spread_frames, 12288u) == SAT_OK);
        scsp::g_current_sample_block = 0u;
        service_until_playing(spread, 0u);
        sat_audio_stream_stats_t st{};
        OK(sat_audio_stream_stats(spread, &st) == SAT_OK && st.consumed_frames == 8192u && st.refill_count == 2u);

        // CA moves into half 1: half 0 becomes inactive and is rewritten over the next frames.
        scsp::g_current_sample_block = 1u;
        scsp::g_upload_count = 0u;
        scsp::g_upload_bytes = 0u;
        service(10u);
        OK(sat_audio_stream_stats(spread, &st) == SAT_OK);
        OK(st.consumed_frames == 8192u && st.refill_count == 2u);            // not whole yet: nothing consumed
        OK(scsp::g_upload_count == 1u);
        const uint32_t first_slice_bytes = scsp::g_upload_bytes;
        OK(first_slice_bytes > 0u && first_slice_bytes < 2u * 4096u);        // a slice, not the whole half
        const uint32_t half0 = scsp::g_last_upload_offset;                    // slice at the start of half 0
        uint32_t f = 11u;
        for (; f < 60u && st.refill_count == 2u; ++f) {
            service(f);
            OK(sat_audio_stream_stats(spread, &st) == SAT_OK);
            OK(scsp::g_last_upload_offset >= half0);                          // never below half 0, never into half 1
            OK(scsp::g_last_upload_offset < half0 + 2u * 4096u);
        }
        OK(st.refill_count == 3u && st.consumed_frames == 12288u && st.underrun_count == 0u);
        OK(scsp::g_upload_bytes == 2u * 4096u);                               // the whole half, exactly once
        OK(f - 10u <= 19u);                                                   // within the time the half lasts
        // Whole again: no further writes until CA moves.
        const uint32_t uploads = scsp::g_upload_count;
        service(f);
        OK(scsp::g_upload_count == uploads);

        // The half changes again while a rewrite is unfinished: finished at once, counted as an underrun.
        OK(sat_audio_stream_write(spread, spread_frames, 8192u) == SAT_OK);
        scsp::g_current_sample_block = 0u;
        service(f + 5u);
        OK(sat_audio_stream_stats(spread, &st) == SAT_OK && st.refill_count == 3u);   // started, not done
        scsp::g_current_sample_block = 1u;
        service(f + 6u);
        OK(sat_audio_stream_stats(spread, &st) == SAT_OK);
        OK(st.underrun_count == 1u && st.refill_count >= 4u);

        // An upload that fails mid-refill is dropped and tried again from the start.
        OK(sat_audio_stream_close(spread) == SAT_OK);
        saturn::core::audio_stream_registry_reset(saturn::core::g_audio_streams);
        OK(sat_audio_stream_open(&spread, &spec11, spread_storage, sizeof(spread_storage)) == SAT_OK);
        OK(sat_audio_stream_write(spread, spread_frames, 12288u) == SAT_OK);
        scsp::g_current_sample_block = 0u;
        service_until_playing(spread, 0u);
        scsp::g_current_sample_block = 1u;
        scsp::g_upload_fail_at = scsp::g_upload_count + 1u;
        service(10u);
        OK(sat_audio_stream_stats(spread, &st) == SAT_OK && st.underrun_count == 1u && st.refill_count == 2u);
        scsp::g_upload_fail_at = 0u;
        scsp::g_upload_bytes = 0u;
        for (uint32_t g = 11u; g < 60u && st.refill_count == 2u; ++g) {
            service(g);
            OK(sat_audio_stream_stats(spread, &st) == SAT_OK);
        }
        OK(st.refill_count == 3u && st.consumed_frames == 12288u && scsp::g_upload_bytes >= 2u * 4096u);
        OK(sat_audio_stream_close(spread) == SAT_OK);

        // A half shorter than four frames is written in one call (44.1 kHz at 25 Hz).
        saturn::core::audio_stream_registry_reset(saturn::core::g_audio_streams);
        sat_audio_spec_t fast = {44100u, 16384u, 1u, SAT_AUDIO_PCM_S16, 0u};
        OK(sat_audio_stream_open(&spread, &fast, spread_storage, sizeof(spread_storage)) == SAT_OK);
        OK(sat_audio_stream_write(spread, spread_frames, 12288u) == SAT_OK);
        scsp::g_current_sample_block = 0u;
        const uint32_t started = service_until_playing(spread, 0u, 25u);
        scsp::g_current_sample_block = 1u;
        service(started + 4u, 25u);
        OK(sat_audio_stream_stats(spread, &st) == SAT_OK && st.refill_count == 3u && st.consumed_frames == 12288u);
        OK(sat_audio_stream_close(spread) == SAT_OK);

        scsp::g_current_sample_block = 0u;
    }
    std::puts("audio stream api: OK");
    return 0;
}
