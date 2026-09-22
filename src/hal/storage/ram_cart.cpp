#include "src/hal/storage/ram_cart.hpp"

#include <stdint.h>

namespace saturn::hal::ram_cart {
namespace {
/* STN-47: address-space P2 (uncached); never address a non-RAM cartridge's
 * DRAM or write to A-Bus registers before checking its ID. */
constexpr uintptr_t kId = 0x24FFFFFFu;
constexpr uintptr_t kInit = 0x257EFFFEu;
constexpr uintptr_t kABusSetCS01 = 0x25FE00B0u;
constexpr uintptr_t kABusRefresh = 0x25FE00B8u;
constexpr uintptr_t kDRAM0 = 0x22400000u;
constexpr uintptr_t kDRAM1 = 0x22600000u;
}

uint8_t probe_id() {
    return *reinterpret_cast<volatile const uint8_t*>(kId);
}

void configure() {
    /* STN-47 mandates a 16-bit write of exactly 1 before A-Bus setup.
     * Set only CS0+CS1; preserve CS2 and reserved register at B4. */
    *reinterpret_cast<volatile uint16_t*>(kInit) = 1u;
    *reinterpret_cast<volatile uint32_t*>(kABusSetCS01) = 0x23301FF0u;
    *reinterpret_cast<volatile uint32_t*>(kABusRefresh) = 0x00000013u;
}

uint8_t* bank_base(uint8_t bank) {
    return reinterpret_cast<uint8_t*>(bank == 0u ? kDRAM0 : kDRAM1);
}
} // namespace saturn::hal::ram_cart
