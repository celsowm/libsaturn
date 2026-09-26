#ifndef SATURN_CORE_INPUT_RUNTIME_HPP
#define SATURN_CORE_INPUT_RUNTIME_HPP

#include <stdint.h>

#include "saturn/input.h"

namespace saturn::core {

constexpr uint16_t kInputEventCapacity = 32u;

/* Everything sat_input_poll learned about one device slot (a port, or one tap
 * behind a multitap). */
struct InputSlot {
    sat_device_info_t info;
    sat_analog_state_t analog;
    sat_mouse_state_t mouse;
    sat_keyboard_state_t keyboard;
};

struct InputRuntime {
    sat_pad_state_t pads[SAT_PAD_PORT_COUNT][SAT_PAD_TAP_MAX];
    InputSlot slots[SAT_PAD_PORT_COUNT][SAT_PAD_TAP_MAX];
    sat_event_t events[kInputEventCapacity];
    uint16_t head;
    uint16_t count;
    uint32_t overflow_count;
};

extern InputRuntime g_input_runtime;

inline void input_runtime_reset(InputRuntime& runtime) {
    for (uint16_t port = 0u; port < SAT_PAD_PORT_COUNT; ++port) {
        for (uint16_t tap = 0u; tap < SAT_PAD_TAP_MAX; ++tap) {
            runtime.pads[port][tap] = {};
            runtime.slots[port][tap] = {};
            runtime.slots[port][tap].info.peripheral_id = 0xFFu;
            runtime.slots[port][tap].info.multitap_id = 0xFu;
        }
    }
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
    uint8_t tap,
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
        event.tap = tap;
        event.control = mask;
        event.value = value;
        input_event_push(runtime, event);
    }
}

/* Applies one device's buttons: pressed / released edges and connect and
 * disconnect events. `tap` is the device index behind a multitap. */
inline void input_apply_pad_sample(
    InputRuntime& runtime,
    uint8_t port,
    bool connected,
    uint16_t held,
    uint8_t tap = 0u
) {
    if (port >= SAT_PAD_PORT_COUNT || tap >= SAT_PAD_TAP_MAX) return;
    const sat_pad_state_t previous = runtime.pads[port][tap];
    sat_pad_state_t next{};
    next.connected = connected ? 1u : 0u;
    next.held = connected ? held : 0u;
    next.pressed = static_cast<uint16_t>((~previous.held) & next.held);
    next.released = static_cast<uint16_t>(previous.held & (~next.held));

    if (connected && previous.connected == 0u) {
        sat_event_t event{};
        event.type = SAT_EVENT_PAD_CONNECTED;
        event.port = port;
        event.tap = tap;
        event.value = 1;
        input_event_push(runtime, event);
    }

    input_emit_button_events(runtime, port, tap, next.pressed, SAT_EVENT_BUTTON_DOWN, 1);
    input_emit_button_events(runtime, port, tap, next.released, SAT_EVENT_BUTTON_UP, 0);

    if (!connected && previous.connected != 0u) {
        sat_event_t event{};
        event.type = SAT_EVENT_PAD_DISCONNECTED;
        event.port = port;
        event.tap = tap;
        event.value = 0;
        input_event_push(runtime, event);
    }

    runtime.pads[port][tap] = next;
}

/* Records what the slot holds. A slot that lost its device forgets the
 * device-specific readings so a stale stick position or mouse motion is
 * never reported for whatever is plugged in next. */
inline void input_set_device_info(
    InputRuntime& runtime, uint8_t port, uint8_t tap, const sat_device_info_t& info) {
    if (port >= SAT_PAD_PORT_COUNT || tap >= SAT_PAD_TAP_MAX) return;
    InputSlot& slot = runtime.slots[port][tap];
    if (slot.info.kind != info.kind || slot.info.peripheral_id != info.peripheral_id) {
        slot.analog = {};
        slot.mouse = {};
        slot.keyboard = {};
    }
    slot.info = info;
}

inline void input_emit_axis(
    InputRuntime& runtime, uint8_t port, uint8_t tap, uint16_t axis, int16_t value) {
    sat_event_t event{};
    event.type = SAT_EVENT_AXIS;
    event.port = port;
    event.tap = tap;
    event.control = axis;
    event.value = value;
    input_event_push(runtime, event);
}

/* Stores the axes and emits an axis event for each one that moved. */
inline void input_apply_analog(
    InputRuntime& runtime, uint8_t port, uint8_t tap, const sat_analog_state_t& next) {
    if (port >= SAT_PAD_PORT_COUNT || tap >= SAT_PAD_TAP_MAX) return;
    sat_analog_state_t& now = runtime.slots[port][tap].analog;
    const sat_analog_state_t before = now;
    now = next;
    if (before.axis_count == 0u) return;   /* first sample: nothing moved yet */
    if (next.x != before.x) input_emit_axis(runtime, port, tap, SAT_AXIS_X, next.x);
    if (next.y != before.y) input_emit_axis(runtime, port, tap, SAT_AXIS_Y, next.y);
    if (next.z != before.z) input_emit_axis(runtime, port, tap, SAT_AXIS_Z, next.z);
    if (next.l != before.l) input_emit_axis(runtime, port, tap, SAT_AXIS_L, next.l);
    if (next.r != before.r) input_emit_axis(runtime, port, tap, SAT_AXIS_R, next.r);
}

inline int16_t input_clamp16(int32_t v) {
    return v > 32767 ? 32767 : v < -32768 ? -32768 : static_cast<int16_t>(v);
}

/* Motion accumulates until sat_mouse_read consumes it. */
inline void input_apply_mouse(
    InputRuntime& runtime, uint8_t port, uint8_t tap, uint8_t buttons, int16_t dx, int16_t dy,
    bool overflow) {
    if (port >= SAT_PAD_PORT_COUNT || tap >= SAT_PAD_TAP_MAX) return;
    sat_mouse_state_t& mouse = runtime.slots[port][tap].mouse;
    mouse.buttons = buttons;
    mouse.dx += dx;
    mouse.dy += dy;
    if (overflow) mouse.overflow = 1u;
    if (dx != 0) input_emit_axis(runtime, port, tap, SAT_AXIS_MOUSE_X, dx);
    if (dy != 0) input_emit_axis(runtime, port, tap, SAT_AXIS_MOUSE_Y, dy);
}

inline void input_apply_keyboard(
    InputRuntime& runtime, uint8_t port, uint8_t tap, uint16_t held, uint8_t locks,
    uint8_t key, bool make, bool brk) {
    if (port >= SAT_PAD_PORT_COUNT || tap >= SAT_PAD_TAP_MAX) return;
    sat_keyboard_state_t& kb = runtime.slots[port][tap].keyboard;
    kb.held = held;
    kb.locks = locks;
    kb.make = make ? 1u : 0u;
    kb.brk = brk ? 1u : 0u;
    if (make || brk) kb.last_key = key;
    if (make || brk) {
        sat_event_t event{};
        event.type = make ? SAT_EVENT_KEY_DOWN : SAT_EVENT_KEY_UP;
        event.port = port;
        event.tap = tap;
        event.control = key;
        event.value = make ? 1 : 0;
        input_event_push(runtime, event);
    }
}

}  // namespace saturn::core

#endif /* SATURN_CORE_INPUT_RUNTIME_HPP */
