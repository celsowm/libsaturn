#ifndef SATURN_HAL_SCU_DSP_HPP
#define SATURN_HAL_SCU_DSP_HPP

#include <stdint.h>

#include "src/hal/scu/dsp_logic.hpp"

namespace saturn::hal::scu::dsp {

/* Reads DSP_PPAF. Reading clears the overflow and end flags. */
dsp_logic::Status status();

/* Program and data RAM can only be touched while the DSP is stopped; the
 * hardware ignores writes and reads garbage otherwise. The callers check. */
void load_program(const uint32_t* words, uint32_t count, uint32_t at);
void write_data(uint32_t bank, uint32_t offset, const uint32_t* words, uint32_t count);
void read_data(uint32_t bank, uint32_t offset, uint32_t* words, uint32_t count);

/* Starts the program at `entry` and returns at once. */
void start(uint32_t entry);
/* Clears EX: the program stops where it is. */
void stop();

/* DSP DMA takes word addresses (byte address / 4) of Work RAM-H. dma_range_ok
 * says whether [p, p + bytes) can be moved by it (Work RAM-H, longword aligned);
 * invalidate() drops the SH-2 cache lines of a span the DSP has written. */
bool dma_range_ok(const void* p, uint32_t bytes);
uint32_t dma_word_address(const void* p);
void invalidate(const void* p, uint32_t bytes);

/* Polls until the program has ended and its DMA drained, for at most
 * `timeout_ticks` FRT ticks (65535 at most). False on timeout. */
bool wait_finished(uint32_t timeout_ticks);

}  // namespace saturn::hal::scu::dsp

#endif
