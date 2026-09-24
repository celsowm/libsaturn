#ifndef SATURN_AUDIO_PLAYBACK_VOICE_POLICY_HPP
#define SATURN_AUDIO_PLAYBACK_VOICE_POLICY_HPP

#include <stdint.h>

/* Pure caller-owned voice selection policy; no SCSP register operations,
 * frame clock, global registries or allocations. Free slots are preferred,
 * then the least-priority oldest nonlooping voice of <= request priority.
 * The caller alone decides whether/when to key_off and count a steal. */
namespace saturn::core::audio::voice {

template <typename Voice>
int32_t choose(const Voice* voices,uint16_t capacity,uint16_t priority) {
    if(!voices || capacity==0u)return -1;
    for(uint16_t i=0u;i<capacity;++i)
        if(voices[i].active==0u)return static_cast<int32_t>(i);
    int32_t best=-1;
    for(uint16_t i=0u;i<capacity;++i) {
        const Voice& v=voices[i];
        if(v.looping!=0u || v.priority>priority)continue;
        if(best<0 || v.priority<voices[best].priority ||
           (v.priority==voices[best].priority &&
            v.start_serial<voices[best].start_serial))
            best=static_cast<int32_t>(i);
    }
    return best;
}

}  // namespace saturn::core::audio::voice

#endif
