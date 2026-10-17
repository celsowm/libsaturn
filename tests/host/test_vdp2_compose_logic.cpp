#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/hal/vdp2/compose_logic.hpp"

using namespace saturn::hal::vdp2::compose;

static void window_positions_drop_the_low_horizontal_bit() {
    uint16_t r[4];
    encode_rect({10u, 20u, 309u, 199u}, false, r);
    assert(r[0] == 20u && r[1] == 20u && r[2] == 618u && r[3] == 199u);
    encode_rect({10u, 20u, 309u, 199u}, true, r);
    assert(r[0] == 10u && r[2] == 309u);
    assert(encode_y(0x3FFu) == 0x1FFu);
    assert(rect_valid({0u, 0u, 1023u, 511u}) && !rect_valid({0u, 0u, 1024u, 0u}) && !rect_valid({0u, 0u, 0u, 512u}));
    /* a start beyond the end is legal: the whole screen is outside */
    assert(rect_valid({200u, 100u, 10u, 5u}));
}

static void line_window_table_is_a_word_address_with_enable() {
    uint16_t u = 0u, l = 0u;
    encode_line_table(0x71234u & ~3u, &u, &l);
    /* byte 0x71234 = word 0x3891A: upper bits 18-16 = 3, lower keeps bits 15-1 */
    assert(u == (0x8000u | 3u) && l == 0x891Au);
    encode_line_table(0x00000u, &u, &l);
    assert(u == 0x8000u && l == 0u);
    assert(line_table_valid(0x10000u) && !line_table_valid(0x10002u) && !line_table_valid(0x80000u));
    assert(line_table_bytes(224u) == 896u);
    assert(encode_span_word(319u, false) == 638u);
}

static void window_control_bytes_follow_the_registers() {
    ScreenWindow s[kScreenCount] = {};
    s[kNbg0] = {kAreaInside, kAreaOff, false, kAreaOff};
    s[kNbg1] = {kAreaOutside, kAreaInside, true, kAreaOff};
    s[kNbg3] = {kAreaOff, kAreaOutside, false, kAreaOff};
    s[kSprite] = {kAreaInside, kAreaInside, false, kAreaOff};
    s[kColorCalc] = {kAreaOff, kAreaInside, true, kAreaOff};
    uint16_t w[4];
    compose_wctl(s, w);
    /* The registers describe the transparent area: shown inside = transparent
     * outside (area bit 1), shown in both (AND) = transparent in either (OR, bit 7 = 0).
     * WCTLA: NBG0 low byte, NBG1 high byte. W0 enable bit 1, area bit 0; W1 enable 3, area 2. */
    assert((w[0] & 0xFFu) == (0x02u | 0x01u | 0x80u));      /* one window, shown "either": hardware AND bit */
    assert(((w[0] >> 8u) & 0xFFu) == (0x02u | 0x08u | 0x04u));   /* NBG1: w0 outside -> area 0; w1 inside -> area 1; AND -> 0 */
    /* WCTLB: NBG2 (nothing) low, NBG3 high (W1 outside -> hardware inside, OR shown -> bit 7) */
    assert((w[1] & 0xFFu) == 0u);
    assert(((w[1] >> 8u) & 0xFFu) == (0x08u | 0x80u));
    /* WCTLC: RBG0 low, sprite high (both inside, OR shown -> 0x80) */
    assert((w[2] & 0xFFu) == 0u && ((w[2] >> 8u) & 0xFFu) == (0x02u | 0x01u | 0x08u | 0x04u | 0x80u));
    /* WCTLD: rotation parameter low byte untouched, colour calc high */
    assert((w[3] & 0xFFu) == 0u && ((w[3] >> 8u) & 0xFFu) == (0x08u | 0x04u));
}

static void mosaic_register_holds_size_minus_one() {
    assert(mosaic_valid(1u, 1u) && mosaic_valid(16u, 16u) && !mosaic_valid(0u, 4u) && !mosaic_valid(4u, 17u));
    assert(compose_mzctl(1u, 1u, 0u) == 0u);
    /* vertical size bits 15-12, horizontal 11-8, layers bits 4-0 */
    assert(compose_mzctl(8u, 4u, 0x03u) == ((3u << 12u) | (7u << 8u) | 0x03u));
    assert(compose_mzctl(16u, 16u, 0xFFu) == (0xFF00u | 0x1Fu));
}

