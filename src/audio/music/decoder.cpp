#include "src/audio/music/internal.hpp"
#include "saturn/music_codecs.h"

namespace saturn::core::music {

uint32_t bytes_per_sample(uint8_t format) {
    if (format == SAT_AUDIO_PCM_S8) return 1u;
    if (format == SAT_AUDIO_PCM_S16) return 2u;
    return 0u;
}

sat_result_t write_channel(MusicSlot& slot, uint32_t stream_index,
                           const uint8_t* frames, uint32_t frame_count) {
    const sat_audio_stream_t target = stream_index == 0u ? slot.stream : slot.stream_r;
    return sat_audio_stream_write(target, frames, frame_count);
}

/* Frames every channel can take: stereo writes both rings in lockstep. */
uint32_t writable_frames(const MusicSlot& slot) {
    uint32_t frames = sat_audio_stream_available(slot.stream);
    if (slot.channels == 2u) {
        const uint32_t available_r = sat_audio_stream_available(slot.stream_r);
        if (available_r < frames) frames = available_r;
    }
    return frames;
}

/* The tracks read from RAM or an asset reader are fed by the program's codec table (saturn/music_codecs.h). */
sat_result_t feed(MusicSlot& slot) {
    if (slot.playing == 0u && slot.prefetch == 0u) return SAT_OK;
    const ResidentFeed* resident = static_cast<const ResidentFeed*>(sat_music_codecs.resident);
    if (resident == nullptr) return SAT_ERR_UNSUPPORTED;
    if (slot.format == SAT_AUDIO_IMA_ADPCM) return resident->ima(slot);
    if (slot.format == SAT_AUDIO_ADX_ADPCM) return resident->adx(slot);
    return resident->pcm(slot);
}

} // namespace saturn::core::music
