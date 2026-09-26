#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "src/hal/vdp2/nbg_logic.hpp"

using namespace saturn::hal::vdp2::nbg;

static Layer cell(Colors colors, uint32_t plane_base, uint8_t char_banks, uint8_t priority = 1u) {
    Layer l{};
    l.enabled = true;
    l.colors = colors;
    l.pn_one_word = true;
    l.pages_x = l.pages_y = 1u;
    l.priority = priority;
    l.transparent = true;
    for (uint8_t p = 0u; p < 4u; ++p) l.plane_address[p] = plane_base;
    l.char_banks = char_banks;
    return l;
}

static void geometry_follows_the_tables() {
    assert(page_bytes(true, false) == 0x2000u && page_bytes(true, true) == 0x800u);
    assert(page_bytes(false, false) == 0x4000u && page_bytes(false, true) == 0x1000u);
    assert(plane_bytes(true, false, 2u, 2u) == 0x8000u);
    assert(bitmap_bytes(0u, Colors::C256) == 128u * 1024u);   /* table 4.11 (2) */
    assert(bitmap_bytes(3u, Colors::C256) == 512u * 1024u);   /* (14) */
    assert(bitmap_bytes(1u, Colors::C16) == 128u * 1024u);    /* (5) */
    assert(bitmap_bytes(0u, Colors::C16M) == 512u * 1024u);   /* (4) */
    assert(reads_for_characters(Colors::C256, 0u) == 2u);     /* table 3.3 */
    assert(reads_for_characters(Colors::C256, 1u) == 4u);
    assert(reads_for_characters(Colors::C16, 2u) == 4u);
    assert(reads_for_characters(Colors::C16M, 0u) == 8u);
    assert(reads_for_pattern_names(2u) == 4u);
}

static void banks_follow_the_vram_split() {
    assert(bank_of(0x00000u, true, true) == kBankA0 && bank_of(0x1FFFFu, true, true) == kBankA0);
    assert(bank_of(0x20000u, true, true) == kBankA1 && bank_of(0x3FFFFu, true, true) == kBankA1);
    assert(bank_of(0x40000u, true, true) == kBankB0 && bank_of(0x60000u, true, true) == kBankB1);
    /* Undivided halves use the first bank's registers. */
    assert(bank_of(0x30000u, false, true) == kBankA0);
    assert(bank_of(0x70000u, true, false) == kBankB0);
    assert(bank_mask_of(0x1F000u, 0x2000u, true, true) == 0x03u);       /* straddles A0/A1 */
    assert(bank_mask_of(0x00000u, 0x80000u, true, true) == 0x0Fu);
    assert(bank_mask_of(0x60000u, 0x8000u, true, true) == 0x08u);
}

static void registers_match_the_manual_and_the_legacy_nbg0() {
    /* The legacy NBG0 setup: 256 colours, 1x1 characters, 1-word names, all
     * four planes at map value 0x3B (byte 0x76000), auxiliary character 0xC. */
    Layer l[kLayerCount]{};
    l[0] = cell(Colors::C256, 0x76000u, kAllBanks, 1u);
    l[0].char_number_supp = 0x0Cu;
    const Registers r = compose(l);
    assert(r.bgon_on == 0x1u);
    assert(r.chctla == 0x0010u);                     /* N0CHCN = 001 (256 colours), 1x1, cells */
    assert(r.pncn[0] == 0x800Cu);                    /* 1-word, auxiliary character 0xC */
    assert(r.mpab[0] == 0x3B3Bu && r.mpcd[0] == 0x3B3Bu);
    assert(r.mpofn == 0u && r.plsz == 0u);
    assert(r.prina == 0x0001u && r.prinb == 0u);     /* NBG0 priority 1 in bits 2-0 */
    assert(r.bgon_opaque_zero == 0u);                /* dot 0 transparent */
}

