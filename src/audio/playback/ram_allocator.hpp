#ifndef SATURN_AUDIO_PLAYBACK_RAM_ALLOCATOR_HPP
#define SATURN_AUDIO_PLAYBACK_RAM_ALLOCATOR_HPP

#include <stdint.h>

/* Bounded, caller-owned Sound RAM range allocator. No SCSP/voice dependency,
 * heap, constructors or global state. The candidate only advances at an
 * overlapping range's end, so fragmentation cannot trap the search. */
namespace saturn::core::audio::ram {

struct Entry {
    uint32_t offset;
    uint32_t size;
    uint8_t used;
};

template <uint16_t Capacity>
struct Pool {
    Entry entries[Capacity];
    uint32_t used;
    uint32_t high_water;

    void reset() {
        for (uint16_t i=0;i<Capacity;++i) entries[i]={};
        used=0u;
        high_water=0u;
    }

    bool allocate(uint32_t begin,uint32_t end,uint32_t size,
                  uint32_t alignment,uint16_t* out_slot,uint32_t* out_offset) {
        if (!out_slot || !out_offset || !size || !alignment ||
            (alignment & (alignment-1u))!=0u || begin>end ||
            size>end-begin || begin>UINT32_MAX-(alignment-1u))
            return false;
        uint16_t free_slot=Capacity;
        for (uint16_t i=0;i<Capacity;++i) {
            if (!entries[i].used) {free_slot=i;break;}
        }
        if (free_slot==Capacity) return false;
        uint32_t candidate=(begin+alignment-1u)&~(alignment-1u);
        while (candidate<=end && size<=end-candidate) {
            bool collision=false;
            uint32_t next=candidate;
            for (uint16_t i=0;i<Capacity;++i) {
                const Entry& entry=entries[i];
                if (!entry.used) continue;
                /* Valid owned ranges cannot wrap the end of the RAM pool. */
                if (entry.offset<end && entry.size<=end-entry.offset &&
                    candidate<entry.offset+entry.size &&
                    entry.offset<candidate+size) {
                    const uint32_t block_end=entry.offset+entry.size;
                    if (block_end>UINT32_MAX-(alignment-1u)) return false;
                    const uint32_t aligned=(block_end+alignment-1u)&~(alignment-1u);
                    if (aligned>next) next=aligned;
                    collision=true;
                }
            }
            if (!collision) {
                entries[free_slot]={candidate,size,1u};
                used+=size;
                if (used>high_water) high_water=used;
                *out_slot=free_slot;
                *out_offset=candidate;
                return true;
            }
            if (next<=candidate) return false;
            candidate=next;
        }
        return false;
    }

    bool release(uint16_t slot) {
        if (slot>=Capacity || !entries[slot].used) return false;
        used-=entries[slot].size;
        entries[slot]={};
        return true;
    }
};

}  // namespace saturn::core::audio::ram

#endif
