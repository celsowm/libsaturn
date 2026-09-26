#include "saturn/input.h"

#include "src/input/runtime.hpp"
#include "src/core/runtime/state.hpp"
#include "src/hal/smpc/smpc.hpp"

namespace {

namespace smpc = saturn::hal::smpc;

/* A mouse has no pad bits of its own; its buttons ride on the pad state so
 * code that only knows pads still sees clicks. */
uint16_t mouse_buttons_as_pad(uint8_t buttons) {
    uint16_t held = 0u;
    if ((buttons & smpc::kMouseLeft) != 0u) held |= SAT_PAD_A;
    if ((buttons & smpc::kMouseRight) != 0u) held |= SAT_PAD_C;
    if ((buttons & smpc::kMouseMiddle) != 0u) held |= SAT_PAD_B;
    if ((buttons & smpc::kMouseStart) != 0u) held |= SAT_PAD_START;
    return held;
}

sat_device_kind_t public_kind(smpc::DeviceKind kind) {
    switch (kind) {
        case smpc::DeviceKind::Pad: return SAT_DEVICE_PAD;
        case smpc::DeviceKind::Analog: return SAT_DEVICE_ANALOG_PAD;
        case smpc::DeviceKind::Mouse: return SAT_DEVICE_MOUSE;
        case smpc::DeviceKind::Keyboard: return SAT_DEVICE_KEYBOARD;
        case smpc::DeviceKind::Unknown: return SAT_DEVICE_UNKNOWN;
        default: return SAT_DEVICE_NONE;
    }
}

/* Files one device (or an empty tap) of a port into the runtime. */
void apply_slot(uint8_t port, uint8_t tap, const smpc::PortSample& p) {
    using namespace saturn::core;
    smpc::DeviceSample empty{};
    empty.id = smpc::kIdNone;
    const smpc::DeviceSample& d = tap < p.tap_count ? p.devices[tap] : empty;

    sat_device_info_t info{};
    info.kind = static_cast<uint8_t>(public_kind(d.kind));
    info.peripheral_id = d.id;
    info.data_size = d.size;
    info.multitap_id = p.multitap_id;
    info.tap_count = p.tap_count;
    if (d.kind == smpc::DeviceKind::Analog) info.axis_count = smpc::analog_axes(d).count;
    input_set_device_info(g_input_runtime, port, tap, info);

    switch (d.kind) {
        case smpc::DeviceKind::Pad:
            input_apply_pad_sample(g_input_runtime, port, true, smpc::device_buttons(d), tap);
            break;
        case smpc::DeviceKind::Analog: {
            input_apply_pad_sample(g_input_runtime, port, true, smpc::device_buttons(d), tap);
            const smpc::AnalogAxes a = smpc::analog_axes(d);
            sat_analog_state_t analog{};
            analog.axis_count = a.count;
            analog.x = a.x;
            analog.y = a.y;
            analog.z = a.z;
            analog.l = a.l;
            analog.r = a.r;
            analog.has_triggers = a.triggers ? 1u : 0u;
            input_apply_analog(g_input_runtime, port, tap, analog);
            break;
        }
        case smpc::DeviceKind::Mouse: {
            const smpc::MouseMotion m = smpc::mouse_motion(d);
            input_apply_pad_sample(g_input_runtime, port, true, mouse_buttons_as_pad(m.buttons), tap);
            input_apply_mouse(g_input_runtime, port, tap, m.buttons, m.dx, m.dy,
                              m.x_overflow || m.y_overflow);
            break;
        }
        case smpc::DeviceKind::Keyboard: {
            const smpc::KeyboardEvent k = smpc::keyboard_event(d);
            const uint16_t held = smpc::device_buttons(d);
            input_apply_pad_sample(g_input_runtime, port, true, held, tap);
            input_apply_keyboard(g_input_runtime, port, tap, held, k.locks, k.key, k.make, k.brk);
            break;
        }
        default:
            input_apply_pad_sample(g_input_runtime, port, false, 0u, tap);
            break;
    }
}

/* Slots past both the devices now present and the ones present last time hold
 * nothing and have nothing to report, so a plain pad costs one slot, not six. */
void apply_port(uint8_t port, const smpc::PortSample& p) {
    using namespace saturn::core;
    const uint8_t previous = g_input_runtime.slots[port][0].info.tap_count;
    uint8_t limit = p.tap_count > previous ? p.tap_count : previous;
    if (limit < 1u) limit = 1u;
    if (limit > SAT_PAD_TAP_MAX) limit = SAT_PAD_TAP_MAX;
    for (uint8_t tap = 0u; tap < limit; ++tap) apply_slot(port, tap, p);
}

/* One INTBACK reads the wanted ports (both, or only `only_port`, which costs
 * what the old per-port read did); only those are applied, so polling one
 * port never consumes the other port's button edges. */
sat_result_t poll_ports(uint8_t only_port) {
    smpc::PeripheralSnapshot snapshot;   /* the parser clears it */
    const uint8_t mask = only_port == 0xFFu ? smpc::kIntbackPortsBoth
                                            : static_cast<uint8_t>(1u << only_port);
    if (!smpc::read_peripherals(&snapshot, mask)) return SAT_ERR_IO;
    for (uint8_t port = 0u; port < SAT_PAD_PORT_COUNT; ++port) {
        if (only_port != 0xFFu && port != only_port) continue;
        apply_port(port, snapshot.ports[port]);
    }
    return SAT_OK;
}

bool valid_slot(uint8_t port, uint8_t tap) {
    return port < SAT_PAD_PORT_COUNT && tap < SAT_PAD_TAP_MAX;
}

}  // namespace