static void every_layer_lands_in_its_own_register_fields() {
    Layer l[kLayerCount]{};
    l[0] = cell(Colors::C2048, 0x00000u, kAllBanks, 5u);
    l[1] = cell(Colors::C32768, 0x02000u, kAllBanks, 4u);
    l[1].char_2x2 = true;
    l[1].pages_x = 2u;
    l[2] = cell(Colors::C256, 0x08000u, kAllBanks, 3u);
    l[3] = cell(Colors::C16, 0x0A000u, kAllBanks, 2u);
    l[3].transparent = false;
    for (int i = 0; i < 4; ++i) l[1].plane_address[i] = 0x02000u * 0u + 0x4000u * static_cast<uint32_t>(i);
    Registers r = compose(l);
    assert(r.bgon_on == 0xFu);
    assert(r.bgon_opaque_zero == 0x0800u);           /* only NBG3 shows dot 0 */
    /* NBG0: char colour 2 at bits 6-4; NBG1: colour 3 at bits 13-12, 2x2 chars at bit 8. */
    assert((r.chctla & 0x0070u) == 0x0020u);
    assert(((r.chctla >> 12u) & 3u) == 3u && ((r.chctla >> 8u) & 1u) == 1u);
    /* NBG2: 256 colours = bit 1; NBG3: 16 colours. */
    assert((r.chctlb & 0x0003u) == 0x0002u && (r.chctlb & 0x0030u) == 0u);
    assert(r.prina == (5u | (4u << 8u)) && r.prinb == (3u | (2u << 8u)));
    assert(((r.plsz >> 2u) & 3u) == 1u);             /* NBG1: 2 pages by 1 */
    assert((r.pncn[1] >> 15u) == 1u);
    /* NBG1 planes A..D at 0, 0x4000, 0x8000, 0xC000 in 0x2000-byte... 2x2 chars,
     * 2x1 pages, 1 word: plane = 0x1000 bytes; values are address / 0x1000. */
    (void)r;
}

static void map_offset_bits_are_shared_by_the_four_planes() {
    /* 1-word 2x2 characters, one page: 0x800-byte planes, map values up to
     * 0x100; bits 8-6 come from the offset register and must agree. */
    Layer l[kLayerCount]{};
    l[0] = cell(Colors::C16, 0u, kAllBanks);
    l[0].char_2x2 = true;
    for (int i = 0; i < 4; ++i) l[0].plane_address[i] = 0x20000u + 0x800u * static_cast<uint32_t>(i);
    assert(validate(0u, l[0], true, true) == Status::Ok);
    const Registers r = compose(l);
    /* 0x20000 / 0x800 = 0x40: bit 6 set -> offset 1, register bits = 0,1,2,3. */
    assert(r.mpofn == 1u);
    assert(r.mpab[0] == ((0u) | (1u << 8u)) && r.mpcd[0] == ((2u) | (3u << 8u)));
    l[0].plane_address[3] = 0x30000u - 0x800u * 0u;      /* another 64-plane window */
    l[0].plane_address[3] = 0x10000u;                     /* map value 0x20: offset 0 */
    assert(validate(0u, l[0], true, true) == Status::BadPlane);
    l[0].plane_address[3] = 0x20003u;                     /* misaligned */
    assert(validate(0u, l[0], true, true) == Status::BadPlane);
    l[0].plane_address[3] = 0x7F800u + 0x800u;            /* past VRAM */
    assert(validate(0u, l[0], true, true) == Status::BadPlane);
}

static void bitmap_layers_use_the_map_offset_and_palette() {
    Layer l[kLayerCount]{};
    l[0].enabled = true;
    l[0].bitmap = true;
    l[0].colors = Colors::C256;
    l[0].bitmap_size = 1u;                 /* 512x512 */
    l[0].plane_address[0] = 0x40000u;      /* 128 KiB units: 2 */
    l[0].palette = 5u;
    l[0].priority = 2u;
    assert(validate(0u, l[0], true, true) == Status::Ok);
    Registers r = compose(l);
    assert((r.chctla & 0x000Eu) == ((1u << 1u) | (1u << 2u)));   /* bitmap enable, size 01 */
    assert(r.bmpna == 5u && r.mpofn == 2u);
    l[0].plane_address[0] = 0x41000u;      /* not on a 128 KiB boundary */
    assert(validate(0u, l[0], true, true) == Status::BadPlane);
    /* NBG2/NBG3 have no bitmap format and only 16 or 256 colours. */
    Layer bad = cell(Colors::C2048, 0u, kAllBanks);
    assert(validate(2u, bad, true, true) == Status::BadFormat);
    bad = cell(Colors::C16, 0u, kAllBanks);
    bad.bitmap = true;
    assert(validate(3u, bad, true, true) == Status::BadFormat);
    /* No 2x2-page plane at 1/4 reduction (manual 4.7). */
    Layer wide = cell(Colors::C16, 0u, kAllBanks);
    wide.pages_x = wide.pages_y = 2u;
    wide.reduction = 2u;
    for (int i = 0; i < 4; ++i) wide.plane_address[i] = 0u;
    assert(validate(0u, wide, true, true) == Status::BadReduction);
}

