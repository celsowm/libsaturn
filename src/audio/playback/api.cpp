#include "saturn/audio.h"

#include "saturn/video.h"
#include "src/audio/streaming/runtime.hpp"
#include "src/audio/playback/ram_allocator.hpp"
#include "src/audio/playback/voice_policy.hpp"
#include "src/audio/playback/clock.hpp"
#include "src/audio/playback/sound_registry.hpp"
#include "src/audio/playback/voice_registry.hpp"
#include "src/core/runtime/state.hpp"
#include "src/hal/scsp/scsp.hpp"
#include "src/hal/vdp2/vdp2.hpp"

namespace saturn::core {
void music_runtime_reset();
}

namespace {

constexpr uint16_t kSoundCapacity = 24u;
constexpr uint16_t kVoiceCapacity = 32u;
constexpr uint16_t kResidentVoiceCapacity = kVoiceCapacity - saturn::core::kAudioStreamCapacity;
constexpr uint16_t kAllocationCapacity = 40u;
constexpr uint32_t kSoundRamBase = saturn::hal::scsp::kSystemReservedBytes;
constexpr uint32_t kResidentSoundRamEnd = saturn::core::kAudioStreamRamBase;

using SoundEntry=saturn::core::audio::sound::Entry;

using VoiceEntry=saturn::core::audio::voice::Entry;

saturn::core::audio::ram::Pool<kAllocationCapacity> g_ram = {};
saturn::core::audio::sound::Registry<kSoundCapacity> g_sound_registry = {};
saturn::core::audio::voice::Registry<kVoiceCapacity> g_voice_registry = {};
uint32_t g_voice_steals = 0u;
uint32_t g_failed_play_requests = 0u;
uint32_t g_start_serial = 0u;
uint8_t g_initialized = 0u;
saturn::core::audio::clock::State g_audio_clock = {};

uint8_t volume_to_tl(uint16_t volume) {
    if (volume > SAT_AUDIO_VOLUME_MAX) volume = SAT_AUDIO_VOLUME_MAX;
    if (volume == 0u) return 0xFFu;
    return static_cast<uint8_t>(((SAT_AUDIO_VOLUME_MAX - volume) * 64u + 127u) / 255u);
}

uint32_t duration_frames(uint32_t sample_count, uint32_t sample_rate, uint32_t pitch_q16) {
    if (sample_rate == 0u || pitch_q16 == 0u) return 1u;
    const uint64_t numerator = static_cast<uint64_t>(sample_count) * 60u * 65536u;
    const uint64_t denominator = static_cast<uint64_t>(sample_rate) * pitch_q16;
    uint64_t frames = (numerator + denominator - 1u) / denominator;
    if (frames == 0u) frames = 1u;
    if (frames > 0x7FFFFFFFu) frames = 0x7FFFFFFFu;
    return static_cast<uint32_t>(frames);
}

void release_voice(uint16_t slot) {
    if (slot >= kVoiceCapacity ||
        g_voice_registry.entries[slot].active == 0u) return;
    saturn::hal::scsp::key_off(static_cast<uint8_t>(slot));
    (void)g_voice_registry.release(slot);
}

int32_t choose_voice(uint16_t priority) {
    const int32_t selected=saturn::core::audio::voice::choose(
        g_voice_registry.entries,kResidentVoiceCapacity,priority);
    if(selected>=0 && g_voice_registry.entries[selected].active!=0u)
        ++g_voice_steals;
    return selected;
}

void reset_runtime_state() {
    g_ram.reset();
    g_sound_registry.reset();
    g_voice_registry.reset();
    g_voice_steals = 0u;
    g_failed_play_requests = 0u;
    g_start_serial = 0u;
    g_audio_clock.reset();
}

}  // namespace