extern "C" sat_result_t sat_input_poll(void) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    return poll_ports(0xFFu);
}

extern "C" sat_result_t sat_pad_poll_port(uint8_t port, sat_pad_state_t* out_state) {
    using namespace saturn::core;
    const sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    if (port >= SAT_PAD_PORT_COUNT || out_state == nullptr) return SAT_ERR_INVALID_ARG;
    SAT_TRY(poll_ports(port));
    *out_state = g_input_runtime.pads[port][0];
    return SAT_OK;
}

extern "C" sat_result_t sat_pad_poll(sat_pad_state_t* out_state) {
    return sat_pad_poll_port(0u, out_state);
}

extern "C" uint16_t sat_pad_held_port(uint8_t port) {
    if (port >= SAT_PAD_PORT_COUNT) return 0u;
    return saturn::core::g_input_runtime.pads[port][0].held;
}

extern "C" uint16_t sat_pad_held(void) {
    return sat_pad_held_port(0u);
}

extern "C" sat_result_t sat_input_device_info(uint8_t port, uint8_t tap,
                                              sat_device_info_t* out_info) {
    using namespace saturn::core;
    SAT_TRY(require_initialized());
    if (!valid_slot(port, tap) || out_info == nullptr) return SAT_ERR_INVALID_ARG;
    *out_info = g_input_runtime.slots[port][tap].info;
    return SAT_OK;
}

extern "C" sat_result_t sat_pad_tap_state(uint8_t port, uint8_t tap, sat_pad_state_t* out_state) {
    using namespace saturn::core;
    SAT_TRY(require_initialized());
    if (!valid_slot(port, tap) || out_state == nullptr) return SAT_ERR_INVALID_ARG;
    *out_state = g_input_runtime.pads[port][tap];
    return SAT_OK;
}

extern "C" sat_result_t sat_pad_analog(uint8_t port, uint8_t tap, sat_analog_state_t* out_analog) {
    using namespace saturn::core;
    SAT_TRY(require_initialized());
    if (!valid_slot(port, tap) || out_analog == nullptr) return SAT_ERR_INVALID_ARG;
    const InputSlot& slot = g_input_runtime.slots[port][tap];
    if (slot.info.kind != SAT_DEVICE_ANALOG_PAD) return SAT_ERR_UNSUPPORTED;
    *out_analog = slot.analog;
    return SAT_OK;
}

extern "C" sat_result_t sat_mouse_read(uint8_t port, uint8_t tap, sat_mouse_state_t* out_mouse) {
    using namespace saturn::core;
    SAT_TRY(require_initialized());
    if (!valid_slot(port, tap) || out_mouse == nullptr) return SAT_ERR_INVALID_ARG;
    InputSlot& slot = g_input_runtime.slots[port][tap];
    if (slot.info.kind != SAT_DEVICE_MOUSE) return SAT_ERR_UNSUPPORTED;
    *out_mouse = slot.mouse;
    slot.mouse.dx = 0;
    slot.mouse.dy = 0;
    slot.mouse.overflow = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_keyboard_read(uint8_t port, uint8_t tap,
                                          sat_keyboard_state_t* out_keyboard) {
    using namespace saturn::core;
    SAT_TRY(require_initialized());
    if (!valid_slot(port, tap) || out_keyboard == nullptr) return SAT_ERR_INVALID_ARG;
    const InputSlot& slot = g_input_runtime.slots[port][tap];
    if (slot.info.kind != SAT_DEVICE_KEYBOARD) return SAT_ERR_UNSUPPORTED;
    *out_keyboard = slot.keyboard;
    return SAT_OK;
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
