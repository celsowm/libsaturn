#include <cstdio>
#include <cstdlib>

#include "saturn/input.h"
#include "src/input/runtime.hpp"
#include "src/core/runtime/state.hpp"
#include "src/hal/smpc/smpc.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace {
using namespace saturn::hal::smpc;

PeripheralSnapshot g_snapshot{};
bool g_read_ok = true;
uint32_t g_reads = 0u;
uint8_t g_last_mask = 0u;

/* Buttons are active low in the SMPC data. */
uint8_t low(uint8_t mask) { return static_cast<uint8_t>(~mask); }

DeviceSample pad(uint8_t d1 = 0xFFu, uint8_t d2 = 0xFFu) {
    DeviceSample d{};
    d.kind = DeviceKind::Pad;
    d.id = 0x02u;
    d.size = d.stored = 2u;
    d.data[0] = d1;
    d.data[1] = d2;
    return d;
}

DeviceSample analog(uint8_t x, uint8_t y, uint8_t r, uint8_t l) {
    DeviceSample d{};
    d.kind = DeviceKind::Analog;
    d.id = 0x16u;
    d.size = d.stored = 6u;
    d.data[0] = d.data[1] = 0xFFu;
    d.data[2] = x;
    d.data[3] = y;
    d.data[4] = r;
    d.data[5] = l;
    return d;
}

DeviceSample mouse(uint8_t flags, uint8_t xd, uint8_t yd) {
    DeviceSample d{};
    d.kind = DeviceKind::Mouse;
    d.id = 0xE3u;
    d.size = d.stored = 3u;
    d.data[0] = flags;
    d.data[1] = xd;
    d.data[2] = yd;
    return d;
}

DeviceSample keyboard(uint8_t d1, uint8_t lock, uint8_t key) {
    DeviceSample d{};
    d.kind = DeviceKind::Keyboard;
    d.id = 0x34u;
    d.size = d.stored = 4u;
    d.data[0] = d1;
    d.data[1] = 0xFFu;
    d.data[2] = lock;
    d.data[3] = key;
    return d;
}

void direct(uint8_t port, const DeviceSample& d) {
    PortSample& p = g_snapshot.ports[port];
    p = {};
    p.parsed = true;
    p.multitap_id = kNoMultitap;
    p.tap_count = 1u;
    p.devices[0] = d;
}

void empty(uint8_t port) {
    PortSample& p = g_snapshot.ports[port];
    p = {};
    p.parsed = true;
    p.multitap_id = kNoMultitap;
}
}

namespace saturn::hal::smpc {
bool read_peripherals(PeripheralSnapshot* out, uint8_t port_mask, ParseStatus* parse) {
    if (parse != nullptr) *parse = ParseStatus::Ok;
    if (out == nullptr || !g_read_ok) return false;
    ++g_reads;
    g_last_mask = port_mask;
    *out = g_snapshot;
    /* A port outside the mask is not in the report. */
    for (uint8_t port = 0u; port < kPortCount; ++port) {
        if ((port_mask & (1u << port)) == 0u) out->ports[port] = PortSample{};
    }
    return true;
}
}

static sat_event_t next_event() {
    sat_event_t event{};
    OK(sat_event_poll(&event) == 1);
    return event;
}

