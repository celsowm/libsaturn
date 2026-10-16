#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/hal/vdp2/rotation_logic.hpp"

using namespace saturn::hal::vdp2;

/* RBG0 as the special-stage floor of sa2-saturn: 8bpp cells, names in A1,
 * characters in B1, the parameter region in A0. */
static rot::Layer floor_layer() {
    rot::Layer l{};
    l.enabled = true;
    l.colors = nbg::Colors::C256;
    l.pn_one_word = true;
    l.pages_x = l.pages_y = 1u;
    l.priority = 3u;
    l.transparent = true;
    l.over = 2u;
    l.name_banks = 1u << nbg::kBankA1;
    l.char_banks = 1u << nbg::kBankB1;
    for (uint8_t p = 0u; p < rot::kPlanes; ++p) l.plane_address[p] = 0x28000u;   /* a blank page */
    l.plane_address[0] = 0x20000u;
    l.plane_address[1] = 0x22000u;
    l.plane_address[4] = 0x24000u;
    l.plane_address[5] = 0x26000u;
    l.char_number_supp = 0x60000u / 0x8000u;
    l.table_address = 0x6800u;
    l.coefficients_a.enabled = true;
    return l;
}

static nbg::Layer text_layer(uint32_t names, uint8_t char_banks) {
    nbg::Layer l{};
    l.enabled = true;
    l.colors = nbg::Colors::C16;
    l.pn_one_word = true;
    l.pages_x = l.pages_y = 1u;
    l.priority = 1u;
    l.transparent = true;
    for (uint8_t p = 0u; p < 4u; ++p) l.plane_address[p] = names;
    l.char_banks = char_banks;
    return l;
}

static void validation_follows_the_manual() {
    rot::Layer l = floor_layer();
    assert(rot::validate(l) == rot::Status::Ok);

    rot::Layer shared = l;   /* one role per bank (p06_20) */
    shared.char_banks = shared.name_banks;
    assert(rot::validate(shared) == rot::Status::BadBanks);

    rot::Layer stray = l;    /* a plane outside the pattern-name banks is not read */
    stray.plane_address[15] = 0x00000u;
    assert(rot::validate(stray) == rot::Status::BadPlane);

    rot::Layer table = l;    /* the parameter region may not sit in an RBG0 bank */
    table.table_address = 0x60000u;
    assert(rot::validate(table) == rot::Status::BadBanks);

    rot::Layer tall = l;     /* PLSZ 10b (1x2 pages) is prohibited */
    tall.pages_x = 1u;
    tall.pages_y = 2u;
    assert(rot::validate(tall) == rot::Status::BadPlane);

    rot::Layer rgb24 = l;
    rgb24.colors = nbg::Colors::C16M;
    assert(rot::validate(rgb24) == rot::Status::BadFormat);
}

/* The A+B region and the parameter modes (p06_30 figure 6.4, p06_32, p06_35). */
static void parameter_region_rules() {
    const rot::Layer l = floor_layer();
    assert(rot::validate(l) == rot::Status::Ok);

    /* RPTA bit 6 is ignored and fixed to 0 for A: a region with byte bit 7
     * set would silently land 0x80 lower (p06_32). */
    rot::Layer hi = l;
    hi.table_address = 0x6880u;
    assert(rot::validate(hi) == rot::Status::BadPlane);

    /* Mode 3 (window switching) needs the rotation window control register,
     * which this manager does not model: refused, not misrendered. */
    rot::Layer window = l;
    window.parameter_mode = 3u;
    assert(rot::validate(window) == rot::Status::BadFormat);

    rot::Layer wild = l;
    wild.parameter_mode = 4u;
    assert(rot::validate(wild) == rot::Status::BadFormat);

    /* Modes that read B need the whole 0xE0 region; A alone needs 0x60. */
    rot::Layer near_top = l;
    near_top.char_banks = 1u << nbg::kBankB0;  /* free the top bank for the region */
    near_top.table_address = 0x7FF40u;          /* bit 7 clear, 4-byte aligned */
    near_top.parameter_mode = 0u;
    assert(rot::validate(near_top) == rot::Status::Ok);
    near_top.parameter_mode = 2u;
    assert(rot::validate(near_top) == rot::Status::BadPlane);   /* +0xE0 leaves VRAM */

    rot::Layer dual = l;
    dual.parameter_mode = 2u;
    dual.coefficients_b.enabled = true;
    dual.coefficients_b.mode = 1u;
    dual.coefficients_b.one_word = true;
    assert(rot::validate(dual) == rot::Status::Ok);

    dual.coefficients_b.mode = 4u;
    assert(rot::validate(dual) == rot::Status::BadFormat);
}

