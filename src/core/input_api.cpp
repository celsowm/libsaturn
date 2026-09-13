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

extern "C" sat_result_t sat_pad_format_held(uint16_t held, char* out, uint16_t out_size) {
    if (out == nullptr || out_size < 19u) {
        return SAT_ERR_INVALID_ARG;
    }
    static const char kHex[17] = "0123456789ABCDEF";
    out[0] = 'P';  out[1] = 'A';  out[2] = 'D';  out[3] = ':';  out[4] = ' ';
    out[5] = kHex[(held >> 12) & 0xFu];
    out[6] = kHex[(held >> 8) & 0xFu];
    out[7] = kHex[(held >> 4) & 0xFu];
    out[8] = kHex[held & 0xFu];
    out[9] = ' ';
    out[10] = (held & SAT_PAD_UP)    ? 'U' : '.';
    out[11] = (held & SAT_PAD_DOWN)  ? 'D' : '.';
    out[12] = (held & SAT_PAD_LEFT)  ? 'L' : '.';
    out[13] = (held & SAT_PAD_RIGHT) ? 'R' : '.';
    out[14] = (held & SAT_PAD_START) ? 'S' : '.';
    out[15] = (held & SAT_PAD_A)     ? 'A' : '.';
    out[16] = (held & SAT_PAD_B)     ? 'B' : '.';
    out[17] = (held & SAT_PAD_C)     ? 'C' : '.';
    out[18] = '\0';
    return SAT_OK;
}

extern "C" sat_result_t sat_pad_format_frame(uint32_t frame_count, char* out, uint16_t out_size) {
    if (out == nullptr || out_size < 14u) {
        return SAT_ERR_INVALID_ARG;
    }
    static const char kHex[17] = "0123456789ABCDEF";
    out[0] = 'F';  out[1] = 'R';  out[2] = 'M';  out[3] = ':';  out[4] = ' ';
    for (int i = 0; i < 8; i++) {
        out[5 + i] = kHex[(frame_count >> ((7 - i) * 4)) & 0xFu];
    }
    out[13] = '\0';
    return SAT_OK;
}
