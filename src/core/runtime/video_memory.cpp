#include "saturn/video_memory.h"
#include "src/core/runtime/internal.hpp"
#include "src/core/memory/section.hpp"

/* The default table (saturn/video_memory.h): a program that defines `sat_video_memory` leaves this file, and the
 * 80 KiB below, out of the link. */
namespace {

constexpr uint16_t kGouraudDefault = 2048u;

SATURN_WRAM_L alignas(4) uint32_t g_commands[saturn::internal::kCmdCapacity * SAT_VDP1_COMMAND_WORDS];
SATURN_WRAM_L alignas(4) uint16_t g_gouraud[kGouraudDefault * SAT_VDP1_GOURAUD_WORDS];

}  // namespace

extern "C" const sat_video_memory_t sat_video_memory = {
    g_commands, saturn::internal::kCmdCapacity, kGouraudDefault, g_gouraud};
