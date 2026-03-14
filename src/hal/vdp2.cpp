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

}  // namespace saturn::hal::vdp2
