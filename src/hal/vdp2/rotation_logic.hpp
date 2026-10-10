#ifndef SATURN_HAL_VDP2_ROTATION_LOGIC_HPP
#define SATURN_HAL_VDP2_ROTATION_LOGIC_HPP

#include <stdint.h>

#include "src/hal/vdp2/nbg_logic.hpp"

/* Pure policy of RBG0 in cell format, the rotation scroll screen the layer
 * manager runs next to NBG0..NBG3 (VDP2 manual chapter 6). RBG0 takes whole
 * VRAM banks (RAMCTL RDBS): the VDP2 ignores those banks' cycle patterns, so
 * the NBGs must fetch from the others. Rotation parameter A only. No register
 * access, so host tests cover it. */
namespace saturn::hal::vdp2::rot {

constexpr uint8_t kPlanes = 16u;            /* the map is 4x4 planes, A..P */
constexpr uint32_t kParamBytes = 0x60u;     /* the part of a parameter table the VDP2 reads */

/* RDBS bank roles (p06_20). */
enum Role : uint16_t { kRoleNone = 0u, kRoleCoefficient = 1u, kRoleNames = 2u, kRoleChars = 3u };

struct Layer {
    bool enabled;
    nbg::Colors colors;        /* 16, 256, 2048 or 32768 */
    bool char_2x2;
    bool pn_one_word;
    uint8_t pages_x, pages_y;  /* plane size: 1x1, 2x1 or 2x2 pages */
    uint8_t priority;          /* 0..7 */
    bool transparent;          /* dot code 0 is transparent */
    uint8_t over;              /* RAOVR: 0 repeat, 1 over pattern name, 2 transparent, 3 512x512 */
    uint16_t over_pattern_name;
    uint8_t palette_supp;      /* PNCR auxiliary palette number bits 6-4 (3) */
    uint8_t char_number_supp;  /* PNCR auxiliary character number bits (5) */
    uint8_t name_banks;        /* banks given to the pattern name tables */
    uint8_t char_banks;        /* banks given to the character data */
    uint32_t plane_address[kPlanes];
    /* Rotation parameter A: the table (byte address, a multiple of 4, outside
     * the banks above) and the coefficient table settings (KTCTL). */
    uint32_t table_address;
    bool coefficients;         /* read a per-line table of 2-word coefficients */
    uint8_t coefficient_mode;  /* 0 kx and ky, 1 kx, 2 ky, 3 Xp */
    uint8_t ktaof;             /* coefficient table address offset (bits 18-16 of KAst units) */
};

enum class Status : uint8_t { Ok, BadFormat, BadPlane, BadBanks };

inline Status validate(const Layer& l) {
    if (l.priority > 7u || l.over > 3u || l.coefficient_mode > 3u || l.ktaof > 7u) return Status::BadFormat;
    if (l.colors == nbg::Colors::C16M) return Status::BadFormat;
    if (l.pages_x < 1u || l.pages_x > 2u || l.pages_y < 1u || l.pages_y > 2u) return Status::BadPlane;
    if (l.pages_x == 1u && l.pages_y == 2u) return Status::BadPlane;   /* PLSZ 10b is prohibited */
    if (l.name_banks == 0u || l.char_banks == 0u || ((l.name_banks | l.char_banks) & ~nbg::kAllBanks) != 0u ||
        (l.name_banks & l.char_banks) != 0u) {
        return Status::BadBanks;
    }
    const uint32_t size = nbg::plane_bytes(l.pn_one_word, l.char_2x2, l.pages_x, l.pages_y);
    uint32_t offset_bits = 0xFFFFFFFFu;
    for (uint8_t p = 0u; p < kPlanes; ++p) {
        const uint32_t a = l.plane_address[p];
        if (a % size != 0u || a + size > nbg::kVramBytes) return Status::BadPlane;
        /* every plane's names must sit in a pattern-name bank */
        if ((nbg::bank_mask_of(a, size, true, true) & ~l.name_banks) != 0u) return Status::BadPlane;
        const uint32_t high = nbg::map_value(a, size) >> 6u;
        if (offset_bits == 0xFFFFFFFFu) offset_bits = high;
        else if (offset_bits != high) return Status::BadPlane;
    }
    if (offset_bits > 7u) return Status::BadPlane;
    if ((l.table_address & 3u) != 0u || l.table_address + kParamBytes > nbg::kVramBytes) return Status::BadPlane;
    if ((nbg::bank_mask_of(l.table_address, kParamBytes, true, true) & (l.name_banks | l.char_banks)) != 0u) {
        return Status::BadBanks;
    }
    return Status::Ok;
}

/* RAMCTL bits 7-0 for the bank roles. */
inline uint16_t rdbs(const Layer& l) {
    uint16_t v = 0u;
    for (uint8_t b = 0u; b < nbg::kBankCount; ++b) {
        uint16_t role = kRoleNone;
        if ((l.name_banks & (1u << b)) != 0u) role = kRoleNames;
        if ((l.char_banks & (1u << b)) != 0u) role = kRoleChars;
        v = static_cast<uint16_t>(v | (role << (2u * b)));
    }
    return v;
}

struct Registers {
    uint16_t bgon;      /* R0ON (bit 4), R0TPON (bit 12) */
    uint16_t ramctl_rdbs;
    uint16_t chctlb;    /* bits 14-8 */
    uint16_t pncr;
    uint16_t plsz;      /* bits 11-8 */
    uint16_t mpofr;     /* bits 2-0 */
    uint16_t mp[8];     /* MPABRA .. MPOPRA */
    uint16_t ovpnra;
    uint16_t ktctl;     /* bits 4-0 */
    uint16_t ktaof;     /* bits 2-0 */
    uint16_t rptau, rptal;
    uint16_t prir;      /* bits 2-0 */
};

inline Registers compose(const Layer& l) {
    Registers r{};
    if (!l.enabled) return r;
    r.bgon = static_cast<uint16_t>(0x0010u | (l.transparent ? 0u : 0x1000u));
    r.ramctl_rdbs = rdbs(l);
    r.chctlb = static_cast<uint16_t>(((static_cast<uint16_t>(l.colors) & 7u) << 12u) | (l.char_2x2 ? 0x0100u : 0u));
    r.pncr = static_cast<uint16_t>(((l.pn_one_word ? 1u : 0u) << 15u) | ((l.palette_supp & 7u) << 5u) |
                                   (l.char_number_supp & 0x1Fu));
    const uint16_t plsz = static_cast<uint16_t>(l.pages_y == 2u ? 3u : (l.pages_x == 2u ? 1u : 0u));
    r.plsz = static_cast<uint16_t>((plsz << 8u) | ((l.over & 3u) << 10u));
    const uint32_t size = nbg::plane_bytes(l.pn_one_word, l.char_2x2, l.pages_x, l.pages_y);
    for (uint8_t p = 0u; p < kPlanes; p += 2u) {
        const uint32_t v0 = nbg::map_value(l.plane_address[p], size);
        const uint32_t v1 = nbg::map_value(l.plane_address[p + 1u], size);
        r.mp[p / 2u] = static_cast<uint16_t>((v0 & 0x3Fu) | ((v1 & 0x3Fu) << 8u));
    }
    r.mpofr = static_cast<uint16_t>((nbg::map_value(l.plane_address[0], size) >> 6u) & 7u);
    r.ovpnra = l.over_pattern_name;
    if (l.coefficients) {
        r.ktctl = static_cast<uint16_t>(0x0001u | ((l.coefficient_mode & 3u) << 2u));
    }
    r.ktaof = static_cast<uint16_t>(l.ktaof & 7u);
    /* RPTA holds the table's word address (bits 18-1; bit 6 reads as 0 for parameter A). */
    r.rptau = static_cast<uint16_t>(((l.table_address >> 1u) >> 16u) & 7u);
    r.rptal = static_cast<uint16_t>((l.table_address >> 1u) & 0xFFFEu);
    r.prir = static_cast<uint16_t>(l.priority & 7u);
    return r;
}

/* NBG cycle slots the rotation layer's banks take: all of them (the VDP2
 * ignores those banks' patterns, so no NBG may count on one). */
inline nbg::Reserved reserved(const Layer& l) {
    nbg::Reserved r{};
    if (!l.enabled) return r;
    for (uint8_t b = 0u; b < nbg::kBankCount; ++b) {
        if (((l.name_banks | l.char_banks) & (1u << b)) == 0u) continue;
        r.mask[b] = 0xFFu;
        for (uint8_t t = 0u; t < nbg::kSlots; ++t) r.cmd[b][t] = nbg::kCmdNone;
    }
    return r;
}

/* ---- rotation parameter table (p06_30 figure 6.2) ----------------------- */

struct Params {
    int32_t xst, yst, zst;     /* 16.16 */
    int32_t dxst, dyst;        /* 16.16, per line */
    int32_t dx, dy;            /* 16.16, per dot */
    int32_t m[6];              /* matrix A..F, 16.16 */
    int16_t px, py, pz, cx, cy, cz;
    int32_t mx, my;            /* 16.16 */
    int32_t kx, ky;            /* 16.16 */
    uint32_t kast;             /* coefficient table start, 16.16 in table entries (low 16 integer bits) */
    int32_t dkast, dkax;       /* 16.16 entries per line, per dot */
};

/* A 32-bit field: the integer part in the upper word and the fraction left
 * aligned in the lower one; `mask` keeps the bits the field has. */
inline void put32(uint16_t* out, uint32_t word, int32_t value, uint32_t mask) {
    const uint32_t v = static_cast<uint32_t>(value) & mask;
    out[word] = static_cast<uint16_t>(v >> 16u);
    out[word + 1u] = static_cast<uint16_t>(v & 0xFFFFu);
}

/* The first 48 words of the table. */
inline void encode_params(const Params& p, uint16_t out[48]) {
    for (uint32_t i = 0u; i < 48u; ++i) out[i] = 0u;
    put32(out, 0u, p.xst, 0x1FFFFFC0u);   /* sign + 12 integer bits . 10 fraction bits */
    put32(out, 2u, p.yst, 0x1FFFFFC0u);
    put32(out, 4u, p.zst, 0x1FFFFFC0u);
    put32(out, 6u, p.dxst, 0x0007FFC0u);  /* sign + 2 . 10 */
    put32(out, 8u, p.dyst, 0x0007FFC0u);
    put32(out, 10u, p.dx, 0x0007FFC0u);
    put32(out, 12u, p.dy, 0x0007FFC0u);
    for (uint32_t i = 0u; i < 6u; ++i) put32(out, 14u + 2u * i, p.m[i], 0x000FFFC0u);   /* sign + 3 . 10 */
    out[26] = static_cast<uint16_t>(p.px & 0x3FFF);  /* sign + 13 integer bits */
    out[27] = static_cast<uint16_t>(p.py & 0x3FFF);
    out[28] = static_cast<uint16_t>(p.pz & 0x3FFF);
    out[30] = static_cast<uint16_t>(p.cx & 0x3FFF);
    out[31] = static_cast<uint16_t>(p.cy & 0x3FFF);
    out[32] = static_cast<uint16_t>(p.cz & 0x3FFF);
    put32(out, 34u, p.mx, 0x3FFFFFC0u);   /* sign + 13 . 10 */
    put32(out, 36u, p.my, 0x3FFFFFC0u);
    put32(out, 38u, p.kx, 0x00FFFFFFu);   /* sign + 7 . 16 */
    put32(out, 40u, p.ky, 0x00FFFFFFu);
    put32(out, 42u, static_cast<int32_t>(p.kast), static_cast<int32_t>(0xFFFFFFC0u));   /* 16 . 10 */
    put32(out, 44u, p.dkast, 0x03FFFFC0u);   /* sign + 9 . 10 */
    put32(out, 46u, p.dkax, 0x03FFFFC0u);
}

/* KAst counts 2-word coefficients (4 bytes, table 6.3): its integer part holds
 * the low 16 bits of byte_address / 4 and KTAOF the 3 above them. */
inline uint32_t kast_integer(uint32_t byte_address) { return (byte_address / 4u) & 0xFFFFu; }
inline uint8_t ktaof_for(uint32_t byte_address) { return static_cast<uint8_t>(((byte_address / 4u) >> 16u) & 7u); }

/* A 2-word coefficient (figure 6.7): word 0 bit 15 transparent, bits 14-8 line
 * colour data (unused, 0), bit 7 sign, bits 6-0 integer; word 1 the fraction. `value` is 16.16; out of range
 * values saturate. */
inline uint32_t encode_coefficient(int32_t value, bool transparent) {
    if (transparent) return 0x80000000u;
    if (value > 0x007FFFFF) value = 0x007FFFFF;
    if (value < -0x00800000) value = -0x00800000;
    return static_cast<uint32_t>(value) & 0x00FFFFFFu;
}

}  // namespace saturn::hal::vdp2::rot

#endif
