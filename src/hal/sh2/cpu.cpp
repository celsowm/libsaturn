#include "src/hal/sh2/cpu.hpp"

namespace saturn::hal::sh2 {

uint32_t read_status() {
    uint32_t value = 0u;
    asm volatile("stc sr, %0" : "=r"(value));
    return value;
}

void write_status(uint32_t status) {
    asm volatile("ldc %0, sr" : : "r"(status) : "memory");
}

uint8_t interrupt_mask() {
    return static_cast<uint8_t>((read_status() >> 4u) & 0x0Fu);
}

void set_interrupt_mask(uint8_t level) {
    uint32_t status = read_status();
    status = (status & ~0x000000F0u) |
             ((static_cast<uint32_t>(level) & 0x0Fu) << 4u);
    write_status(status);
}

uint32_t save_and_mask_interrupts() {
    const uint32_t previous = read_status();
    write_status(previous | 0x000000F0u);
    return previous;
}

void restore_interrupts(uint32_t status) {
    write_status(status);
}

uint32_t read_vector_base() {
    uint32_t value = 0u;
    asm volatile("stc vbr, %0" : "=r"(value));
    return value;
}

void write_vector_base(uint32_t address) {
    asm volatile("ldc %0, vbr" : : "r"(address) : "memory");
}

void compiler_barrier() {
    asm volatile("" : : : "memory");
}

void idle() {
    asm volatile("sleep" : : : "memory");
}

}  // namespace saturn::hal::sh2
