#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "saturn/scsp_dsp.h"
#include "src/audio/playback/state.hpp"
#include "src/hal/scsp/dsp.hpp"
#include "src/hal/scsp/dsp_logic.hpp"
#include "src/hal/scsp/scsp_dsp_presets.h"

namespace dsp = saturn::hal::scsp::dsp;
using namespace saturn::core::audio::playback;

// What the stubbed hardware saw.
static dsp::Program g_loaded = {};
static uint16_t g_coef[64];
static uint16_t g_madrs[32];
static uint32_t g_loads = 0u, g_stops = 0u, g_coef_writes = 0u;
static uint8_t g_return_level = 0xFFu, g_return_pan = 0xFFu, g_default_send = 0xFFu;
static uint8_t g_send_slot = 0xFFu, g_send_level = 0xFFu;
static bool g_load_ok = true;

namespace saturn::hal::scsp {
void mute_slot(uint8_t) {}
void key_off(uint8_t) {}
uint8_t encode_pan(int16_t pan) { return static_cast<uint8_t>(pan & 0x1F); }
void set_effect_return(uint8_t efreg, uint8_t level, uint8_t pan) {
    assert(efreg == 0u);
    g_return_level = level;
    g_return_pan = pan;
}
uint16_t sound_word(uint32_t byte_offset) { return static_cast<uint16_t>(byte_offset >> 1); }
void set_default_effect_send(uint8_t level) { g_default_send = level; }
void set_effect_send(uint8_t slot, uint8_t level) {
    g_send_slot = slot;
    g_send_level = level;
}
namespace dsp {
bool load(const Program& program) {
    if (!g_load_ok) return false;
    ++g_loads;
    g_loaded = program;
    std::memcpy(g_coef, program.coef, sizeof(g_coef));
    std::memcpy(g_madrs, program.madrs, sizeof(g_madrs));
    return true;
}
void stop() { ++g_stops; }
bool set_coef(uint32_t index, uint16_t value) {
    if (index >= 64u) return false;
    g_coef[index] = value;
    ++g_coef_writes;
    return true;
}
}  // namespace dsp
}  // namespace saturn::hal::scsp

