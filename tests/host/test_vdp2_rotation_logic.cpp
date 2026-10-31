#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/hal/vdp2/rotation_logic.hpp"

using namespace saturn::hal::vdp2;

/* RBG0 as the special-stage floor of sa2-saturn: 8bpp cells, names in A1,
 * characters in B1, the parameter table in A0. */
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
    l.coefficients = true;
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

    rot::Layer table = l;    /* the parameter table may not sit in an RBG0 bank */
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
    assert(r.ktctl == 0x0001u);                /* coefficient table, 2 words, mode 0 */
    assert(r.rptau == 0u && r.rptal == 0x6800u / 2u);
    assert(r.prir == 3u);

    rot::Layer off = floor_layer();
    off.enabled = false;
    const rot::Registers z = rot::compose(off);
    assert(z.bgon == 0u && z.ramctl_rdbs == 0u && z.ktctl == 0u);

    rot::Layer opaque = floor_layer();
    opaque.transparent = false;
    opaque.coefficient_mode = 3u;
    const rot::Registers o = rot::compose(opaque);
    assert(o.bgon == 0x1010u && o.ktctl == 0x000Du);
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

static void bitmap_floor_coexists_with_panorama_and_emission() {
    rot::Layer floor{};
    floor.enabled=true; floor.bitmap=true; floor.colors=nbg::Colors::C16;
    floor.bitmap_address=0x40000u; floor.char_banks=1u<<nbg::kBankB0;
    floor.table_address=0x3c000u; floor.priority=2u; floor.palette_supp=6u;
    floor.coefficients=true;
    assert(rot::validate(floor)==rot::Status::Ok);
    const auto r=rot::compose(floor);
    assert(r.ramctl_rdbs==0x30u && r.chctlb==0x0200u && r.mpofr==2u && r.bmpnb==6u);
    assert(rot::reserved(floor).mask[nbg::kBankB0]==0xffu);
    assert(rot::reserved(floor).mask[nbg::kBankB1]==0u);
    auto invalid=floor; invalid.table_address=0x40000u;
    assert(rot::validate(invalid)==rot::Status::BadBanks);
    invalid=floor; invalid.bitmap_address=0x50000u;
    assert(rot::validate(invalid)==rot::Status::BadBanks);
    invalid=floor; invalid.char_banks|=1u<<nbg::kBankB1;
    assert(rot::validate(invalid)==rot::Status::BadBanks);
    nbg::Layer layers[nbg::kLayerCount]{};
    layers[0].enabled=true;layers[0].bitmap=true;layers[0].colors=nbg::Colors::C256;
    layers[0].bitmap_size=2u;layers[0].priority=1u;
    layers[1].enabled=true;layers[1].colors=nbg::Colors::C32768;
    layers[1].pn_one_word=false;layers[1].pages_x=layers[1].pages_y=1u;
    layers[1].priority=7u;layers[1].char_banks=1u<<nbg::kBankB1;
    for (unsigned i=0;i<4;++i)layers[1].plane_address[i]=0x60000u;
    const auto reserved=rot::reserved(floor);
    const auto plan=nbg::plan_cycles(layers,true,true,&reserved);
    assert(plan.status==nbg::Status::Ok);
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

static void coefficients_encode_figure_6_7() {
    assert(rot::encode_coefficient(0x10000, true) == 0x80000000u);
    assert(rot::encode_coefficient(0x18000, false) == 0x00018000u);   /* 1.5 */
    assert(rot::encode_coefficient(-0x10000, false) == 0x00FF0000u);  /* -1: sign bit 7 of word 0 */
    assert(rot::encode_coefficient(0x7FFFFFFF, false) == 0x007FFFFFu);
    assert(rot::encode_coefficient(-0x7FFFFFFF, false) == 0x00800000u);
    assert(rot::kast_integer(0x46A00u) == ((0x46A00u / 4u) & 0xFFFFu));
    assert(rot::ktaof_for(0x46A00u) == 1u);   /* past 256 KiB: the offset register (table 6.3) */
    assert(rot::ktaof_for(0x6A00u) == 0u);
}

int main() {
    validation_follows_the_manual();
    registers_compose();
    nbgs_keep_off_the_rotation_banks();
    bitmap_floor_coexists_with_panorama_and_emission();
    params_encode_figure_6_2();
    coefficients_encode_figure_6_7();
    std::puts("test_vdp2_rotation_logic: ok");
    return 0;
}
