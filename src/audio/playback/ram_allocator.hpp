#ifndef SATURN_AUDIO_PLAYBACK_RAM_ALLOCATOR_HPP
#define SATURN_AUDIO_PLAYBACK_RAM_ALLOCATOR_HPP

#include <stdint.h>

/* Heap-free first-fit Sound RAM allocator. Stable allocation slot handles
 * are independent of the ordered index: only the index is shifted on
 * insertion/removal. Find-gap is O(live allocations), not one full scan per
 * hole. The caller owns all storage and policy/VRAM remains elsewhere. */
namespace saturn::core::audio::ram {

struct Entry {
    uint32_t offset;
    uint32_t size;
    uint8_t used;
};

template <uint16_t Capacity>
struct Pool {
    Entry entries[Capacity];
    uint16_t order[Capacity]; /* Live slot IDs in ascending offset order. */
    uint16_t live_count;
    uint32_t used;
    uint32_t high_water;

    void reset() {
        for(uint16_t i=0u;i<Capacity;++i) entries[i]={};
        live_count=0u;
        used=0u;
        high_water=0u;
    }

    bool allocate(uint32_t begin,uint32_t end,uint32_t size,
                  uint32_t alignment,uint16_t* out_slot,uint32_t* out_offset) {
        if(!out_slot||!out_offset||!size||!alignment||
           (alignment&(alignment-1u))!=0u||begin>end||size>end-begin||
           begin>UINT32_MAX-(alignment-1u)||live_count>=Capacity)
            return false;
        uint16_t free_slot=Capacity;
        for(uint16_t i=0u;i<Capacity;++i)
            if(!entries[i].used){free_slot=i;break;}
        if(free_slot==Capacity)return false;
        uint32_t candidate=(begin+alignment-1u)&~(alignment-1u);
        for(uint16_t i=0u;i<live_count;++i){
            if(candidate>end||size>end-candidate)return false;
            const Entry& existing=entries[order[i]];
            if(candidate+size<=existing.offset)break; /* Gap before block. */
            const uint32_t block_end=existing.offset+existing.size;
            if(candidate<block_end) {
                if(block_end>UINT32_MAX-(alignment-1u))return false;
                candidate=(block_end+alignment-1u)&~(alignment-1u);
            }
        }
        if(candidate>end||size>end-candidate)return false;
        entries[free_slot]={candidate,size,1u};
        uint16_t pos=live_count;
        while(pos!=0u && entries[order[pos-1u]].offset>candidate){
            order[pos]=order[pos-1u];
            --pos;
        }
        order[pos]=free_slot;
        ++live_count;
        used+=size;
        if(used>high_water)high_water=used;
        *out_slot=free_slot;
        *out_offset=candidate;
        return true;
    }

    bool release(uint16_t slot) {
        if(slot>=Capacity||!entries[slot].used)return false;
        uint16_t pos=0u;
        while(pos<live_count&&order[pos]!=slot)++pos;
        if(pos==live_count)return false;
        for(uint16_t i=pos+1u;i<live_count;++i)order[i-1u]=order[i];
        --live_count;
        used-=entries[slot].size;
        entries[slot]={};
        return true;
    }
};

}  // namespace saturn::core::audio::ram

#endif
