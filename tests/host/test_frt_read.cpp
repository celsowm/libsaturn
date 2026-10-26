#include <cassert>
#include "src/hal/sh2/frt_read_logic.hpp"
int main() {
    using saturn::hal::sh2::frt::read_consistent;
    /* IRQ overwrites TEMP after high=12 at 12FF, before low at 1300.
     * The old two-load reader returned 1200: a false 65282-tick timeout
     * relative to 12FE, instead of the true two elapsed ticks. */
    const uint8_t highs[]={0x12,0x13,0x13,0x13};unsigned h=0;
    const uint16_t value=read_consistent([&](){return highs[h++];},[](){return uint8_t{0};});
    assert(value==0x1300 && h==4);
    assert(static_cast<uint16_t>(value-0x12FE)==2);
    assert(static_cast<uint16_t>(0x1200-0x12FE)>60000);
    const uint8_t wrap[]={0xFF,0,0,0};h=0;
    assert(read_consistent([&](){return wrap[h++];},[](){return uint8_t{5};})==5);
    assert(read_consistent([](){return uint8_t{0xAB};},[](){return uint8_t{0xCD};})==0xABCD);
}