int main() {
    g_ram.reset();
    g_voice_registry.reset();
    sat_effect_info_t info = {};

    // not usable before the audio runtime is up
    g_initialized = 0u;
    sat_effect_echo_params_t echo = {50u, 0x8000, 0x9999, 0x8000};
    assert(sat_effect_echo_start(&echo) == SAT_ERR_NOT_INITIALIZED);
    g_initialized = 1u;

    // echo: 50 ms is 2205 samples, an 8K-word ring, 8 KiB aligned, taken from the sound pool
    assert(sat_effect_echo_start(&echo) == SAT_OK);
    assert(g_loads == 1u && g_loaded.step_count == kScspDspEchoSteps && std::memcmp(g_loaded.steps, kScspDspEchoProgram, sizeof(kScspDspEchoProgram)) == 0);
    assert(g_loaded.ring_length == 0u && g_loaded.ring_offset % 0x2000u == 0u);
    assert(g_loaded.ring_offset >= saturn::hal::scsp::kSystemReservedBytes);
    assert(g_madrs[kScspDspMadrsEchoRead] == 2205u && g_madrs[kScspDspMadrsEchoWrite] == 0u);
    assert(g_coef[kScspDspCoefInput] == 0x0800u);
    assert(g_coef[kScspDspCoefFeedback] == 0x0800u);
    assert(g_coef[kScspDspCoefWet] == 0x0999u);
    assert(g_coef[kScspDspCoefZero] == 0u);
    assert(g_ram.used == 0x4000u);
    assert(g_return_level == 7u);
    assert(sat_effect_info(&info) == SAT_OK && info.kind == SAT_EFFECT_ECHO && info.delay_samples == 2205u);
    assert(info.ring_offset == g_loaded.ring_offset && info.ring_bytes == 0x4000u);

    // a long delay takes a bigger ring
    echo.delay_ms = 1000u;
    assert(sat_effect_echo_start(&echo) == SAT_OK);
    assert(g_loaded.ring_length == 3u && g_madrs[kScspDspMadrsEchoRead] == 44100u);
    assert(g_stops == 1u);                        // the first effect was stopped, and its ring freed
    assert(g_ram.used == 0x20000u);

    // live gains
    assert(sat_effect_set_gains(0x4000, 0x4000, 0x4000) == SAT_OK && g_coef_writes == 3u);
    assert(g_coef[kScspDspCoefFeedback] == 0x0400u);
    assert(sat_effect_set_gains(-1, 0, 0) == SAT_ERR_INVALID_ARG);

    // reverb: four combs at fixed delays, a 16K-word ring
    sat_effect_reverb_params_t reverb = {0xC7AE, 0x4000, 0x5999};
    assert(sat_effect_reverb_start(&reverb) == SAT_OK);
    assert(std::memcmp(g_loaded.steps, kScspDspReverbProgram, sizeof(kScspDspReverbProgram)) == 0 && g_loaded.ring_length == kScspDspReverbRingLength);
    for (unsigned i = 0u; i < kScspDspReverbCombs; ++i) {
        assert(g_madrs[i] == i * kScspDspReverbSpacing + kScspDspReverbDelays[i]);
        assert(g_madrs[kScspDspReverbCombs + i] == i * kScspDspReverbSpacing);
    }
    assert(g_ram.used == 0x8000u);
    assert(sat_effect_info(&info) == SAT_OK && info.kind == SAT_EFFECT_REVERB);

    // invalid arguments change nothing
    const uint32_t loads = g_loads;
    echo.delay_ms = 0u;
    assert(sat_effect_echo_start(&echo) == SAT_ERR_INVALID_ARG);
    echo.delay_ms = 1001u;
    assert(sat_effect_echo_start(&echo) == SAT_ERR_INVALID_ARG);
    echo.delay_ms = 50u;
    echo.feedback = -1;
    assert(sat_effect_echo_start(&echo) == SAT_ERR_INVALID_ARG);
    assert(sat_effect_echo_start(nullptr) == SAT_ERR_INVALID_ARG && sat_effect_reverb_start(nullptr) == SAT_ERR_INVALID_ARG);
    assert(g_loads == loads);
    assert(sat_effect_info(&info) == SAT_OK && info.kind == SAT_EFFECT_REVERB);

    // a failed load frees the ring it took, and the previous effect is already gone
    g_load_ok = false;
    echo.feedback = 0x8000;
    assert(sat_effect_echo_start(&echo) == SAT_ERR_INVALID_ARG);
    g_load_ok = true;
    assert(g_ram.used == 0u);
    assert(sat_effect_info(&info) == SAT_OK && info.kind == SAT_EFFECT_NONE && info.ring_bytes == 0u);

    // routing
    assert(sat_effect_set_default_send(4u) == SAT_OK && g_default_send == 4u);
    assert(sat_effect_set_default_send(8u) == SAT_ERR_INVALID_ARG);
    assert(sat_effect_set_return(5u, -3) == SAT_OK);
    assert(sat_effect_set_return(8u, 0) == SAT_ERR_INVALID_ARG);
    assert(sat_effect_set_gains(0x4000, 0x4000, 0x4000) == SAT_ERR_NOT_INITIALIZED);   // nothing runs
    assert(sat_effect_echo_start(&echo) == SAT_OK);
    assert(g_return_level == 5u && g_return_pan == 0x1Du);
    assert(sat_effect_set_return(2u, 4) == SAT_OK && g_return_level == 2u && g_return_pan == 4u);

    auto* voice = g_voice_registry.activate(3u);
    assert(voice != nullptr);
    const sat_voice_t handle{3u, voice->generation};
    assert(sat_voice_set_effect_send(handle, 6u) == SAT_OK && g_send_slot == 3u && g_send_level == 6u);
    assert(sat_voice_set_effect_send(handle, 8u) == SAT_ERR_INVALID_ARG);
    const sat_voice_t stale{3u, static_cast<uint16_t>(voice->generation + 1u)};
    assert(sat_voice_set_effect_send(stale, 1u) == SAT_ERR_INVALID_ARG && g_send_level == 6u);

    // the delay line can be read while the effect runs (the stub returns the word address)
    int16_t line[4] = {};
    assert(sat_effect_read_ring(0u, line, 4u) == SAT_OK);
    assert(line[0] == static_cast<int16_t>(g_loaded.ring_offset >> 1) && line[3] == line[0] + 3);
    assert(sat_effect_read_ring(8190u, line, 4u) == SAT_ERR_INVALID_ARG);   // past the 8K-word ring
    assert(sat_effect_read_ring(8188u, line, 4u) == SAT_OK);
    assert(sat_effect_read_ring(0u, nullptr, 1u) == SAT_ERR_INVALID_ARG);

    // stopping silences the return and frees the ring; it is safe to repeat
    assert(sat_effect_stop() == SAT_OK && g_return_level == 0u && g_ram.used == 0u);
    assert(sat_effect_read_ring(0u, line, 1u) == SAT_ERR_NOT_INITIALIZED);
    const uint32_t stops = g_stops;
    assert(sat_effect_stop() == SAT_OK && g_stops == stops);

    std::puts("PASS: test_scsp_dsp_api.cpp");
    return 0;
}
