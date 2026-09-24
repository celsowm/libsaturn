#include "src/audio/playback/state.hpp"
#include "saturn/video.h"

using namespace saturn::core::audio::playback;

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
