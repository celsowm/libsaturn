#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/hal/smpc/peripheral_logic.hpp"

using namespace saturn::hal::smpc;

static const bool kBoth[kPortCount] = {true, true};

/* Streams below are what INTBACK returns: Ymir's format for a directly
 * connected device, the manual's figures for taps. Buttons are active low. */

static void control_pad_and_empty_port() {
    const uint8_t s[] = {0xF1, 0x02, 0x7B, 0xFF, 0xF0};
    PeripheralSnapshot snap;
    assert(parse_snapshot(s, sizeof s, kBoth, &snap) == ParseStatus::Ok);
    const PortSample& p1 = snap.ports[0];
    assert(p1.parsed && p1.multitap_id == kNoMultitap && p1.tap_count == 1u);
    assert(p1.devices[0].kind == DeviceKind::Pad && p1.devices[0].id == 0x02u);
    assert(p1.devices[0].size == 2u);
    /* 0x7B = 0111 1011: bit 7 (right) clear = pressed, bit 2 (A) clear = pressed. */
    assert(device_buttons(p1.devices[0]) == (SAT_PAD_RIGHT | SAT_PAD_A));
    const PortSample& p2 = snap.ports[1];
    assert(p2.parsed && p2.tap_count == 0u && p2.devices[0].kind == DeviceKind::None);
}

static void three_d_pad_in_analog_mode() {
    /* ID 0x16: buttons, then X, Y, R trigger, L trigger. */
    const uint8_t s[] = {0xF1, 0x16, 0xFF, 0xFF, 0x40, 0xC0, 0x22, 0xEE, 0xF0};
    PeripheralSnapshot snap;
    assert(parse_snapshot(s, sizeof s, kBoth, &snap) == ParseStatus::Ok);
    const DeviceSample& d = snap.ports[0].devices[0];
    assert(d.kind == DeviceKind::Analog && d.size == 6u);
    const AnalogAxes a = analog_axes(d);
    assert(a.count == 4u && a.x == 0x40u && a.y == 0xC0u);
    assert(a.triggers && a.r == 0x22u && a.l == 0xEEu);
    assert(device_buttons(d) == 0u);
}

static void three_d_pad_in_digital_mode_is_a_pad() {
    /* Ymir reports type 1 with two bytes; hardware reports 0x02. Both are pads. */
    const uint8_t ymir[] = {0xF1, 0x12, 0xFB, 0xFF, 0xF0};
    const uint8_t hw[] = {0xF1, 0x02, 0xFB, 0xFF, 0xF0};
    PeripheralSnapshot a, b;
    assert(parse_snapshot(ymir, sizeof ymir, kBoth, &a) == ParseStatus::Ok);
    assert(parse_snapshot(hw, sizeof hw, kBoth, &b) == ParseStatus::Ok);
    assert(a.ports[0].devices[0].kind == DeviceKind::Pad);
    assert(b.ports[0].devices[0].kind == DeviceKind::Pad);
    assert(device_buttons(a.ports[0].devices[0]) == SAT_PAD_A);
    assert(analog_axes(a.ports[0].devices[0]).count == 0u);
}

static void three_axis_analog_stick_has_a_throttle() {
    const uint8_t s[] = {0xF1, 0x15, 0xFF, 0xFF, 0x10, 0x20, 0x30, 0xF0};
    PeripheralSnapshot snap;
    assert(parse_snapshot(s, sizeof s, kBoth, &snap) == ParseStatus::Ok);
    const AnalogAxes a = analog_axes(snap.ports[0].devices[0]);
    assert(a.count == 3u && a.x == 0x10u && a.y == 0x20u && a.z == 0x30u && !a.triggers);
}

