#include "src/hal/scu/dma.hpp"

// Host tests that compile the real VDP1/VDP2/SCSP HAL sources never touch
// SCU-DMA hardware; uploads through them are reported as done.
namespace saturn::hal::scu::dma {
sat_result_t copy(void*, const void*, uint32_t) { return SAT_OK; }
}
