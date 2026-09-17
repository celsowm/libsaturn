#include <cstdio>
#include <cstdlib>

#include "src/core/input_runtime.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

static sat_event_t pop(saturn::core::InputRuntime& runtime) {
    sat_event_t event{};
    OK(saturn::core::input_event_pop(runtime, &event));
    return event;
}

int main() {
    using namespace saturn::core;
    InputRuntime runtime{};
    input_runtime_reset(runtime);

    input_apply_pad_sample(runtime, 0u, false, 0u);
    OK(runtime.count == 0u);
    OK(runtime.pads[0].connected == 0u);

    input_apply_pad_sample(runtime, 0u, true, SAT_PAD_A | SAT_PAD_RIGHT);
    OK(runtime.pads[0].connected == 1u);
    OK(runtime.pads[0].held == (SAT_PAD_A | SAT_PAD_RIGHT));
    OK(runtime.pads[0].pressed == (SAT_PAD_A | SAT_PAD_RIGHT));
    OK(runtime.count == 3u);

    sat_event_t event = pop(runtime);
    OK(event.type == SAT_EVENT_PAD_CONNECTED && event.port == 0u);
    event = pop(runtime);
    OK(event.type == SAT_EVENT_BUTTON_DOWN && event.control == SAT_PAD_A);
    event = pop(runtime);
    OK(event.type == SAT_EVENT_BUTTON_DOWN && event.control == SAT_PAD_RIGHT);

    input_apply_pad_sample(runtime, 0u, true, SAT_PAD_A | SAT_PAD_B);
    OK(runtime.pads[0].pressed == SAT_PAD_B);
    OK(runtime.pads[0].released == SAT_PAD_RIGHT);
    event = pop(runtime);
    OK(event.type == SAT_EVENT_BUTTON_DOWN && event.control == SAT_PAD_B);
    event = pop(runtime);
    OK(event.type == SAT_EVENT_BUTTON_UP && event.control == SAT_PAD_RIGHT);

    input_apply_pad_sample(runtime, 1u, true, SAT_PAD_C);
    OK(runtime.pads[1].held == SAT_PAD_C);
    OK(runtime.pads[0].held == (SAT_PAD_A | SAT_PAD_B));
    event = pop(runtime);
    OK(event.type == SAT_EVENT_PAD_CONNECTED && event.port == 1u);
    event = pop(runtime);
    OK(event.type == SAT_EVENT_BUTTON_DOWN && event.port == 1u && event.control == SAT_PAD_C);

    input_apply_pad_sample(runtime, 0u, false, 0u);
    OK(runtime.pads[0].connected == 0u);
    OK(runtime.pads[0].held == 0u);
    OK(runtime.pads[0].released == (SAT_PAD_A | SAT_PAD_B));
    event = pop(runtime);
    OK(event.type == SAT_EVENT_BUTTON_UP && event.control == SAT_PAD_A);
    event = pop(runtime);
    OK(event.type == SAT_EVENT_BUTTON_UP && event.control == SAT_PAD_B);
    event = pop(runtime);
    OK(event.type == SAT_EVENT_PAD_DISCONNECTED && event.port == 0u);
    OK(runtime.count == 0u);

    input_runtime_reset(runtime);
    for (uint16_t i = 0u; i < kInputEventCapacity + 5u; ++i) {
        sat_event_t e{};
        e.type = SAT_EVENT_AXIS;
        e.control = i;
        input_event_push(runtime, e);
    }
    OK(runtime.count == kInputEventCapacity);
    OK(runtime.overflow_count == 5u);
    event = pop(runtime);
    OK(event.control == 5u);

    std::puts("input runtime: OK");
    return 0;
}