static void shuttle_mouse_is_nine_bit_two_complement_with_y_up_raw() {
    /* Left held; X = -5 (sign set, XD = 0xFB); Y raw = +3 (up) -> dy = -3 down-screen. */
    const uint8_t s[] = {0xF1, 0xE3, 0x11, 0xFB, 0x03, 0xF0};
    PeripheralSnapshot snap;
    assert(parse_snapshot(s, sizeof s, kBoth, &snap) == ParseStatus::Ok);
    const DeviceSample& d = snap.ports[0].devices[0];
    assert(d.kind == DeviceKind::Mouse);
    const MouseMotion m = mouse_motion(d);
    assert(m.buttons == kMouseLeft && m.dx == -5 && m.dy == -3);
    assert(!m.x_overflow && !m.y_overflow);

    /* Y over, start + middle held, X = 255, Y = -256 (sign set, XD 0). */
    const uint8_t t[] = {0xF1, 0xE3, 0xA0 | 0x0C, 0xFF, 0x00, 0xF0};
    assert(parse_snapshot(t, sizeof t, kBoth, &snap) == ParseStatus::Ok);
    const MouseMotion o = mouse_motion(snap.ports[0].devices[0]);
    assert(o.buttons == (kMouseStart | kMouseMiddle) && o.dx == 255 && o.dy == 256);
    assert(o.y_overflow && !o.x_overflow);
}

static void standard_pointing_device_type_two_parses_like_the_mouse() {
    const uint8_t s[] = {0xF1, 0x23, 0x02, 0x04, 0x30 | 0x00, 0xF0};
    PeripheralSnapshot snap;
    assert(parse_snapshot(s, sizeof s, kBoth, &snap) == ParseStatus::Ok);
    const DeviceSample& d = snap.ports[0].devices[0];
    assert(d.kind == DeviceKind::Mouse);
    const MouseMotion m = mouse_motion(d);
    assert(m.buttons == kMouseRight && m.dx == 4 && m.dy == -0x30);
}

static void keyboard_reports_locks_and_key_events() {
    /* Down arrow held on the pad-equivalent byte, caps lock lit, key 0x2B made. */
    const uint8_t s[] = {0xF1, 0x34, 0xDF, 0xFF, 0x40 | 0x06 | 0x08, 0x2B, 0xF0};
    PeripheralSnapshot snap;
    assert(parse_snapshot(s, sizeof s, kBoth, &snap) == ParseStatus::Ok);
    const DeviceSample& d = snap.ports[0].devices[0];
    assert(d.kind == DeviceKind::Keyboard);
    assert(device_buttons(d) == SAT_PAD_DOWN);
    const KeyboardEvent k = keyboard_event(d);
    assert(k.locks == kKeyboardCaps && k.make && !k.brk && k.key == 0x2Bu);
    /* Break with num lock. */
    const uint8_t t[] = {0xF1, 0x34, 0xFF, 0xFF, 0x20 | 0x06 | 0x01, 0x11, 0xF0};
    assert(parse_snapshot(t, sizeof t, kBoth, &snap) == ParseStatus::Ok);
    const KeyboardEvent b = keyboard_event(snap.ports[0].devices[0]);
    assert(b.locks == kKeyboardNum && !b.make && b.brk && b.key == 0x11u);
}

static void six_player_adapter_reports_one_device_per_tap() {
    /* Status 0x16: multitap ID 1, six connectors: pad, empty, analog, pad,
     * unknown MD device, empty. Port 2 direct pad. */
    const uint8_t s[] = {
        0x16,
        0x02, 0xFB, 0xFF,                            /* tap 0: pad, A held */
        0xFF,                                        /* tap 1: nothing */
        0x16, 0xFF, 0xFF, 0x80, 0x80, 0x00, 0x00,    /* tap 2: 3D pad */
        0x02, 0xFF, 0xFF,                            /* tap 3: pad */
        0xF7,                                        /* tap 4: unknown MD id 7, no data */
        0xFF,                                        /* tap 5: nothing */
        0xF1, 0x02, 0xFF, 0xFF};
    PeripheralSnapshot snap;
    assert(parse_snapshot(s, sizeof s, kBoth, &snap) == ParseStatus::Ok);
    const PortSample& p = snap.ports[0];
    assert(p.multitap_id == 1u && p.tap_count == 6u);
    assert(p.devices[0].kind == DeviceKind::Pad && device_buttons(p.devices[0]) == SAT_PAD_A);
    assert(p.devices[1].kind == DeviceKind::None && p.devices[1].id == kIdNone);
    assert(p.devices[2].kind == DeviceKind::Analog && analog_axes(p.devices[2]).x == 0x80u);
    assert(p.devices[3].kind == DeviceKind::Pad);
    assert(p.devices[4].kind == DeviceKind::Unknown && p.devices[4].id == 0xF7u);
    assert(p.devices[5].kind == DeviceKind::None);
    assert(snap.ports[1].tap_count == 1u && snap.ports[1].devices[0].kind == DeviceKind::Pad);
}

