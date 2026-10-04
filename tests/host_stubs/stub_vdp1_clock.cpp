#include "src/hal/scu/scu.hpp"
#include "src/hal/vdp2/vdp2.hpp"

// Host tests that link the real VDP1 HAL: the frame clock and TVSTAT its
// draw/swap waits read. An uncalibrated clock (0 ticks per frame) and a
// display that is never in VBlank; these tests never wait on a real list.
namespace saturn::hal::scu {
uint16_t ticks_per_frame() { return 0u; }
uint64_t elapsed_ticks() { return 0u; }
uint32_t frame_counter() { return 0u; }
}

namespace saturn::hal::vdp2 {
uint16_t read_tvstat() { return 0u; }
}
