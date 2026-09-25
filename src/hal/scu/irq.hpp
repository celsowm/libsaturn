#ifndef SATURN_HAL_SCU_IRQ_HPP
#define SATURN_HAL_SCU_IRQ_HPP

#include <stdint.h>

/* SCU interrupts on the Master SH-2. Sources are the SCU status bits
 * 0..13 (irq_logic.hpp). VBlank-IN is always enabled while interrupts run:
 * it keeps the frame clock and the display-frame count. */
namespace saturn::hal::scu::irq {

using Handler = void (*)(void* user);

/* Installs the handlers, enables VBlank-IN and lowers the SR mask, then
 * waits for the first VBlank-IN. False (and everything restored) when none
 * arrives within a few frames: the caller keeps polling. */
bool start();
/* Masks every source, restores the previous vectors and the SR mask. */
void stop();
bool live();

/* Null removes the handler and masks the source (VBlank-IN stays on). */
void set_handler(uint8_t source, Handler handler, void* user);
uint32_t count(uint8_t source);

void set_timer0_line(uint16_t line);
void set_timer1(uint16_t dots, bool timer0_line_only);

}  // namespace saturn::hal::scu::irq

#endif