static bool same_pattern(const Plan& a, const Plan& b) {
    return std::memcmp(a.cmd, b.cmd, sizeof a.cmd) == 0;
}

static void one_layer_reproduces_the_legacy_nbg0_pattern() {
    Layer l[kLayerCount]{};
    l[0] = cell(Colors::C256, 0x76000u, 0x08u /* B1 */, 1u);
    const Plan plan = plan_cycles(l, true, true);
    assert(plan.status == Status::Ok);
    /* Bank B1: PN at T0, character reads at T1 and T2, nothing at T3, CPU after. */
    assert(plan.cyc[6] == 0x044Fu && plan.cyc[7] == 0xEEEEu);
    /* The other banks hold nothing: no access first, CPU later. */
    assert(plan.cyc[0] == 0xFEEEu && plan.cyc[1] == 0xEEEEu);
    assert(check_pattern(l, true, true, plan.cmd));
}

static void the_manual_example_is_found_and_checks_out() {
    /* Figure 3.8: NBG0 256 colours at 1/2 reduction (names in A0, characters
     * in B0 and B1); NBG1 256 colours 1x with vertical cell scroll (names in A0
     * and A1, table in A0, characters in B0/B1); NBG3 16 colours 1x (names in
     * A1, characters in A1 and B0). */
    Layer l[kLayerCount]{};
    l[0] = cell(Colors::C256, 0x00000u, 0x0Cu);
    l[0].reduction = 1u;
    l[1] = cell(Colors::C256, 0x00000u, 0x0Cu);
    l[1].plane_address[0] = 0x02000u;
    l[1].plane_address[1] = 0x22000u;    /* names in A0 and A1 */
    l[1].plane_address[2] = 0x02000u;
    l[1].plane_address[3] = 0x22000u;
    /* Planes A..D must share the map offset: 0x2000 units -> values 1 and 0x11. */
    l[1].vcs = true;
    l[1].vcs_address = 0x0F000u;
    l[3] = cell(Colors::C16, 0x24000u, 0x06u);        /* names in A1; characters A1, B0 */
    const Plan plan = plan_cycles(l, true, true);
    assert(plan.status == Status::Ok);
    assert(check_pattern(l, true, true, plan.cmd));
    /* Names of NBG0 are read twice (1/2), its characters four times. */
    int pn0 = 0, cg0 = 0;
    for (uint32_t t = 0u; t < kSlots; ++t) {
        pn0 += plan.cmd[kBankA0][t] == kCmdPn0;
        cg0 += plan.cmd[kBankB0][t] == kCmdCg0;
    }
    assert(pn0 == 2 && cg0 == 4);
}

static void the_checker_catches_broken_patterns() {
    Layer l[kLayerCount]{};
    l[0] = cell(Colors::C16, 0x02000u, 0x04u);         /* names in A0, characters in B0 */
    Plan plan = plan_cycles(l, true, true);
    assert(plan.status == Status::Ok && check_pattern(l, true, true, plan.cmd));
    uint8_t broken[kBankCount][kSlots];
    std::memcpy(broken, plan.cmd, sizeof broken);
    for (uint32_t t = 0u; t < kSlots; ++t) {
        if (broken[kBankB0][t] == kCmdCg0) broken[kBankB0][t] = kCmdNone;    /* drop the read */
    }
    assert(!check_pattern(l, true, true, broken));
    std::memcpy(broken, plan.cmd, sizeof broken);
    for (uint32_t t = 0u; t < kSlots; ++t) {
        if (broken[kBankA0][t] == kCmdPn0) { broken[kBankA0][t] = kCmdNone; broken[kBankA0][7] = kCmdPn0; }
    }
    /* Names at T7 allow characters only at T3 (table 3.4): the found pattern
     * had them earlier, so moving the name read breaks the rule. */
    assert(!check_pattern(l, true, true, broken));
}

static void impossible_combinations_are_refused() {
    /* 16.7 M colours need eight reads; a second such layer in the same banks
     * leaves no room within eight timings. */
    Layer l[kLayerCount]{};
    l[0] = cell(Colors::C16M, 0x00000u, 0x04u);
    l[1] = cell(Colors::C16M, 0x02000u, 0x04u);
    assert(plan_cycles(l, true, true).status == Status::NoCyclePattern);
    /* Even one layer cannot exceed a bank: 1/4 reduction of 256 colours is 8
     * character reads plus four name reads, more than eight timings when the
     * names share the characters' bank. */
    Layer m[kLayerCount]{};
    m[0] = cell(Colors::C256, 0x40000u, 0x04u);
    m[0].reduction = 2u;
    assert(plan_cycles(m, true, true).status == Status::NoCyclePattern);
    /* Names in both A0 and B0 are not allowed. */
    Layer n[kLayerCount]{};
    n[0] = cell(Colors::C16, 0x00000u, 0x0Cu);
    n[0].plane_address[1] = 0x40000u;
    n[0].plane_address[2] = 0x00000u;
    n[0].plane_address[3] = 0x40000u;
    assert(plan_cycles(n, true, true).status != Status::Ok);
}

