#include "saturn/input.h"

#include "src/core/logic.hpp"
#include "src/core/runtime_state.hpp"
#include "src/hal/smpc.hpp"

extern "C" sat_result_t sat_pad_poll(sat_pad_state_t* out_state) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (out_state == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }

    const uint16_t held = saturn::hal::smpc::read_digital_pad();
    g_state.pad = compute_pad_state(g_state.pad.held, held);
    *out_state = g_state.pad;
    return SAT_OK;
}

extern "C" uint16_t sat_pad_held(void) {
    using namespace saturn::core;
    return g_state.pad.held;
}
