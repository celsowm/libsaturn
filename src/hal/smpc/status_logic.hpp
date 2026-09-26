#ifndef SATURN_HAL_SMPC_STATUS_LOGIC_HPP
#define SATURN_HAL_SMPC_STATUS_LOGIC_HPP

#include <stdint.h>

/* Pure SMPC status and RTC encodings (SMPC manual 2.3-2.5): the INTBACK
 * status block, SETTIME and SETSMEM parameters, INTBACK chunk reassembly.
 * No register access. */
namespace saturn::hal::smpc {

constexpr uint8_t kCmdIntback = 0x10u;
constexpr uint8_t kCmdSetTime = 0x16u;
constexpr uint8_t kCmdSetSmem = 0x17u;
/* IREG1 of a peripheral-only INTBACK: PEN, OPE=1 (no optimization), and per
 * port either 15-byte mode (00) or 0-byte mode (11: the port is not read and
 * its data is absent from the report). Bit 0 of `port_mask` is port 1. */
constexpr uint8_t kIntbackPortsBoth = 0x3u;
inline uint8_t intback_ireg1(uint8_t port_mask) {
    uint8_t ireg1 = 0x0Au;
    if ((port_mask & 0x1u) == 0u) ireg1 = static_cast<uint8_t>(ireg1 | 0x30u);
    if ((port_mask & 0x2u) == 0u) ireg1 = static_cast<uint8_t>(ireg1 | 0xC0u);
    return ireg1;
}
constexpr uint8_t kIntbackContinue = 0x80u;
constexpr uint8_t kIntbackBreak = 0x40u;
/* SR bit 5: peripheral data remains (NPE / PDE). */
constexpr uint8_t kSrMoreData = 0x20u;
/* The SMPC interrupt status bit in the SCU (source 7). */
constexpr uint32_t kIstSmpc = 1u << 7u;
constexpr uint16_t kChunkBytes = 32u;
/* Two ports of a six-tap adapter with 15-byte devices need under 200 bytes. */
constexpr uint16_t kMaxStreamBytes = 256u;

struct RtcTime {
    uint16_t year;     /* 1980..2099 */
    uint8_t month;     /* 1..12 */
    uint8_t day;       /* 1..31 */
    uint8_t weekday;   /* 0 = Sunday */
    uint8_t hour;      /* 0..23 */
    uint8_t minute;
    uint8_t second;
};

struct SmpcStatus {
    bool time_set;           /* SETTIME has run since the SMPC cold reset (STE) */
    bool reset_disabled;     /* RESD: the reset button is ignored */
    uint8_t cartridge_code;  /* CTG1-0 */
    uint8_t area_code;
    uint8_t system_status1;  /* OREG10: DOTSEL, MSHNMI, SYSRES, SNDRES bits */
    uint8_t system_status2;  /* OREG11 */
    uint8_t smem[4];         /* the four battery-backed bytes */
    RtcTime time;
};

inline uint8_t to_bcd(uint8_t v) {
    return static_cast<uint8_t>(((v / 10u) << 4u) | (v % 10u));
}

inline bool valid_bcd(uint8_t v) {
    return (v >> 4u) <= 9u && (v & 0x0Fu) <= 9u;
}

inline uint8_t from_bcd(uint8_t v) {
    return static_cast<uint8_t>((v >> 4u) * 10u + (v & 0x0Fu));
}

inline bool is_leap_year(uint16_t year) {
    return (year % 4u == 0u && year % 100u != 0u) || year % 400u == 0u;
}

inline uint8_t days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1u || month > 12u) return 0u;
    return static_cast<uint8_t>(kDays[month - 1u] + (month == 2u && is_leap_year(year) ? 1u : 0u));
}

