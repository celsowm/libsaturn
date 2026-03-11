#include "src/hal/vdp2.hpp"

#ifndef SAT_VDP_PROFILE_LEGACY
#define SAT_VDP_PROFILE_LEGACY 0
#endif

namespace saturn::hal::vdp2 {

namespace {

#if SAT_VDP_PROFILE_LEGACY
constexpr bool kUseLegacyVdpProfile = true;
#else
constexpr bool kUseLegacyVdpProfile = false;
#endif

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
    TVMD = kUseLegacyVdpProfile ? 0x0010 : 0x0000;
    RAMCTL = kUseLegacyVdpProfile ? 0x1F00 : 0x1300;
    BGON = kUseLegacyVdpProfile ? 0x0001 : 0x0000;
    CHCTLA = kUseLegacyVdpProfile ? 0x0002 : 0x0000;
    PRISA = 0x0006;
    PRINA = kUseLegacyVdpProfile ? 0x0001 : 0x0000;
    TVMD = kUseLegacyVdpProfile ? 0x8110 : 0x8100;
}

uint16_t read_tvstat() {
    return TVSTAT;
}

}  // namespace saturn::hal::vdp2
