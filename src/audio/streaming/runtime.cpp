#include "src/audio/streaming/runtime.hpp"
#include "src/core/memory/section.hpp"

namespace saturn::core {

/* Slot state only: the upload to Sound RAM reads the caller's ring directly, so there are no staging
 * buffers. crt0 and the audio reset both clear it. */
AudioStreamRegistry g_audio_streams SATURN_WRAM_L = {};

}  // namespace saturn::core
