#include "src/hal/vdp2/color_calc.hpp"

#include "src/graphics/vdp2/color_calc_logic.hpp"
#include "src/hal/vdp2/vdp2.hpp"

namespace saturn::hal::vdp2_color_calc {

namespace {

constexpr uintptr_t kUncached = 0x20000000u;
constexpr uintptr_t kVdp2Base = 0x05F80000u;
constexpr uint16_t kTvstatVblank = 0x0008u;

#define VDP2_REG16(offset) \
    (*reinterpret_cast<volatile uint16_t*>(kUncached | (kVdp2Base + (offset))))

#define SPCTL VDP2_REG16(0x0E0u)
#define CCRSA VDP2_REG16(0x100u)
#define CCRSB VDP2_REG16(0x102u)
#define CCRSC VDP2_REG16(0x104u)
#define CCRSD VDP2_REG16(0x106u)

uint8_t g_enabled = 0u;
uint8_t g_normal_priority = 6u;
uint8_t g_ratio[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
uint8_t g_strict_alpha = 0u;
uint8_t g_mode = SAT_VDP2_COLOR_CALC_RATIO;
saturn::core::vdp2_color_calc::ModeClaim g_claim = {0u, 0u};

uint16_t current_ccctl() {
    return saturn::core::vdp2_color_calc::compose_ccctl(g_enabled != 0u, g_mode);
}

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
        SPCTL = compose_spctl(fade_priority, saturn::hal::vdp2::sprite_type_bits());
        /* Share PRISA with the generic VDP2 layer replay. A direct write
         * here fixes only one VBlank; the next layers_commit() would restore
         * its old all-opaque 0x0707 shadow and cause a per-frame fade blink. */
        saturn::hal::vdp2::set_sprite_priority_pair(
            g_normal_priority, fade_priority);
        saturn::hal::vdp2::set_color_calc_control(current_ccctl());
    } else {
        /* Default condition, SPCCEN below disabled; type 0 with RGB mixed
         * in, or type C in hi-res, where the framebuffer is 8 bits/pixel. */
        SPCTL = saturn::hal::vdp2::sprite_type_bits();
        saturn::hal::vdp2::set_sprite_priority_pair(
            g_normal_priority, g_normal_priority);
        saturn::hal::vdp2::set_color_calc_control(0u);
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

sat_result_t select_alpha_slot(uint8_t alpha, uint8_t* out_slot, uint8_t* out_alpha) {
    if (g_enabled == 0u) return SAT_ERR_NOT_INITIALIZED;
    return saturn::core::vdp2_color_calc::choose_alpha_slot(
        alpha, g_ratio, g_strict_alpha != 0u, out_slot, out_alpha);
}

void set_strict_alpha(bool strict) {
    g_strict_alpha = strict ? 1u : 0u;
}

sat_result_t claim_mode(uint8_t mode) {
    if (g_enabled == 0u) return SAT_ERR_NOT_INITIALIZED;
    const sat_result_t st = saturn::core::vdp2_color_calc::claim_mode(g_claim, mode);
    if (st != SAT_OK) return st;
    if (g_mode != mode) {
        /* CCCTL is latched at VBlank, the same one that shows the frame now
         * being drawn, and commit_layers() replays it from here on. */
        g_mode = mode;
        saturn::hal::vdp2::set_color_calc_control(current_ccctl());
    }
    return SAT_OK;
}

void begin_frame() {
    saturn::core::vdp2_color_calc::claim_reset(g_claim);
}

void disable() {
    g_enabled = 0u;
    g_mode = SAT_VDP2_COLOR_CALC_RATIO;
    saturn::core::vdp2_color_calc::claim_reset(g_claim);
    commit();
}

}  // namespace saturn::hal::vdp2_color_calc
