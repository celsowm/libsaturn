#ifndef SATURN_HAL_ABUS_HPP
#define SATURN_HAL_ABUS_HPP

#include <stdint.h>

namespace saturn::hal::abus {

/* The HAL seam is small so host tests can stand in a fake cartridge. Areas are
 * 0 = CS0, 1 = CS1; the caller has already range-checked and authorized. */
uint8_t read_id();
void read(uint32_t area, uint32_t offset, void* destination, uint32_t bytes);
void write(uint32_t area, uint32_t offset, const void* source, uint32_t bytes);

}  // namespace saturn::hal::abus

#endif
