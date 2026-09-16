#include "src/hal/vdp2_color_calc.hpp"

#include "src/core/vdp2_color_calc_logic.hpp"
#include "src/hal/vdp2.hpp"

namespace saturn::hal::vdp2_color_calc {

namespace {

constexpr uintptr_t kUncached = 0x20000000u;
constexpr uintptr_t kVdp2Base = 0x05F80000u;
constexpr uint16_t kTvstatVblank = 0x0008u;

#define VDP2_REG16(offset) \
    (*reinterpret_cast<volatile uint16_t*>(kUncached | (kVdp2Base + (offset))))

#define SPCTL VDP2_REG16(0x0E0u)
#define CCCTL VDP2_REG16(0x0ECu)
#define PRISA VDP2_REG16(0x0F0u)
#define CCRSA VDP2_REG16(0x100u)
#define CCRSB VDP2_REG16(0x102u)
#define CCRSC VDP2_REG16(0x104u)
#define CCRSD VDP2_REG16(0x106u)

uint8_t g_enabled = 0u;
uint8_t g_normal_priority = 6u;
uint8_t g_ratio[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

/* VDP2 configuration registers are latched during VBlank.  A caller can
 * easily spend most of that short window in SMPC polling before reaching a
 * color-calc commit, which makes otherwise-correct writes disappear on real
 * hardware and timing-accurate emulators.  Keep the public commit explicit,
 * but make the HAL defensive: if the call arrives outside VBlank, wait for
 * the next VBlank start before touching SPCTL/PRISA/CCCTL/CCRSA-D. */
void ensure_vblank(void) {
    if ((saturn::hal::vdp2::read_tvstat() & kTvstatVblank) == 0u) {
        saturn::hal::vdp2::wait_vblank_start();
    }
}

}  // namespace

void commit() {
    using namespace saturn::core::vdp2_color_calc;

    ensure_vblank();

    CCRSA = compose_ratio_pair(g_ratio[0], g_ratio[1]);
    CCRSB = compose_ratio_pair(g_ratio[2], g_ratio[3]);
    CCRSC = compose_ratio_pair(g_ratio[4], g_ratio[5]);
    CCRSD = compose_ratio_pair(g_ratio[6], g_ratio[7]);

    if (g_enabled != 0u) {
        const uint8_t fade_priority = static_cast<uint8_t>(g_normal_priority - 1u);
        /* Type 0 + equality against selector-1's priority. Ordinary sprites
         * keep selector 0 and therefore do not satisfy the condition. */
        SPCTL = compose_spctl(fade_priority);
        PRISA = compose_prisa(g_normal_priority);
        CCCTL = kCcctlSpriteEnable;
    } else {
        SPCTL = 0u; /* Type 0, default condition; SPCCEN below is disabled. */
        PRISA = compose_prisa_disabled(g_normal_priority);
        CCCTL = 0u;
    }
}

void configure(const sat_vdp2_sprite_color_calc_config_t& config) {
    g_enabled = config.enabled;
    g_normal_priority = config.normal_priority;
    for (uint8_t i = 0u; i < 8u; ++i) {
        g_ratio[i] = config.ratio[i];
    }
    commit();
}

void set_ratio(uint8_t slot, uint8_t ratio) {
    if (slot < 8u) {
        g_ratio[slot] = ratio;
    }
    commit();
}

void disable() {
    g_enabled = 0u;
    commit();
}

}  // namespace saturn::hal::vdp2_color_calc
