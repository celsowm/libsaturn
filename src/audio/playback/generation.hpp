#ifndef SATURN_AUDIO_PLAYBACK_GENERATION_HPP
#define SATURN_AUDIO_PLAYBACK_GENERATION_HPP

#include <stdint.h>

namespace saturn::core::audio {

/* Reusable nonzero 16-bit handle generation. Slot release/reset invalidates
 * outstanding handles; as with any finite generation, reuse after 65535
 * invalidations can wrap, so do not retain stale handles indefinitely. */
inline uint16_t next_generation(uint16_t current) {
    const uint16_t next=static_cast<uint16_t>(current+1u);
    return next==0u?1u:next;
}

}  // namespace saturn::core::audio

#endif
