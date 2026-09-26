#include "saturn/scsp_dsp.h"

#include "src/audio/playback/state.hpp"
#include "src/hal/scsp/dsp.hpp"
#include "src/hal/scsp/dsp_logic.hpp"
#include "src/hal/scsp/scsp.hpp"
#include "src/hal/scsp/scsp_dsp_presets.h"

namespace {

using namespace saturn::core::audio::playback;
namespace logic = saturn::hal::scsp::dsp_logic;
namespace dsp = saturn::hal::scsp::dsp;
namespace scsp = saturn::hal::scsp;

constexpr uint8_t kReturnEfreg = 0u;

struct EffectState {
    uint8_t kind;
    uint8_t ring_length;
    uint8_t default_send;
    uint8_t return_level;
    int16_t return_pan;
    uint16_t ring_slot;
    uint32_t ring_offset;
    uint32_t delay_samples;
};

EffectState g_effect = {0u, 0u, 0u, 7u, 0, 0u, 0u, 0u};

void apply_return() {
    scsp::set_effect_return(kReturnEfreg, g_effect.kind != SAT_EFFECT_NONE ? g_effect.return_level : 0u,
                            scsp::encode_pan(g_effect.return_pan));
}

sat_result_t launch(uint8_t kind, const uint16_t (*steps)[4], uint32_t step_count, const uint16_t* coef,
                    const uint16_t* madrs, uint8_t ring_length, uint32_t delay_samples) {
    (void)sat_effect_stop();
    uint16_t allocation = 0u;
    uint32_t offset = 0u;
    if (!g_ram.allocate(kSoundRamBase, kResidentSoundRamEnd, logic::ring_bytes(ring_length),
                        logic::kRingUnitBytes, &allocation, &offset)) {
        return SAT_ERR_CAPACITY;
    }
    dsp::Program program = {steps, step_count, coef, madrs, ring_length, offset};
    if (!dsp::load(program)) {
        (void)g_ram.release(allocation);
        return SAT_ERR_INVALID_ARG;
    }
    g_effect.kind = kind;
    g_effect.ring_length = ring_length;
    g_effect.ring_slot = allocation;
    g_effect.ring_offset = offset;
    g_effect.delay_samples = delay_samples;
    apply_return();
    return SAT_OK;
}

void fill_gains(uint16_t* coef, sat_fx16_t feedback, sat_fx16_t wet, sat_fx16_t input_gain) {
    coef[kScspDspCoefInput] = logic::coefficient_from_fx16(input_gain);
    coef[kScspDspCoefFeedback] = logic::coefficient_from_fx16(feedback);
    coef[kScspDspCoefWet] = logic::coefficient_from_fx16(wet);
}

bool gains_ok(sat_fx16_t feedback, sat_fx16_t wet, sat_fx16_t input_gain) {
    return feedback >= 0 && wet >= 0 && input_gain >= 0;
}

}  // namespace

extern "C" sat_result_t sat_effect_echo_start(const sat_effect_echo_params_t* params) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    if (params == nullptr || params->delay_ms == 0u || params->delay_ms > 1000u ||
        !gains_ok(params->feedback, params->wet, params->input_gain)) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint32_t delay = logic::samples_from_ms(params->delay_ms);
    const uint8_t ring = logic::ring_length_for_delay(delay);
    if (ring >= logic::kRingLengthCodes) return SAT_ERR_INVALID_ARG;
    uint16_t coef[logic::kCoefCount] = {};
    uint16_t madrs[logic::kMadrsCount] = {};
    fill_gains(coef, params->feedback, params->wet, params->input_gain);
    madrs[kScspDspMadrsEchoRead] = static_cast<uint16_t>(delay);
    madrs[kScspDspMadrsEchoWrite] = 0u;
    return launch(SAT_EFFECT_ECHO, kScspDspEchoProgram, kScspDspEchoSteps, coef, madrs, ring, delay);
}

