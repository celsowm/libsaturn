#include "src/hal/sh2/frt.hpp"

#include "src/hal/sh2/cpu.hpp"

namespace saturn::hal::sh2::frt {

namespace {

#define FRT_TIER (*reinterpret_cast<volatile uint8_t*>(0xFFFFFE10u))
#define FRT_FTCSR (*reinterpret_cast<volatile uint8_t*>(0xFFFFFE11u))
#define FRT_FRC_H (*reinterpret_cast<volatile uint8_t*>(0xFFFFFE12u))
#define FRT_FRC_L (*reinterpret_cast<volatile uint8_t*>(0xFFFFFE13u))
#define FRT_TCR (*reinterpret_cast<volatile uint8_t*>(0xFFFFFE16u))

}  // namespace

uint16_t counter() {
    /* Reading FRC high latches the low byte on the SH-2 FRT bus. */
    const uint16_t high = FRT_FRC_H;
    const uint16_t low = FRT_FRC_L;
    return static_cast<uint16_t>((high << 8u) | low);
}

uint8_t timer_control() {
    return FRT_TCR;
}

void set_prescaler(Prescaler prescaler) {
    /* Preserve input-edge and other timer configuration bits. */
    FRT_TCR = static_cast<uint8_t>((FRT_TCR & 0xFCu) |
                                   (static_cast<uint8_t>(prescaler) & 0x03u));
    compiler_barrier();
}

uint8_t timer_interrupt_enable() {
    return FRT_TIER;
}

void set_input_capture_interrupt_enabled(bool enabled) {
    uint8_t value = FRT_TIER;
    if (enabled) {
        value = static_cast<uint8_t>(value | kInputCaptureInterruptEnable);
    } else {
        value = static_cast<uint8_t>(value & ~kInputCaptureInterruptEnable);
    }
    FRT_TIER = value;
    compiler_barrier();
}

uint8_t status() {
    return FRT_FTCSR;
}

bool capture_pending() {
    return (status() & kCaptureFlag) != 0u;
}

void acknowledge_capture() {
    /* FRT status bits are cleared by writing zero to the selected bit. */
    FRT_FTCSR = static_cast<uint8_t>(FRT_FTCSR & ~kCaptureFlag);
    compiler_barrier();
}

void configure_polling() {
    /* The Saturn dual-CPU guide specifies TIER=01h for flag polling.  Keep
     * compare/overflow ownership out of this helper and only disable the FRT
     * input-capture interrupt. */
    set_input_capture_interrupt_enabled(false);
    acknowledge_capture();
}

}  // namespace saturn::hal::sh2::frt