static void ratios_pack_by_screen() {
    ScreenRatios r{};
    r.nbg[0] = 1u;
    r.nbg[1] = 2u;
    r.nbg[2] = 3u;
    r.nbg[3] = 4u;
    r.rbg0 = 5u;
    r.line_color = 6u;
    r.back = 7u;
    uint16_t o[4];
    compose_ratios(r, o);
    assert(o[0] == 0x0201u && o[1] == 0x0403u && o[2] == 0x0005u && o[3] == 0x0706u);
    r.nbg[0] = 0xFFu;   /* only five bits exist */
    compose_ratios(r, o);
    assert(o[0] == 0x021Fu);
    /* table: 0 -> 31:1, 16 -> 15:17, 31 -> 0:32 */
    assert(top_weight(0u) == 31u && second_weight(0u) == 1u);
    assert(top_weight(16u) == 15u && second_weight(16u) == 17u);
    assert(top_weight(31u) == 0u && second_weight(31u) == 32u);
    assert(ratio_for_second(16u) == 15u && ratio_for_second(0u) == 0u && ratio_for_second(99u) == 31u);
    assert(kCcctlLayerMask == 0x3Fu);
}

static void palette_sprites_mix_by_colour_msb() {
    /* type 1; SPCCCS = 3 (colour MSB) in bits 13-12; SPCCEN is CCCTL bit 6, outside the layer mask */
    assert(palette_sprite_spctl(false) == 0x0001u);
    assert(palette_sprite_spctl(true) == 0x3001u);
    assert((kCcctlSprite & kCcctlLayerMask) == 0u && kCcctlSprite == 0x0040u);
    assert(kCcctlAdd == 0x0100u && (kCcctlAdd & (kCcctlLayerMask | kCcctlSprite)) == 0u);
}

static void palette_window_sprites_use_type_3() {
    /* type 3 + SPWINEN (bit 4), colour condition unchanged */
    assert(palette_sprite_spctl(false, true) == 0x0013u);
    assert(palette_sprite_spctl(true, true) == 0x3013u);
    /* SW: enable bit 5, area bit 4 set when shown inside (transparent outside) */
    ScreenWindow in{};
    in.sw = kAreaInside;
    in.logic_and = true;
    assert(screen_byte(in) == 0x30u);
    ScreenWindow out{};
    out.sw = kAreaOutside;
    assert(screen_byte(out) == 0xA0u); /* shown where either: transparent where all (AND, bit 7) */
    /* the sprite screen's own SW: SPSWE (13) with SPSWA (12) clear, inside transparent */
    assert(kWctlcSpriteHidesWindow == 0x2000u);
}

static void cram_banks_pack_into_craofa_and_craofb() {
    CramBanks b{0u, 0u};
    b = with_cram_bank(b, 0u, 2u); /* NBG0 bits 2-0 */
    b = with_cram_bank(b, 3u, 7u); /* NBG3 bits 14-12 */
    assert(b.craofa == 0x7002u && b.craofb == 0u);
    b = with_cram_bank(b, 4u, 5u); /* RBG0 bits 2-0 */
    b = with_cram_bank(b, 5u, 3u); /* sprites bits 6-4 */
    assert(b.craofb == 0x0035u);
    b = with_cram_bank(b, 0u, 0u); /* replaces, keeps the others */
    b = with_cram_bank(b, 5u, 1u);
    assert(b.craofa == 0x7000u && b.craofb == 0x0015u);
    b = with_cram_bank(b, 1u, 9u); /* 3 bits only */
    assert(b.craofa == 0x7010u);
    b = with_cram_bank(b, 6u, 4u); /* not a palette screen: unchanged */
    assert(b.craofa == 0x7010u && b.craofb == 0x0015u);
}

static void hires_sprites_light_all_or_high_codes() {
    /* off: plain type C, PRISA untouched */
    assert(hires_sprite_spctl(0x0606u, kHiresSpriteCcOff) == 0x000Cu);
    assert(hires_sprite_prisa(0x0606u, kHiresSpriteCcOff) == 0x0606u);
    /* all: priority >= 0 (SPCCCS 10, SPCCN 0), PRISA untouched */
    assert(hires_sprite_spctl(0x0606u, kHiresSpriteCcAll) == 0x200Cu);
    assert(hires_sprite_prisa(0x0606u, kHiresSpriteCcAll) == 0x0606u);
    /* high codes: 0-127 one below, 128-255 keep p; condition == S1PRIN */
    assert(hires_sprite_prisa(0x0606u, kHiresSpriteCcHighCodes) == 0x0605u);
    assert(hires_sprite_spctl(0x0606u, kHiresSpriteCcHighCodes) == 0x160Cu);
    assert(hires_sprite_prisa(0x0707u, kHiresSpriteCcHighCodes) == 0x0706u);
    assert(hires_sprite_spctl(0x0707u, kHiresSpriteCcHighCodes) == 0x170Cu);
    /* p < 2 cannot go below (0 is transparent): the lit codes go above */
    assert(hires_sprite_prisa(0x0101u, kHiresSpriteCcHighCodes) == 0x0201u);
    assert(hires_sprite_spctl(0x0101u, kHiresSpriteCcHighCodes) == 0x120Cu);
    assert(hires_sprite_prisa(0x0000u, kHiresSpriteCcHighCodes) == 0x0201u);
    assert(kLnclenSprite == 0x0020u);
}