static void registers_compose() {
    const rot::Registers r = rot::compose(floor_layer());
    assert(r.bgon == 0x0010u);                 /* R0ON, transparency kept */
    assert(r.ramctl_rdbs == 0xC8u);            /* A1 names (10b, bits 3-2), B1 characters (11b, bits 7-6) */
    assert(r.chctlb == 0x1000u);               /* R0CHCN 256 colours */
    assert(r.pncr == (0x8000u | 12u));         /* 1-word names, characters from 0x60000 */
    assert(r.plsz == 0x0800u);                 /* 1x1 pages, outside the map transparent */
    assert(r.mp[0] == ((0x22000u / 0x2000u) << 8u | 0x20000u / 0x2000u));   /* planes A, B */
    assert(r.mp[2] == ((0x26000u / 0x2000u) << 8u | 0x24000u / 0x2000u));   /* planes E, F */
    assert(r.mp[7] == (0x14u << 8u | 0x14u));  /* planes O, P: the blank page */
    assert(r.mpofr == 0u);
    assert(r.rpmd == 0u);                      /* parameter set A (p06_35) */
    assert(r.ktctl == 0x0001u);                /* A: coefficient table, 2 words, mode 0 */
    assert(r.rptau == 0u && r.rptal == 0x6800u / 2u);
    assert(r.prir == 3u);

    rot::Layer off = floor_layer();
    off.enabled = false;
    const rot::Registers z = rot::compose(off);
    assert(z.bgon == 0u && z.ramctl_rdbs == 0u && z.ktctl == 0u && z.rpmd == 0u);

    rot::Layer opaque = floor_layer();
    opaque.transparent = false;
    opaque.coefficients_a.mode = 3u;
    const rot::Registers o = rot::compose(opaque);
    assert(o.bgon == 0x1010u && o.ktctl == 0x000Du);

    /* Both coefficient contracts compose (KTCTL halves, p06_42) with the
     * parameter mode and both coefficient offsets (KTAOF, p06_43). */
    rot::Layer dual = floor_layer();
    dual.parameter_mode = 2u;
    dual.coefficients_b.enabled = true;
    dual.coefficients_b.mode = 1u;
    dual.coefficients_b.one_word = true;
    dual.ktaof = 2u;
    dual.ktaof_b = 5u;
    const rot::Registers d = rot::compose(dual);
    assert(d.rpmd == 2u);
    assert(d.ktctl == 0x0701u);   /* A: 1|0<<1|0<<2 .. B: enable|1-word|mode 1 at bit 8 */
    assert(d.ktaof == 0x0502u);  /* A bits 2-0, B bits 10-8 */
}

static void nbgs_keep_off_the_rotation_banks() {
    const rot::Layer l = floor_layer();
    const nbg::Reserved reserved = rot::reserved(l);
    assert(reserved.mask[nbg::kBankA1] == 0xFFu && reserved.mask[nbg::kBankB1] == 0xFFu);
    assert(reserved.mask[nbg::kBankA0] == 0u && reserved.mask[nbg::kBankB0] == 0u);

    nbg::Layer layers[nbg::kLayerCount]{};
    layers[0] = text_layer(0x00000u, 1u << nbg::kBankB0);
    layers[1] = text_layer(0x02000u, 1u << nbg::kBankB0);
    nbg::Plan plan = nbg::plan_cycles(layers, true, true, &reserved);
    assert(plan.status == nbg::Status::Ok);
    /* nothing of the NBGs in the rotation banks */
    for (uint8_t t = 0u; t < nbg::kSlots; ++t) {
        assert(plan.cmd[nbg::kBankA1][t] == nbg::kCmdNone && plan.cmd[nbg::kBankB1][t] == nbg::kCmdNone);
    }

    layers[2] = text_layer(0x20000u, 1u << nbg::kBankB1);   /* NBG2 in the rotation banks */
    plan = nbg::plan_cycles(layers, true, true, &reserved);
    assert(plan.status == nbg::Status::NoCyclePattern);

    rot::Layer off = l;
    off.enabled = false;
    const nbg::Reserved none = rot::reserved(off);
    plan = nbg::plan_cycles(layers, true, true, &none);
    assert(plan.status == nbg::Status::Ok);
}