static void four_light_layers_share_the_banks() {
    Layer l[kLayerCount]{};
    l[0] = cell(Colors::C16, 0x00000u, 0x04u);        /* names A0, chars B0 */
    l[1] = cell(Colors::C16, 0x02000u, 0x08u);        /* names A0, chars B1 */
    l[2] = cell(Colors::C16, 0x20000u, 0x04u);        /* names A1, chars B0 */
    l[3] = cell(Colors::C16, 0x22000u, 0x08u);        /* names A1, chars B1 */
    const Plan plan = plan_cycles(l, true, true);
    assert(plan.status == Status::Ok);
    assert(check_pattern(l, true, true, plan.cmd));
    /* Deterministic: the same inputs give the same pattern. */
    assert(same_pattern(plan, plan_cycles(l, true, true)));
}

static void reserved_slots_are_left_alone() {
    Layer l[kLayerCount]{};
    l[0] = cell(Colors::C16, 0x00000u, 0x08u);
    Reserved keep{};
    keep.mask[kBankB1] = 0x07u;                       /* another layer owns B1 T0-T2 */
    keep.cmd[kBankB1][0] = kCmdPn1;
    keep.cmd[kBankB1][1] = kCmdCg1;
    keep.cmd[kBankB1][2] = kCmdCg1;
    const Plan plan = plan_cycles(l, true, true, &keep);
    assert(plan.status == Status::Ok);
    assert(plan.cmd[kBankB1][0] == kCmdPn1 && plan.cmd[kBankB1][1] == kCmdCg1);
    assert(plan.cmd[kBankB1][2] == kCmdCg1);
    /* The new layer's character read moved past the reserved timings. */
    bool found = false;
    for (uint32_t t = 3u; t < kSlots; ++t) found = found || plan.cmd[kBankB1][t] == kCmdCg0;
    assert(found);
}

static void free_slots_follow_the_cpu_access_rule() {
    Layer l[kLayerCount]{};
    l[0] = cell(Colors::C16, 0x00000u, 0x01u);        /* names and chars both in A0 */
    const Plan plan = plan_cycles(l, true, true);
    assert(plan.status == Status::Ok);
    /* B0/B1 are wholly free and VRAM-B is divided: no access first, then CPU. */
    assert(plan.cmd[kBankB0][0] == kCmdNone && plan.cmd[kBankB1][0] == kCmdNone);
    for (uint32_t t = 1u; t < kSlots; ++t) {
        assert(plan.cmd[kBankB0][t] == kCmdCpu && plan.cmd[kBankB1][t] == kCmdCpu);
    }
    /* An undivided VRAM-A: free timings of the first bank are CPU access. */
    const Plan undivided = plan_cycles(l, false, false);
    assert(undivided.status == Status::Ok);
    assert(undivided.cmd[kBankB0][0] == kCmdCpu);
}

static void vertical_cell_scroll_reads_are_ordered() {
    Layer l[kLayerCount]{};
    l[0] = cell(Colors::C16, 0x00000u, 0x04u);
    l[0].vcs = true;
    l[0].vcs_address = 0x1F000u;
    l[1] = cell(Colors::C16, 0x02000u, 0x08u);
    l[1].vcs = true;
    l[1].vcs_address = 0x1F000u;   /* one table, entries alternating */
    const Plan plan = plan_cycles(l, true, true);
    assert(plan.status == Status::Ok);
    int t0 = -1, t1 = -1;
    for (uint32_t t = 0u; t < kSlots; ++t) {
        if (plan.cmd[kBankA0][t] == kCmdVcs0) t0 = static_cast<int>(t);
        if (plan.cmd[kBankA0][t] == kCmdVcs1) t1 = static_cast<int>(t);
    }
    assert(t0 >= 0 && t0 <= 1 && t1 >= 0 && t1 <= 2 && t0 < t1);
    assert(check_pattern(l, true, true, plan.cmd));
    const Registers r = compose(l);
    assert(r.scrctl == 0x0101u);
}