static void pads_and_events_as_before() {
    using namespace saturn::core;
    direct(0u, pad(low(0x04u)));            /* A */
    direct(1u, pad(low(0x10u)));            /* the byte for B is bit 0; bit 4 is Up */
    g_snapshot.ports[1].devices[0] = pad(low(0x01u));   /* B */
    OK(sat_input_poll() == SAT_OK);
    OK(g_reads == 1u && g_last_mask == 0x3u);   /* one INTBACK serves both ports */
    OK(sat_pad_held() == SAT_PAD_A);
    OK(sat_pad_held_port(1u) == SAT_PAD_B);
    OK(g_input_runtime.pads[0][0].connected == 1u);
    OK(g_input_runtime.pads[1][0].connected == 1u);

    sat_event_t event = next_event();
    OK(event.type == SAT_EVENT_PAD_CONNECTED && event.port == 0u && event.tap == 0u);
    event = next_event();
    OK(event.type == SAT_EVENT_BUTTON_DOWN && event.port == 0u && event.control == SAT_PAD_A);
    event = next_event();
    OK(event.type == SAT_EVENT_PAD_CONNECTED && event.port == 1u);
    event = next_event();
    OK(event.type == SAT_EVENT_BUTTON_DOWN && event.port == 1u && event.control == SAT_PAD_B);
    OK(sat_event_poll(&event) == 0);

    /* Polling one port applies only that port: the other keeps its edges. */
    direct(0u, pad(low(0x02u)));            /* C on port 0 (not applied) */
    direct(1u, pad(low(0x02u)));            /* C on port 1 */
    sat_pad_state_t state{};
    OK(sat_pad_poll_port(1u, &state) == SAT_OK);
    OK(g_last_mask == 0x2u);                /* port 2 alone: port 1 stays in 0-byte mode */
    OK(state.held == SAT_PAD_C && state.pressed == SAT_PAD_C && state.released == SAT_PAD_B);
    OK(g_input_runtime.pads[0][0].held == SAT_PAD_A);
    event = next_event();
    OK(event.type == SAT_EVENT_BUTTON_DOWN && event.port == 1u && event.control == SAT_PAD_C);
    event = next_event();
    OK(event.type == SAT_EVENT_BUTTON_UP && event.port == 1u && event.control == SAT_PAD_B);
    OK(sat_event_poll(&event) == 0);

    empty(0u);
    OK(sat_pad_poll(&state) == SAT_OK);
    OK(state.connected == 0u && state.released == SAT_PAD_A);
    event = next_event();
    OK(event.type == SAT_EVENT_BUTTON_UP && event.control == SAT_PAD_A);
    event = next_event();
    OK(event.type == SAT_EVENT_PAD_DISCONNECTED && event.port == 0u);

    g_read_ok = false;
    const sat_pad_state_t before = g_input_runtime.pads[1][0];
    OK(sat_pad_poll_port(1u, &state) == SAT_ERR_IO);
    OK(g_input_runtime.pads[1][0].held == before.held);
    g_read_ok = true;
    OK(sat_pad_poll_port(2u, &state) == SAT_ERR_INVALID_ARG);
    OK(sat_event_poll(nullptr) == SAT_ERR_INVALID_ARG);
}

static void analog_pad_reports_axes_and_moves() {
    using namespace saturn::core;
    input_runtime_reset(g_input_runtime);
    direct(0u, analog(0x80u, 0x80u, 0x00u, 0x00u));
    empty(1u);
    OK(sat_input_poll() == SAT_OK);
    sat_device_info_t info{};
    OK(sat_input_device_info(0u, 0u, &info) == SAT_OK);
    OK(info.kind == SAT_DEVICE_ANALOG_PAD && info.peripheral_id == 0x16u && info.axis_count == 4u);
    OK(info.multitap_id == 0xFu && info.tap_count == 1u);
    sat_analog_state_t a{};
    OK(sat_pad_analog(0u, 0u, &a) == SAT_OK);
    OK(a.axis_count == 4u && a.x == 0x80u && a.y == 0x80u && a.has_triggers == 1u);
    /* The first sample raises no axis events, only the connection. */
    sat_event_t event = next_event();
    OK(event.type == SAT_EVENT_PAD_CONNECTED);
    OK(sat_event_poll(&event) == 0);

    direct(0u, analog(0x90u, 0x80u, 0x00u, 0xFFu));
    OK(sat_input_poll() == SAT_OK);
    event = next_event();
    OK(event.type == SAT_EVENT_AXIS && event.control == SAT_AXIS_X && event.value == 0x90);
    event = next_event();
    OK(event.type == SAT_EVENT_AXIS && event.control == SAT_AXIS_L && event.value == 0xFF);
    OK(sat_event_poll(&event) == 0);
    OK(sat_pad_analog(0u, 0u, &a) == SAT_OK && a.x == 0x90u && a.l == 0xFFu);

    /* A digital pad has no axes, and a pad in that slot forgets them. */
    OK(sat_pad_analog(1u, 0u, &a) == SAT_ERR_UNSUPPORTED);
    direct(0u, pad());
    OK(sat_input_poll() == SAT_OK);
    OK(sat_pad_analog(0u, 0u, &a) == SAT_ERR_UNSUPPORTED);
    OK(g_input_runtime.slots[0][0].analog.axis_count == 0u);
    OK(sat_pad_analog(2u, 0u, &a) == SAT_ERR_INVALID_ARG);
    OK(sat_pad_analog(0u, 6u, &a) == SAT_ERR_INVALID_ARG);
    OK(sat_pad_analog(0u, 0u, nullptr) == SAT_ERR_INVALID_ARG);
}