static void params_encode_figure_6_2() {
    rot::Params p{};
    p.xst = -(40 << 16);
    p.yst = (-64 << 16) | 0x8000;          /* -63.5 */
    p.dyst = 2 << 16;
    p.dx = 1 << 16;
    p.m[0] = 0x10000;                       /* A = 1 */
    p.m[1] = -0x8000;                       /* B = -0.5 */
    p.px = -3;
    p.cy = 100;
    p.mx = (700 << 16) | 0xC000;            /* 700.75 */
    p.kx = p.ky = 0x10000;
    p.kast = static_cast<uint32_t>(rot::kast_integer(0x6A00u)) << 16u;
    p.dkast = 0x10000;
    uint16_t w[48];
    rot::encode_params(p, w);
    assert(w[0] == 0x1FD8u && w[1] == 0u);           /* -40: 13 integer bits kept */
    assert(w[2] == 0x1FC0u && w[3] == 0x8000u);      /* -63.5 */
    assert(w[8] == 0x0002u && w[9] == 0u);           /* dYst 2 */
    assert(w[10] == 0x0001u);                        /* dX 1 */
    assert(w[14] == 0x0001u && w[15] == 0u);         /* A */
    assert(w[16] == 0x000Fu && w[17] == 0x8000u);    /* B -0.5: sign + 3 integer bits */
    assert(w[26] == 0x3FFDu && w[31] == 100u);       /* Px, Cy */
    assert(w[34] == 700u && w[35] == 0xC000u);       /* Mx */
    assert(w[38] == 0x0001u && w[39] == 0u);         /* kx */
    assert(w[42] == 0x6A00u / 4u && w[43] == 0u);    /* KAst integer: 4-byte units */
    assert(w[44] == 0x0001u && w[46] == 0u);         /* one coefficient per line, none per dot */
    /* fraction bits below the 10 the VDP2 has are dropped */
    p.m[0] = 0x10000 | 0x3F;
    rot::encode_params(p, w);
    assert(w[15] == 0u);
}

/* The whole region is A at +0, 0x20 ignored words, B at +0x80 (figure 6.4). */
static void params_ab_region_layout() {
    rot::Params a{};
    a.xst = 11 << 16;
    a.dkax = 3 << 16;
    rot::Params b{};
    b.xst = -(22 << 16);
    b.dkax = 4 << 16;
    uint16_t w[rot::kParamsABWords];
    rot::encode_params_ab(a, b, w);
    uint16_t solo[48];
    rot::encode_params(a, solo);
    for (uint32_t i = 0u; i < 48u; ++i) assert(w[i] == solo[i]);          /* A at +0 */
    for (uint32_t i = 48u; i < 64u; ++i) assert(w[i] == 0u);              /* the ignored gap */
    rot::encode_params(b, solo);
    for (uint32_t i = 0u; i < 48u; ++i) assert(w[64u + i] == solo[i]);    /* B at +0x80 */
    assert(w[64] == 0x1FEAu);                                            /* -22, sign in bit 12 */
    assert((rot::kParamBytes + rot::kParamGapBytes + rot::kParamBytes) == 0xE0u);
    assert(rot::kParamsABWords == 112u);
}

/* Sign and magnitude are separate fields in both entry sizes (figure 6.7):
 * a negative value is the sign bit plus the magnitude, never a two's
 * complement shift. */
static void coefficients_encode_figure_6_7() {
    assert(rot::encode_coefficient(0x10000, true) == 0x80010000u);  /* flag keeps the value */
    assert(rot::encode_coefficient(0x18000, false) == 0x00018000u); /* 1.5 */
    assert(rot::encode_coefficient(-0x10000, false) == 0x00810000u);/* -1: sign bit + integer 1 */
    assert(rot::encode_coefficient(-0x18000, false) == 0x00818000u);/* -1.5 */
    assert(rot::encode_coefficient(0x7FFFFFFF, false) == 0x007FFFFFu);   /* +127.99.. */
    assert(rot::encode_coefficient(-0x7FFFFFFF, false) == 0x00FFFFFFu);  /* -127.99.. */

    assert(rot::encode_coefficient_1word(0x10000, false) == 0x0400u);   /* 1.0 */
    assert(rot::encode_coefficient_1word(-0x10000, false) == 0x4400u);   /* -1.0: sign + 1 */
    assert(rot::encode_coefficient_1word(0x18000, false) == 0x0600u);    /* 1.5: int 1, frac 0x200 */
    assert(rot::encode_coefficient_1word(0x10000, true) == 0x8400u);     /* flagged, value kept */
    assert(rot::encode_coefficient_1word(0x00100000, false) == 0x3FFFu); /* 16.0 saturates */
    assert(rot::encode_coefficient_1word(-0x00100000, false) == 0x7FFFu);

    /* KAst counts entries: 4 bytes per 2-word entry, 2 per 1-word (p06_43). */
    assert(rot::kast_integer(0x46A00u) == ((0x46A00u / 4u) & 0xFFFFu));
    assert(rot::ktaof_for(0x46A00u) == 1u);   /* past 256 KiB: the offset register (table 6.3) */
    assert(rot::ktaof_for(0x6A00u) == 0u);
    assert(rot::kast_integer_1word(0x46A00u) == ((0x46A00u / 2u) & 0xFFFFu));
    assert(rot::ktaof_for_1word(0x7F000u) == 3u);   /* 2-word would say 1: the unit differs */
    assert(rot::ktaof_for(0x7F000u) == 1u);
}

int main() {
    validation_follows_the_manual();
    parameter_region_rules();
    registers_compose();
    nbgs_keep_off_the_rotation_banks();
    params_encode_figure_6_2();
    params_ab_region_layout();
    coefficients_encode_figure_6_7();
    std::puts("test_vdp2_rotation_logic: ok");
    return 0;
}
