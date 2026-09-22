#include "saturn/time.h"

#include "src/core/runtime/state.hpp"
#include "src/core/runtime/time_logic.hpp"
#include "src/hal/scu/scu.hpp"

extern "C" uint32_t sat_time_ms(void) {
    using namespace saturn::core;
    if (!g_state.initialized) {
        return 0u;
    }
    const uint32_t frames_per_second = g_state.config.ntsc != 0u ? 60u : 50u;
    return time_logic::ticks_to_ms(
        saturn::hal::scu::elapsed_ticks(),
        saturn::hal::scu::ticks_per_frame(),
        frames_per_second);
}

extern "C" sat_result_t sat_delay_ms(uint32_t ms) {
    using namespace saturn::core;
    if (require_initialized() != SAT_OK) {
        return SAT_ERR_NOT_INITIALIZED;
    }
    if (saturn::hal::scu::ticks_per_frame() == 0u) {
        return SAT_ERR_UNSUPPORTED;
    }
    const uint32_t start = sat_time_ms();
    while (time_logic::elapsed_ms(sat_time_ms(), start) < ms) {
    }
    return SAT_OK;
}