static void mouse_motion_accumulates_until_read() {
    using namespace saturn::core;
    input_runtime_reset(g_input_runtime);
    /* Left held, X = -5, Y raw +3 (up) -> dy -3. */
    direct(0u, mouse(0x11u, 0xFBu, 0x03u));
    empty(1u);
    OK(sat_input_poll() == SAT_OK);
    direct(0u, mouse(0x02u, 0x02u, 0x00u));           /* right held, dx +2 */
    OK(sat_input_poll() == SAT_OK);
    sat_mouse_state_t m{};
    OK(sat_mouse_read(0u, 0u, &m) == SAT_OK);
    OK(m.buttons == SAT_MOUSE_RIGHT && m.dx == -3 && m.dy == -3 && m.overflow == 0u);
    OK(sat_mouse_read(0u, 0u, &m) == SAT_OK && m.dx == 0 && m.dy == 0);
    OK(sat_pad_held() == SAT_PAD_C);                  /* buttons ride on the pad state */
    sat_event_t event = next_event();
    OK(event.type == SAT_EVENT_PAD_CONNECTED);
    event = next_event();
    OK(event.type == SAT_EVENT_BUTTON_DOWN && event.control == SAT_PAD_A);
    event = next_event();
    OK(event.type == SAT_EVENT_AXIS && event.control == SAT_AXIS_MOUSE_X && event.value == -5);
    event = next_event();
    OK(event.type == SAT_EVENT_AXIS && event.control == SAT_AXIS_MOUSE_Y && event.value == -3);
    direct(0u, mouse(0x80u, 0x00u, 0x00u));           /* overflow flag alone */
    OK(sat_input_poll() == SAT_OK);
    OK(sat_mouse_read(0u, 0u, &m) == SAT_OK && m.overflow == 1u);
    sat_analog_state_t a{};
    OK(sat_pad_analog(0u, 0u, &a) == SAT_ERR_UNSUPPORTED);
    sat_keyboard_state_t k{};
    OK(sat_keyboard_read(0u, 0u, &k) == SAT_ERR_UNSUPPORTED);
}

static void keyboard_reports_keys_and_locks() {
    using namespace saturn::core;
    input_runtime_reset(g_input_runtime);
    direct(0u, keyboard(low(0x20u), 0x40u | 0x06u | 0x08u, 0x2Bu));   /* Down, caps, make */
    empty(1u);
    OK(sat_input_poll() == SAT_OK);
    sat_keyboard_state_t k{};
    OK(sat_keyboard_read(0u, 0u, &k) == SAT_OK);
    OK(k.held == SAT_PAD_DOWN && k.locks == SAT_KEYBOARD_CAPS && k.last_key == 0x2Bu && k.make == 1u);
    sat_event_t event = next_event();
    OK(event.type == SAT_EVENT_PAD_CONNECTED);
    event = next_event();
    OK(event.type == SAT_EVENT_BUTTON_DOWN && event.control == SAT_PAD_DOWN);
    event = next_event();
    OK(event.type == SAT_EVENT_KEY_DOWN && event.control == 0x2Bu && event.value == 1);
    direct(0u, keyboard(low(0x20u), 0x40u | 0x06u | 0x01u, 0x2Bu));   /* released */
    OK(sat_input_poll() == SAT_OK);
    event = next_event();
    OK(event.type == SAT_EVENT_KEY_UP && event.control == 0x2Bu && event.value == 0);
}

