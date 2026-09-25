#include "src/hal/scu/scu.hpp"

#include "src/hal/scu/irq.hpp"
#include "src/hal/scu/irq_logic.hpp"
#include "src/hal/sh2/cpu.hpp"
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
uint32_t g_last_vblank_count = 0;
uint32_t g_irq_fallbacks = 0;
constexpr uint16_t kVblankFlag = 0x0008u;
constexpr uint32_t kMaxPollSpins = 2000000u;
/* Fewer ticks than this per frame is not a working timer. */
constexpr uint16_t kMinTicksPerFrame = 64u;

/* Reading the high byte latches the low byte, so the pair is consistent. */
uint16_t read_frc() {
    return saturn::hal::sh2::frt::counter();
}

/* Callers mask interrupts: the VBlank-IN handler observes the clock too. */
uint64_t observe_elapsed_ticks() {
    g_clock_ticks = irq_logic::extend_ticks(g_clock_ticks, &g_clock_last_frc, read_frc());
    return g_clock_ticks;
}

}  // namespace

/* Interrupt-driven frames once the SCU delivers VBlank-IN. The frame clock
 * must be calibrated first: start() bounds its wait with it. */
void init_interrupts() {
    if (irq::start()) g_last_vblank_count = irq::count(irq_logic::kVblankIn);
}

void shutdown_interrupts() {
    irq::stop();
}

/* Called from the VBlank-IN handler: observing the FRT once a frame keeps
 * the 16-bit counter from wrapping unseen (~0.3 s at phi/128), however long
 * the program goes without calling sat_time_ms or sat_wait_vblank. */
void on_vblank_in() {
    if (g_ticks_per_frame != 0u) (void)observe_elapsed_ticks();
}

uint32_t irq_fallbacks() {
    return g_irq_fallbacks;
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
    if (irq::live()) {
        /* The next VBlank-IN is the same edge the poll below waits for; the
         * handler's count also says exactly how many frames went by. */
        const uint32_t seen = irq::count(irq_logic::kVblankIn);
        const uint64_t begin = elapsed_ticks();
        uint32_t spins = 0u;
        bool stalled = false;
        while (irq::count(irq_logic::kVblankIn) == seen) {
            stalled = g_ticks_per_frame != 0u
                ? irq_logic::vblank_stalled(elapsed_ticks() - begin, g_ticks_per_frame)
                : ++spins >= kMaxPollSpins;
            if (stalled) break;
        }
        if (!stalled) {
            const uint32_t now = irq::count(irq_logic::kVblankIn);
            g_display_frames += irq_logic::frames_between(g_last_vblank_count, now);
            g_last_vblank_count = now;
            ++g_frame_counter;
            if (g_ticks_per_frame != 0u) g_last_display_tick_total = elapsed_ticks();
            return;
        }
        /* The host stopped delivering VBlank-IN: poll from now on. */
        ++g_irq_fallbacks;
        irq::stop();
        if (g_ticks_per_frame != 0u) g_last_display_tick_total = elapsed_ticks();
    }
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
    const uint32_t status = saturn::hal::sh2::save_and_mask_interrupts();
    const uint64_t ticks = observe_elapsed_ticks();
    saturn::hal::sh2::restore_interrupts(status);
    return ticks;
}

}  // namespace saturn::hal::scu
