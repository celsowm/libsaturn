#include "src/core/runtime/state.hpp"
#include "src/core/memory/section.hpp"

namespace saturn::core {

/* The VDP1 command list and Gouraud tables are not here: they are written in VDP1 VRAM (src/hal/vdp1/vdp1.cpp). */
RuntimeState g_state SATURN_WRAM_L = {};

}  // namespace saturn::core