static void multitap_devices_get_their_own_slots() {
    using namespace saturn::core;
    input_runtime_reset(g_input_runtime);
    PortSample& p = g_snapshot.ports[0];
    p = {};
    p.parsed = true;
    p.multitap_id = 1u;
    p.tap_count = 3u;
    p.devices[0] = pad(low(0x04u));                       /* A */
    p.devices[1] = DeviceSample{};                        /* empty tap */
    p.devices[1].id = kIdNone;
    p.devices[2] = analog(0x10u, 0x20u, 0x30u, 0x40u);
    empty(1u);
    OK(sat_input_poll() == SAT_OK);
    sat_device_info_t info{};
    OK(sat_input_device_info(0u, 0u, &info) == SAT_OK);
    OK(info.kind == SAT_DEVICE_PAD && info.multitap_id == 1u && info.tap_count == 3u);
    OK(sat_input_device_info(0u, 1u, &info) == SAT_OK && info.kind == SAT_DEVICE_NONE);
    OK(sat_input_device_info(0u, 2u, &info) == SAT_OK && info.kind == SAT_DEVICE_ANALOG_PAD);
    OK(sat_input_device_info(0u, 3u, &info) == SAT_OK && info.kind == SAT_DEVICE_NONE);
    sat_pad_state_t state{};
    OK(sat_pad_tap_state(0u, 0u, &state) == SAT_OK && state.held == SAT_PAD_A);
    OK(sat_pad_tap_state(0u, 1u, &state) == SAT_OK && state.connected == 0u);
    OK(sat_pad_tap_state(0u, 2u, &state) == SAT_OK && state.connected == 1u);
    sat_analog_state_t a{};
    OK(sat_pad_analog(0u, 2u, &a) == SAT_OK && a.x == 0x10u && a.r == 0x30u && a.l == 0x40u);
    OK(sat_pad_analog(0u, 0u, &a) == SAT_ERR_UNSUPPORTED);

    /* Events name their tap. */
    sat_event_t event = next_event();
    OK(event.type == SAT_EVENT_PAD_CONNECTED && event.tap == 0u);
    event = next_event();
    OK(event.type == SAT_EVENT_BUTTON_DOWN && event.tap == 0u && event.control == SAT_PAD_A);
    event = next_event();
    OK(event.type == SAT_EVENT_PAD_CONNECTED && event.tap == 2u);
    OK(sat_event_poll(&event) == 0);

    OK(sat_input_device_info(0u, 6u, &info) == SAT_ERR_INVALID_ARG);
    OK(sat_pad_tap_state(2u, 0u, &state) == SAT_ERR_INVALID_ARG);
    OK(sat_input_device_info(0u, 0u, nullptr) == SAT_ERR_INVALID_ARG);
}

static void unknown_devices_are_reported_but_not_pads() {
    using namespace saturn::core;
    input_runtime_reset(g_input_runtime);
    DeviceSample d{};
    d.kind = DeviceKind::Unknown;
    d.id = 0xA5u;
    direct(0u, d);
    empty(1u);
    OK(sat_input_poll() == SAT_OK);
    sat_device_info_t info{};
    OK(sat_input_device_info(0u, 0u, &info) == SAT_OK);
    OK(info.kind == SAT_DEVICE_UNKNOWN && info.peripheral_id == 0xA5u);
    sat_pad_state_t state{};
    OK(sat_pad_tap_state(0u, 0u, &state) == SAT_OK && state.connected == 0u);
}

int main() {
    using namespace saturn::core;

    /* Nothing before sat_init. */
    g_state = {};
    sat_pad_state_t state{};
    OK(sat_input_poll() == SAT_ERR_NOT_INITIALIZED);
    OK(sat_pad_poll_port(0u, &state) == SAT_ERR_NOT_INITIALIZED);
    sat_device_info_t info{};
    OK(sat_input_device_info(0u, 0u, &info) == SAT_ERR_NOT_INITIALIZED);

    g_state.initialized = true;
    input_runtime_reset(g_input_runtime);
    pads_and_events_as_before();
    analog_pad_reports_axes_and_moves();
    mouse_motion_accumulates_until_read();
    keyboard_reports_keys_and_locks();
    multitap_devices_get_their_own_slots();
    unknown_devices_are_reported_but_not_pads();

    std::puts("input api: OK");
    return 0;
}
