#ifndef SATURN_HAL_SCU_HPP
#define SATURN_HAL_SCU_HPP

#include <stdint.h>

namespace saturn::hal::scu {

/* Starts SCU interrupts (VBlank-IN first); polling stays the fallback.
 * Call after init_frame_clock. */
void init_interrupts();
void shutdown_interrupts();
/* VBlank-IN handler work: keeps the frame clock observed every frame. */
void on_vblank_in();
/* Times VBlank-IN stopped arriving and wait_vblank went back to polling. */
uint32_t irq_fallbacks();
/* Calibrates the display-frame clock; needs the display already enabled. */
void init_frame_clock();
void wait_vblank();
/* sat_wait_vblank calls, i.e. program frames. */
uint32_t frame_counter();
/* Display frames elapsed since init_frame_clock, including missed ones. */
uint32_t display_frames();
/* Calibrated FRT ticks in one display frame; zero if calibration failed. */
uint16_t ticks_per_frame();
/* FRT ticks elapsed since init_frame_clock, extended in software across wraps
 * whenever this clock is observed. */
uint64_t elapsed_ticks();

}  // namespace saturn::hal::scu

#endif
