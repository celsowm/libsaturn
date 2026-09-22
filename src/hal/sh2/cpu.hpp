#ifndef SATURN_HAL_SH2_CPU_HPP
#define SATURN_HAL_SH2_CPU_HPP

#include <stdint.h>

namespace saturn::hal::sh2 {

enum class CpuRole : uint8_t {
    Master = 0,
    Slave = 1,
};

uint32_t read_status();
void write_status(uint32_t status);
uint8_t interrupt_mask();
void set_interrupt_mask(uint8_t level);
uint32_t save_and_mask_interrupts();
void restore_interrupts(uint32_t status);
uint32_t read_vector_base();
void write_vector_base(uint32_t address);
void compiler_barrier();
void idle();

}  // namespace saturn::hal::sh2

#endif
