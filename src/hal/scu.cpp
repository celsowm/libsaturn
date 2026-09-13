#include "src/hal/scu.hpp"

#include "src/hal/vdp2.hpp"

namespace saturn::hal::scu {

namespace {

uint32_t g_frame_counter = 0;
constexpr uint16_t kVblankFlag = 0x0008u;
constexpr uint32_t kMaxPollSpins = 2000000u;

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
}

uint32_t frame_counter() {
    return g_frame_counter;
}

}  // namespace saturn::hal::scu
