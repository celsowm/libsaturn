#include <cassert>
#include <cstdint>
#include <cstdio>
#include "src/audio/playback/state.hpp"
#include "src/core/runtime/state.hpp"
#include "saturn/video.h"

namespace saturn::core {RuntimeState g_state={};}

static uint32_t frame=100u;
static bool configure_ok=false;
static uint32_t key_off_count=0u;
static uint32_t key_on_count=0u;
static uint32_t configure_count=0u;

extern "C" uint32_t sat_frame_count(void) { return frame; }
namespace saturn::hal::scsp {
bool configure_slot(uint8_t, const SlotConfig&) {
    ++configure_count;
    if(!configure_ok)return false;  // Hardware contract: no writes on reject.
    key_off(0u);                    // Mirrors the real configure_slot.
    return true;
}
void key_off(uint8_t) {++key_off_count;}
void key_on(uint8_t) {++key_on_count;}
uint8_t encode_pan(int16_t) {return 0u;}
void set_slot_level_pan(uint8_t,uint8_t,uint8_t,uint8_t) {}
}

int main() {
    using namespace saturn::core::audio::playback;
    g_sound_registry.reset();
    g_voice_registry.reset();
    g_audio_clock.reset();
    g_initialized=1u;
    saturn::core::g_state.config.ntsc=0u; // PAL: expiry after 50 frames.
    auto* sound=g_sound_registry.activate(0u);
    assert(sound);
    sound->sample_count=22050u;
    sound->sample_rate=22050u;
    sound->ram_offset=4096u;
    sound->format=SAT_AUDIO_PCM_S8;
    const sat_sound_t sound_handle{0u,sound->generation};
    // Fill resident slots so the request must steal slot 0.
    for(uint16_t i=0u;i<kResidentVoiceCapacity;++i) {
        auto* old=g_voice_registry.activate(i);
        assert(old);
        old->priority=i==0u?0u:9u;
        old->looping=0u;
        old->start_serial=i;
    }
    const sat_voice_t old_handle{0u,g_voice_registry.entries[0].generation};
    const uint16_t original_generation=old_handle.generation;
    sat_sound_play_params_t params{};
    params.volume=SAT_AUDIO_VOLUME_MAX;
    params.pitch=SAT_FX16_ONE;
    params.priority=1u;
    sat_voice_t new_handle{999u,999u};

    assert(sat_sound_play(sound_handle,&params,&new_handle)==SAT_ERR_UNSUPPORTED);
    assert(configure_count==1u && key_off_count==0u && key_on_count==0u);
    assert(g_voice_registry.resolve(old_handle)!=nullptr);
    assert(g_voice_steals==0u && g_failed_play_requests==1u);
    assert(new_handle.slot==999u && new_handle.generation==999u);

    configure_ok=true;
    assert(sat_sound_play(sound_handle,&params,&new_handle)==SAT_OK);
    assert(configure_count==2u && key_off_count==1u && key_on_count==1u);
    assert(g_voice_steals==1u && g_failed_play_requests==1u);
    assert(new_handle.slot==0u && new_handle.generation!=original_generation);
    assert(g_voice_registry.resolve(old_handle)==nullptr);
    assert(g_voice_registry.resolve(new_handle)!=nullptr);
    assert(g_voice_registry.entries[0].end_frame==150u);
    // After a CD read pumped the audio-service clock, a new play starts
    // from that later service frame rather than the stalled app frame.
    g_audio_clock.valid=1u;
    g_audio_clock.service_frame=200u;
    g_audio_clock.last_app_frame=100u;
    assert(sat_voice_stop(new_handle)==SAT_OK);
    const uint32_t off_after_stop=key_off_count;
    assert(sat_sound_play(sound_handle,&params,&new_handle)==SAT_OK);
    assert(g_voice_registry.entries[0].end_frame==250u);
    assert(g_voice_steals==1u);
    assert(key_off_count==off_after_stop+1u); // configure only; no steal.
    std::puts("audio voice transaction: failed steal leaves old voice alive; PAL/CD OK");
}
