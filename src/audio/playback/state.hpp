#ifndef SATURN_AUDIO_PLAYBACK_STATE_HPP
#define SATURN_AUDIO_PLAYBACK_STATE_HPP

#include <stdint.h>
#include "saturn/audio.h"
#include "src/audio/streaming/runtime.hpp"
#include "src/audio/playback/ram_allocator.hpp"
#include "src/audio/playback/voice_policy.hpp"
#include "src/audio/playback/clock.hpp"
#include "src/audio/playback/sound_registry.hpp"
#include "src/audio/playback/voice_registry.hpp"
#include "src/hal/scsp/scsp.hpp"

/* Private wiring for the resident sound playback service. This is deliberately
 * not a public abstraction: independent pure policies retain their separate
 * headers, while the SCSP service composes storage and hardware transactions. */
namespace saturn::core::audio::playback {

constexpr uint16_t kSoundCapacity=24u;
constexpr uint16_t kVoiceCapacity=32u;
constexpr uint16_t kResidentVoiceCapacity=
    kVoiceCapacity-saturn::core::kAudioStreamCapacity;
constexpr uint16_t kAllocationCapacity=40u;
constexpr uint32_t kSoundRamBase=saturn::hal::scsp::kSystemReservedBytes;
constexpr uint32_t kResidentSoundRamEnd=saturn::core::kAudioStreamRamBase;

using SoundEntry=saturn::core::audio::sound::Entry;
using VoiceEntry=saturn::core::audio::voice::Entry;

extern saturn::core::audio::ram::Pool<kAllocationCapacity> g_ram;
extern saturn::core::audio::sound::Registry<kSoundCapacity> g_sound_registry;
extern saturn::core::audio::voice::Registry<kVoiceCapacity> g_voice_registry;
extern uint32_t g_voice_steals;
extern uint32_t g_failed_play_requests;
extern uint32_t g_start_serial;
extern uint8_t g_initialized;
extern saturn::core::audio::clock::State g_audio_clock;

uint8_t volume_to_tl(uint16_t volume);
uint32_t duration_frames(uint32_t sample_count,uint32_t sample_rate,uint32_t pitch_q16);
void release_voice(uint16_t slot);
int32_t choose_voice(uint16_t priority);
void reset_runtime_state();

}  // namespace saturn::core::audio::playback

#endif
