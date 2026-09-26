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
    s[kNbg0] = {kAreaInside, kAreaOff, false};
    s[kNbg1] = {kAreaOutside, kAreaInside, true};
    s[kNbg3] = {kAreaOff, kAreaOutside, false};
    s[kSprite] = {kAreaInside, kAreaInside, false};
    s[kColorCalc] = {kAreaOff, kAreaInside, true};
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

int main() {
    window_positions_drop_the_low_horizontal_bit();
    line_window_table_is_a_word_address_with_enable();
    window_control_bytes_follow_the_registers();
    mosaic_register_holds_size_minus_one();
    ratios_pack_by_screen();
    std::puts("PASS: test_vdp2_compose_logic.cpp");
    return 0;
}
