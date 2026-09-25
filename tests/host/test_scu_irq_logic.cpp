#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/hal/scu/irq_logic.hpp"

using namespace saturn::hal::scu::irq_logic;

static void levels_and_vectors_follow_the_scu_table() {
    assert(level(0) == 15u && vector(0) == 0x40u);   /* VBlank-IN */
    assert(level(3) == 12u && vector(3) == 0x43u);   /* timer 0 */
    assert(level(7) == 8u && level(8) == 8u);        /* SMPC, PAD */
    assert(level(9) == 6u && level(10) == 6u && level(11) == 5u);
    assert(level(13) == 2u && vector(13) == 0x4Du);  /* sprite draw end */
    assert(level(14) == 0u);
}

static void ims_masks_everything_not_enabled() {
    assert(compose_ims(0u) == 0xBFFFu);
    assert(compose_ims(1u << kVblankIn) == 0xBFFEu);
    assert(compose_ims((1u << kVblankIn) | (1u << 13)) == 0x9FFEu);
    /* Bits past the internal sources never unmask the A-bus. */
    assert((compose_ims(0xFFFFu) & 0x8000u) == 0x8000u);
    assert(compose_ims(0xFFFFu) == 0x8000u);
}

static void sr_mask_admits_the_lowest_enabled_level_only() {
    assert(sr_mask(0u) == 15u);
    assert(sr_mask(1u << kVblankIn) == 14u);
    assert(sr_mask((1u << kVblankIn) | (1u << kTimer0)) == 11u);
    assert(sr_mask((1u << kVblankIn) | (1u << 13)) == 1u);
    assert(sr_mask(1u << 11) == 4u);
}

static void timer_registers_compose() {
    assert(!timers_needed(1u << kVblankIn));
    assert(timers_needed(1u << kTimer0) && timers_needed(1u << kTimer1));
    assert(compose_t1md(false, false) == 0u);
    assert(compose_t1md(true, false) == 0x0001u);
    assert(compose_t1md(true, true) == 0x0101u);
}

static void stalled_after_two_frames_of_timer_time() {
    assert(!vblank_stalled(7000u, 3500u));
    assert(vblank_stalled(7001u, 3500u));
    assert(!vblank_stalled(1000000u, 0u)); /* uncalibrated: caller bounds it */
}

static void frames_between_counts_wrap_safely() {
    assert(frames_between(10u, 11u) == 1u);
    assert(frames_between(10u, 13u) == 3u);
    assert(frames_between(10u, 10u) == 1u);
    assert(frames_between(0xFFFFFFFFu, 1u) == 2u);
}

/* The sat_time_ms wrap: a 16-bit FRT only extends correctly when observed
 * at least once per wrap. Observed every frame (what the VBlank-IN handler
 * does) a second of busy work is counted in full; observed once after it,
 * all but the last partial wrap is lost. */
static void clock_extends_only_when_observed_each_wrap() {
    constexpr uint32_t kTicksPerFrame = 3500u; /* ~phi/128 at 60 Hz */
    uint64_t every_frame = 0u;
    uint16_t last_a = 0u;
    uint32_t frc = 0u;
    for (int f = 0; f < 60; ++f) {
        frc += kTicksPerFrame;
        every_frame = extend_ticks(every_frame, &last_a, static_cast<uint16_t>(frc));
    }
    assert(every_frame == 60u * kTicksPerFrame);

    uint64_t once = 0u;
    uint16_t last_b = 0u;
    once = extend_ticks(once, &last_b, static_cast<uint16_t>(frc));
    assert(once == (60u * kTicksPerFrame) % 65536u);
    assert(once < every_frame);
}

int main() {
    levels_and_vectors_follow_the_scu_table();
    ims_masks_everything_not_enabled();
    sr_mask_admits_the_lowest_enabled_level_only();
    timer_registers_compose();
    stalled_after_two_frames_of_timer_time();
    frames_between_counts_wrap_safely();
    clock_extends_only_when_observed_each_wrap();
    std::puts("test_scu_irq_logic: OK");
    return 0;
}
