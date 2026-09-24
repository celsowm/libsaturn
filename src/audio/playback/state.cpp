#include "src/audio/playback/state.hpp"

namespace saturn::core::audio::playback {

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

}  // namespace saturn::core::audio::playback
