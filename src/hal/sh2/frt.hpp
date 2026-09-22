#ifndef SATURN_HAL_SH2_FRT_HPP
#define SATURN_HAL_SH2_FRT_HPP

#include <stdint.h>

namespace saturn::hal::sh2::frt {

enum class Prescaler : uint8_t {
    Divide8 = 0u,
    Divide32 = 1u,
    Divide128 = 2u,
};

constexpr uint8_t kCaptureFlag = 0x80u;
constexpr uint8_t kInputCaptureInterruptEnable = 0x80u;

uint16_t counter();
uint8_t timer_control();
void set_prescaler(Prescaler prescaler);
uint8_t timer_interrupt_enable();
void set_input_capture_interrupt_enabled(bool enabled);
uint8_t status();
bool capture_pending();
void acknowledge_capture();
void configure_polling();

}  // namespace saturn::hal::sh2::frt

#endif
