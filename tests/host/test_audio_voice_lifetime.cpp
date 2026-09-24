#include <cassert>
#include <cstdint>
#include <cstdio>
#include "src/audio/playback/clock.hpp"
#include "src/audio/playback/voice_lifetime.hpp"

namespace l=saturn::core::audio::voice::lifetime;
using saturn::core::audio::clock::State;

int main() {
    constexpr uint32_t one_second=22050u;
    assert(l::duration_frames(one_second,22050u,65536u,60u)==60u);
    assert(l::duration_frames(one_second,22050u,65536u,50u)==50u);
    assert(l::duration_frames(one_second,22050u,2u*65536u,50u)==25u);
    assert(l::duration_frames(1u,22050u,65536u,50u)==1u);
    assert(l::duration_frames(0u,22050u,65536u,50u)==1u);
    assert(l::duration_frames(1u,0u,65536u,50u)==1u);
    assert(l::duration_frames(UINT32_MAX,1u,1u,60u)==0x7FFFFFFFu);

    State clock{};
    assert(clock.tick(100u,0u)==100u);
    assert(l::play_start(100u,clock.service_frame,clock.valid!=0u)==100u);
    const uint32_t end=100u+l::duration_frames(4u,4u,65536u,50u);
    // A stalled app frame must not keep an expired voice active indefinitely.
    for(uint32_t edge=0u;edge<49u;++edge){
        clock.tick(100u,1u);
        clock.tick(100u,0u);
    }
    assert(clock.service_frame==149u);
    assert(!l::expired(clock.service_frame,end));
    assert(clock.tick(100u,1u)==150u);
    assert(l::expired(clock.service_frame,end));
    assert(l::play_start(100u,150u,true)==150u);
    assert(l::play_start(151u,150u,true)==151u);
    assert(l::play_start(99u,200u,false)==99u);
    assert(l::play_start(0u,UINT32_MAX,true)==0u);
    assert(l::play_start(UINT32_MAX,0u,true)==0u);
    assert(!l::expired(UINT32_MAX,0u));
    assert(l::expired(0u,0u));
    std::puts("audio voice lifetime: PAL/NTSC, CD-stalled frame, wrap OK");
}
