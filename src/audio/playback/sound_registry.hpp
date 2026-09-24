#ifndef SATURN_AUDIO_PLAYBACK_SOUND_REGISTRY_HPP
#define SATURN_AUDIO_PLAYBACK_SOUND_REGISTRY_HPP

#include <stdint.h>
#include "saturn/audio.h"
#include "src/audio/playback/generation.hpp"

/* Pure bounded sound-handle registry. The caller owns the storage and the
 * SCSP upload/release transaction; this registry only owns slot lifetimes,
 * generation invalidation, and metadata. No heap, clock, or hardware calls. */
namespace saturn::core::audio::sound {

struct Entry {
    uint32_t ram_offset;
    uint32_t byte_count;
    uint32_t sample_count;
    uint32_t sample_rate;
    uint16_t loop_start;
    uint16_t loop_end;
    uint16_t generation;
    uint16_t allocation_slot;
    uint8_t format;
    uint8_t loop;
    uint8_t used;
    uint8_t reserved;
};

template <uint16_t Capacity>
struct Registry {
    Entry entries[Capacity];

    void reset() {
        for(uint16_t i=0u;i<Capacity;++i) {
            const uint16_t generation=saturn::core::audio::next_generation(entries[i].generation);
            entries[i]={};
            entries[i].generation=generation;
        }
    }

    uint16_t first_free() const {
        for(uint16_t i=0u;i<Capacity;++i)
            if(!entries[i].used)return i;
        return Capacity;
    }

    uint16_t active_count() const {
        uint16_t total=0u;
        for(uint16_t i=0u;i<Capacity;++i)
            total+=entries[i].used!=0u?1u:0u;
        return total;
    }

    Entry* resolve(sat_sound_t sound) {
        if(sound.slot>=Capacity)return nullptr;
        Entry& entry=entries[sound.slot];
        return entry.used!=0u && entry.generation==sound.generation
            ? &entry:nullptr;
    }

    /* Activate only an unoccupied slot, AFTER the caller has successfully
     * reserved and uploaded the PCM; failure paths never consume a handle. */
    Entry* activate(uint16_t slot) {
        if(slot>=Capacity || entries[slot].used)return nullptr;
        const uint16_t generation=saturn::core::audio::next_generation(entries[slot].generation);
        entries[slot]={};
        entries[slot].used=1u;
        entries[slot].generation=generation;
        return &entries[slot];
    }

    /* Existing voices must be keyed off and RAM released by the caller
     * BEFORE invalidation, while the old generation is still resolvable. */
    bool invalidate(sat_sound_t sound) {
        Entry* entry=resolve(sound);
        if(!entry)return false;
        const uint16_t generation=saturn::core::audio::next_generation(entry->generation);
        *entry={};
        entry->generation=generation;
        return true;
    }
};

}  // namespace saturn::core::audio::sound

#endif
