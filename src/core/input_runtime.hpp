#ifndef SATURN_CORE_INPUT_RUNTIME_HPP
#define SATURN_CORE_INPUT_RUNTIME_HPP

#include <stdint.h>

#include "saturn/input.h"

namespace saturn::core {

constexpr uint16_t kInputEventCapacity = 32u;

struct InputRuntime {
    sat_pad_state_t pads[SAT_PAD_PORT_COUNT];
    sat_event_t events[kInputEventCapacity];
    uint16_t head;
    uint16_t count;
    uint32_t overflow_count;
};

extern InputRuntime g_input_runtime;

inline void input_runtime_reset(InputRuntime& runtime) {
    for (uint16_t i = 0u; i < SAT_PAD_PORT_COUNT; ++i) runtime.pads[i] = {};
    for (uint16_t i = 0u; i < kInputEventCapacity; ++i) runtime.events[i] = {};
    runtime.head = 0u;
    runtime.count = 0u;
    runtime.overflow_count = 0u;
}

inline void input_event_push(InputRuntime& runtime, const sat_event_t& event) {
    if (runtime.count == kInputEventCapacity) {
        runtime.head = static_cast<uint16_t>((runtime.head + 1u) % kInputEventCapacity);
        --runtime.count;
        ++runtime.overflow_count;
    }
    const uint16_t tail =
        static_cast<uint16_t>((runtime.head + runtime.count) % kInputEventCapacity);
    runtime.events[tail] = event;
    ++runtime.count;
}

inline bool input_event_pop(InputRuntime& runtime, sat_event_t* out_event) {
    if (out_event == nullptr || runtime.count == 0u) return false;
    *out_event = runtime.events[runtime.head];
    runtime.head = static_cast<uint16_t>((runtime.head + 1u) % kInputEventCapacity);
    --runtime.count;
    return true;
}

inline void input_emit_button_events(
    InputRuntime& runtime,
    uint8_t port,
    uint16_t bits,
    uint16_t type,
    int16_t value
) {
    for (uint16_t bit = 0u; bit < 16u; ++bit) {
        const uint16_t mask = static_cast<uint16_t>(1u << bit);
        if ((bits & mask) == 0u) continue;
        sat_event_t event{};
        event.type = type;
        event.port = port;
        event.control = mask;
        event.value = value;
        input_event_push(runtime, event);
    }
}

inline void input_apply_pad_sample(
    InputRuntime& runtime,
    uint8_t port,
    bool connected,
    uint16_t held
) {
    if (port >= SAT_PAD_PORT_COUNT) return;
    const sat_pad_state_t previous = runtime.pads[port];
    sat_pad_state_t next{};
    next.connected = connected ? 1u : 0u;
    next.held = connected ? held : 0u;
    next.pressed = static_cast<uint16_t>((~previous.held) & next.held);
    next.released = static_cast<uint16_t>(previous.held & (~next.held));

    if (connected && previous.connected == 0u) {
        sat_event_t event{};
        event.type = SAT_EVENT_PAD_CONNECTED;
        event.port = port;
        event.value = 1;
        input_event_push(runtime, event);
    }

    input_emit_button_events(runtime, port, next.pressed, SAT_EVENT_BUTTON_DOWN, 1);
    input_emit_button_events(runtime, port, next.released, SAT_EVENT_BUTTON_UP, 0);

    if (!connected && previous.connected != 0u) {
        sat_event_t event{};
        event.type = SAT_EVENT_PAD_DISCONNECTED;
        event.port = port;
        event.value = 0;
        input_event_push(runtime, event);
    }

    runtime.pads[port] = next;
}

}  // namespace saturn::core

#endif /* SATURN_CORE_INPUT_RUNTIME_HPP */
