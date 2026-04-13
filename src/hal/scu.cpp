#include "src/hal/scu.hpp"

#include "src/hal/vdp2.hpp"

namespace saturn::hal::scu {

namespace {

uint32_t g_frame_counter = 0;
constexpr uint16_t kVblankFlag = 0x0008u;
constexpr uint32_t kMaxPollSpins = 2000000u;

}  // namespace

void init_interrupts() {
    // MVP path uses VDP2 TVSTAT polling for vblank sync; keep SCU setup minimal.
}

void wait_vblank() {
    const uint16_t before = static_cast<uint16_t>(vdp2::read_tvstat() & kVblankFlag);
    uint32_t spin = 0;
    while (static_cast<uint16_t>(vdp2::read_tvstat() & kVblankFlag) == before) {
        ++spin;
        if (spin >= kMaxPollSpins) {
            break;
        }
    }
    ++g_frame_counter;
}

uint32_t frame_counter() {
    return g_frame_counter;
}

}  // namespace saturn::hal::scu
