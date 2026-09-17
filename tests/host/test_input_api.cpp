#include <cstdio>
#include <cstdlib>

#include "saturn/input.h"
#include "src/core/input_runtime.hpp"
#include "src/core/runtime_state.hpp"
#include "src/hal/smpc.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace {
saturn::hal::smpc::DigitalPadSample g_samples[SAT_PAD_PORT_COUNT]{};
bool g_read_ok[SAT_PAD_PORT_COUNT] = {true, true};
uint16_t g_reads[SAT_PAD_PORT_COUNT]{};
}

namespace saturn::hal::smpc {
bool read_digital_pad(uint8_t port, DigitalPadSample* out_sample) {
    if (port >= SAT_PAD_PORT_COUNT || out_sample == nullptr || !g_read_ok[port]) return false;
    ++g_reads[port];
    *out_sample = g_samples[port];
    return true;
}
}

static sat_event_t next_event() {
    sat_event_t event{};
    OK(sat_event_poll(&event) == 1);
    return event;
}

int main() {
    using namespace saturn::core;
    g_state = {};
    g_state.initialized = true;
    input_runtime_reset(g_input_runtime);

    g_samples[0] = {SAT_PAD_A, true};
    g_samples[1] = {SAT_PAD_B, true};
    OK(sat_input_poll() == SAT_OK);
    OK(g_reads[0] == 1u && g_reads[1] == 1u);
    OK(sat_pad_held() == SAT_PAD_A);
    OK(sat_pad_held_port(1u) == SAT_PAD_B);
    OK(g_input_runtime.pads[0].connected == 1u);
    OK(g_input_runtime.pads[1].connected == 1u);

    sat_event_t event = next_event();
    OK(event.type == SAT_EVENT_PAD_CONNECTED && event.port == 0u);
    event = next_event();
    OK(event.type == SAT_EVENT_BUTTON_DOWN && event.port == 0u && event.control == SAT_PAD_A);
    event = next_event();
    OK(event.type == SAT_EVENT_PAD_CONNECTED && event.port == 1u);
    event = next_event();
    OK(event.type == SAT_EVENT_BUTTON_DOWN && event.port == 1u && event.control == SAT_PAD_B);
    OK(sat_event_poll(&event) == 0);

    g_samples[1] = {SAT_PAD_C, true};
    sat_pad_state_t state{};
    OK(sat_pad_poll_port(1u, &state) == SAT_OK);
    OK(state.held == SAT_PAD_C);
    OK(state.pressed == SAT_PAD_C);
    OK(state.released == SAT_PAD_B);
    OK(g_input_runtime.pads[0].held == SAT_PAD_A);

    event = next_event();
    OK(event.type == SAT_EVENT_BUTTON_DOWN && event.control == SAT_PAD_C);
    event = next_event();
    OK(event.type == SAT_EVENT_BUTTON_UP && event.control == SAT_PAD_B);

    g_samples[0] = {0u, false};
    OK(sat_pad_poll(&state) == SAT_OK);
    OK(state.connected == 0u && state.released == SAT_PAD_A);
    event = next_event();
    OK(event.type == SAT_EVENT_BUTTON_UP && event.control == SAT_PAD_A);
    event = next_event();
    OK(event.type == SAT_EVENT_PAD_DISCONNECTED && event.port == 0u);

    g_read_ok[1] = false;
    const sat_pad_state_t before = g_input_runtime.pads[1];
    OK(sat_pad_poll_port(1u, &state) == SAT_ERR_IO);
    OK(g_input_runtime.pads[1].held == before.held);
    OK(sat_pad_poll_port(2u, &state) == SAT_ERR_INVALID_ARG);
    OK(sat_event_poll(nullptr) == SAT_ERR_INVALID_ARG);

    std::puts("input api: OK");
    return 0;
}
