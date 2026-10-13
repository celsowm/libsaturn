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

/* Convert frames <-> milliseconds at the given refresh rate. Pass
 * sat_video_refresh_hz() (60 for NTSC, 50 for PAL) so the conversion
 * stays correct on a PAL console. Inline so there is no call cost:
 * the multiplication and division collapse on the SH-2, and the
 * caller can elide the Hz argument by inlining its own constant.
 * Both helpers wrap on overflow the same way the surrounding
 * sat_time_ms() does. */
static inline uint32_t sat_f2ms(uint32_t frames, uint32_t hz) {
    return hz == 0u ? 0u : (frames * 1000u) / hz;
}
static inline uint32_t sat_ms2f(uint32_t ms, uint32_t hz) {
    /* Round down: a partial frame at the end of a millisecond is one
     * frame the next display tick, not the current one. */
    return (ms * hz) / 1000u;
}

#ifdef __cplusplus
}
#endif

#endif /* SATURN_TIME_H */
