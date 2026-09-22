#include "saturn/input.h"

#include "src/input/runtime.hpp"
#include "src/core/runtime/state.hpp"
#include "src/hal/smpc/smpc.hpp"

namespace {

sat_result_t poll_port(uint8_t port, sat_pad_state_t* out_state) {
    using namespace saturn::core;
    if (port >= SAT_PAD_PORT_COUNT || out_state == nullptr) return SAT_ERR_INVALID_ARG;

    saturn::hal::smpc::DigitalPadSample sample{};
    if (!saturn::hal::smpc::read_digital_pad(port, &sample)) return SAT_ERR_IO;

    input_apply_pad_sample(g_input_runtime, port, sample.connected, sample.held);
    *out_state = g_input_runtime.pads[port];
    return SAT_OK;
}

}  // namespace

extern "C" sat_result_t sat_input_poll(void) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;

    for (uint8_t port = 0u; port < SAT_PAD_PORT_COUNT; ++port) {
        sat_pad_state_t state{};
        st = poll_port(port, &state);
        if (st != SAT_OK) return st;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_pad_poll_port(uint8_t port, sat_pad_state_t* out_state) {
    using namespace saturn::core;
    const sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    return poll_port(port, out_state);
}

extern "C" sat_result_t sat_pad_poll(sat_pad_state_t* out_state) {
    return sat_pad_poll_port(0u, out_state);
}

extern "C" uint16_t sat_pad_held_port(uint8_t port) {
    if (port >= SAT_PAD_PORT_COUNT) return 0u;
    return saturn::core::g_input_runtime.pads[port].held;
}

extern "C" uint16_t sat_pad_held(void) {
    return sat_pad_held_port(0u);
}

extern "C" int sat_event_poll(sat_event_t* out_event) {
    using namespace saturn::core;
    const sat_result_t st = require_initialized();
    if (st != SAT_OK) return static_cast<int>(st);
    if (out_event == nullptr) return static_cast<int>(SAT_ERR_INVALID_ARG);
    return input_event_pop(g_input_runtime, out_event) ? 1 : 0;
}

extern "C" uint16_t sat_event_capacity(void) {
    return saturn::core::kInputEventCapacity;
}

extern "C" uint16_t sat_event_count(void) {
    return saturn::core::g_input_runtime.count;
}

extern "C" uint32_t sat_event_overflow_count(void) {
    return saturn::core::g_input_runtime.overflow_count;
}

extern "C" sat_result_t sat_pad_format_held(uint16_t held, char* out, uint16_t out_size) {
    if (out == nullptr || out_size < 19u) return SAT_ERR_INVALID_ARG;
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
    if (out == nullptr || out_size < 14u) return SAT_ERR_INVALID_ARG;
    static const char kHex[17] = "0123456789ABCDEF";
    out[0] = 'F';  out[1] = 'R';  out[2] = 'M';  out[3] = ':';  out[4] = ' ';
    for (int i = 0; i < 8; ++i) {
        out[5 + i] = kHex[(frame_count >> ((7 - i) * 4)) & 0xFu];
    }
    out[13] = '\0';
    return SAT_OK;
}
