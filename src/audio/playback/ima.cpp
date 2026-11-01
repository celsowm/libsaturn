#include "saturn/audio.h"

#include "src/audio/playback/ima_adpcm.hpp"

extern "C" sat_result_t sat_audio_ima_decode_block(const void* block, uint8_t channels, void* out_planar_s16be) {
    if (block == nullptr || out_planar_s16be == nullptr) return SAT_ERR_INVALID_ARG;
    return saturn::core::audio::ima_adpcm::decode_block(static_cast<const uint8_t*>(block), channels,
                                                       static_cast<uint8_t*>(out_planar_s16be))
               ? SAT_OK : SAT_ERR_INVALID_ARG;
}
