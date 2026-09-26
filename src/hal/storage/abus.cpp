#include "src/hal/storage/abus.hpp"

namespace saturn::hal::abus {
namespace {
/* Uncached (address space P2) views of the A-Bus windows. */
constexpr uintptr_t kCs0 = 0x22000000u;
constexpr uintptr_t kCs1 = 0x24000000u;
constexpr uintptr_t kId = 0x24FFFFFFu;

uintptr_t base(uint32_t area) {
    return area == 0u ? kCs0 : kCs1;
}
}  // namespace

uint8_t read_id() {
    return *reinterpret_cast<volatile const uint8_t*>(kId);
}

/* Byte accesses only: the A-Bus is 16 bits wide and cartridges differ in what
 * they answer to wider or unaligned reads. */
void read(uint32_t area, uint32_t offset, void* destination, uint32_t bytes) {
    volatile const uint8_t* source = reinterpret_cast<volatile const uint8_t*>(base(area) + offset);
    uint8_t* out = static_cast<uint8_t*>(destination);
    for (uint32_t i = 0u; i < bytes; ++i) out[i] = source[i];
}

void write(uint32_t area, uint32_t offset, const void* source, uint32_t bytes) {
    volatile uint8_t* target = reinterpret_cast<volatile uint8_t*>(base(area) + offset);
    const uint8_t* in = static_cast<const uint8_t*>(source);
    for (uint32_t i = 0u; i < bytes; ++i) target[i] = in[i];
}

}  // namespace saturn::hal::abus
