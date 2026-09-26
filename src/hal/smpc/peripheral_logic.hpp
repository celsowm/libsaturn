#ifndef SATURN_HAL_SMPC_PERIPHERAL_LOGIC_HPP
#define SATURN_HAL_SMPC_PERIPHERAL_LOGIC_HPP

#include <stdint.h>

#include "src/hal/smpc/input_logic.hpp"

/* Pure SMPC peripheral-report parsing (SMPC manual chapter 3): the byte
 * stream INTBACK returns, port by port, into typed device samples. No
 * register access, so host tests feed it recorded streams. */
namespace saturn::hal::smpc {

constexpr uint8_t kPortCount = 2u;
constexpr uint8_t kMaxTaps = 6u;
/* Data bytes kept per device; longer reports are consumed and truncated. */
constexpr uint8_t kMaxDeviceData = 15u;
/* Port status upper nibble when a device is plugged in directly or nothing is. */
constexpr uint8_t kNoMultitap = 0xFu;
constexpr uint8_t kIdNone = 0xFFu;

enum class DeviceKind : uint8_t {
    None,
    Pad,       /* digital pad, or a 3D pad in digital mode */
    Analog,    /* one or more axes after the two button bytes */
    Mouse,
    Keyboard,
    Unknown    /* a device SMPC cannot interpret; only its ID is known */
};

struct DeviceSample {
    DeviceKind kind;
    uint8_t id;                     /* type << 4 | size, or kIdNone */
    uint8_t size;                   /* data bytes the device sent */
    uint8_t stored;                 /* bytes kept in data[]; the rest of data[] is unset */
    uint8_t data[kMaxDeviceData];
};

/* Only devices[0 .. tap_count) are set by the parser, and devices[0] is a
 * cleared "nothing" device when the port is empty. */
struct PortSample {
    bool parsed;                    /* this port's report was present */
    uint8_t multitap_id;            /* kNoMultitap, else 0 = Sega tap, 1 = 6P adapter */
    uint8_t tap_count;              /* 0 nothing, 1 direct, 2..6 taps */
    DeviceSample devices[kMaxTaps];
};

struct PeripheralSnapshot {
    PortSample ports[kPortCount];
};

enum class ParseStatus : uint8_t {
    Ok,
    Truncated,  /* the stream ended inside a report */
    Malformed   /* a count or size no SMPC would produce */
};

inline DeviceKind classify_device(uint8_t id, uint8_t size) {
    if (id == kIdNone) return DeviceKind::None;
    const uint8_t type = static_cast<uint8_t>(id >> 4u);
    switch (type) {
        case 0x0u: return size >= 2u ? DeviceKind::Pad : DeviceKind::Unknown;
        /* A 3D Control Pad in digital mode reports type 1 with two bytes. */
        case 0x1u:
            if (size < 2u) return DeviceKind::Unknown;
            return size == 2u ? DeviceKind::Pad : DeviceKind::Analog;
        case 0x2u: return size >= 3u ? DeviceKind::Mouse : DeviceKind::Unknown;
        case 0x3u: return size >= 4u ? DeviceKind::Keyboard : DeviceKind::Unknown;
        /* The Shuttle Mouse identifies itself as an MD-derived 0xE3. */
        case 0xEu: return (id == 0xE3u && size >= 3u) ? DeviceKind::Mouse : DeviceKind::Unknown;
        default: return DeviceKind::Unknown;
    }
}

/* Parses one device at s[0..len). On success returns the bytes used; a tap
 * that is empty is one byte (0xFF). Returns 0 with `status` set on failure. */
inline uint16_t parse_device(const uint8_t* s, uint16_t len, DeviceSample* out,
                             ParseStatus* status) {
    out->kind = DeviceKind::None;
    out->id = kIdNone;
    out->size = 0u;
    out->stored = 0u;
    if (len < 1u) { *status = ParseStatus::Truncated; return 0u; }
    const uint8_t id = s[0];
    uint16_t used = 1u;
    if (id == kIdNone) { *status = ParseStatus::Ok; return used; }
    uint16_t size = static_cast<uint8_t>(id & 0x0Fu);
    if (size == 0u && (id >> 4u) != 0xFu) {
        /* Extended size: 16..255 bytes, or a 15-byte cap in 15-byte mode. */
        if (len < 2u) { *status = ParseStatus::Truncated; return 0u; }
        size = s[1];
        used = 2u;
    }
    /* Type F names a Mega Drive device SMPC does not know: the low nibble is
     * its ID, not a size, and no data follows. */
    if ((id >> 4u) == 0xFu) size = 0u;
    if (len < used + size) { *status = ParseStatus::Truncated; return 0u; }
    out->id = id;
    out->size = static_cast<uint8_t>(size);
    out->stored = size > kMaxDeviceData ? kMaxDeviceData : static_cast<uint8_t>(size);
    for (uint8_t i = 0u; i < out->stored; ++i) out->data[i] = s[used + i];
    out->kind = classify_device(id, static_cast<uint8_t>(size));
    *status = ParseStatus::Ok;
    return static_cast<uint16_t>(used + size);
}

/* Parses one port's report: status byte, then one device per connector. */
inline uint16_t parse_port(const uint8_t* s, uint16_t len, PortSample* out,
                           ParseStatus* status) {
    out->parsed = false;
    out->multitap_id = kNoMultitap;
    out->tap_count = 0u;
    if (len < 1u) { *status = ParseStatus::Truncated; return 0u; }
    const uint8_t port_status = s[0];
    out->multitap_id = static_cast<uint8_t>(port_status >> 4u);
    out->tap_count = static_cast<uint8_t>(port_status & 0x0Fu);
    if (out->tap_count > kMaxTaps) { *status = ParseStatus::Malformed; return 0u; }
    uint16_t used = 1u;
    if (out->tap_count == 0u) {
        out->devices[0].kind = DeviceKind::None;
        out->devices[0].id = kIdNone;
        out->devices[0].size = out->devices[0].stored = 0u;
    }
    for (uint8_t tap = 0u; tap < out->tap_count; ++tap) {
        ParseStatus st = ParseStatus::Ok;
        const uint16_t n = parse_device(s + used, static_cast<uint16_t>(len - used),
                                        &out->devices[tap], &st);
        if (st != ParseStatus::Ok) { *status = st; return 0u; }
        used = static_cast<uint16_t>(used + n);
    }
    out->parsed = true;
    *status = ParseStatus::Ok;
    return used;
}

/* Parses the whole stream. A port in 0-byte mode is absent from it. */
inline ParseStatus parse_snapshot(const uint8_t* s, uint16_t len, const bool enabled[kPortCount],
                                  PeripheralSnapshot* out) {
    uint16_t at = 0u;
    for (uint8_t port = 0u; port < kPortCount; ++port) {
        PortSample& ps = out->ports[port];
        ps.parsed = false;
        ps.multitap_id = kNoMultitap;
        ps.tap_count = 0u;
        ps.devices[0].kind = DeviceKind::None;
        ps.devices[0].id = kIdNone;
        ps.devices[0].size = ps.devices[0].stored = 0u;
        if (!enabled[port]) continue;
        ParseStatus st = ParseStatus::Ok;
        const uint16_t n = parse_port(s + at, static_cast<uint16_t>(len - at),
                                      &out->ports[port], &st);
        if (st != ParseStatus::Ok) return st;
        at = static_cast<uint16_t>(at + n);
    }
    return ParseStatus::Ok;
}

/* ---- decoded values --------------------------------------------------- */

/* Pad-equivalent buttons of the first two data bytes (pads, analog pads and
 * keyboards share them); 0 for anything else. */
inline uint16_t device_buttons(const DeviceSample& d) {
    switch (d.kind) {
        case DeviceKind::Pad:
        case DeviceKind::Analog:
        case DeviceKind::Keyboard:
            return d.stored >= 2u ? translate_standard_pad(d.data[0], d.data[1]) : 0u;
        default: return 0u;
    }
}

struct AnalogAxes {
    uint8_t count;    /* axes after the buttons */
    uint8_t x, y;     /* 0 = left/up, 255 = right/down, ~128 centred */
    uint8_t z;        /* third axis when there is exactly one more (throttle) */
    uint8_t l, r;     /* analog triggers of a 3D Control Pad: 0 released, 255 pressed */
    bool triggers;
};

/* 3D Control Pad (four axes): X, Y, then R and L triggers. Three axes:
 * X, Y, Z. */
inline AnalogAxes analog_axes(const DeviceSample& d) {
    AnalogAxes a{};
    if (d.kind != DeviceKind::Analog || d.stored < 3u) return a;
    a.count = static_cast<uint8_t>(d.stored - 2u);
    a.x = d.data[2];
    a.y = a.count >= 2u ? d.data[3] : 0u;
    if (a.count == 3u) a.z = d.data[4];
    if (a.count >= 4u) {
        a.r = d.data[4];
        a.l = d.data[5];
        a.triggers = true;
    }
    return a;
}

constexpr uint8_t kMouseLeft = 1u << 0u;
constexpr uint8_t kMouseRight = 1u << 1u;
constexpr uint8_t kMouseMiddle = 1u << 2u;
constexpr uint8_t kMouseStart = 1u << 3u;

struct MouseMotion {
    uint8_t buttons;    /* kMouse* bits */
    int16_t dx, dy;     /* counts since the last read; dy positive = down the screen */
    bool x_overflow;
    bool y_overflow;
};

/* The report is a 9-bit two's complement per axis (sign bit + 8 bits) and
 * carries Y positive = up, so dy is negated to screen orientation. */
inline MouseMotion mouse_motion(const DeviceSample& d) {
    MouseMotion m{};
    if (d.kind != DeviceKind::Mouse || d.stored < 3u) return m;
    const uint8_t flags = d.data[0];
    m.buttons = static_cast<uint8_t>(flags & 0x0Fu);
    const int16_t x = static_cast<int16_t>(d.data[1]) - ((flags & 0x10u) != 0u ? 256 : 0);
    const int16_t y = static_cast<int16_t>(d.data[2]) - ((flags & 0x20u) != 0u ? 256 : 0);
    m.dx = x;
    m.dy = static_cast<int16_t>(-y);
    m.x_overflow = (flags & 0x40u) != 0u;
    m.y_overflow = (flags & 0x80u) != 0u;
    return m;
}

constexpr uint8_t kKeyboardCaps = 1u << 0u;
constexpr uint8_t kKeyboardNum = 1u << 1u;
constexpr uint8_t kKeyboardScroll = 1u << 2u;

struct KeyboardEvent {
    uint8_t locks;      /* kKeyboard* bits */
    bool make;          /* `key` was pressed */
    bool brk;           /* `key` was released */
    uint8_t key;        /* key number (SMPC keyboard table) */
};

inline KeyboardEvent keyboard_event(const DeviceSample& d) {
    KeyboardEvent k{};
    if (d.kind != DeviceKind::Keyboard || d.stored < 4u) return k;
    const uint8_t s = d.data[2];
    k.locks = static_cast<uint8_t>(((s >> 6u) & 1u) | (((s >> 5u) & 1u) << 1u) |
                                   (((s >> 4u) & 1u) << 2u));
    k.make = (s & 0x08u) != 0u;
    k.brk = (s & 0x01u) != 0u;
    k.key = d.data[3];
    return k;
}

}  // namespace saturn::hal::smpc

#endif /* SATURN_HAL_SMPC_PERIPHERAL_LOGIC_HPP */
