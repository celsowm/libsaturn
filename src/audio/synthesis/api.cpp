#include "saturn/audio.h"

#include "src/audio/synthesis/logic.hpp"

using namespace saturn::core::audio_synth;

extern "C" sat_result_t sat_audio_synth_blip(
    int8_t* out, uint32_t count, uint8_t step) {
    return blip(out, count, step);
}

extern "C" sat_result_t sat_audio_synth_noise(int8_t* out, uint32_t count) {
    return noise(out, count);
}

extern "C" sat_result_t sat_audio_synth_arpeggio_note(
    int8_t* out, uint32_t count, uint8_t period, uint32_t bass_phase) {
    return arpeggio_note(out, count, period, bass_phase);
}