extern "C" sat_result_t sat_audio_init(void) {
    if (g_initialized != 0u) return SAT_OK;
    saturn::core::music_runtime_reset();
    saturn::core::audio_stream_registry_reset(saturn::core::g_audio_streams);
    reset_runtime_state();
    if (!saturn::hal::scsp::init()) return SAT_ERR_NOT_INITIALIZED;
    g_initialized = 1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_audio_shutdown(void) {
    if (g_initialized == 0u) return SAT_OK;
    saturn::hal::scsp::shutdown();
    saturn::core::audio_stream_registry_reset(saturn::core::g_audio_streams);
    saturn::core::music_runtime_reset();
    reset_runtime_state();
    g_initialized = 0u;
    return SAT_OK;
}

extern "C" uint8_t sat_audio_is_initialized(void) {
    return g_initialized;
}

extern "C" sat_result_t sat_audio_update(void) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    const uint32_t now = sat_frame_count();
    const uint8_t vblank =
        (saturn::hal::vdp2::read_tvstat() & 0x0008u) != 0u ? 1u : 0u;
    const uint32_t service_frame=g_audio_clock.tick(now,vblank);
    const uint32_t display_rate = saturn::core::g_state.config.ntsc != 0u ? 60u : 50u;
    saturn::core::audio_stream_service(
        saturn::core::g_audio_streams, service_frame, display_rate);
    for (uint16_t i = 0; i < kResidentVoiceCapacity; ++i) {
        VoiceEntry& voice = g_voice_registry.entries[i];
        if (voice.active == 0u || voice.looping != 0u) continue;
        if (static_cast<int32_t>(now - voice.end_frame) >= 0) release_voice(i);
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_audio_set_master_volume(uint16_t volume) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    if (volume > SAT_AUDIO_VOLUME_MAX) volume = SAT_AUDIO_VOLUME_MAX;
    const uint8_t level = static_cast<uint8_t>((volume * 15u + 127u) / 255u);
    saturn::hal::scsp::set_master_volume(level);
    return SAT_OK;
}

extern "C" sat_result_t sat_audio_get_stats(sat_audio_stats_t* out_stats) {
    if (out_stats == nullptr) return SAT_ERR_INVALID_ARG;
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;

    uint16_t sounds = 0u;
    uint16_t voices = 0u;
    sounds=g_sound_registry.active_count();
    voices=g_voice_registry.active_count(kResidentVoiceCapacity);

    out_stats->sound_ram_used = g_ram.used;
    out_stats->sound_ram_capacity = kResidentSoundRamEnd - kSoundRamBase;
    out_stats->sound_ram_high_water = g_ram.high_water;
    out_stats->voice_steals = g_voice_steals;
    out_stats->failed_play_requests = g_failed_play_requests;
    out_stats->resident_sounds = sounds;
    out_stats->resident_sound_capacity = kSoundCapacity;
    out_stats->active_voices = voices;
    out_stats->voice_capacity = kResidentVoiceCapacity;
    return SAT_OK;
}

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

extern "C" sat_result_t sat_sound_play(sat_sound_t sound, const sat_sound_play_params_t* params, sat_voice_t* out_voice) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    SoundEntry* entry = g_sound_registry.resolve(sound);
    if (entry == nullptr) return SAT_ERR_INVALID_ARG;

    uint16_t volume = SAT_AUDIO_VOLUME_MAX;
    int16_t pan = SAT_AUDIO_PAN_CENTER;
    uint16_t priority = 0u;
    uint32_t pitch_q16 = SAT_FX16_ONE;
    if (params != nullptr) {
        volume = params->volume;
        pan = params->pan;
        priority = params->priority;
        if (params->pitch > 0) pitch_q16 = static_cast<uint32_t>(params->pitch);
    }
    if (volume > SAT_AUDIO_VOLUME_MAX) volume = SAT_AUDIO_VOLUME_MAX;
    if (pan < SAT_AUDIO_PAN_LEFT) pan = SAT_AUDIO_PAN_LEFT;
    if (pan > SAT_AUDIO_PAN_RIGHT) pan = SAT_AUDIO_PAN_RIGHT;

    const int32_t selected = choose_voice(priority);
    if (selected < 0) {
        ++g_failed_play_requests;
        return SAT_ERR_CAPACITY;
    }
    const uint16_t voice_slot = static_cast<uint16_t>(selected);
    if (g_voice_registry.entries[voice_slot].active != 0u) release_voice(voice_slot);

    saturn::hal::scsp::SlotConfig config = {};
    config.start_address = entry->ram_offset;
    config.sample_rate = entry->sample_rate;
    config.sample_count = static_cast<uint16_t>(entry->sample_count);
    config.loop_start = entry->loop_start;
    config.loop_end = entry->loop_end;
    config.pitch_scale_q16 = pitch_q16;
    config.pcm8 = entry->format == SAT_AUDIO_PCM_S8 ? 1u : 0u;
    config.loop = entry->loop;
    config.total_level = volume_to_tl(volume);
    config.direct_level = 7u;
    config.pan = saturn::hal::scsp::encode_pan(pan);
    if (!saturn::hal::scsp::configure_slot(static_cast<uint8_t>(voice_slot), config)) {
        ++g_failed_play_requests;
        return SAT_ERR_UNSUPPORTED;
    }

    VoiceEntry& voice = *g_voice_registry.activate(voice_slot);
    const uint16_t generation=voice.generation;
    voice.looping = entry->loop;
    voice.sound_slot = sound.slot;
    voice.sound_generation = sound.generation;
    voice.priority = priority;
    voice.volume = volume;
    voice.pan = pan;
    voice.start_serial = ++g_start_serial;
    voice.end_frame = sat_frame_count() + duration_frames(entry->sample_count, entry->sample_rate, pitch_q16);

    saturn::hal::scsp::key_on(static_cast<uint8_t>(voice_slot));

    if (out_voice != nullptr) {
        out_voice->slot = voice_slot;
        out_voice->generation = generation;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_sound_stop_all_instances(sat_sound_t sound) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    if (g_sound_registry.resolve(sound) == nullptr) return SAT_ERR_INVALID_ARG;
    for (uint16_t i = 0; i < kVoiceCapacity; ++i) {
        VoiceEntry& voice = g_voice_registry.entries[i];
        if (voice.active != 0u && voice.sound_slot == sound.slot && voice.sound_generation == sound.generation) {
            release_voice(i);
        }
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_voice_stop(sat_voice_t voice) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    VoiceEntry* entry = g_voice_registry.resolve(voice);
    if (entry == nullptr) return SAT_ERR_INVALID_ARG;
    release_voice(voice.slot);
    return SAT_OK;
}

extern "C" sat_result_t sat_voice_set_volume(sat_voice_t voice, uint16_t volume) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    VoiceEntry* entry = g_voice_registry.resolve(voice);
    if (entry == nullptr) return SAT_ERR_INVALID_ARG;
    if (volume > SAT_AUDIO_VOLUME_MAX) volume = SAT_AUDIO_VOLUME_MAX;
    entry->volume = volume;
    saturn::hal::scsp::set_slot_level_pan(
        static_cast<uint8_t>(voice.slot), volume_to_tl(volume), 7u,
        saturn::hal::scsp::encode_pan(entry->pan)
    );
    return SAT_OK;
}

extern "C" sat_result_t sat_voice_set_pan(sat_voice_t voice, int16_t pan) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    VoiceEntry* entry = g_voice_registry.resolve(voice);
    if (entry == nullptr) return SAT_ERR_INVALID_ARG;
    if (pan < SAT_AUDIO_PAN_LEFT) pan = SAT_AUDIO_PAN_LEFT;
    if (pan > SAT_AUDIO_PAN_RIGHT) pan = SAT_AUDIO_PAN_RIGHT;
    entry->pan = pan;
    saturn::hal::scsp::set_slot_level_pan(
        static_cast<uint8_t>(voice.slot), volume_to_tl(entry->volume), 7u,
        saturn::hal::scsp::encode_pan(pan)
    );
    return SAT_OK;
}

extern "C" uint8_t sat_voice_is_playing(sat_voice_t voice) {
    if (g_initialized == 0u) return 0u;
    return g_voice_registry.resolve(voice) != nullptr ? 1u : 0u;
}
