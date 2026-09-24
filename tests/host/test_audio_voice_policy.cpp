#include <cassert>
#include <cstdio>
#include "src/audio/playback/voice_policy.hpp"

struct Voice {
    uint8_t active;
    uint8_t looping;
    uint16_t priority;
    uint32_t start_serial;
};

int main() {
    using saturn::core::audio::voice::choose;
    Voice v[4]={};
    assert(choose(v,4u,0u)==0);
    assert(choose(v,0u,0u)==-1);
    assert(choose<Voice>(nullptr,4u,0u)==-1);
    for(auto& a:v)a.active=1u;
    v[0]={1u,1u,0u,1u}; /* Looping: never steal. */
    v[1]={1u,0u,9u,1u};
    v[2]={1u,0u,3u,9u};
    v[3]={1u,0u,3u,2u};
    assert(choose(v,4u,2u)==-1);
    assert(choose(v,4u,3u)==3); /* Lowest priority, oldest. */
    v[3].start_serial=10u;
    assert(choose(v,4u,3u)==2);
    v[0].active=0u;
    assert(choose(v,4u,0u)==0); /* Free beats eligible steal. */
    std::puts("audio voice selection policy: OK");
}
