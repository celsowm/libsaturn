#include "src/hal/scu.hpp"

#include "src/hal/vdp2.hpp"

namespace saturn::hal::scu {

namespace {

volatile uint32_t g_frame_counter = 0;

volatile uint32_t& SCU_IST = *reinterpret_cast<volatile uint32_t*>(0x05A0001C);
volatile uint32_t& SCU_IMS = *reinterpret_cast<volatile uint32_t*>(0x05A00024);
uint32_t* const IVT = reinterpret_cast<uint32_t*>(0x06000300);

constexpr uint32_t kVblankInMask = 1u << 0;

extern "C" void scu_vblank_in_handler() {
    SCU_IST &= ~kVblankInMask;
    ++g_frame_counter;
}

void unmask_interrupts() {
    asm volatile(
        "stc sr, r0      \n"
        "mov #0x0F, r1   \n"
        "and r1, r0      \n"
        "ldc r0, sr      \n"
        :
        :
        : "r0", "r1"
    );
}

}  // namespace

void init_interrupts() {
    IVT[0x40] = reinterpret_cast<uint32_t>(&scu_vblank_in_handler);
    SCU_IMS &= ~kVblankInMask;
    unmask_interrupts();
}

void wait_vblank() {
    const uint32_t before = g_frame_counter;
    uint32_t spin = 0;
    while (g_frame_counter == before) {
        ++spin;
        if (spin > 1000000u) {
            while ((vdp2::read_tvstat() & 0x0008u) == 0u) {
            }
            while ((vdp2::read_tvstat() & 0x0008u) != 0u) {
            }
            ++g_frame_counter;
            break;
        }
    }
}

uint32_t frame_counter() {
    return g_frame_counter;
}

}  // namespace saturn::hal::scu

