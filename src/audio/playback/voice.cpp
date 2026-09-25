#include "src/audio/playback/state.hpp"
#include "saturn/video.h"
#include "src/core/runtime/state.hpp"

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
    const bool replacing=g_voice_registry.entries[voice_slot].active!=0u;

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
    /* configure_slot validates BEFORE touching SCSP state and keys off the
     * selected slot only on success. Do not invalidate an old voice or count
     * a steal if hardware rejects the replacement. Calling release_voice
     * beforehand made failed play requests destructively stop good audio. */
    if (!saturn::hal::scsp::configure_slot(static_cast<uint8_t>(voice_slot), config)) {
        ++g_failed_play_requests;
        return SAT_ERR_UNSUPPORTED;
    }
    if (replacing) {
        /* Hardware already keyed off during successful configure_slot. */
        (void)g_voice_registry.release(voice_slot);
        ++g_voice_steals;
    }

    VoiceEntry* const activated = g_voice_registry.activate(voice_slot);
    if (activated == nullptr) {
        /* Unreachable while choose_voice() returns a free or released slot,
         * but the registry can refuse; without this check GCC isolates the
         * null path into an abort() call the runtime does not provide. */
        saturn::hal::scsp::key_off(static_cast<uint8_t>(voice_slot));
        ++g_failed_play_requests;
        return SAT_ERR_CAPACITY;
    }
    VoiceEntry& voice = *activated;
    const uint16_t generation=voice.generation;
    voice.looping = entry->loop;
    voice.sound_slot = sound.slot;
    voice.sound_generation = sound.generation;
    voice.priority = priority;
    voice.volume = volume;
    voice.pan = pan;
    voice.start_serial = ++g_start_serial;
    /* Resident voice expiry uses the SAME monotonic service frame as PCM
     * streaming, including VBlank edges sampled during synchronous CD reads.
     * Duration is measured in the actual NTSC/PAL display rate, not a hard-
     * coded 60 Hz assumption. */
    const uint32_t now=sat_frame_count();
    const uint32_t start=saturn::core::audio::voice::lifetime::play_start(
        now,g_audio_clock.service_frame,g_audio_clock.valid!=0u);
    const uint32_t display_rate=
        saturn::core::g_state.config.ntsc!=0u?60u:50u;
    voice.end_frame=start+
        saturn::core::audio::voice::lifetime::duration_frames(
            entry->sample_count,entry->sample_rate,pitch_q16,display_rate);

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
