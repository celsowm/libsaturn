#ifndef SATURN_HAL_VDP2_COMPOSE_LOGIC_HPP
#define SATURN_HAL_VDP2_COMPOSE_LOGIC_HPP

#include <stdint.h>

/* Pure VDP2 composition policy: the two normal windows (W0, W1) and their line
 * window tables, the per-screen window control, mosaic, and the per-screen
 * colour calculation enables and ratios (manual chapters 8, 4.11 and 12).
 * No register access, so host tests cover it. */
namespace saturn::hal::vdp2::compose {

/* Screens a window can be applied to (WCTLA-WCTLD). The colour calculation
 * window switches colour calculation off where it is not "enabled". */
enum Screen : uint8_t {
    kNbg0 = 0, kNbg1 = 1, kNbg2 = 2, kNbg3 = 3, kRbg0 = 4, kSprite = 5, kColorCalc = 6
};
constexpr uint8_t kScreenCount = 7u;

/* Where a screen is SHOWN relative to a window. The hardware describes the
 * opposite: its window area is the part made transparent (ymir and the manual's
 * "transparency processing window"), and several windows combine by OR/AND on
 * that transparent area. screen_byte() translates, by De Morgan: shown inside
 * is transparent outside, and "shown in both" (AND) is "transparent in either"
 * (OR). Measured on Ymir with a line window and a rectangle. */
enum Area : uint8_t { kAreaOff = 0, kAreaInside = 1, kAreaOutside = 2 };

struct ScreenWindow {
    uint8_t w0;          /* Area */
    uint8_t w1;          /* Area */
    bool logic_and;      /* shown where both windows say so (true) or where either does */
};

/* A window rectangle in TV dots, start and end inclusive. In normal (non
 * hi-res) mode the horizontal registers drop the least significant bit, so
 * an x coordinate is stored shifted left by one. */
struct Rect {
    uint16_t x0, y0, x1, y1;
};

struct Window {
    Rect rect;
    bool line;                  /* horizontal extent comes from a per-line table */
    uint32_t line_table;        /* byte address in VRAM, a multiple of 4 */
};

constexpr uint16_t kMaxX = 1023u;
constexpr uint16_t kMaxY = 511u;

inline uint16_t encode_x(uint16_t x, bool hires) {
    return static_cast<uint16_t>((hires ? x : static_cast<uint16_t>(x << 1u)) & 0x03FFu);
}

inline uint16_t encode_y(uint16_t y) {
    return static_cast<uint16_t>(y & 0x01FFu);
}

/* Register words of one window: WPSX, WPSY, WPEX, WPEY. */
inline void encode_rect(const Rect& r, bool hires, uint16_t out[4]) {
    out[0] = encode_x(r.x0, hires);
    out[1] = encode_y(r.y0);
    out[2] = encode_x(r.x1, hires);
    out[3] = encode_y(r.y1);
}

/* A rectangle the hardware can express. A start beyond its end is legal (the
 * whole screen is outside the window), so only the range is checked. */
inline bool rect_valid(const Rect& r) {
    return r.x0 <= kMaxX && r.x1 <= kMaxX && r.y0 <= kMaxY && r.y1 <= kMaxY;
}

/* Line window table words: start then end of each line, the same 10-bit
 * horizontal coding as the position registers. */
inline uint16_t encode_span_word(uint16_t x, bool hires) {
    return encode_x(x, hires);
}

/* LWTAnU/LWTAnL from a byte address: the table address is a word address
 * (byte / 2) with bit 0 fixed at 0, bit 15 of the upper word enabling it. */
inline void encode_line_table(uint32_t byte_address, uint16_t* upper, uint16_t* lower) {
    const uint32_t words = byte_address >> 1u;
    *upper = static_cast<uint16_t>(0x8000u | ((words >> 16u) & 0x0007u));
    *lower = static_cast<uint16_t>(words & 0xFFFEu);
}

inline bool line_table_valid(uint32_t byte_address) {
    return (byte_address & 3u) == 0u && byte_address < 0x80000u;
}

/* Bytes of a line window table for `lines` lines. */
inline uint32_t line_table_bytes(uint32_t lines) {
    return lines * 4u;
}

/* One screen's byte of the window control registers: bit 0 W0 area, 1 W0
 * enable, 2 W1 area, 3 W1 enable, 4 SW area, 5 SW enable, 7 logic. The sprite
 * window is not offered (it needs palette-only sprites), so SW stays off.
 * The register bits describe the transparent part (area bit 1 = outside the
 * window, logic bit 1 = AND), so both are the inverse of "shown". */
inline uint16_t screen_byte(const ScreenWindow& s) {
    uint16_t v = 0u;
    if (s.w0 != kAreaOff) v = static_cast<uint16_t>(v | 0x02u | (s.w0 == kAreaInside ? 0x01u : 0u));
    if (s.w1 != kAreaOff) v = static_cast<uint16_t>(v | 0x08u | (s.w1 == kAreaInside ? 0x04u : 0u));
    if (s.w0 != kAreaOff || s.w1 != kAreaOff) {
        if (!s.logic_and) v = static_cast<uint16_t>(v | 0x80u);
    }
    return v;
}

/* WCTLA..WCTLD. The rotation parameter window (low byte of WCTLD) is left
 * to the rotation code. */
inline void compose_wctl(const ScreenWindow screens[kScreenCount], uint16_t out[4]) {
    static const uint8_t reg[kScreenCount] = {0u, 0u, 1u, 1u, 2u, 2u, 3u};
    static const uint8_t shift[kScreenCount] = {0u, 8u, 0u, 8u, 0u, 8u, 8u};
    out[0] = out[1] = out[2] = out[3] = 0u;
    for (uint8_t i = 0u; i < kScreenCount; ++i) {
        out[reg[i]] = static_cast<uint16_t>(out[reg[i]] | (screen_byte(screens[i]) << shift[i]));
    }
}

/* ------------------------------------------------------------------ */
/* Mosaic                                                               */
/* ------------------------------------------------------------------ */

constexpr uint8_t kMosaicMin = 1u;
constexpr uint8_t kMosaicMax = 16u;

inline bool mosaic_valid(uint8_t width, uint8_t height) {
    return width >= kMosaicMin && width <= kMosaicMax && height >= kMosaicMin && height <= kMosaicMax;
}

/* MZCTL: vertical size (bits 15-12) and horizontal size (11-8), each size-1;
 * per-screen enables N0-N3 in bits 3-0 and RBG0 in bit 4. A mask of 0 leaves
 * the sizes in place with every screen off. */
inline uint16_t compose_mzctl(uint8_t width, uint8_t height, uint8_t screen_mask) {
    return static_cast<uint16_t>(((static_cast<uint16_t>(height - 1u) & 0x0Fu) << 12u) |
                                 ((static_cast<uint16_t>(width - 1u) & 0x0Fu) << 8u) |
                                 (screen_mask & 0x1Fu));
}

/* ------------------------------------------------------------------ */
/* Colour calculation per screen                                        */
/* ------------------------------------------------------------------ */

/* CCRNA..CCRLB order: N0 N1 | N2 N3 | R0 | LNCL BACK. The ratio register
 * selects top:second weights of (31 - r) : (r + 1), in 32nds: 0 is 31:1 and
 * 31 is 0:32. */
constexpr uint8_t kMaxRatio = 31u;

struct ScreenRatios {
    uint8_t nbg[4];
    uint8_t rbg0;
    uint8_t line_color;
    uint8_t back;
};

/* CCCTL bits 0-5: N0CCEN-N3CCEN, R0CCEN, LCCCEN. The sprite bit (6) and the
 * ratio-mode and add bits belong to the sprite colour calculation module. */
constexpr uint16_t kCcctlLayerMask = 0x003Fu;

inline uint16_t ratio_word(uint8_t low, uint8_t high) {
    return static_cast<uint16_t>((low & 0x1Fu) | ((high & 0x1Fu) << 8u));
}

inline void compose_ratios(const ScreenRatios& r, uint16_t out[4]) {
    out[0] = ratio_word(r.nbg[0], r.nbg[1]);
    out[1] = ratio_word(r.nbg[2], r.nbg[3]);
    out[2] = ratio_word(r.rbg0, 0u);
    out[3] = ratio_word(r.line_color, r.back);
}

/* Weights of the top and second images, in 32nds, that a ratio register
 * value selects (table of the colour calculation ratio bits). */
inline uint8_t top_weight(uint8_t ratio) {
    return static_cast<uint8_t>(31u - (ratio & 0x1Fu));
}

inline uint8_t second_weight(uint8_t ratio) {
    return static_cast<uint8_t>((ratio & 0x1Fu) + 1u);
}

/* Ratio register value whose second image weight is nearest to
 * `second_32nds` (1..32); values outside are clamped. */
inline uint8_t ratio_for_second(uint8_t second_32nds) {
    if (second_32nds < 1u) return 0u;
    if (second_32nds > 32u) return kMaxRatio;
    return static_cast<uint8_t>(second_32nds - 1u);
}

}  // namespace saturn::hal::vdp2::compose

#endif  // SATURN_HAL_VDP2_COMPOSE_LOGIC_HPP