static void ccctl_extras_encode_and_exclude_each_other() {
    /* the bits: EXCCEN 10, BOKEN 15, BOKN 14-12 (manual p12_14) */
    assert(kCcctlExccen == 0x0400u);
    assert((kCcctlExccen & (kCcctlLayerMask | kCcctlSprite | kCcctlAdd | kCcctlBoken)) == 0u);
    assert(kCcctlBoken == 0x8000u && kCcctlBoknShift == 12u);
    assert(((kCcctlBoken | (7u << kCcctlBoknShift)) >> kCcctlBoknShift) == 0xFu);
    /* BOKN 3 and 7 are setting-prohibited (p12_14) */
    assert(blur_screen_valid(0u) && blur_screen_valid(2u) && blur_screen_valid(6u));
    assert(!blur_screen_valid(3u) && !blur_screen_valid(7u) && !blur_screen_valid(8u));
    /* encoding */
    CcctlExtras e{};
    assert(extras_word(e) == 0u);
    assert(extras_set_extended(e, true, false) == kExtrasOk);
    assert(e.extended && extras_word(e) == kCcctlExccen);
    /* blur on clears extended; BOKEN carries the BOKN screen */
    assert(extras_set_blur(e, 5u, true, false, 0u, false) == kExtrasOk);
    assert(extras_word(e) == static_cast<uint16_t>(kCcctlBoken | (5u << kCcctlBoknShift)));
    assert(!e.extended && e.blur && e.blur_screen == 5u);
    /* blur off leaves nothing behind */
    assert(extras_set_blur(e, 5u, false, false, 0u, false) == kExtrasOk);
    assert(extras_word(e) == 0u);
    /* extended on clears a pending blur */
    assert(extras_set_blur(e, 1u, true, false, 0u, false) == kExtrasOk);
    assert(extras_set_extended(e, true, false) == kExtrasOk);
    assert(e.extended && !e.blur && extras_word(e) == kCcctlExccen);
}

static void ccctl_extras_refuse_what_the_hardware_refuses() {
    CcctlExtras e{};
    /* EXCCEN: normal TV mode only - not hi-res, not dedicated monitor (p12_12) */
    assert(extras_set_extended(e, true, true) == kExtrasHires);
    assert(!e.extended && extras_word(e) == 0u);
    /* BOKEN: the setting-prohibited BOKN numbers come first */
    assert(extras_set_blur(e, 3u, true, false, 0u, false) == kExtrasBlurScreen);
    assert(extras_set_blur(e, 7u, true, false, 0u, false) == kExtrasBlurScreen);
    assert(extras_set_blur(e, 8u, true, false, 0u, false) == kExtrasBlurScreen);
    /* then the mode rules: normal TV mode, CRAM mode 0, no line colour (p12_13) */
    assert(extras_set_blur(e, 2u, true, true, 0u, false) == kExtrasHires);
    assert(extras_set_blur(e, 2u, true, false, 1u, false) == kExtrasCramMode);
    assert(extras_set_blur(e, 2u, true, false, 0u, true) == kExtrasLineColor);
    assert(!e.blur && extras_word(e) == 0u);
    /* a refusal never mutates a live configuration */
    assert(extras_set_extended(e, true, false) == kExtrasOk);
    assert(extras_set_blur(e, 2u, true, true, 1u, true) == kExtrasHires);
    assert(e.extended && !e.blur && extras_word(e) == kCcctlExccen);
    /* disable is accepted once the screen number is legal, whatever the modes */
    assert(extras_set_blur(e, 2u, false, true, 1u, true) == kExtrasOk);
    assert(!e.blur && e.extended);
    assert(extras_set_extended(e, false, true) == kExtrasOk);
    assert(extras_word(e) == 0u);
}

int main() {
    window_positions_drop_the_low_horizontal_bit();
    line_window_table_is_a_word_address_with_enable();
    window_control_bytes_follow_the_registers();
    mosaic_register_holds_size_minus_one();
    ratios_pack_by_screen();
    palette_sprites_mix_by_colour_msb();
    palette_window_sprites_use_type_3();
    cram_banks_pack_into_craofa_and_craofb();
    hires_sprites_light_all_or_high_codes();
    ccctl_extras_encode_and_exclude_each_other();
    ccctl_extras_refuse_what_the_hardware_refuses();
    std::puts("PASS: test_vdp2_compose_logic.cpp");
    return 0;
}
