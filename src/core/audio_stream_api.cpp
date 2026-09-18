#include "saturn/audio.h"

#include "src/core/audio_stream_runtime.hpp"
#include "src/hal/scsp.hpp"

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

namespace saturn::core {

namespace {

uint32_t stream_playback_duration(uint32_t sample_count, uint32_t sample_rate) {
    if (sample_count == 0u || sample_rate == 0u) return 1u;
    const uint64_t numerator = static_cast<uint64_t>(sample_count) * 60u;
    uint32_t frames = static_cast<uint32_t>((numerator + sample_rate - 1u) / sample_rate);
    return frames == 0u ? 1u : frames;
}

}  // namespace

void audio_stream_stop_playback(AudioStreamSlot& slot) {
    if (slot.hardware_playing != 0u) {
        saturn::hal::scsp::key_off(slot.scsp_slot);
        slot.hardware_playing = 0u;
    }
    slot.playback_end_frame = 0u;
}

void audio_stream_service(AudioStreamRegistry& registry, uint32_t frame_now) {
    for (uint16_t i = 0u; i < kAudioStreamCapacity; ++i) {
        AudioStreamSlot& slot = registry.slots[i];
        if (slot.used == 0u || slot.ring.paused != 0u) continue;
        if (slot.hardware_playing != 0u &&
            static_cast<int32_t>(frame_now - slot.playback_end_frame) < 0) {
            continue;
        }
        audio_stream_stop_playback(slot);
        if (slot.ring.buffered_frames == 0u) {
            ++slot.ring.underrun_count;
            continue;
        }

        const uint32_t requested_frames = slot.ring.capacity_frames < kAudioStreamChunkFrames
            ? slot.ring.capacity_frames : kAudioStreamChunkFrames;
        const uint32_t frames = audio_stream_copy_out(
            slot.ring, slot.staging, requested_frames);
        if (frames == 0u) continue;
        const uint32_t bytes_per_sample = slot.format == SAT_AUDIO_PCM_S16 ? 2u : 1u;
        const uint32_t byte_count = frames * bytes_per_sample;
        const uint32_t buffer_offset = slot.sound_ram_offset +
            static_cast<uint32_t>(slot.playback_buffer) * kAudioStreamChunkBytes;
        if (!saturn::hal::scsp::upload(buffer_offset, slot.staging, byte_count)) continue;

        saturn::hal::scsp::SlotConfig config{};
        config.start_address = buffer_offset;
        config.sample_rate = slot.sample_rate;
        config.sample_count = static_cast<uint16_t>(frames);
        config.loop_start = 0u;
        // LEA also terminates a non-looping waveform.  Zero here would make
        // every streamed chunk stop after its first sample.
        config.loop_end = static_cast<uint16_t>(frames - 1u);
        config.pitch_scale_q16 = SAT_FX16_ONE;
        config.pcm8 = slot.format == SAT_AUDIO_PCM_S8 ? 1u : 0u;
        config.loop = 0u;
        config.total_level = 0u;
        config.direct_level = 7u;
        config.pan = slot.pan;
        if (!saturn::hal::scsp::configure_slot(slot.scsp_slot, config)) continue;
        saturn::hal::scsp::key_on(slot.scsp_slot);
        slot.hardware_playing = 1u;
        slot.playback_end_frame = frame_now + stream_playback_duration(frames, slot.sample_rate);
        slot.playback_buffer ^= 1u;
        slot.consumed_frames += frames;
        ++slot.refill_count;
    }
}

}  // namespace saturn::core

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
        slot.sample_rate = spec->sample_rate;
        slot.sound_ram_offset = kAudioStreamRamBase +
            static_cast<uint32_t>(i) * 2u * kAudioStreamChunkBytes;
        slot.playback_end_frame = 0u;
        slot.consumed_frames = 0u;
        slot.refill_count = 0u;
        slot.format = spec->format;
        slot.scsp_slot = static_cast<uint8_t>(kAudioStreamScspSlotBase + i);
        slot.playback_buffer = 0u;
        slot.hardware_playing = 0u;
        slot.pan = saturn::hal::scsp::encode_pan(SAT_AUDIO_PAN_CENTER);
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
    audio_stream_stop_playback(*slot);
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

extern "C" sat_result_t sat_audio_stream_set_pan(sat_audio_stream_t stream, int16_t pan) {
    using namespace saturn::core;
    if (sat_audio_is_initialized() == 0u) return SAT_ERR_NOT_INITIALIZED;
    AudioStreamSlot* slot = audio_stream_resolve(g_audio_streams, stream);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    if (pan < SAT_AUDIO_PAN_LEFT) pan = SAT_AUDIO_PAN_LEFT;
    if (pan > SAT_AUDIO_PAN_RIGHT) pan = SAT_AUDIO_PAN_RIGHT;
    // Streams run at full level; only the placement changes.  Keep the value
    // in the stream state too: every chunk is configured afresh before it is
    // keyed on, so a direct register write alone would be lost at refill.
    slot->pan = saturn::hal::scsp::encode_pan(pan);
    saturn::hal::scsp::set_slot_level_pan(
        slot->scsp_slot, 0u, 7u, slot->pan);
    return SAT_OK;
}

extern "C" sat_result_t sat_audio_stream_flush(sat_audio_stream_t stream) {
    using namespace saturn::core;
    if (sat_audio_is_initialized() == 0u) return SAT_ERR_NOT_INITIALIZED;
    AudioStreamSlot* slot = audio_stream_resolve(g_audio_streams, stream);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    AudioStreamRing& ring = slot->ring;
    audio_stream_stop_playback(*slot);
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
    audio_stream_stop_playback(*slot);
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
    out_stats->consumed_frames = slot->consumed_frames;
    out_stats->refill_count = slot->refill_count;
    out_stats->paused = ring.paused;
    out_stats->playing = slot->hardware_playing;
    out_stats->reserved0 = 0u;
    out_stats->reserved1 = 0u;
    return SAT_OK;
}
