#ifndef SATURN_HAL_RAM_CART_HPP
#define SATURN_HAL_RAM_CART_HPP

#include <stdint.h>
namespace saturn::hal::ram_cart {

/* HAL seam is deliberately small so host tests can substitute an in-memory
 * cartridge without dereferencing Saturn hardware registers. */
uint8_t probe_id();
void configure();
uint8_t* bank_base(uint8_t bank);

} // namespace saturn::hal::ram_cart
#endif
