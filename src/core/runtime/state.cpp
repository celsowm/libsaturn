#include "src/core/runtime/state.hpp"
#include "src/core/memory/section.hpp"

namespace saturn::core {

/* The VDP1 command list is not here: it comes from sat_video_memory (saturn/video_memory.h). */
RuntimeState g_state SATURN_WRAM_L = {};

}  // namespace saturn::core
