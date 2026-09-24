#include <cassert>
#include <cstdio>
#include "src/audio/playback/ram_allocator.hpp"

using saturn::core::audio::ram::Pool;
int main() {
    Pool<3u> p{};
    p.reset();
    uint16_t slot=99u;
    uint32_t offset=99u;
    assert(!p.allocate(100u,120u,0u,2u,&slot,&offset));
    assert(!p.allocate(100u,120u,2u,3u,&slot,&offset));
    assert(!p.allocate(100u,120u,2u,2u,nullptr,&offset));
    assert(slot==99u && offset==99u && p.used==0u);
    assert(p.allocate(100u,120u,4u,2u,&slot,&offset));
    assert(slot==0u && offset==100u && p.used==4u);
    assert(p.allocate(100u,120u,4u,2u,&slot,&offset));
    assert(slot==1u && offset==104u && p.high_water==8u);
    assert(p.allocate(100u,120u,4u,2u,&slot,&offset));
    assert(slot==2u && offset==108u && p.used==12u);
    assert(!p.allocate(100u,120u,4u,2u,&slot,&offset));
    assert(p.release(1u) && p.used==8u);
    assert(!p.release(1u) && !p.release(5u));
    assert(p.allocate(100u,120u,4u,2u,&slot,&offset));
    assert(slot==1u && offset==104u && p.high_water==12u);
    assert(p.release(0u) && p.release(1u) && p.release(2u));
    assert(p.used==0u && p.high_water==12u);
    p.reset();
    assert(p.high_water==0u && p.used==0u);
    assert(p.allocate(101u,120u,5u,4u,&slot,&offset));
    assert(offset==104u && (offset&3u)==0u);
    std::puts("audio Sound RAM allocator: OK");
}
