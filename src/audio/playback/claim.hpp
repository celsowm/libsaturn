#ifndef SATURN_AUDIO_PLAYBACK_CLAIM_HPP
#define SATURN_AUDIO_PLAYBACK_CLAIM_HPP

#include <stdint.h>

/* Slots and Sound RAM claimed by the sampler (src/audio/sampler). Resident voices and streams
 * check here before taking a slot or a buffer; it lives in a header so that the streaming code
 * links without the playback or sampler objects. */
namespace saturn::core::audio::claim {

struct State {
    uint32_t slots;     /* bit n: SCSP slot n */
    uint32_t ram_begin; /* Sound RAM byte offsets, [begin, end) */
    uint32_t ram_end;
};

inline State g_claim = {};

inline bool slot_claimed(uint32_t slot) {
    return slot < 32u && ((g_claim.slots >> slot) & 1u) != 0u;
}

inline bool ram_claimed(uint32_t begin, uint32_t end) {
    return g_claim.ram_begin < g_claim.ram_end && begin < g_claim.ram_end && end > g_claim.ram_begin;
}

/* Streaming rings [0, g_lent_rings) lent to the resident sample area (sat_sound_ram_borrow_stream_rings):
 * resident sounds may lie there, so no stream opens on them. */
inline uint8_t g_lent_rings = 0u;

inline bool ring_lent(uint32_t ring) {
    return ring < g_lent_rings;
}

}  // namespace saturn::core::audio::claim

#endif
