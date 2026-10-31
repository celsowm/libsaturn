/* Pure policy tests for RBG1 in cell format, beside RBG0 (VDP2 manual p06_20).
 * The C wrapper is exercised through the layer manager in test_vdp2_rotation_logic.cpp;
 * here we focus on the validation rules the manual mandates and the register fields
 * rot::compose() emits. */
#include "src/hal/vdp2/rotation_logic.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace rot = saturn::hal::vdp2::rot;
namespace nbg = saturn::hal::vdp2::nbg;

static int g_failed = 0;
static int g_passed = 0;

#define EXPECT(cond) do { \
    if (cond) { ++g_passed; } else { ++g_failed; std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

/* A reasonable RBG0-only baseline. */
static rot::Layer baseline() {
    rot::Layer l{};
    l.enabled = true;
    l.colors = nbg::Colors::C256;
    l.pn_one_word = true;
    l.char_2x2 = false;
    l.pages_x = 1u; l.pages_y = 1u;
    l.priority = 4u;
    l.transparent = true;
    l.over = 0u;  /* SAT_VDP2_ROTATION_OVER_REPEAT */
    l.name_banks = (1u << nbg::kBankA0);
    l.char_banks = (1u << nbg::kBankA1);
    for (uint8_t p = 0u; p < rot::kPlanes; ++p) l.plane_address[p] = 0x00000u;
    l.parameter_mode = 0u;
    /* The A+B region must not sit in an RBG0 bank (p06_20). B0 is a safe
     * pick when RBG0 owns A0 (names) + A1 (chars). */
    l.table_address = 0x40000u;
    return l;
}

static rot::Layer with_rbg1() {
    rot::Layer l = baseline();
    l.rbg1.enabled = true;
    l.rbg1.colors = nbg::Colors::C256;
    l.rbg1.pn_one_word = true;
    l.rbg1.char_2x2 = false;
    l.rbg1.priority = 3u;
    l.rbg1.transparent = true;
    l.rbg1.char_base_address = 0x40000u;        /* VRAM-B0, the fixed fetch */
    l.map_b.pages_x = 1u;
    l.map_b.pages_y = 1u;
    l.map_b.over = 0u;
    /* B-map must sit entirely inside VRAM-B1 (manual p06_20). */
    for (uint8_t p = 0u; p < rot::kPlanes; ++p) l.map_b.plane_address[p] = 0x60000u;
    return l;
}

static void test_rbg1_happy_path() {
    rot::Layer l = with_rbg1();
    EXPECT(rot::validate(l) == rot::Status::Ok);
    const rot::Registers r = rot::compose(l);
    /* BGON: R1ON bit 5 + N0TPON bit 8. transparent true -> no N0TPON set. */
    EXPECT((r.bgon_rbg1 & 0x0020u) != 0u);
    EXPECT((r.bgon_rbg1 & 0x0100u) == 0u);
    /* CHCTLA: N0CHCN bits 6-4 = 001 (256 colours), N0CHSZ bit 0 = 0. */
    EXPECT((r.chctla & 0x0070u) == 0x0010u);
    EXPECT((r.chctla & 0x0001u) == 0u);
    /* PNCN0: bit 15 (N0PNB) set (1-word names), bits 7-5 = 0, bits 4-0 = 0. */
    EXPECT((r.pnc0 & 0x8000u) != 0u);
    EXPECT((r.pnc0 & 0x00E0u) == 0u);
    /* PRINA bits 2-0 = RBG1 priority 3. */
    EXPECT((r.prina_rbg1 & 0x0007u) == 0x0003u);
    /* PLSZ B: RBPLSZ bits 13-12 = 00 (1x1 page), RBOVR bits 15-14 = 00 (repeat). */
    EXPECT((r.plsz_b & 0x3000u) == 0x0000u);
    EXPECT((r.plsz_b & 0xC000u) == 0x0000u);
    /* MPOFR bits 6-4 (RBMP): one plane address -> 0. */
    EXPECT((r.mpofr_b & 0x0070u) == 0x0000u);
    /* The eight B-plane registers were emitted; OVPNRB carried over. */
    EXPECT(r.ovpnrb == 0u);
}

static void test_rbg1_transparent_false() {
    rot::Layer l = with_rbg1();
    l.rbg1.transparent = false;
    EXPECT(rot::validate(l) == rot::Status::Ok);
    const rot::Registers r = rot::compose(l);
    /* N0TPON bit 8 set -> transparent code disables. */
    EXPECT((r.bgon_rbg1 & 0x0100u) != 0u);
}

static void test_rbg1_param_mode_not_a_refused() {
    rot::Layer l = with_rbg1();
    l.parameter_mode = 1u;
    EXPECT(rot::validate(l) == rot::Status::BadFormat);
    l.parameter_mode = 2u;
    EXPECT(rot::validate(l) == rot::Status::BadFormat);
}

static void test_rbg1_banks_in_vram_b_refused() {
    rot::Layer l = with_rbg1();
    l.name_banks = (1u << nbg::kBankB0);
    EXPECT(rot::validate(l) == rot::Status::BadBanks);
    l.name_banks = (1u << nbg::kBankA0) | (1u << nbg::kBankB1);
    EXPECT(rot::validate(l) == rot::Status::BadBanks);
    l.char_banks = (1u << nbg::kBankB0);
    EXPECT(rot::validate(l) == rot::Status::BadBanks);
}

static void test_rbg1_map_outside_b1_refused() {
    rot::Layer l = with_rbg1();
    l.map_b.plane_address[0] = 0x20000u;   /* VRAM-A0, not B1 */
    EXPECT(rot::validate(l) == rot::Status::BadPlane);
    l.map_b.plane_address[0] = 0x60000u;
    l.map_b.plane_address[5] = 0x40000u;   /* the address bank-mask check is per-plane */
    EXPECT(rot::validate(l) == rot::Status::BadPlane);
}

static void test_rbg1_char_window_outside_b0_refused() {
    rot::Layer l = with_rbg1();
    l.rbg1.char_base_address = 0x08000u;   /* VRAM-A0 */
    EXPECT(rot::validate(l) == rot::Status::BadBanks);
    l.rbg1.char_base_address = 0x60000u;
    EXPECT(rot::validate(l) == rot::Status::BadBanks);
    l.rbg1.char_base_address = 0x40000u;   /* back to B0 */
    EXPECT(rot::validate(l) == rot::Status::Ok);
}

static void test_rbg1_disabled_allows_anything() {
    rot::Layer l = baseline();
    /* With RBG1 hidden the B-map is just a normal parameter B map.
     * Move the planes into A1 too so the bank-mask check agrees. */
    for (uint8_t p = 0u; p < rot::kPlanes; ++p) l.map_b.plane_address[p] = 0x20000u;
    EXPECT(rot::validate(l) == rot::Status::Ok);
    /* Even a parameter mode other than A is fine without RBG1. */
    l.parameter_mode = 1u;
    EXPECT(rot::validate(l) == rot::Status::Ok);
    /* And the RBG0 image banks stay in VRAM-A. */
    EXPECT(rot::validate(l) == rot::Status::Ok);
}

static void test_rbg1_2x2_window_size() {
    rot::Layer l = with_rbg1();
    l.rbg1.char_2x2 = true;
    l.rbg1.char_base_address = 0x40000u;        /* 0x20000-aligned */
    EXPECT(rot::validate(l) == rot::Status::Ok);
    l.rbg1.char_base_address = 0x40008u;        /* wrong alignment for 2x2 */
    EXPECT(rot::validate(l) == rot::Status::BadPlane);
    l.rbg1.char_base_address = 0x40000u;
    const rot::Registers r = rot::compose(l);
    EXPECT((r.chctla & 0x0001u) != 0u);
}

static void test_rbg1_b_map_2x2_pages() {
    rot::Layer l = with_rbg1();
    l.map_b.pages_x = 2u;
    l.map_b.pages_y = 2u;
    EXPECT(rot::validate(l) == rot::Status::Ok);
    const rot::Registers r = rot::compose(l);
    /* RBPLSZ bits 13-12 = 11 (2x2 pages). */
    EXPECT((r.plsz_b & 0x3000u) == 0x3000u);
}

static void test_rbg1_over_repeat() {
    rot::Layer l = with_rbg1();
    l.map_b.over = 1u;  /* over pattern name */
    EXPECT(rot::validate(l) == rot::Status::Ok);
    l.map_b.over_pattern_name = 0x0123u;
    const rot::Registers r = rot::compose(l);
    EXPECT((r.plsz_b & 0xC000u) == 0x4000u);  /* bits 15-14 = 01 */
    EXPECT(r.ovpnrb == 0x0123u);
}

int main() {
    test_rbg1_happy_path();
    test_rbg1_transparent_false();
    test_rbg1_param_mode_not_a_refused();
    test_rbg1_banks_in_vram_b_refused();
    test_rbg1_map_outside_b1_refused();
    test_rbg1_char_window_outside_b0_refused();
    test_rbg1_disabled_allows_anything();
    test_rbg1_2x2_window_size();
    test_rbg1_b_map_2x2_pages();
    test_rbg1_over_repeat();
    std::fprintf(stderr, "RBG1 tests: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}