/* 0 = Sunday. */
inline uint8_t weekday_of(uint16_t year, uint8_t month, uint8_t day) {
    static const uint8_t kOffset[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    uint16_t y = year;
    if (month < 3u) --y;
    const uint32_t w = y + y / 4u - y / 100u + y / 400u + kOffset[(month - 1u) % 12u] + day;
    return static_cast<uint8_t>(w % 7u);
}

/* The SMPC keeps leap years correct up to 2099 and takes any other value as
 * undefined, so refuse them here instead. */
inline bool valid_time(const RtcTime& t) {
    return t.year >= 1980u && t.year <= 2099u && t.month >= 1u && t.month <= 12u &&
           t.day >= 1u && t.day <= days_in_month(t.year, t.month) && t.hour < 24u &&
           t.minute < 60u && t.second < 60u;
}

/* SETTIME parameters IREG0..6; the weekday is derived from the date. */
inline void encode_settime(const RtcTime& t, uint8_t ireg[7]) {
    ireg[0] = to_bcd(static_cast<uint8_t>(t.year / 100u));
    ireg[1] = to_bcd(static_cast<uint8_t>(t.year % 100u));
    ireg[2] = static_cast<uint8_t>((weekday_of(t.year, t.month, t.day) << 4u) | t.month);
    ireg[3] = to_bcd(t.day);
    ireg[4] = to_bcd(t.hour);
    ireg[5] = to_bcd(t.minute);
    ireg[6] = to_bcd(t.second);
}

/* The status block of INTBACK: OREG0..15. False if the RTC bytes are not
 * BCD (a wedged or unpowered SMPC returns garbage). */
inline bool parse_status(const uint8_t oreg[16], SmpcStatus* out) {
    *out = {};
    out->time_set = (oreg[0] & 0x80u) != 0u;
    out->reset_disabled = (oreg[0] & 0x40u) != 0u;
    for (uint8_t i = 1u; i <= 7u; ++i) {
        if (i == 3u) continue;                    /* weekday and month are hex */
        if (!valid_bcd(oreg[i])) return false;
    }
    out->time.year = static_cast<uint16_t>(from_bcd(oreg[1]) * 100u + from_bcd(oreg[2]));
    out->time.weekday = static_cast<uint8_t>(oreg[3] >> 4u);
    out->time.month = static_cast<uint8_t>(oreg[3] & 0x0Fu);
    out->time.day = from_bcd(oreg[4]);
    out->time.hour = from_bcd(oreg[5]);
    out->time.minute = from_bcd(oreg[6]);
    out->time.second = from_bcd(oreg[7]);
    out->cartridge_code = static_cast<uint8_t>(oreg[8] & 0x03u);
    out->area_code = static_cast<uint8_t>(oreg[9] & 0x0Fu);
    out->system_status1 = oreg[10];
    out->system_status2 = oreg[11];
    for (uint8_t i = 0u; i < 4u; ++i) out->smem[i] = oreg[12 + i];
    return true;
}

/* INTBACK peripheral chunks are 32 OREG bytes each; a report continues in
 * the next chunk while SR says data remains. */
struct ChunkStream {
    uint8_t bytes[kMaxStreamBytes];
    uint16_t length;
    uint8_t chunks;
    bool overflow;
};

/* Appends one chunk. False when the stream would exceed its capacity (the
 * caller should send a break request). */
inline bool add_chunk(ChunkStream* stream, const uint8_t oreg[kChunkBytes]) {
    if (stream->length + kChunkBytes > kMaxStreamBytes) {
        stream->overflow = true;
        return false;
    }
    for (uint16_t i = 0u; i < kChunkBytes; ++i) stream->bytes[stream->length + i] = oreg[i];
    stream->length = static_cast<uint16_t>(stream->length + kChunkBytes);
    ++stream->chunks;
    return true;
}

inline bool more_chunks(uint8_t sr) {
    return (sr & kSrMoreData) != 0u;
}

}  // namespace saturn::hal::smpc

#endif /* SATURN_HAL_SMPC_STATUS_LOGIC_HPP */
