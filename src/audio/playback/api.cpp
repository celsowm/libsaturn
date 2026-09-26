#include "src/audio/playback/state.hpp"
#include "saturn/video.h"
#include "src/core/runtime/state.hpp"
#include "src/hal/vdp2/vdp2.hpp"
#include "saturn/scsp_dsp.h"

namespace saturn::core {
void music_runtime_reset();
}

using namespace saturn::core::audio::playback;

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
    (void)sat_effect_stop();
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
        if (saturn::core::audio::voice::lifetime::expired(
                service_frame,voice.end_frame)) release_voice(i);
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

