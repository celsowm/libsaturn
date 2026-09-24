#ifndef SATURN_AUDIO_PLAYBACK_VOICE_REGISTRY_HPP
#define SATURN_AUDIO_PLAYBACK_VOICE_REGISTRY_HPP

#include <stdint.h>
#include "saturn/audio.h"
#include "src/audio/playback/generation.hpp"

/* Pure caller-owned voice identity/lifetime registry. Physical key on/off,
 * SCSP slot configuration, steal counting and frame scheduling are owned by
 * the playback facade, never by these handle operations. */
namespace saturn::core::audio::voice {

struct Entry {
    uint32_t end_frame;
    uint32_t start_serial;
    uint16_t generation;
    uint16_t sound_slot;
    uint16_t sound_generation;
    uint16_t priority;
    uint16_t volume;
    int16_t pan;
    uint8_t active;
    uint8_t looping;
};

template <uint16_t Capacity>
struct Registry {
    Entry entries[Capacity];

    void reset() {
        for(uint16_t i=0u;i<Capacity;++i) {
            const uint16_t generation=saturn::core::audio::next_generation(
                entries[i].generation);
            entries[i]={};
            entries[i].generation=generation;
        }
    }

    Entry* resolve(sat_voice_t voice) {
        if(voice.slot>=Capacity)return nullptr;
        Entry& entry=entries[voice.slot];
        return entry.active!=0u && entry.generation==voice.generation
            ? &entry:nullptr;
    }

    uint16_t active_count(uint16_t limit) const {
        const uint16_t count=limit<Capacity?limit:Capacity;
        uint16_t total=0u;
        for(uint16_t i=0u;i<count;++i)
            total+=entries[i].active!=0u?1u:0u;
        return total;
    }

    /* Called after a successful hardware configuration; the selected slot
     * must have been keyed off first if it was previously active. */
    Entry* activate(uint16_t slot) {
        if(slot>=Capacity || entries[slot].active)return nullptr;
        const uint16_t generation=saturn::core::audio::next_generation(
            entries[slot].generation);
        entries[slot]={};
        entries[slot].generation=generation;
        entries[slot].active=1u;
        return &entries[slot];
    }

    /* The facade keys off the physical voice BEFORE releasing its handle. */
    bool release(uint16_t slot) {
        if(slot>=Capacity || !entries[slot].active)return false;
        entries[slot].active=0u;
        return true;
    }
};

}  // namespace saturn::core::audio::voice

#endif
