#include "src/hal/vdp2.hpp"

#include "src/core/logic.hpp"

namespace saturn::hal::vdp2 {

namespace {

constexpr uintptr_t kUncached = 0x20000000u;
constexpr uintptr_t kVdp2Base = 0x05F80000u;
constexpr uint16_t kTvmdDisp = 0x8000u;
constexpr uint16_t kTvmdBdclmd = 0x0100u;
constexpr uint16_t kTvstatVblank = 0x0008u;
constexpr uint16_t kNbg0MapPlaneAIndex = 0x003Bu;  // JoEngine-style NBG0 map base (MPABN0/MPCDN0 = 0x3B3B)
constexpr uint32_t kVdp2VramWordCapacity = (512u * 1024u) / 2u;
constexpr uint32_t kBackdropTableWordOffset = 0x3FFFFu;
/* VDP2 cycle pattern commands:
 * 0-7 = NBG0-NBG3 pattern/character reads
 * 8   = RBG0 parameter table read
 * 9   = RBG0 bitmap read (bitmap mode) / coefficient read (cell mode)
 * C   = Color RAM read
 * E   = CPU read/write
 * F   = No access
 */
constexpr uint16_t kRbg0BitmapCyclePatternLo = 0x9E9Eu;  /* Bitmap read + CPU */
constexpr uint16_t kRbg0BitmapCyclePatternHi = 0x9E9Eu;
constexpr uint16_t kRbg0ParamCyclePatternLo = 0x8E8Eu;   /* Param read + CPU */
constexpr uint16_t kRbg0ParamCyclePatternHi = 0x8E8Eu;
uint16_t g_nbg0_map_plane_index = kNbg0MapPlaneAIndex;
uint16_t g_last_rbg0_bgon_written = 0x0000u;
uint16_t g_last_rbg0_ramctl_written = 0x0000u;
uint16_t g_last_rbg0_chctlb_written = 0x0000u;
uint16_t g_last_rbg0_mpofr_written = 0x0000u;
uint16_t g_last_rbg0_rptau_written = 0x0000u;
uint16_t g_last_rbg0_rptal_written = 0x0000u;
uint16_t g_last_rbg0_rprctl_written = 0x0000u;
uint16_t g_last_rbg0_ktctl_written = 0x0000u;
uint16_t g_last_rbg0_rpmd_written = 0x0000u;
uint16_t g_last_rbg0_prir_written = 0x0000u;
uint16_t g_last_rbg0_bmpnb_written = 0x0000u;
uint16_t g_last_rbg0_plsz_written = 0x0000u;
uint16_t g_last_rbg0_cycle_a0l = 0xEEEEu;
uint16_t g_last_rbg0_cycle_a0u = 0xEEEEu;
uint16_t g_last_rbg0_cycle_a1l = 0xEEEEu;
uint16_t g_last_rbg0_cycle_a1u = 0xEEEEu;
uint16_t g_last_rbg0_cycle_b0l = 0xEEEEu;
uint16_t g_last_rbg0_cycle_b0u = 0xEEEEu;
uint16_t g_last_rbg0_cycle_b1l = 0xEEEEu;
uint16_t g_last_rbg0_cycle_b1u = 0xEEEEu;

template <uint32_t Offset>
volatile uint16_t& reg() {
    return *reinterpret_cast<volatile uint16_t*>(kUncached | (kVdp2Base + Offset));
}

volatile uint16_t* const VDP2_VRAM_16 = reinterpret_cast<volatile uint16_t*>(kUncached | 0x05E00000u);
volatile uint16_t* const VDP2_CRAM_16 = reinterpret_cast<volatile uint16_t*>(kUncached | 0x05F00000u);

volatile uint16_t& TVMD = reg<0x000>();
volatile uint16_t& TVSTAT = reg<0x004>();
volatile uint16_t& VRSIZE = reg<0x006>();
volatile uint16_t& RAMCTL = reg<0x00E>();
volatile uint16_t& CYCA0L = reg<0x010>();
volatile uint16_t& CYCA0U = reg<0x012>();
volatile uint16_t& CYCA1L = reg<0x014>();
volatile uint16_t& CYCA1U = reg<0x016>();
volatile uint16_t& CYCB0L = reg<0x018>();
volatile uint16_t& CYCB0U = reg<0x01A>();
volatile uint16_t& CYCB1L = reg<0x01C>();
volatile uint16_t& CYCB1U = reg<0x01E>();
volatile uint16_t& BGON = reg<0x020>();
volatile uint16_t& CHCTLA = reg<0x028>();
volatile uint16_t& PNCN0 = reg<0x030>();
volatile uint16_t& PLSZ = reg<0x03A>();
volatile uint16_t& MPOFN = reg<0x03C>();
volatile uint16_t& MPOFR = reg<0x03E>();
volatile uint16_t& MPABN0 = reg<0x040>();
volatile uint16_t& MPCDN0 = reg<0x042>();
volatile uint16_t& SCXIN0 = reg<0x070>();
volatile uint16_t& SCXDN0 = reg<0x072>();
volatile uint16_t& SCYIN0 = reg<0x074>();
volatile uint16_t& SCYDN0 = reg<0x076>();
volatile uint16_t& ZMXIN0 = reg<0x078>();
volatile uint16_t& ZMXDN0 = reg<0x07A>();
volatile uint16_t& ZMYIN0 = reg<0x07C>();
volatile uint16_t& ZMYDN0 = reg<0x07E>();
volatile uint16_t& BKTAU = reg<0x0AC>();
volatile uint16_t& BKTAL = reg<0x0AE>();
volatile uint16_t& PRISA = reg<0x0F0>();
volatile uint16_t& PRINA = reg<0x0F8>();

inline uint16_t to_scroll_fraction(uint16_t fraction) {
    if (fraction <= 0x00FFu) {
        return static_cast<uint16_t>(fraction << 8u);
    }
    return static_cast<uint16_t>(fraction & 0xFF00u);
}

inline void set_backdrop_table_word_offset(uint32_t word_offset) {
    const uint32_t bkta = word_offset & 0x0007FFFFu;
    BKTAU = static_cast<uint16_t>((bkta >> 16u) & 0x0007u);  // BKCLMD=0 (single color)
    BKTAL = static_cast<uint16_t>(bkta & 0xFFFFu);
}

inline void set_vram_cycle_pattern(uint16_t bank_id, uint16_t low_pattern, uint16_t high_pattern) {
    switch (bank_id & 0x0003u) {
    case 0u:
        CYCA0L = low_pattern;
        CYCA0U = high_pattern;
        g_last_rbg0_cycle_a0l = low_pattern;
        g_last_rbg0_cycle_a0u = high_pattern;
        break;
    case 1u:
        CYCA1L = low_pattern;
        CYCA1U = high_pattern;
        g_last_rbg0_cycle_a1l = low_pattern;
        g_last_rbg0_cycle_a1u = high_pattern;
        break;
    case 2u:
        CYCB0L = low_pattern;
        CYCB0U = high_pattern;
        g_last_rbg0_cycle_b0l = low_pattern;
        g_last_rbg0_cycle_b0u = high_pattern;
        break;
    case 3u:
        CYCB1L = low_pattern;
        CYCB1U = high_pattern;
        g_last_rbg0_cycle_b1l = low_pattern;
        g_last_rbg0_cycle_b1u = high_pattern;
        break;
    default:
        break;
    }
}

inline void ensure_rbg0_bank_fetch_visible(uint32_t word_offset, bool is_bitmap_bank) {
    const uint16_t bank_id = static_cast<uint16_t>((word_offset >> 16u) & 0x0003u);
    /* Use cycle patterns with RBG0 bitmap read (9) or parameter read (8)
     * alternated with CPU access (E) so the VDP2 can fetch data while the
     * CPU can still update tables during VBlank.
     */
    if (is_bitmap_bank) {
        set_vram_cycle_pattern(bank_id, kRbg0BitmapCyclePatternLo, kRbg0BitmapCyclePatternHi);
    } else {
        set_vram_cycle_pattern(bank_id, kRbg0ParamCyclePatternLo, kRbg0ParamCyclePatternHi);
    }
}

constexpr double kPi = 3.141592653589793238462643383279502884;

inline double wrap_degrees(double degrees) {
    while (degrees > 180.0) {
        degrees -= 360.0;
    }
    while (degrees < -180.0) {
        degrees += 360.0;
    }
    return degrees;
}

inline void sincos_degrees(int32_t angle_deg, double& out_sin, double& out_cos) {
    double degrees = wrap_degrees(static_cast<double>(angle_deg));
    double sign_s = 1.0;
    double sign_c = 1.0;

    if (degrees > 90.0) {
        degrees = 180.0 - degrees;
        sign_c = -1.0;
    } else if (degrees < -90.0) {
        degrees = 180.0 + degrees;
        sign_s = -1.0;
        sign_c = -1.0;
    }

    const double radians = degrees * (kPi / 180.0);
    const double r2 = radians * radians;
    const double r4 = r2 * r2;
    const double r6 = r4 * r2;
    const double r8 = r4 * r4;

    const double sin_approx = radians * (1.0 - (r2 / 6.0) + (r4 / 120.0) - (r6 / 5040.0));
    const double cos_approx = 1.0 - (r2 / 2.0) + (r4 / 24.0) - (r6 / 720.0) + (r8 / 40320.0);

    out_sin = sign_s * sin_approx;
    out_cos = sign_c * cos_approx;
}

inline int32_t fx16_from_double(double value) {
    const double scaled = value * 65536.0;
    return static_cast<int32_t>(scaled >= 0.0 ? (scaled + 0.5) : (scaled - 0.5));
}

inline void write_fx16_pair(volatile uint16_t* vram, uint32_t word_offset, double value) {
    const uint32_t bits = static_cast<uint32_t>(fx16_from_double(value));
    vram[word_offset + 0u] = static_cast<uint16_t>((bits >> 16u) & 0xFFFFu);
    vram[word_offset + 1u] = static_cast<uint16_t>(bits & 0xFFFFu);
}

}  // namespace

void init_ntsc_320x224() {
    TVMD = 0x0000;
    VRSIZE = 0x0000;
    /* Mirror the JoEngine baseline VDP2 setup for NBG0 cell format.
     * This keeps the register state consistent with the working benchmark.
     */
    RAMCTL = 0x1327u;

    CYCA0L = 0x5555u;
    CYCA0U = 0xFEEEu;
    CYCA1L = 0x5555u;
    CYCA1U = 0xFEEEu;
    CYCB0L = 0xFFFFu;
    CYCB0U = 0xEEEEu;
    CYCB1L = 0x044Fu;
    CYCB1U = 0xEEEEu;

    BGON = 0x0000;
    CHCTLA = 0x3210u;
    PNCN0 = 0x800Cu;  // 1-word pattern name + 12-bit character number
    PLSZ = 0x0000u;
    MPOFN = 0x0000u;
    set_nbg0_map_plane_index(kNbg0MapPlaneAIndex);

    SCXIN0 = 0x0000;
    SCXDN0 = 0x0000;
    SCYIN0 = 0x0000;
    SCYDN0 = 0x0000;
    ZMXIN0 = 0x0001;
    ZMXDN0 = 0x0000;
    ZMYIN0 = 0x0001;
    ZMYDN0 = 0x0000;

    PRISA = 0x0606;
    PRINA = static_cast<uint16_t>((PRINA & 0xFFF8u) | 0x0001u);

    set_backdrop_color(0x0000);
    TVMD = static_cast<uint16_t>(kTvmdDisp | kTvmdBdclmd);
}

uint16_t read_tvstat() {
    return TVSTAT;
}

void configure_nbg0_character(CharacterSize char_size, ColorMode color_mode) {
    uint16_t value = 0x3200u;
    const uint16_t mode = static_cast<uint16_t>(color_mode) & 0x0007u;
    value = static_cast<uint16_t>(value | static_cast<uint16_t>(mode << 4u));
    if (char_size == CHAR_SIZE_2x2) {
        value = static_cast<uint16_t>(value | 0x0001u);
    }
    CHCTLA = value;
}

void configure_nbg0_text_layout() {
    /* Match the working vdp2_nbg0_image direct register sequence exactly.
     * NOTE: CHCTLA is set by configure_nbg0_character() before this call,
     * so we do NOT overwrite it here. */
    TVMD = 0x0000u;  /* Disable display during config */

    RAMCTL = 0x1327u;

    CYCA0L = 0x5555u;
    CYCA0U = 0xFEEEu;
    CYCA1L = 0x5555u;
    CYCA1U = 0xFEEEu;
    CYCB0L = 0xFFFFu;
    CYCB0U = 0xEEEEu;
    CYCB1L = 0x044Fu;
    CYCB1U = 0xEEEEu;

    BGON = 0x0000u;
    /* CHCTLA is set by configure_nbg0_character() - do not overwrite */
    PNCN0 = 0xC00Cu;   /* 1-word pattern name + 12-bit char number, cell base in B1 */
    PLSZ = 0x0000u;
    MPOFN = 0x0000u;
    set_nbg0_map_plane_index(g_nbg0_map_plane_index);

    SCXIN0 = 0x0000;
    SCXDN0 = 0x0000;
    SCYIN0 = 0x0000;
    SCYDN0 = 0x0000;
    ZMXIN0 = 0x0001;
    ZMXDN0 = 0x0000;
    ZMYIN0 = 0x0001;
    ZMYDN0 = 0x0000;

    PRISA = 0x0606u;
    PRINA = 0x0607u;

    BGON = 0x0101u;
    TVMD = 0x8100u;
}

void set_nbg0_map_plane_index(uint16_t plane_index) {
    const uint16_t clamped = static_cast<uint16_t>(plane_index & 0x003Fu);
    g_nbg0_map_plane_index = clamped;
    const uint16_t packed = static_cast<uint16_t>((clamped << 8u) | clamped);
    MPABN0 = packed;
    MPCDN0 = packed;
}

uint16_t nbg0_map_plane_index() {
    return g_nbg0_map_plane_index;
}

void set_nbg0_transparent_code_enabled(bool enabled) {
    uint16_t value = BGON;
    if (enabled) {
        value = static_cast<uint16_t>(value & static_cast<uint16_t>(~0x0100u));
    } else {
        value = static_cast<uint16_t>(value | 0x0100u);
    }
    BGON = value;
}

void set_nbg0_scroll(uint16_t x_integer, uint16_t x_fraction, uint16_t y_integer, uint16_t y_fraction) {
    SCXIN0 = static_cast<uint16_t>(x_integer & 0x07FFu);
    SCXDN0 = to_scroll_fraction(x_fraction);
    SCYIN0 = static_cast<uint16_t>(y_integer & 0x07FFu);
    SCYDN0 = to_scroll_fraction(y_fraction);
}

void enable_nbg0(bool enable) {
    uint16_t value = BGON;
    if (enable) {
        value = static_cast<uint16_t>(value | 0x0001u);
    } else {
        value = static_cast<uint16_t>(value & static_cast<uint16_t>(~0x0001u));
    }
    BGON = value;
}

void upload_palette(const uint16_t* palette_rgb555, uint16_t count, uint16_t offset) {
    if (palette_rgb555 == nullptr || count == 0u) {
        return;
    }
    for (uint16_t i = 0; i < count; ++i) {
        VDP2_CRAM_16[static_cast<uint32_t>(offset) + i] = palette_rgb555[i];
    }
}

void upload_palette_4(const uint16_t palette_rgb555[4], uint16_t offset) {
    upload_palette(palette_rgb555, 4, offset);
}

void upload_palette_16(const uint16_t palette_rgb555[16], uint16_t offset) {
    upload_palette(palette_rgb555, 16, offset);
}

void wait_vblank_start() {
    while ((TVSTAT & kTvstatVblank) != 0u) {
    }
    while ((TVSTAT & kTvstatVblank) == 0u) {
    }
}

void wait_vblank_end() {
    while ((TVSTAT & kTvstatVblank) == 0u) {
    }
    while ((TVSTAT & kTvstatVblank) != 0u) {
    }
}

void set_screen_mode(ScreenMode mode) {
    RAMCTL = static_cast<uint16_t>((RAMCTL & 0xCFFFu) | static_cast<uint16_t>(mode));
}

void set_display_enable(bool enable) {
    uint16_t value = TVMD;
    if (enable) {
        value = static_cast<uint16_t>(value | kTvmdDisp);
    } else {
        value = static_cast<uint16_t>(value & static_cast<uint16_t>(~kTvmdDisp));
    }
    TVMD = value;
}

void set_map_offset(uint16_t offset) {
    MPOFN = static_cast<uint16_t>((MPOFN & 0xFFF8u) | (offset & 0x0007u));
}

void set_backdrop_color(uint16_t rgb555) {
    set_backdrop_table_word_offset(kBackdropTableWordOffset);
    VDP2_VRAM_16[kBackdropTableWordOffset] = static_cast<uint16_t>(rgb555 & 0x7FFFu);
    TVMD = static_cast<uint16_t>(TVMD | kTvmdBdclmd);
}

void write_vram_words(uint32_t word_offset, const uint16_t* words, uint32_t word_count) {
    if (words == nullptr || word_count == 0u) {
        return;
    }
    for (uint32_t i = 0; i < word_count; ++i) {
        VDP2_VRAM_16[word_offset + i] = words[i];
    }
}

void fill_vram_words(uint32_t word_offset, uint16_t value, uint32_t word_count) {
    if (word_count == 0u) {
        return;
    }
    for (uint32_t i = 0; i < word_count; ++i) {
        VDP2_VRAM_16[word_offset + i] = value;
    }
}

/* ================================================================== */
/* RBG0 (Rotation Background 0) Implementation                        */
/* ================================================================== */

volatile uint16_t& CHCTLB = reg<0x02A>();
volatile uint16_t& RNCN0  = reg<0x038>();
volatile uint16_t& RPMD   = reg<0x0B0>();
volatile uint16_t& RPRCTL = reg<0x0B2>();
volatile uint16_t& KTCTL  = reg<0x0B4>();
volatile uint16_t& RPTAU  = reg<0x0B8>();
volatile uint16_t& RPTAL  = reg<0x0BA>();
volatile uint16_t& PRIR   = reg<0x0FC>();
volatile uint16_t& BMPNB  = reg<0x02E>();

void configure_rbg0_bitmap(RBG0BitmapSize bitmap_size, ColorMode color_mode,
                           uint32_t bitmap_base_word, uint32_t rot_param_base_word) {
    if (!saturn::core::is_supported_rbg0_bitmap_size(static_cast<sat_vdp2_rbg0_bitmap_size_t>(bitmap_size)) ||
        color_mode > COLOR_MODE_16770000) {
        return;
    }

    /* Configure RAMCTL usage bits for the bank that stores the bitmap.
     * Only the selected bitmap bank needs to be marked as bitmap data.
     * Rotation parameters are addressed through RPTA and do not use a
     * dedicated RAMCTL usage class.
     */
    uint16_t ramctl = 0x1100u;
    const uint16_t bitmap_bank_id = static_cast<uint16_t>((bitmap_base_word >> 16u) & 0x0003u);
    switch (bitmap_bank_id) {
    case 0u:  // VRAM-A0
        ramctl = static_cast<uint16_t>(ramctl | 0x0003u);
        break;
    case 1u:  // VRAM-A1
        ramctl = static_cast<uint16_t>(ramctl | 0x000Cu);
        break;
    case 2u:  // VRAM-B0
        ramctl = static_cast<uint16_t>(ramctl | 0x0030u);
        break;
    case 3u:  // VRAM-B1
        ramctl = static_cast<uint16_t>(ramctl | 0x00C0u);
        break;
    default:
        break;
    }

    const uint16_t rot_bank_id = static_cast<uint16_t>((rot_param_base_word >> 16u) & 0x0003u);
    switch (rot_bank_id) {
    case 0u:  // VRAM-A0
        ramctl = static_cast<uint16_t>(ramctl | 0x0001u);
        break;
    case 1u:  // VRAM-A1
        ramctl = static_cast<uint16_t>(ramctl | 0x0004u);
        break;
    case 2u:  // VRAM-B0
        ramctl = static_cast<uint16_t>(ramctl | 0x0010u);
        break;
    case 3u:  // VRAM-B1
        ramctl = static_cast<uint16_t>(ramctl | 0x0040u);
        break;
    default:
        break;
    }

    RAMCTL = ramctl;
    g_last_rbg0_ramctl_written = ramctl;

    ensure_rbg0_bank_fetch_visible(bitmap_base_word, true);
    ensure_rbg0_bank_fetch_visible(rot_param_base_word, false);

    /* Configure CHCTLB for RBG0.
     * R0CHCN uses bits 14..12, R0BMEN is bit 9 and R0BMSZ is bit 10.
     * The register lives at 18002AH, not 18002CH.
     */
    const uint16_t chctlb = static_cast<uint16_t>((CHCTLB & 0x89FFu) |
        saturn::core::compose_rbg0_bitmap_control_word(
            static_cast<sat_vdp2_color_mode_t>(color_mode),
            static_cast<sat_vdp2_rbg0_bitmap_size_t>(bitmap_size)));
    CHCTLB = chctlb;
    g_last_rbg0_chctlb_written = chctlb;

    /* Select the VRAM bank that contains the bitmap data.
     * RBG0 bitmap mode only selects a bank base, not an arbitrary word offset.
     */
    const uint16_t bitmap_bank = static_cast<uint16_t>((bitmap_base_word >> 16u) & 0x0007u);
    const uint16_t mpofr = static_cast<uint16_t>((MPOFR & 0xFFF8u) | bitmap_bank);
    MPOFR = mpofr;
    g_last_rbg0_mpofr_written = mpofr;

    /* RNCN0 is ignored in bitmap mode */

    /* Configure RPTA (Rotation Parameter Table Address).
     * RPTA stores address bits A18..A1 directly, i.e. the VRAM word offset.
     * Bit 0 of RPTAL is unused; bit 6 is forced to 0 for parameter A.
     */
    const uint16_t rptau = static_cast<uint16_t>((rot_param_base_word >> 16u) & 0x0007u);
    const uint16_t rptal = static_cast<uint16_t>(rot_param_base_word & 0xFFFEu);
    RPTAU = rptau;
    RPTAL = rptal;
    g_last_rbg0_rptau_written = rptau;
    g_last_rbg0_rptal_written = rptal;

    /* Set default rotation parameter mode (A) */
    RPMD = 0x0000u;
    g_last_rbg0_rpmd_written = 0x0000u;
    RPRCTL = 0x0000u;
    KTCTL = 0x0000u;
    g_last_rbg0_rprctl_written = 0x0000u;
    g_last_rbg0_ktctl_written = 0x0000u;

    /* RBG0 priority (R0PRIN) to visible foreground level. */
    const uint16_t prir = static_cast<uint16_t>((PRIR & 0xFFF8u) | 0x0007u);
    PRIR = prir;
    g_last_rbg0_prir_written = prir;

    /* Configure BMPNB for bitmap palette control.
     * The example uses palette 0, so leave the bitmap palette bits cleared.
     */
    BMPNB = 0x0000u;
    g_last_rbg0_bmpnb_written = 0x0000u;

    /* Set screen-over (overflow) mode to repeat image outside display area.
     * RAOVR bits 11..10 = 00: repeat image (infinite plane effect).
     * RAPLSZ bits 9..8 = 00: 1x1 page plane size.
     */
    const uint16_t plsz = static_cast<uint16_t>(PLSZ & 0xF0FFu);
    PLSZ = plsz;
    g_last_rbg0_plsz_written = plsz;
}

void enable_rbg0(bool enable) {
    /* BGON bit 4 = R0ON (RBG0 enable), bit 12 = R0TPON (transparent code disable). */
    uint16_t bgon = BGON;
    if (enable) {
        bgon = static_cast<uint16_t>(bgon | 0x1010u);
    } else {
        bgon = static_cast<uint16_t>(bgon & static_cast<uint16_t>(~0x1010u));
    }
    BGON = bgon;
    g_last_rbg0_bgon_written = bgon;
}

uint16_t last_rbg0_bgon_written() {
    return g_last_rbg0_bgon_written;
}

uint16_t last_rbg0_ramctl_written() {
    return g_last_rbg0_ramctl_written;
}

uint16_t last_rbg0_chctlb_written() {
    return g_last_rbg0_chctlb_written;
}

uint16_t last_rbg0_mpofr_written() {
    return g_last_rbg0_mpofr_written;
}

uint16_t last_rbg0_rptau_written() {
    return g_last_rbg0_rptau_written;
}

uint16_t last_rbg0_rptal_written() {
    return g_last_rbg0_rptal_written;
}

uint16_t last_rbg0_rprctl_written() {
    return g_last_rbg0_rprctl_written;
}

uint16_t last_rbg0_ktctl_written() {
    return g_last_rbg0_ktctl_written;
}

uint16_t last_rbg0_rpmd_written() {
    return g_last_rbg0_rpmd_written;
}

uint16_t last_rbg0_prir_written() {
    return g_last_rbg0_prir_written;
}

uint16_t last_rbg0_bmpnb_written() {
    return g_last_rbg0_bmpnb_written;
}

uint16_t last_rbg0_plsz_written() {
    return g_last_rbg0_plsz_written;
}

void commit_rbg0_config() {
    /* Re-apply all RBG0 VDP2 register values from their shadows.
     * Must be called during VBlank for writes to take effect. */
    RAMCTL = g_last_rbg0_ramctl_written;
    CYCA0L = g_last_rbg0_cycle_a0l;
    CYCA0U = g_last_rbg0_cycle_a0u;
    CYCA1L = g_last_rbg0_cycle_a1l;
    CYCA1U = g_last_rbg0_cycle_a1u;
    CYCB0L = g_last_rbg0_cycle_b0l;
    CYCB0U = g_last_rbg0_cycle_b0u;
    CYCB1L = g_last_rbg0_cycle_b1l;
    CYCB1U = g_last_rbg0_cycle_b1u;
    CHCTLB = g_last_rbg0_chctlb_written;
    MPOFR = g_last_rbg0_mpofr_written;
    BMPNB = g_last_rbg0_bmpnb_written;
    PLSZ = g_last_rbg0_plsz_written;
    RPTAU = g_last_rbg0_rptau_written;
    RPTAL = g_last_rbg0_rptal_written;
    RPMD = g_last_rbg0_rpmd_written;
    RPRCTL = g_last_rbg0_rprctl_written;
    KTCTL = g_last_rbg0_ktctl_written;
    PRIR = g_last_rbg0_prir_written;
    BGON = g_last_rbg0_bgon_written;
}

void set_rbg0_param_mode(RBG0ParamMode mode) {
    RPMD = static_cast<uint16_t>(mode & 0x0003u);
}

void set_rbg0_rotation_read_control(uint16_t rprctl) {
    RPRCTL = rprctl;
    g_last_rbg0_rprctl_written = rprctl;
}

void set_rbg0_coefficient_control(uint16_t ktctl) {
    KTCTL = ktctl;
    g_last_rbg0_ktctl_written = ktctl;
}

void upload_rbg0_rotation_params(uint32_t rot_param_word_offset, const uint16_t* params, uint32_t word_count) {
    if (params == nullptr || word_count == 0u) {
        return;
    }
    if (word_count > saturn::core::kRbg0RotationParamWordCount) {
        return;
    }
    if (saturn::core::validate_rbg0_rotation_table_offset(rot_param_word_offset) != SAT_OK) {
        return;
    }
    write_vram_words(rot_param_word_offset, params, word_count);
}

void set_rbg0_scroll(uint32_t rot_param_word_offset,
                     int32_t xst_int, int32_t xst_frac,
                     int32_t yst_int, int32_t yst_frac) {
    if (saturn::core::validate_rbg0_rotation_table_offset(rot_param_word_offset) != SAT_OK) {
        return;
    }

    volatile uint16_t* vram = VDP2_VRAM_16;

    vram[rot_param_word_offset + saturn::core::kRbg0ScrollWordOffset] = static_cast<uint16_t>(xst_int & 0x1FFFu);
    vram[rot_param_word_offset + saturn::core::kRbg0ScrollFracWordOffset] = static_cast<uint16_t>(xst_frac & 0xFFFFu);
    vram[rot_param_word_offset + saturn::core::kRbg0ScrollYWordOffset] = static_cast<uint16_t>(yst_int & 0x1FFFu);
    vram[rot_param_word_offset + saturn::core::kRbg0ScrollYFracWordOffset] = static_cast<uint16_t>(yst_frac & 0xFFFFu);
}

void set_rbg0_rotation_matrix(uint32_t rot_param_word_offset,
                              int32_t angle_x, int32_t angle_y, int32_t angle_z) {
    if (saturn::core::validate_rbg0_rotation_table_offset(rot_param_word_offset) != SAT_OK) {
        return;
    }

    volatile uint16_t* vram = VDP2_VRAM_16;
    const uint32_t base = rot_param_word_offset + saturn::core::kRbg0MatrixWordOffset;

    double sin_x = 0.0;
    double cos_x = 1.0;
    double sin_y = 0.0;
    double cos_y = 1.0;
    double sin_z = 0.0;
    double cos_z = 1.0;

    sincos_degrees(angle_x, sin_x, cos_x);
    sincos_degrees(angle_y, sin_y, cos_y);
    sincos_degrees(angle_z, sin_z, cos_z);

    /* ZYX rotation order: pitch (X), yaw (Y), roll (Z). */
    const double a = cos_z * cos_y;
    const double b = (cos_z * sin_y * sin_x) - (sin_z * cos_x);
    const double c = (cos_z * sin_y * cos_x) + (sin_z * sin_x);
    const double d = sin_z * cos_y;
    const double e = (sin_z * sin_y * sin_x) + (cos_z * cos_x);
    const double f = (sin_z * sin_y * cos_x) - (cos_z * sin_x);

    write_fx16_pair(vram, base + 0u, a);
    write_fx16_pair(vram, base + 2u, b);
    write_fx16_pair(vram, base + 4u, c);
    write_fx16_pair(vram, base + 6u, d);
    write_fx16_pair(vram, base + 8u, e);
    write_fx16_pair(vram, base + 10u, f);
}

void set_rbg0_viewpoint(uint32_t rot_param_word_offset,
                        int32_t px, int32_t py, int32_t pz) {
    if (saturn::core::validate_rbg0_rotation_table_offset(rot_param_word_offset) != SAT_OK) {
        return;
    }

    volatile uint16_t* vram = VDP2_VRAM_16;
    const uint32_t base = rot_param_word_offset + saturn::core::kRbg0ViewpointWordOffset;

    /* Viewpoint is stored as signed integer-only fields. */
    vram[base + 0] = static_cast<uint16_t>(static_cast<int16_t>(px >> 16u));
    vram[base + 1] = static_cast<uint16_t>(static_cast<int16_t>(py >> 16u));
    vram[base + 2] = static_cast<uint16_t>(static_cast<int16_t>(pz >> 16u));
}

void set_rbg0_center(uint32_t rot_param_word_offset,
                     int32_t cx, int32_t cy, int32_t cz) {
    if (saturn::core::validate_rbg0_rotation_table_offset(rot_param_word_offset) != SAT_OK) {
        return;
    }

    volatile uint16_t* vram = VDP2_VRAM_16;
    const uint32_t base = rot_param_word_offset + saturn::core::kRbg0CenterWordOffset;

    /* Center is stored as signed integer-only fields. */
    vram[base + 0] = static_cast<uint16_t>(static_cast<int16_t>(cx >> 16u));
    vram[base + 1] = static_cast<uint16_t>(static_cast<int16_t>(cy >> 16u));
    vram[base + 2] = static_cast<uint16_t>(static_cast<int16_t>(cz >> 16u));
}

void set_rbg0_scaling(uint32_t rot_param_word_offset,
                      int32_t kx, int32_t ky) {
    if (saturn::core::validate_rbg0_rotation_table_offset(rot_param_word_offset) != SAT_OK) {
        return;
    }

    volatile uint16_t* vram = VDP2_VRAM_16;
    const uint32_t base = rot_param_word_offset + saturn::core::kRbg0ScalingWordOffset;

    /* Scaling uses a fixed-point split across the two words. */
    vram[base + 0] = static_cast<uint16_t>((kx >> 16u) & 0x7FFFu);
    vram[base + 1] = static_cast<uint16_t>(kx & 0xFFFFu);
    vram[base + 2] = static_cast<uint16_t>((ky >> 16u) & 0x7FFFu);
    vram[base + 3] = static_cast<uint16_t>(ky & 0xFFFFu);
}

/* Set screen vertical coordinate increments (ΔXst, ΔYst).
 * For a normal 1:1 2D bitmap: ΔXst = 0, ΔYst = 1.0.
 */
void set_rbg0_vertical_increments(uint32_t rot_param_word_offset,
                                   int32_t dxst_int, int32_t dxst_frac,
                                   int32_t dyst_int, int32_t dyst_frac) {
    if (saturn::core::validate_rbg0_rotation_table_offset(rot_param_word_offset) != SAT_OK) {
        return;
    }

    volatile uint16_t* vram = VDP2_VRAM_16;

    vram[rot_param_word_offset + saturn::core::kRbg0DScrollWordOffset] = static_cast<uint16_t>(dxst_int & 0x07FFu);
    vram[rot_param_word_offset + saturn::core::kRbg0DScrollFracWordOffset] = static_cast<uint16_t>(dxst_frac & 0xFFFFu);
    vram[rot_param_word_offset + saturn::core::kRbg0DScrollYWordOffset] = static_cast<uint16_t>(dyst_int & 0x07FFu);
    vram[rot_param_word_offset + saturn::core::kRbg0DScrollYFracWordOffset] = static_cast<uint16_t>(dyst_frac & 0xFFFFu);
}

/* Set screen horizontal coordinate increments (ΔX, ΔY).
 * For a normal 1:1 2D bitmap: ΔX = 1.0, ΔY = 0.
 * Integer field is 9-bit signed, fraction is 10-bit.
 */
void set_rbg0_coordinate_increments(uint32_t rot_param_word_offset,
                                     int32_t dx_int, int32_t dx_frac,
                                     int32_t dy_int, int32_t dy_frac) {
    if (saturn::core::validate_rbg0_rotation_table_offset(rot_param_word_offset) != SAT_OK) {
        return;
    }

    volatile uint16_t* vram = VDP2_VRAM_16;

    vram[rot_param_word_offset + saturn::core::kRbg0DotStepWordOffset] = static_cast<uint16_t>(dx_int & 0x07FFu);
    vram[rot_param_word_offset + saturn::core::kRbg0DotStepFracWordOffset] = static_cast<uint16_t>(dx_frac & 0xFFFFu);
    vram[rot_param_word_offset + saturn::core::kRbg0DotStepYWordOffset] = static_cast<uint16_t>(dy_int & 0x07FFu);
    vram[rot_param_word_offset + saturn::core::kRbg0DotStepYFracWordOffset] = static_cast<uint16_t>(dy_frac & 0xFFFFu);
}

}  // namespace saturn::hal::vdp2
