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
    // Fragmented RAM: reused metadata slot is inserted in physical offset
    // order, so one linear gap scan remains deterministic across holes.
    Pool<5u> fragmented{};
    fragmented.reset();
    uint16_t slots[4]={};
    uint32_t positions[4]={};
    for(uint16_t i=0u;i<4u;++i)
        assert(fragmented.allocate(100u,132u,4u,4u,
            &slots[i],&positions[i]));
    assert(positions[0]==100u && positions[3]==112u);
    assert(fragmented.release(slots[1]));
    assert(fragmented.release(slots[3]));
    assert(fragmented.allocate(100u,132u,4u,4u,&slot,&offset));
    assert(slot==slots[1] && offset==104u);
    assert(fragmented.order[0]==slots[0] &&
           fragmented.order[1]==slots[1] &&
           fragmented.order[2]==slots[2]);
    assert(fragmented.release(slots[0]));
    assert(fragmented.allocate(100u,132u,8u,4u,&slot,&offset));
    assert(offset==112u); // Neither the initial 4-byte gap nor middle fit.
    assert(fragmented.live_count==3u && fragmented.used==16u);

    std::puts("audio Sound RAM allocator: OK");
}