extern "C" sat_result_t sat_effect_reverb_start(const sat_effect_reverb_params_t* params) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    if (params == nullptr || !gains_ok(params->feedback, params->wet, params->input_gain)) {
        return SAT_ERR_INVALID_ARG;
    }
    uint16_t coef[logic::kCoefCount] = {};
    uint16_t madrs[logic::kMadrsCount] = {};
    fill_gains(coef, params->feedback, params->wet, params->input_gain);
    for (uint32_t i = 0u; i < kScspDspReverbCombs; ++i) {
        madrs[i] = static_cast<uint16_t>(i * kScspDspReverbSpacing + kScspDspReverbDelays[i]);
        madrs[kScspDspReverbCombs + i] = static_cast<uint16_t>(i * kScspDspReverbSpacing);
    }
    return launch(SAT_EFFECT_REVERB, kScspDspReverbProgram, kScspDspReverbSteps, coef, madrs,
                  kScspDspReverbRingLength, kScspDspReverbDelays[0]);
}

extern "C" sat_result_t sat_effect_stop(void) {
    if (g_effect.kind == SAT_EFFECT_NONE) return SAT_OK;
    dsp::stop();
    g_effect.kind = SAT_EFFECT_NONE;
    apply_return();
    (void)g_ram.release(g_effect.ring_slot);
    g_effect.ring_slot = 0u;
    g_effect.ring_offset = 0u;
    g_effect.delay_samples = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_effect_set_gains(sat_fx16_t feedback, sat_fx16_t wet, sat_fx16_t input_gain) {
    if (g_effect.kind == SAT_EFFECT_NONE) return SAT_ERR_NOT_INITIALIZED;
    if (!gains_ok(feedback, wet, input_gain)) return SAT_ERR_INVALID_ARG;
    return dsp::set_coef(kScspDspCoefInput, logic::coefficient_from_fx16(input_gain)) &&
                   dsp::set_coef(kScspDspCoefFeedback, logic::coefficient_from_fx16(feedback)) &&
                   dsp::set_coef(kScspDspCoefWet, logic::coefficient_from_fx16(wet))
               ? SAT_OK
               : SAT_ERR_INVALID_ARG;
}

extern "C" sat_result_t sat_effect_set_default_send(uint8_t level) {
    if (level > logic::kMaxLevel) return SAT_ERR_INVALID_ARG;
    g_effect.default_send = level;
    scsp::set_default_effect_send(level);
    return SAT_OK;
}

extern "C" sat_result_t sat_voice_set_effect_send(sat_voice_t voice, uint8_t level) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    if (level > logic::kMaxLevel || g_voice_registry.resolve(voice) == nullptr) return SAT_ERR_INVALID_ARG;
    scsp::set_effect_send(static_cast<uint8_t>(voice.slot), level);
    return SAT_OK;
}

extern "C" sat_result_t sat_effect_set_return(uint8_t level, int16_t pan) {
    if (level > logic::kMaxLevel) return SAT_ERR_INVALID_ARG;
    if (pan < SAT_AUDIO_PAN_LEFT) pan = SAT_AUDIO_PAN_LEFT;
    if (pan > SAT_AUDIO_PAN_RIGHT) pan = SAT_AUDIO_PAN_RIGHT;
    g_effect.return_level = level;
    g_effect.return_pan = pan;
    if (g_effect.kind != SAT_EFFECT_NONE) apply_return();
    return SAT_OK;
}

extern "C" sat_result_t sat_effect_info(sat_effect_info_t* out_info) {
    if (out_info == nullptr) return SAT_ERR_INVALID_ARG;
    *out_info = {};
    out_info->kind = g_effect.kind;
    out_info->ring_length = g_effect.ring_length;
    out_info->default_send = g_effect.default_send;
    out_info->return_level = g_effect.return_level;
    out_info->ring_offset = g_effect.ring_offset;
    out_info->ring_bytes = g_effect.kind != SAT_EFFECT_NONE ? logic::ring_bytes(g_effect.ring_length) : 0u;
    out_info->delay_samples = g_effect.delay_samples;
    return SAT_OK;
}

extern "C" sat_result_t sat_effect_read_ring(uint32_t first_word, int16_t* out, uint32_t count) {
    if (out == nullptr) return SAT_ERR_INVALID_ARG;
    if (g_effect.kind == SAT_EFFECT_NONE) return SAT_ERR_NOT_INITIALIZED;
    const uint32_t words = logic::ring_words(g_effect.ring_length);
    if (first_word > words || count > words - first_word) return SAT_ERR_INVALID_ARG;
    for (uint32_t i = 0u; i < count; ++i) {
        out[i] = static_cast<int16_t>(scsp::sound_word(g_effect.ring_offset + (first_word + i) * 2u));
    }
    return SAT_OK;
}
