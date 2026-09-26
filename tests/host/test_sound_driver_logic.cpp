#include <cassert>
#include <cstdio>

#include "src/hal/scsp/driver_logic.hpp"

using namespace saturn::hal::scsp::driver_logic;

int main() {
    // ticks: one per 256 samples = 5.805 ms
    assert(ticks_per_second_x100() == 17226u || ticks_per_second_x100() == 17227u);
    assert(ticks_from_ms(0u) == 0u && ticks_from_ms(1000u) == 172u);
    assert(ticks_from_ms(6u) == 1u && ticks_from_ms(3u) == 1u && ticks_from_ms(2u) == 0u);
    assert(ms_from_ticks(172u) >= 997u && ms_from_ticks(172u) <= 1000u);
    assert(ms_from_ticks(ticks_from_ms(5000u)) >= 4990u && ms_from_ticks(ticks_from_ms(5000u)) <= 5010u);

    // the ring holds one fewer than its size: head == tail is empty
    assert(ring_count(0u, 0u) == 0u && !ring_full(0u, 0u));
    assert(ring_count(5u, 2u) == 3u && ring_count(2u, 5u) == 61u);   // wrapped
    assert(ring_full(63u, 0u) && ring_full(10u, 11u) && !ring_full(62u, 0u));
    assert(ring_entry_offset(0u) == kRingOffset && ring_entry_offset(64u) == kRingOffset);
    assert(ring_entry_offset(3u) == kRingOffset + 24u);
    assert(kRingOffset + kRingEntries * kEntryBytes <= kLogOffset);        // the ring does not run into the log
    assert(kLogOffset + kLogEntries * 4u <= kMailbox + kMailboxBytes);     // the log fits the cleared area
    assert(log_entry_offset(65u) == kLogOffset + 4u);

    // register writes: even offsets inside the 4 KiB block; bit 15 is the marker flag
    assert(register_ok(0u) && register_ok(0x41Eu) && register_ok(0xFFEu));
    assert(!register_ok(1u) && !register_ok(0x1000u) && !register_ok(0x8000u));
    assert((kMarkerBit & 0x0FFEu) == 0u);
    assert(combine_tick(0x0001u, 0x0002u) == 0x00010002u);

    std::puts("PASS: test_sound_driver_logic.cpp");
    return 0;
}