static void extended_sizes_are_consumed_and_truncated() {
    /* 255-byte mode: ID low nibble 0, then the size byte. Here a 20-byte device. */
    uint8_t s[1 + 1 + 1 + 20 + 1] = {0x01 + 0x0F /*status: nothing*/};
    s[0] = 0xF1;
    s[1] = 0x00;    /* type 0, extended size follows */
    s[2] = 20u;
    for (uint8_t i = 0u; i < 20u; ++i) s[3 + i] = static_cast<uint8_t>(0xF0u + (i & 3u));
    s[23] = 0xF0;
    PeripheralSnapshot snap;
    assert(parse_snapshot(s, sizeof s, kBoth, &snap) == ParseStatus::Ok);
    const DeviceSample& d = snap.ports[0].devices[0];
    assert(d.size == 20u && d.stored == kMaxDeviceData && d.kind == DeviceKind::Pad);
    assert(snap.ports[1].parsed && snap.ports[1].tap_count == 0u);
}

static void a_port_in_zero_byte_mode_is_absent_from_the_stream() {
    const uint8_t s[] = {0xF1, 0x02, 0xFF, 0xFF};
    const bool only_two[kPortCount] = {false, true};
    PeripheralSnapshot snap;
    assert(parse_snapshot(s, sizeof s, only_two, &snap) == ParseStatus::Ok);
    assert(!snap.ports[0].parsed && snap.ports[0].tap_count == 0u);
    assert(snap.ports[1].parsed && snap.ports[1].devices[0].kind == DeviceKind::Pad);
}

static void short_or_impossible_streams_fail_without_overrun() {
    PeripheralSnapshot snap;
    const uint8_t cut_in_data[] = {0xF1, 0x16, 0xFF, 0xFF, 0x40};
    assert(parse_snapshot(cut_in_data, sizeof cut_in_data, kBoth, &snap) ==
           ParseStatus::Truncated);
    const uint8_t no_second_port[] = {0xF1, 0x02, 0xFF, 0xFF};
    assert(parse_snapshot(no_second_port, sizeof no_second_port, kBoth, &snap) ==
           ParseStatus::Truncated);
    assert(parse_snapshot(nullptr, 0u, kBoth, &snap) == ParseStatus::Truncated);
    const uint8_t seven_connectors[] = {0xF7, 0xF0};
    assert(parse_snapshot(seven_connectors, sizeof seven_connectors, kBoth, &snap) ==
           ParseStatus::Malformed);
    const uint8_t cut_extended[] = {0xF1, 0x00};
    assert(parse_snapshot(cut_extended, sizeof cut_extended, kBoth, &snap) ==
           ParseStatus::Truncated);
}

static void classification_covers_the_documented_types() {
    assert(classify_device(0x02u, 2u) == DeviceKind::Pad);
    assert(classify_device(0x01u, 1u) == DeviceKind::Unknown);   /* too short to be a pad */
    assert(classify_device(0x16u, 6u) == DeviceKind::Analog);
    assert(classify_device(0x23u, 3u) == DeviceKind::Mouse);
    assert(classify_device(0x34u, 4u) == DeviceKind::Keyboard);
    assert(classify_device(0xE3u, 3u) == DeviceKind::Mouse);
    assert(classify_device(0xE2u, 2u) == DeviceKind::Unknown);
    assert(classify_device(0xA0u, 0u) == DeviceKind::Unknown);
    assert(classify_device(0xFFu, 0u) == DeviceKind::None);
}

int main() {
    control_pad_and_empty_port();
    three_d_pad_in_analog_mode();
    three_d_pad_in_digital_mode_is_a_pad();
    three_axis_analog_stick_has_a_throttle();
    shuttle_mouse_is_nine_bit_two_complement_with_y_up_raw();
    standard_pointing_device_type_two_parses_like_the_mouse();
    keyboard_reports_locks_and_key_events();
    six_player_adapter_reports_one_device_per_tap();
    extended_sizes_are_consumed_and_truncated();
    a_port_in_zero_byte_mode_is_absent_from_the_stream();
    short_or_impossible_streams_fail_without_overrun();
    classification_covers_the_documented_types();
    std::printf("PASS: test_smpc_peripheral_logic.cpp\n");
    return 0;
}
