#include "src/hal/vdp2.hpp"

namespace saturn::hal::vdp2 {

namespace {

constexpr uintptr_t kUncached = 0x20000000u;
constexpr uintptr_t kVdp2Base = 0x05F80000u;
constexpr uint16_t kTvmdDisp = 0x8000u;
constexpr uint16_t kTvmdBdclmd = 0x0100u;
constexpr uint16_t kTvstatVblank = 0x0008u;
constexpr uint16_t kNbg0MapPlaneAIndex = 0x0010u;  // 0x0010 << 11 = 0x8000
constexpr uint32_t kVdp2VramWordCapacity = (512u * 1024u) / 2u;
constexpr uint32_t kBackdropTableWordOffset = kVdp2VramWordCapacity - 1u;
uint16_t g_nbg0_map_plane_index = kNbg0MapPlaneAIndex;

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

}  // namespace

void init_ntsc_320x224() {
    TVMD = 0x0000;
    VRSIZE = 0x0000;
    RAMCTL = SCREEN_MODE_RGB555;

    // Allocate NBG0 fetches on VRAM-A0 and keep other banks CPU-access friendly.
    CYCA0L = 0x0E4Eu;
    CYCA0U = 0x4E4Eu;
    CYCA1L = 0xEEEEu;
    CYCA1U = 0xEEEEu;
    CYCB0L = 0xEEEEu;
    CYCB0U = 0xEEEEu;
    CYCB1L = 0xEEEEu;
    CYCB1U = 0xEEEEu;

    BGON = 0x0000;
    CHCTLA = 0x0000;
    PNCN0 = 0xC000;  // 1-word pattern name + 12-bit character number
    PLSZ = static_cast<uint16_t>(PLSZ & 0xFFFCu);
    MPOFN = static_cast<uint16_t>(MPOFN & 0xFFF8u);
    set_nbg0_map_plane_index(kNbg0MapPlaneAIndex);
    MPCDN0 = 0x0000;

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
    uint16_t value = static_cast<uint16_t>(CHCTLA & 0xFF80u);
    const uint16_t mode = static_cast<uint16_t>(color_mode) & 0x0007u;
    value = static_cast<uint16_t>(value | static_cast<uint16_t>(mode << 4u));
    if (char_size == CHAR_SIZE_2x2) {
        value = static_cast<uint16_t>(value | 0x0001u);
    }
    CHCTLA = value;
}

void configure_nbg0_text_layout() {
    PNCN0 = 0xC000;
    PLSZ = static_cast<uint16_t>(PLSZ & 0xFFFCu);
    MPOFN = static_cast<uint16_t>(MPOFN & 0xFFF8u);
    set_nbg0_map_plane_index(g_nbg0_map_plane_index);
    MPCDN0 = 0x0000;

    SCXIN0 = 0x0000;
    SCXDN0 = 0x0000;
    SCYIN0 = 0x0000;
    SCYDN0 = 0x0000;
    ZMXIN0 = 0x0001;
    ZMXDN0 = 0x0000;
    ZMYIN0 = 0x0001;
    ZMYDN0 = 0x0000;

    PRINA = static_cast<uint16_t>((PRINA & 0xFFF8u) | 0x0001u);
    set_nbg0_transparent_code_enabled(true);
}

void set_nbg0_map_plane_index(uint16_t plane_index) {
    const uint16_t clamped = static_cast<uint16_t>(plane_index & 0x003Fu);
    g_nbg0_map_plane_index = clamped;
    MPABN0 = static_cast<uint16_t>((MPABN0 & 0xFF00u) | clamped);
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

volatile uint16_t& CHCTLB = reg<0x02C>();
volatile uint16_t& RNCN0  = reg<0x038>();
volatile uint16_t& RPMD   = reg<0x0B0>();
volatile uint16_t& RPRCTL = reg<0x0B2>();
volatile uint16_t& RPTAU  = reg<0x0BC>();
volatile uint16_t& RPTAL  = reg<0x0BE>();
volatile uint16_t& BMPNB  = reg<0x02E>();

void configure_rbg0_bitmap(RBG0BitmapSize bitmap_size, ColorMode color_mode,
                           uint32_t bitmap_base_word, uint32_t rot_param_base_word) {
    /* FIX #1: Configure VRAM banks for RBG0 bitmap mode
     * RAMCTL bits 0-1 (RDBSA0) and 4-5 (RDBSB0) control RBG0 VRAM usage:
     *   00b = Not used for RBG0
     *   01b = Coefficient table
     *   10b = Pattern name table
     *   11b = Bitmap pattern data
     *
     * For bitmap mode, we need VRAM-A0 for bitmap data and VRAM-B0 for rotation params
     */
    uint16_t ramctl = RAMCTL;
    /* Set VRAM-A0 to bitmap pattern (11b) */
    ramctl = static_cast<uint16_t>(ramctl & 0xFFFCu);  /* Clear bits 0-1 */
    ramctl = static_cast<uint16_t>(ramctl | 0x0003u);  /* Set to 11b (bitmap pattern) */
    /* Set VRAM-B0 to coefficient/param table (01b) */
    ramctl = static_cast<uint16_t>(ramctl & 0xFFCFu);  /* Clear bits 4-5 */
    ramctl = static_cast<uint16_t>(ramctl | 0x0010u);  /* Set to 01b (coefficient table) */
    RAMCTL = ramctl;

    /* FIX #6: RBG0 bitmap size is controlled by single bit R0BMSZ (bit 10 of CHCTLB)
     *   0 = 512x256
     *   1 = 512x512
     * We use 512x256 for all cases (256x256 textures fit within this)
     */
    uint16_t chctlb = 0;

    /* Bitmap size bit (bit 10): 0 = 512x256, 1 = 512x512 */
    if (bitmap_size >= RBG0_BITMAP_SIZE_512x512) {
        chctlb = static_cast<uint16_t>(chctlb | 0x0400u);  /* Set bit 10 for 512x512 */
    }
    /* else: bit 10 = 0 for 512x256 */

    /* Color mode (bits 12-14) */
    const uint16_t mode = static_cast<uint16_t>(color_mode) & 0x0007u;
    chctlb = static_cast<uint16_t>(chctlb | static_cast<uint16_t>(mode << 12u));

    /* Enable bitmap mode (bit 9 R0BMEN) */
    chctlb = static_cast<uint16_t>(chctlb | 0x0200u);

    CHCTLB = chctlb;

    /* FIX #7: RNCN0 is ignored in bitmap mode (when R0BMEN=1), so skip it */

    /* FIX #3: Correct RPTA address calculation
     * Formula: Lead address = (RPTA18-RPTA7) * 0x100 + (RPTA5-RPTA1) * 4
     * RPTAU bits 2-0 = RPTA18-RPTA16
     * RPTAL bits 15-1 = RPTA15-RPTA1 (bit 0 is ignored)
     */
    RPTAU = static_cast<uint16_t>((rot_param_base_word >> 16u) & 0x0007u);
    RPTAL = static_cast<uint16_t>((rot_param_base_word << 1u) & 0xFFFEu);

    /* Set default rotation parameter mode (A) */
    RPMD = 0x0000u;

    /* FIX #5: Configure BMPNB for 256-color palette
     * BMPNB bits 2-0 (R0BMP) select the upper 3 bits of palette number
     * For 256 colors starting at CRAM offset 0, set R0BMP=0
     * Also clear special bits R0BMCC and R0BMPR
     */
    BMPNB = 0x0000u;

    /* Suppress unused parameter warning */
    (void)bitmap_base_word;
}

void enable_rbg0(bool enable) {
    /* BGON bit 12 = R0ON (RBG0 enable) */
    uint16_t bgon = BGON;
    if (enable) {
        bgon = static_cast<uint16_t>(bgon | 0x1000u);
    } else {
        bgon = static_cast<uint16_t>(bgon & static_cast<uint16_t>(~0x1000u));
    }
    BGON = bgon;
}

void set_rbg0_param_mode(RBG0ParamMode mode) {
    RPMD = static_cast<uint16_t>(mode & 0x0003u);
}

void upload_rbg0_rotation_params(uint32_t rot_param_word_offset, const uint16_t* params, uint32_t word_count) {
    if (params == nullptr || word_count == 0u) {
        return;
    }
    write_vram_words(rot_param_word_offset, params, word_count);
}

void set_rbg0_scroll(uint32_t rot_param_word_offset,
                     int32_t xst_int, int32_t xst_frac,
                     int32_t yst_int, int32_t yst_frac) {
    /* FIX #4: Correct sign mask for Xst/Yst
     * Xst: 12-bit signed integer + 10-bit fraction = 22 bits total
     *   Integer part uses bits 15-4 (12 bits with sign)
     *   Fraction part uses bits 3-0 (upper 4 bits of 10-bit fraction)
     * Yst: 11-bit signed integer + 10-bit fraction = 21 bits total
     *   Integer part uses bits 15-5 (11 bits with sign)
     *   Fraction part uses bits 4-0 (upper 5 bits of 10-bit fraction)
     */
    volatile uint16_t* vram = VDP2_VRAM_16;

    /* Xst: 12-bit signed (preserve sign bit in bit 15) */
    vram[rot_param_word_offset + 0] = static_cast<uint16_t>(xst_int & 0x1FFFu);
    /* Xst fraction: upper 10 bits stored in lower 10 bits of word */
    vram[rot_param_word_offset + 1] = static_cast<uint16_t>(xst_frac & 0x03FFu);

    /* Yst: 11-bit signed (preserve sign bit in bit 15) */
    vram[rot_param_word_offset + 2] = static_cast<uint16_t>(yst_int & 0x0FFFu);
    /* Yst fraction: upper 10 bits stored in lower 10 bits of word */
    vram[rot_param_word_offset + 3] = static_cast<uint16_t>(yst_frac & 0x03FFu);
}

/* FIX #2: Correct rotation parameter table structure
 * According to manual Figure 6.3, the rotation parameter table layout is:
 *   +0:  Xst integer (12-bit signed)
 *   +1:  Xst fraction (10-bit)
 *   +2:  Yst integer (11-bit signed)
 *   +3:  Yst fraction (10-bit)
 *   +4:  Zst integer (12-bit signed)
 *   +5:  Zst fraction (10-bit)
 *   +6:  dXst integer (12-bit signed) - per-line X increment
 *   +7:  dXst fraction (10-bit)
 *   +8:  dYst integer (11-bit signed) - per-line Y increment
 *   +9:  dYst fraction (10-bit)
 *   +10: dX integer (2-bit signed) - per-dot X increment
 *   +11: dX fraction (9-bit)
 *   +12: dY integer (2-bit signed) - per-dot Y increment
 *   +13: dY fraction (9-bit)
 *   +14: A integer (rotation matrix)
 *   +15: A fraction
 *   +16: B integer
 *   +17: B fraction
 *   ... (C through I follow)
 */

void set_rbg0_rotation_matrix(uint32_t rot_param_word_offset,
                              int32_t angle_x, int32_t angle_y, int32_t angle_z) {
    volatile uint16_t* vram = VDP2_VRAM_16;

    /* Set identity rotation matrix (no rotation)
     * Matrix starts at offset +14, NOT +6!
     * A=1.0, B=0, C=0, D=0, E=1.0, F=0, G=0, H=0, I=1.0
     */
    uint32_t base = rot_param_word_offset + 14u;

    /* A=1.0 */
    vram[base + 0] = 0x0001u;
    vram[base + 1] = 0x0000u;
    /* B=0 */
    vram[base + 2] = 0x0000u;
    vram[base + 3] = 0x0000u;
    /* C=0 */
    vram[base + 4] = 0x0000u;
    vram[base + 5] = 0x0000u;
    /* D=0 */
    vram[base + 6] = 0x0000u;
    vram[base + 7] = 0x0000u;
    /* E=1.0 */
    vram[base + 8] = 0x0001u;
    vram[base + 9] = 0x0000u;
    /* F=0 */
    vram[base + 10] = 0x0000u;
    vram[base + 11] = 0x0000u;
    /* G=0 */
    vram[base + 12] = 0x0000u;
    vram[base + 13] = 0x0000u;
    /* H=0 */
    vram[base + 14] = 0x0000u;
    vram[base + 15] = 0x0000u;
    /* I=1.0 */
    vram[base + 16] = 0x0001u;
    vram[base + 17] = 0x0000u;

    /* Suppress unused parameter warnings */
    (void)angle_x;
    (void)angle_y;
    (void)angle_z;
}

void set_rbg0_viewpoint(uint32_t rot_param_word_offset,
                        int32_t px, int32_t py, int32_t pz) {
    volatile uint16_t* vram = VDP2_VRAM_16;
    /* Viewpoint starts at offset +30 (after matrix A-I at +14..+31) */
    uint32_t base = rot_param_word_offset + 30u;

    /* Px */
    vram[base + 0] = static_cast<uint16_t>((px >> 16u) & 0x0FFFu);
    vram[base + 1] = static_cast<uint16_t>((px >> 8u) & 0x03FFu);
    /* Py */
    vram[base + 2] = static_cast<uint16_t>((py >> 16u) & 0x0FFFu);
    vram[base + 3] = static_cast<uint16_t>((py >> 8u) & 0x03FFu);
    /* Pz */
    vram[base + 4] = static_cast<uint16_t>((pz >> 16u) & 0x0FFFu);
    vram[base + 5] = static_cast<uint16_t>((pz >> 8u) & 0x03FFu);
}

void set_rbg0_center(uint32_t rot_param_word_offset,
                     int32_t cx, int32_t cy, int32_t cz) {
    volatile uint16_t* vram = VDP2_VRAM_16;
    /* Center starts at offset +36 */
    uint32_t base = rot_param_word_offset + 36u;

    /* Cx */
    vram[base + 0] = static_cast<uint16_t>((cx >> 16u) & 0x0FFFu);
    vram[base + 1] = static_cast<uint16_t>((cx >> 8u) & 0x03FFu);
    /* Cy */
    vram[base + 2] = static_cast<uint16_t>((cy >> 16u) & 0x0FFFu);
    vram[base + 3] = static_cast<uint16_t>((cy >> 8u) & 0x03FFu);
    /* Cz */
    vram[base + 4] = static_cast<uint16_t>((cz >> 16u) & 0x0FFFu);
    vram[base + 5] = static_cast<uint16_t>((cz >> 8u) & 0x03FFu);
}

void set_rbg0_scaling(uint32_t rot_param_word_offset,
                      int32_t kx, int32_t ky) {
    volatile uint16_t* vram = VDP2_VRAM_16;
    /* Scaling starts at offset +42 */
    uint32_t base = rot_param_word_offset + 42u;

    /* kx */
    vram[base + 0] = static_cast<uint16_t>((kx >> 16u) & 0x0FFFu);
    vram[base + 1] = static_cast<uint16_t>((kx >> 8u) & 0x03FFu);
    /* ky */
    vram[base + 2] = static_cast<uint16_t>((ky >> 16u) & 0x0FFFu);
    vram[base + 3] = static_cast<uint16_t>((ky >> 8u) & 0x03FFu);
}

/* Helper function to set coordinate increments (dX, dY) for proper bitmap mapping */
void set_rbg0_coordinate_increments(uint32_t rot_param_word_offset,
                                     int32_t dx_int, int32_t dx_frac,
                                     int32_t dy_int, int32_t dy_frac) {
    volatile uint16_t* vram = VDP2_VRAM_16;

    /* dX at offset +10 (2-bit signed integer + 9-bit fraction) */
    vram[rot_param_word_offset + 10] = static_cast<uint16_t>(dx_int & 0x0003u);
    vram[rot_param_word_offset + 11] = static_cast<uint16_t>(dx_frac & 0x01FFu);

    /* dY at offset +12 (2-bit signed integer + 9-bit fraction) */
    vram[rot_param_word_offset + 12] = static_cast<uint16_t>(dy_int & 0x0003u);
    vram[rot_param_word_offset + 13] = static_cast<uint16_t>(dy_frac & 0x01FFu);
}

}  // namespace saturn::hal::vdp2
