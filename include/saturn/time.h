#ifndef SATURN_TIME_H
#define SATURN_TIME_H

#include <stdint.h>
#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Milliseconds elapsed since sat_init(). Wraps naturally after 2^32 ms;
 * subtract timestamps as unsigned values to obtain wrap-safe short deltas.
 * While SCU interrupts run (sat_irq_active) the VBlank-IN handler observes
 * the 16-bit timer behind it every frame, so the count stays exact however
 * long the program goes without calling this. Without them it must be read
 * at least every ~0.3 s or that span is lost. */
uint32_t sat_time_ms(void);

/* Extended free-running timer ticks elapsed since sat_init(). The value uses
 * the SH-2 FRT prescaler selected by the runtime and is not converted to a
 * wall-clock unit. Returns zero before initialization or if the frame-clock
 * calibration failed. */
uint64_t sat_time_ticks(void);

/* Raw 16-bit SH-2 FRT counter. Reading the high byte latches the low byte;
 * subtract two readings as uint16_t to measure short intervals across wrap.
 * This value has no sat_init() origin and is not extended by the library. */
uint16_t sat_time_frc(void);

/* Busy-waits for at least the requested duration. */
sat_result_t sat_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_TIME_H */
