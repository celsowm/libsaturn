#include "src/hal/vdp2.hpp"

namespace saturn::hal::vdp2 {

namespace {

template <uint32_t Offset>
volatile uint16_t& reg() {
    return *reinterpret_cast<volatile uint16_t*>(0x05F80000u + Offset);
}

volatile uint16_t& TVMD = reg<0x000>();
volatile uint16_t& TVSTAT = reg<0x004>();
volatile uint16_t& RAMCTL = reg<0x00E>();
volatile uint16_t& BGON = reg<0x010>();
volatile uint16_t& CHCTLA = reg<0x018>();
volatile uint16_t& PRISA = reg<0x0A0>();
volatile uint16_t& PRINA = reg<0x0A8>();

}  // namespace

void init_ntsc_320x224() {
    TVMD = 0x0000;
    RAMCTL = 0x1300;
    BGON = 0x0000;
    CHCTLA = 0x0000;
    PRISA = 0x0006;
    PRINA = 0x0000;
    TVMD = 0x8100;
}

uint16_t read_tvstat() {
    return TVSTAT;
}

}  // namespace saturn::hal::vdp2
