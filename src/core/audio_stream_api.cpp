#include "saturn/audio.h"

#include "src/core/audio_stream_runtime.hpp"

namespace {

uint32_t bytes_per_sample(uint8_t format) {
    if (format == SAT_AUDIO_PCM_S8) return 1u;
    if (format == SAT_AUDIO_PCM_S16) return 2u;
    return 0u;
}

sat_result_t validate_stream_spec(
    const sat_audio_spec_t* spec,
    void* buffer,
    uint32_t buffer_bytes,
    uint32_t* out_frame_bytes
) {
    if (spec == nullptr || buffer == nullptr || out_frame_bytes == nullptr ||
        spec->sample_rate == 0u || spec->buffer_frames == 0u || spec->channels != 1u) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint32_t sample_bytes = bytes_per_sample(spec->format);
    if (sample_bytes == 0u) return SAT_ERR_UNSUPPORTED;
    const uint64_t frame_bytes = static_cast<uint64_t>(sample_bytes) * spec->channels;
    const uint64_t required_bytes = frame_bytes * spec->buffer_frames;
    if (required_bytes > 0xFFFFFFFFu || buffer_bytes < required_bytes) return SAT_ERR_INVALID_ARG;
    *out_frame_bytes = static_cast<uint32_t>(frame_bytes);
    return SAT_OK;
}

}  // namespace

extern "C" sat_result_t sat_audio_stream_open(
    sat_audio_stream_t* out_stream,
    const sat_audio_spec_t* spec,
    void* buffer,
    uint32_t buffer_bytes
) {
    if (out_stream == nullptr) return SAT_ERR_INVALID_ARG;
    if (sat_audio_is_initialized() == 0u) return SAT_ERR_NOT_INITIALIZED;
    uint32_t frame_bytes = 0u;
    SAT_TRY(validate_stream_spec(spec, buffer, buffer_bytes, &frame_bytes));

    using namespace saturn::core;
    for (uint16_t i = 0u; i < kAudioStreamCapacity; ++i) {
        AudioStreamSlot& slot = g_audio_streams.slots[i];
        if (slot.used != 0u) continue;
        if (slot.generation == 0u) slot.generation = 1u;
        slot.used = 1u;
        slot.ring = {};
        slot.ring.buffer = static_cast<uint8_t*>(buffer);
        slot.ring.capacity_frames = spec->buffer_frames;
        slot.ring.frame_bytes = frame_bytes;
        audio_stream_ring_reset(slot.ring);
        out_stream->slot = i;
        out_stream->generation = slot.generation;
        return SAT_OK;
    }
    return SAT_ERR_CAPACITY;
}

extern "C" sat_result_t sat_audio_stream_write(
    sat_audio_stream_t stream,
    const void* frames,
    uint32_t frame_count
) {
    using namespace saturn::core;
    AudioStreamSlot* slot = audio_stream_resolve(g_audio_streams, stream);
    if (sat_audio_is_initialized() == 0u) return SAT_ERR_NOT_INITIALIZED;
    if (slot == nullptr || frames == nullptr || frame_count == 0u) return SAT_ERR_INVALID_ARG;
    AudioStreamRing& ring = slot->ring;
    if (frame_count > ring.capacity_frames - ring.buffered_frames) {
        ++ring.rejected_write_count;
        return SAT_ERR_CAPACITY;
    }
    audio_stream_copy_in(ring, static_cast<const uint8_t*>(frames), frame_count);
    return SAT_OK;
}

extern "C" uint32_t sat_audio_stream_available(sat_audio_stream_t stream) {
    using namespace saturn::core;
    const AudioStreamSlot* slot = audio_stream_resolve(g_audio_streams, stream);
    if (sat_audio_is_initialized() == 0u || slot == nullptr) return 0u;
    return slot->ring.capacity_frames - slot->ring.buffered_frames;
}

extern "C" uint32_t sat_audio_stream_buffered(sat_audio_stream_t stream) {
    using namespace saturn::core;
    const AudioStreamSlot* slot = audio_stream_resolve(g_audio_streams, stream);
    if (sat_audio_is_initialized() == 0u || slot == nullptr) return 0u;
    return slot->ring.buffered_frames;
}

extern "C" sat_result_t sat_audio_stream_pause(sat_audio_stream_t stream) {
    using namespace saturn::core;
    if (sat_audio_is_initialized() == 0u) return SAT_ERR_NOT_INITIALIZED;
    AudioStreamSlot* slot = audio_stream_resolve(g_audio_streams, stream);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    slot->ring.paused = 1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_audio_stream_resume(sat_audio_stream_t stream) {
    using namespace saturn::core;
    if (sat_audio_is_initialized() == 0u) return SAT_ERR_NOT_INITIALIZED;
    AudioStreamSlot* slot = audio_stream_resolve(g_audio_streams, stream);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    slot->ring.paused = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_audio_stream_flush(sat_audio_stream_t stream) {
    using namespace saturn::core;
    if (sat_audio_is_initialized() == 0u) return SAT_ERR_NOT_INITIALIZED;
    AudioStreamSlot* slot = audio_stream_resolve(g_audio_streams, stream);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    AudioStreamRing& ring = slot->ring;
    ring.read_frame = ring.write_frame;
    ring.buffered_frames = 0u;
    audio_stream_update_fill_stats(ring);
    return SAT_OK;
}

extern "C" sat_result_t sat_audio_stream_close(sat_audio_stream_t stream) {
    using namespace saturn::core;
    if (sat_audio_is_initialized() == 0u) return SAT_ERR_NOT_INITIALIZED;
    AudioStreamSlot* slot = audio_stream_resolve(g_audio_streams, stream);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    slot->used = 0u;
    slot->generation = next_audio_stream_generation(slot->generation);
    slot->ring = {};
    return SAT_OK;
}

extern "C" sat_result_t sat_audio_stream_stats(
    sat_audio_stream_t stream,
    sat_audio_stream_stats_t* out_stats
) {
    using namespace saturn::core;
    if (out_stats == nullptr) return SAT_ERR_INVALID_ARG;
    if (sat_audio_is_initialized() == 0u) return SAT_ERR_NOT_INITIALIZED;
    const AudioStreamSlot* slot = audio_stream_resolve(g_audio_streams, stream);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    const AudioStreamRing& ring = slot->ring;
    out_stats->buffered_frames = ring.buffered_frames;
    out_stats->capacity_frames = ring.capacity_frames;
    out_stats->underrun_count = ring.underrun_count;
    out_stats->rejected_write_count = ring.rejected_write_count;
    out_stats->minimum_fill = ring.minimum_fill;
    out_stats->maximum_fill = ring.maximum_fill;
    out_stats->paused = ring.paused;
    out_stats->reserved0 = 0u;
    out_stats->reserved1 = 0u;
    return SAT_OK;
}
