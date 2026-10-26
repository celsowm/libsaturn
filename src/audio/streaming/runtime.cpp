#include "src/audio/streaming/runtime.hpp"
#include "src/core/memory/section.hpp"

namespace saturn::core {

/* Four 8-KiB upload staging buffers dominate this registry. Preserve High
 * WRAM for code/active model data; crt0 and the audio reset both clear it.
 * SCSP Sound RAM addresses, refill slicing and ring ownership are unchanged. */
AudioStreamRegistry g_audio_streams SATURN_WRAM_L = {};

}  // namespace saturn::core
