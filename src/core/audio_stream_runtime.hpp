#ifndef SATURN_CORE_AUDIO_STREAM_RUNTIME_HPP
#define SATURN_CORE_AUDIO_STREAM_RUNTIME_HPP

#include <stdint.h>

#include "saturn/audio.h"

namespace saturn::core {

constexpr uint16_t kAudioStreamCapacity = 4u;
constexpr uint8_t kAudioStreamScspSlotBase = 28u;
constexpr uint32_t kAudioStreamChunkFrames = 1024u;
constexpr uint32_t kAudioStreamChunkBytes = kAudioStreamChunkFrames * 2u;
constexpr uint32_t kAudioStreamRamBytes =
    static_cast<uint32_t>(kAudioStreamCapacity) * 2u * kAudioStreamChunkBytes;
constexpr uint32_t kAudioStreamRamBase = 0x80000u - kAudioStreamRamBytes;

struct AudioStreamRing {
    uint8_t* buffer;
    uint32_t capacity_frames;
    uint32_t frame_bytes;
    uint32_t read_frame;
    uint32_t write_frame;
    uint32_t buffered_frames;
    uint32_t underrun_count;
    uint32_t rejected_write_count;
    uint32_t minimum_fill;
    uint32_t maximum_fill;
    uint8_t paused;
};

struct AudioStreamSlot {
    AudioStreamRing ring;
    uint32_t sample_rate;
    uint32_t sound_ram_offset;
    uint32_t playback_end_frame;
    uint32_t playback_start_frame;
    uint32_t playback_chunk_frames;
    uint32_t serviced_chunks;
    uint32_t consumed_frames;
    uint32_t refill_count;
    uint8_t format;
    uint8_t scsp_slot;
    uint8_t playback_buffer;
    uint8_t hardware_playing;
    uint8_t pan;
    uint8_t seamless_loop;
    uint8_t reserved0;
    uint16_t reserved1;
    uint8_t staging[kAudioStreamChunkBytes];
    uint16_t generation;
    uint8_t used;
    uint8_t reserved;
};

struct AudioStreamRegistry {
    AudioStreamSlot slots[kAudioStreamCapacity];
};

extern AudioStreamRegistry g_audio_streams;

inline uint16_t next_audio_stream_generation(uint16_t generation) {
    ++generation;
    return generation == 0u ? 1u : generation;
}

inline void audio_stream_ring_reset(AudioStreamRing& ring) {
    ring.read_frame = 0u;
    ring.write_frame = 0u;
    ring.buffered_frames = 0u;
    ring.underrun_count = 0u;
    ring.rejected_write_count = 0u;
    ring.minimum_fill = ring.capacity_frames;
    ring.maximum_fill = 0u;
    ring.paused = 0u;
}

inline void audio_stream_registry_reset(AudioStreamRegistry& registry) {
    for (uint16_t i = 0u; i < kAudioStreamCapacity; ++i) {
        AudioStreamSlot& slot = registry.slots[i];
        slot.generation = next_audio_stream_generation(slot.generation);
        slot.used = 0u;
        slot.ring = {};
        slot.sample_rate = 0u;
        slot.sound_ram_offset = 0u;
        slot.playback_end_frame = 0u;
        slot.playback_start_frame = 0u;
        slot.playback_chunk_frames = 0u;
        slot.serviced_chunks = 0u;
        slot.consumed_frames = 0u;
        slot.refill_count = 0u;
        slot.format = 0u;
        slot.scsp_slot = 0u;
        slot.playback_buffer = 0u;
        slot.hardware_playing = 0u;
        slot.pan = 0u;
        slot.seamless_loop = 0u;
        slot.reserved0 = 0u;
        slot.reserved1 = 0u;
    }
}

inline AudioStreamSlot* audio_stream_resolve(
    AudioStreamRegistry& registry,
    sat_audio_stream_t stream
) {
    if (stream.slot >= kAudioStreamCapacity || stream.generation == 0u) return nullptr;
    AudioStreamSlot& slot = registry.slots[stream.slot];
    if (slot.used == 0u || slot.generation != stream.generation) return nullptr;
    return &slot;
}

inline const AudioStreamSlot* audio_stream_resolve(
    const AudioStreamRegistry& registry,
    sat_audio_stream_t stream
) {
    if (stream.slot >= kAudioStreamCapacity || stream.generation == 0u) return nullptr;
    const AudioStreamSlot& slot = registry.slots[stream.slot];
    if (slot.used == 0u || slot.generation != stream.generation) return nullptr;
    return &slot;
}

inline void audio_stream_update_fill_stats(AudioStreamRing& ring) {
    if (ring.buffered_frames < ring.minimum_fill) ring.minimum_fill = ring.buffered_frames;
    if (ring.buffered_frames > ring.maximum_fill) ring.maximum_fill = ring.buffered_frames;
}

inline void audio_stream_copy_in(
    AudioStreamRing& ring,
    const uint8_t* source,
    uint32_t frame_count
) {
    const uint32_t first_frames =
        frame_count < (ring.capacity_frames - ring.write_frame)
            ? frame_count : (ring.capacity_frames - ring.write_frame);
    const uint32_t first_bytes = first_frames * ring.frame_bytes;
    for (uint32_t i = 0u; i < first_bytes; ++i) {
        ring.buffer[ring.write_frame * ring.frame_bytes + i] = source[i];
    }
    const uint32_t remaining_frames = frame_count - first_frames;
    for (uint32_t i = 0u; i < remaining_frames * ring.frame_bytes; ++i) {
        ring.buffer[i] = source[first_bytes + i];
    }
    ring.write_frame = (ring.write_frame + frame_count) % ring.capacity_frames;
    ring.buffered_frames += frame_count;
    audio_stream_update_fill_stats(ring);
}

inline uint32_t audio_stream_copy_out(
    AudioStreamRing& ring,
    uint8_t* destination,
    uint32_t frame_count
) {
    if (frame_count > ring.buffered_frames) {
        ++ring.underrun_count;
        frame_count = ring.buffered_frames;
    }
    const uint32_t first_frames =
        frame_count < (ring.capacity_frames - ring.read_frame)
            ? frame_count : (ring.capacity_frames - ring.read_frame);
    const uint32_t first_bytes = first_frames * ring.frame_bytes;
    for (uint32_t i = 0u; i < first_bytes; ++i) {
        destination[i] = ring.buffer[ring.read_frame * ring.frame_bytes + i];
    }
    const uint32_t remaining_frames = frame_count - first_frames;
    for (uint32_t i = 0u; i < remaining_frames * ring.frame_bytes; ++i) {
        destination[first_bytes + i] = ring.buffer[i];
    }
    ring.read_frame = (ring.read_frame + frame_count) % ring.capacity_frames;
    ring.buffered_frames -= frame_count;
    audio_stream_update_fill_stats(ring);
    return frame_count;
}

/* Hardware-facing service implemented in audio_stream_api.cpp. Large streams
 * keep one SCSP slot running over a double buffer and refill only the half that
 * has just finished; small caller buffers retain the bounded one-shot fallback.
 * At most one steady-state refill is performed per stream per call. */
void audio_stream_service(
    AudioStreamRegistry& registry,
    uint32_t frame_now,
    uint32_t display_rate = 60u
);
void audio_stream_stop_playback(AudioStreamSlot& slot);

}  // namespace saturn::core

#endif /* SATURN_CORE_AUDIO_STREAM_RUNTIME_HPP */
