#include "src/audio/playback/state.hpp"

using namespace saturn::core::audio::playback;

extern "C" sat_result_t sat_sound_create(sat_sound_t* out_sound, const sat_sound_desc_t* desc) {
    if (out_sound == nullptr || desc == nullptr || desc->samples == nullptr) return SAT_ERR_INVALID_ARG;
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    if (desc->sample_count == 0u || desc->sample_count > 65535u || desc->sample_rate == 0u) return SAT_ERR_INVALID_ARG;
    if (desc->format != SAT_AUDIO_PCM_S8 && desc->format != SAT_AUDIO_PCM_S16) return SAT_ERR_UNSUPPORTED;

    const uint16_t sound_slot=g_sound_registry.first_free();
    if (sound_slot == kSoundCapacity) return SAT_ERR_CAPACITY;

    const uint32_t bytes_per_sample = desc->format == SAT_AUDIO_PCM_S16 ? 2u : 1u;
    const uint32_t byte_count = desc->sample_count * bytes_per_sample;
    uint16_t allocation_slot = 0u;
    uint32_t offset = 0u;
    if (!g_ram.allocate(kSoundRamBase,kResidentSoundRamEnd,byte_count,
                        2u,&allocation_slot,&offset)) return SAT_ERR_CAPACITY;

    if (!saturn::hal::scsp::upload(offset, desc->samples, byte_count)) {
        (void)g_ram.release(allocation_slot);
        return SAT_ERR_INVALID_ARG;
    }

    SoundEntry& entry = *g_sound_registry.activate(sound_slot);
    const uint16_t generation=entry.generation;
    entry.ram_offset = offset;
    entry.byte_count = byte_count;
    entry.sample_count = desc->sample_count;
    entry.sample_rate = desc->sample_rate;
    entry.format = desc->format;
    entry.loop = desc->loop != 0u ? 1u : 0u;
    entry.loop_start = desc->loop_start;
    entry.loop_end = desc->loop_end;
    entry.allocation_slot = allocation_slot;

    if (entry.loop != 0u) {
        if (entry.loop_end == 0u || entry.loop_end >= entry.sample_count) {
            entry.loop_end = static_cast<uint16_t>(entry.sample_count - 1u);
        }
        if (entry.loop_start >= entry.loop_end) entry.loop_start = 0u;
    } else {
        entry.loop_start = 0u;
        entry.loop_end = static_cast<uint16_t>(entry.sample_count - 1u);
    }

    out_sound->slot = sound_slot;
    out_sound->generation = generation;
    return SAT_OK;
}

extern "C" sat_result_t sat_sound_unload(sat_sound_t sound) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    SoundEntry* entry = g_sound_registry.resolve(sound);
    if (entry == nullptr) return SAT_ERR_INVALID_ARG;

    sat_sound_stop_all_instances(sound);
    (void)g_ram.release(entry->allocation_slot);
    (void)g_sound_registry.invalidate(sound);
    return SAT_OK;
}

