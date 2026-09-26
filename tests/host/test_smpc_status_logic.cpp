#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/hal/smpc/status_logic.hpp"

using namespace saturn::hal::smpc;

static void bcd_round_trips_and_is_checked() {
    for (uint8_t v = 0u; v < 100u; ++v) {
        assert(valid_bcd(to_bcd(v)) && from_bcd(to_bcd(v)) == v);
    }
    assert(to_bcd(59u) == 0x59u && from_bcd(0x23u) == 23u);
    assert(!valid_bcd(0x1Au) && !valid_bcd(0xA0u) && valid_bcd(0x99u));
}

static void calendar_helpers() {
    assert(is_leap_year(2000u) && is_leap_year(2024u) && !is_leap_year(1900u) &&
           !is_leap_year(2023u));
    assert(days_in_month(2024u, 2u) == 29u && days_in_month(2023u, 2u) == 28u);
    assert(days_in_month(2026u, 12u) == 31u && days_in_month(2026u, 13u) == 0u);
    /* The manual's cold-reset time: Friday 1993-12-31. */
    assert(weekday_of(1993u, 12u, 31u) == 5u);
    assert(weekday_of(2000u, 1u, 1u) == 6u);   /* Saturday */
    assert(weekday_of(2024u, 2u, 29u) == 4u);  /* Thursday */
    assert(weekday_of(1996u, 3u, 1u) == 5u);   /* Friday */
    assert(weekday_of(2026u, 9u, 25u) == 5u);  /* Friday */
}

static void time_validation_matches_the_manual() {
    RtcTime t{1993u, 12u, 31u, 5u, 23u, 59u, 59u};
    assert(valid_time(t));
    t = {2099u, 12u, 31u, 4u, 0u, 0u, 0u};
    assert(valid_time(t));
    t.year = 2100u;
    assert(!valid_time(t));
    t = {1979u, 1u, 1u, 0u, 0u, 0u, 0u};
    assert(!valid_time(t));
    t = {2023u, 2u, 29u, 0u, 0u, 0u, 0u};
    assert(!valid_time(t));       /* not a leap year */
    t = {2024u, 2u, 29u, 0u, 0u, 0u, 0u};
    assert(valid_time(t));
    t = {2024u, 13u, 1u, 0u, 0u, 0u, 0u};
    assert(!valid_time(t));
    t = {2024u, 4u, 31u, 0u, 0u, 0u, 0u};
    assert(!valid_time(t));
    t = {2024u, 4u, 30u, 0u, 24u, 0u, 0u};
    assert(!valid_time(t));
    t = {2024u, 4u, 30u, 0u, 23u, 60u, 0u};
    assert(!valid_time(t));
    t = {2024u, 4u, 30u, 0u, 23u, 59u, 60u};
    assert(!valid_time(t));
}

static void settime_parameters_encode_the_manual_layout() {
    const RtcTime t{2026u, 9u, 25u, 0u, 21u, 5u, 9u};   /* weekday input is ignored */
    uint8_t ireg[7];
    encode_settime(t, ireg);
    assert(ireg[0] == 0x20u && ireg[1] == 0x26u);
    assert(ireg[2] == ((5u << 4u) | 9u));                /* Friday, September */
    assert(ireg[3] == 0x25u && ireg[4] == 0x21u && ireg[5] == 0x05u && ireg[6] == 0x09u);
    const RtcTime dec{1999u, 12u, 31u, 0u, 23u, 59u, 59u};
    encode_settime(dec, ireg);
    assert(ireg[2] == ((5u << 4u) | 12u) && ireg[0] == 0x19u && ireg[1] == 0x99u);
}

static void status_block_decodes() {
    uint8_t oreg[16] = {
        0x80 | 0x40,      /* STE, RESD */
        0x20, 0x26,       /* 2026 */
        (5u << 4u) | 9u,  /* Friday, September */
        0x25, 0x21, 0x05, 0x09,
        0x02,             /* cartridge code */
        0x04,             /* area code */
        0x34, 0x00,
        0x11, 0x22, 0x33, 0x44};
    SmpcStatus st;
    assert(parse_status(oreg, &st));
    assert(st.time_set && st.reset_disabled);
    assert(st.time.year == 2026u && st.time.month == 9u && st.time.day == 25u);
    assert(st.time.weekday == 5u && st.time.hour == 21u && st.time.minute == 5u &&
           st.time.second == 9u);
    assert(st.cartridge_code == 2u && st.area_code == 4u);
    assert(st.system_status1 == 0x34u && st.smem[0] == 0x11u && st.smem[3] == 0x44u);
    assert(valid_time(st.time));
    oreg[5] = 0x2A;                       /* not BCD */
    assert(!parse_status(oreg, &st));
}

static void chunks_reassemble_and_are_bounded() {
    ChunkStream s{};
    uint8_t chunk[kChunkBytes];
    for (uint16_t i = 0u; i < kChunkBytes; ++i) chunk[i] = static_cast<uint8_t>(i);
    assert(add_chunk(&s, chunk));
    for (uint16_t i = 0u; i < kChunkBytes; ++i) chunk[i] = static_cast<uint8_t>(0x80u + i);
    assert(add_chunk(&s, chunk));
    assert(s.chunks == 2u && s.length == 64u);
    assert(s.bytes[0] == 0u && s.bytes[31] == 31u && s.bytes[32] == 0x80u && s.bytes[63] == 0x9Fu);
    while (add_chunk(&s, chunk)) {}
    assert(s.overflow && s.length == kMaxStreamBytes && s.chunks == kMaxStreamBytes / kChunkBytes);
    assert(!more_chunks(0x80u) && more_chunks(0xA0u) && !more_chunks(0x90u));
}

static void command_constants_follow_the_manual() {
    assert(kCmdIntback == 0x10u && kCmdSetTime == 0x16u && kCmdSetSmem == 0x17u);
    /* PEN + OPE=1, ports in 15-byte mode (00) or 0-byte mode (11). */
    assert(intback_ireg1(kIntbackPortsBoth) == 0x0Au);
    assert(intback_ireg1(0x1u) == 0xCAu);    /* the value the per-port read used for port 1 */
    assert(intback_ireg1(0x2u) == 0x3Au);    /* ... and for port 2 */
    assert(intback_ireg1(0x0u) == 0xFAu);
    assert(kIstSmpc == 0x80u);
}

int main() {
    bcd_round_trips_and_is_checked();
    calendar_helpers();
    time_validation_matches_the_manual();
    settime_parameters_encode_the_manual_layout();
    status_block_decodes();
    chunks_reassemble_and_are_bounded();
    command_constants_follow_the_manual();
    std::printf("PASS: test_smpc_status_logic.cpp\n");
    return 0;
}
