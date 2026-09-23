#include "src/core/runtime/state.hpp"

namespace saturn::core {

/* The VDP1 command buffer dominates this structure's size; keep it in
 * zero-initialized WRAM-L so applications with large resident assets retain
 * the contiguous WRAM-H needed for executable code and model tables. */
RuntimeState g_state __attribute__((section(".wram_l"))) = {};

}  // namespace saturn::core