static void reduction_sets_zmctl() {
    Layer l[kLayerCount]{};
    l[0] = cell(Colors::C16, 0x00000u, 0x04u);
    l[0].reduction = 1u;
    l[1] = cell(Colors::C16, 0x02000u, 0x08u);
    l[1].reduction = 2u;
    const Registers r = compose(l);
    assert(r.zmctl == (0x0001u | (2u << 8u)));
}

static void line_scroll_registers_and_table_geometry() {
    Layer l[kLayerCount]{};
    l[0] = cell(Colors::C16, 0x00000u, 0x04u);
    l[0].ls_h = true;
    l[0].ls_zoom = true;
    l[0].ls_interval = 2u;               /* every 4 lines */
    l[0].ls_address = 0x1E000u;
    l[1] = cell(Colors::C16, 0x02000u, 0x08u);
    l[1].ls_h = l[1].ls_v = true;
    l[1].ls_address = 0x71234u & ~3u;
    l[1].vcs = true;
    l[1].vcs_address = 0x1F000u;
    assert(validate(0u, l[0], true, true) == Status::Ok && validate(1u, l[1], true, true) == Status::Ok);
    const Registers r = compose(l);
    /* NBG0: LSCX bit 1, LZMX bit 3, interval 2 at bits 5-4. NBG1: VCSC, LSCX, LSCY. */
    assert((r.scrctl & 0x00FFu) == (0x02u | 0x08u | (2u << 4u)));
    assert(((r.scrctl >> 8u) & 0xFFu) == (0x01u | 0x02u | 0x04u));
    /* Word addresses: 0x1E000 bytes is word 0xF000. */
    assert(r.lsta_u[0] == 0u && r.lsta_l[0] == 0xF000u);
    assert(r.lsta_u[1] == 3u && r.lsta_l[1] == 0x891Au);   /* 0x71234 bytes = word 0x3891A */
    assert(r.vcsta_u == 0u && r.vcsta_l == 0xF800u);    /* 0x1F000 bytes = word 0xF800 */
    /* Table sizes: two words per enabled field, one entry per interval. */
    assert(line_scroll_entry_words(true, false, false) == 2u);
    assert(line_scroll_entry_words(true, true, true) == 6u);
    assert(line_scroll_entries(224u, 0u) == 224u && line_scroll_entries(224u, 2u) == 56u);
    assert(line_scroll_entries(225u, 3u) == 29u);
    /* Values: integer part 11 bits, 8 fraction bits in bits 15-8. */
    uint16_t i = 0u, f = 0u;
    encode_scroll(0x00058000, &i, &f);
    assert(i == 5u && f == 0x8000u);
    encode_scroll(-0x00010000, &i, &f);
    assert(i == 0x07FFu && f == 0u);
    encode_increment(0x00018000u, &i, &f);
    assert(i == 1u && f == 0x8000u);
    assert(vcs_word_offset(3u, 0u, false) == 6u);
    assert(vcs_word_offset(3u, 0u, true) == 12u && vcs_word_offset(3u, 1u, true) == 14u);
    /* NBG2/NBG3 have no line scroll; the two layers share one cell scroll table. */
    Layer bad = cell(Colors::C16, 0u, kAllBanks);
    bad.ls_h = true;
    assert(validate(2u, bad, true, true) == Status::BadFormat);
    bad = cell(Colors::C16, 0u, kAllBanks);
    bad.ls_h = true;
    bad.ls_address = 2u;                 /* not long-word aligned */
    assert(validate(0u, bad, true, true) == Status::BadPlane);
    l[0].vcs = true;
    l[0].vcs_address = 0x1F100u;
    assert(plan_cycles(l, true, true).status == Status::BadPlane);
}

int main() {
    geometry_follows_the_tables();
    banks_follow_the_vram_split();
    registers_match_the_manual_and_the_legacy_nbg0();
    every_layer_lands_in_its_own_register_fields();
    map_offset_bits_are_shared_by_the_four_planes();
    bitmap_layers_use_the_map_offset_and_palette();
    one_layer_reproduces_the_legacy_nbg0_pattern();
    the_manual_example_is_found_and_checks_out();
    the_checker_catches_broken_patterns();
    impossible_combinations_are_refused();
    four_light_layers_share_the_banks();
    reserved_slots_are_left_alone();
    free_slots_follow_the_cpu_access_rule();
    vertical_cell_scroll_reads_are_ordered();
    reduction_sets_zmctl();
    line_scroll_registers_and_table_geometry();
    std::printf("PASS: test_vdp2_nbg_logic.cpp\n");
    return 0;
}
