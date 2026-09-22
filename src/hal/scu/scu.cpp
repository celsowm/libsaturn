#include "src/hal/scu/scu.hpp"

#include "src/hal/sh2/frt.hpp"
#include "src/hal/vdp2/vdp2.hpp"

namespace saturn::hal::scu {

namespace {

uint32_t g_frame_counter = 0;
uint32_t g_display_frames = 0;
uint16_t g_ticks_per_frame = 0;
uint16_t g_clock_last_frc = 0;
uint64_t g_clock_ticks = 0;
uint64_t g_last_display_tick_total = 0;
constexpr uint16_t kVblankFlag = 0x0008u;
constexpr uint32_t kMaxPollSpins = 2000000u;
/* Fewer ticks than this per frame is not a working timer. */
constexpr uint16_t kMinTicksPerFrame = 64u;

/* Reading the high byte latches the low byte, so the pair is consistent. */
uint16_t read_frc() {
    return saturn::hal::sh2::frt::counter();
}

uint64_t observe_elapsed_ticks() {
    const uint16_t now = read_frc();
    const uint16_t delta = static_cast<uint16_t>(now - g_clock_last_frc);
    g_clock_last_frc = now;
    g_clock_ticks += delta;
    return g_clock_ticks;
}

}  // namespace

void init_interrupts() {
    // MVP path uses VDP2 TVSTAT polling for vblank sync; keep SCU setup minimal.
}

/* Spins until TVSTAT.VBLANK reads `level`, bounded so a CRTC that is not
 * advancing (display off, or a host that never toggles the flag) degrades into
 * a slow frame instead of hanging the whole program. */
static void wait_vblank_level(uint16_t level) {
    uint32_t spin = 0;
    while (static_cast<uint16_t>(vdp2::read_tvstat() & kVblankFlag) != level) {
        if (++spin >= kMaxPollSpins) {
            return;
        }
    }
}

/* Polling a VBLANK edge only says a frame boundary was reached, not how many
 * went by while the program was busy. The free-running timer keeps counting
 * regardless, so it is what turns "a program frame" into "display frames".
 *
 * phi/128 puts an NTSC frame at a few thousand ticks: fine resolution, with
 * the 16-bit counter still spanning well over ten frames between reads. The
 * tick rate is measured between two real VBLANK edges instead of derived
 * from a clock constant, so the 320/352 dot clocks and PAL need no table. */
void init_frame_clock() {
    /* Only the two clock-select bits belong to the frame clock.  Input
     * capture and compare/overflow enables are intentionally preserved. */
    saturn::hal::sh2::frt::set_prescaler(
        saturn::hal::sh2::frt::Prescaler::Divide128);
    wait_vblank_level(0u);
    wait_vblank_level(kVblankFlag);
    const uint16_t start = read_frc();
    wait_vblank_level(0u);
    wait_vblank_level(kVblankFlag);
    const uint16_t end = read_frc();
    const uint16_t ticks = static_cast<uint16_t>(end - start);
    g_ticks_per_frame = (ticks >= kMinTicksPerFrame) ? ticks : 0u;
    g_clock_last_frc = end;
    g_clock_ticks = 0u;
    g_last_display_tick_total = 0u;
    g_frame_counter = 0u;
    g_display_frames = 0u;
}

/* Waits for the START of the next vertical blank.
 *
 * Deliberately a rising edge, not "any edge": VBLANK toggles twice per frame,
 * so waiting for a change syncs to whichever edge comes next and the loop runs
 * at 60 or 120 Hz depending on how long the caller's own frame work took.
 * That makes every animation speed a function of scene complexity. Leaving any
 * in-progress vblank first, then waiting for the flag to rise, pins the loop to
 * exactly one real frame. */
void wait_vblank() {
    wait_vblank_level(0u);
    wait_vblank_level(kVblankFlag);
    ++g_frame_counter;
    if (g_ticks_per_frame == 0u) {
        ++g_display_frames;
        return;
    }

    const uint64_t now = observe_elapsed_ticks();
    const uint64_t delta = now - g_last_display_tick_total;
    g_last_display_tick_total = now;
    uint32_t frames = static_cast<uint32_t>(
        (delta + (static_cast<uint64_t>(g_ticks_per_frame) / 2u)) / g_ticks_per_frame);
    if (frames == 0u) {
        frames = 1u;
    }
    g_display_frames += frames;
}

uint32_t frame_counter() {
    return g_frame_counter;
}

uint32_t display_frames() {
    return g_display_frames;
}

uint16_t ticks_per_frame() {
    return g_ticks_per_frame;
}

uint64_t elapsed_ticks() {
    if (g_ticks_per_frame == 0u) {
        return 0u;
    }
    return observe_elapsed_ticks();
}

}  // namespace saturn::hal::scu
