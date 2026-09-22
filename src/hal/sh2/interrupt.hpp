#ifndef SATURN_HAL_SH2_INTERRUPT_HPP
#define SATURN_HAL_SH2_INTERRUPT_HPP

#include <stdint.h>

namespace saturn::hal::sh2::interrupt {

using Handler = void (*)();

uint32_t vector_address(uint8_t vector);
Handler install(uint8_t vector, Handler handler);
Handler read(uint8_t vector);
void restore(uint8_t vector, Handler handler);

}  // namespace saturn::hal::sh2::interrupt

#endif
