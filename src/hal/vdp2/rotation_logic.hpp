#ifndef SATURN_HAL_VDP2_ROTATION_LOGIC_HPP
#define SATURN_HAL_VDP2_ROTATION_LOGIC_HPP

#include <stdint.h>

#include "src/hal/vdp2/nbg_logic.hpp"

/* Pure policy of RBG0 in cell format, the rotation scroll screen the layer
 * manager runs next to NBG0..NBG3 (VDP2 manual chapter 6). RBG0 takes whole
 * VRAM banks (RAMCTL RDBS): the VDP2 ignores those banks' cycle patterns, so
 * the NBGs must fetch from the others. Both rotation parameter sets A and B
 * and their coefficient tables are modeled; per-dot coefficients are not
 * (they need a whole rotation bank or Color RAM, p06_40, which this manager
 * does not claim). No register access, so host tests cover it.
 *
 * Manual grounding (docs/sega_saturn_hardware/hard/vdp2/hon/):
 *   - p06_30 (6.3): one parameter set is a 0x60-byte table; A and B are one
 *     region, A at +0, 0x20 ignored bytes, B at +0x80 (figure 6.4).
 *   - p06_32: RPTA is one address register for the region; its bit 6 is
 *     ignored on write and fixed to 0 for A, 1 for B - so the region's byte
 *     address must have bit 7 clear and the VDP2 always reads B at
 *     region | 0x80.
 *   - p06_34 (6.3.5), p06_35: RPMD selects the parameter mode: 0 A only,
 *     1 B only, 2 per-line A/B by the coefficient's most significant bit,
 *     3 window switching (RPAWIN). Mode 3 needs the rotation window
 *     control register, which this manager does not model: validate refuses
 *     it rather than silently misrender.
 *   - p06_40 (6.4), p06_42: KTCTL carries one coefficient contract per set:
 *     enable (RAKTE/RBKTE), data size 2 or 1 words (RAKDBS/RBKDBS), mode
 *     kx ky / kx / ky / Xp (RAKMD/RBKMD) and line-colour composition
 *     (RAKLCE/RBKLCE, not modeled here). Figure 6.7 fixes the entry
 *     formats; the entry's bit 15 hides the line (modes 0 and 1) or selects
 *     the B image for that line (mode 2).
 *   - p06_43: KTAOF holds A's coefficient address offset in bits 2-0 and
 *     B's in bits 10-8. */
namespace saturn::hal::vdp2::rot {

constexpr uint8_t kPlanes = 16u;            /* the map is 4x4 planes, A..P */
constexpr uint32_t kParamBytes = 0x60u;     /* the part of one set the VDP2 reads */
constexpr uint32_t kParamGapBytes = 0x20u;  /* ignored bytes between A and B */
constexpr uint32_t kParamsABBytes = kParamBytes + kParamGapBytes + kParamBytes;  /* 0xE0 */
constexpr uint32_t kParamsABWords = kParamsABBytes / 2u;                          /* 112 */

/* RDBS bank roles (p06_20). */
enum Role : uint16_t { kRoleNone = 0u, kRoleCoefficient = 1u, kRoleNames = 2u, kRoleChars = 3u };

/* The coefficient table contract of one parameter set (KTCTL bits 4-0 for
 * A, 12-8 for B, p06_42). */
struct Coefficients {
    bool enabled;      /* RxKTE: read a per-line coefficient table */
    uint8_t mode;      /* 0 kx and ky, 1 kx, 2 ky, 3 Xp (figure 6.4 of 6.3) */
    bool one_word;     /* RxKDBS: 1-word entries instead of 2-word */
};

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
    /* The rotation parameter region (byte address): bit 7 must be clear and
     * the whole A+B region must fit in VRAM when B is read (p06_30, p06_32).
     * sat_vdp2_rotation_write_params fills it. */
    uint32_t table_address;
    uint8_t parameter_mode;    /* RPMD 0..2; 3 (window) is refused */
    Coefficients coefficients_a;
    Coefficients coefficients_b;
    uint8_t ktaof;             /* KTAOF bits 2-0: A's coefficient table offset */
    uint8_t ktaof_b;           /* KTAOF bits 10-8: B's */
};

enum class Status : uint8_t { Ok, BadFormat, BadPlane, BadBanks };

inline Status validate(const Layer& l) {
    if (l.priority > 7u || l.over > 3u || l.ktaof > 7u || l.ktaof_b > 7u) return Status::BadFormat;
    if (l.parameter_mode > 3u) return Status::BadFormat;
    if (l.parameter_mode == 3u) return Status::BadFormat;   /* RPAWIN not modeled (p06_34) */
    if (l.colors == nbg::Colors::C16M) return Status::BadFormat;
    if (l.coefficients_a.mode > 3u || l.coefficients_b.mode > 3u) return Status::BadFormat;
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
    /* RPTA bit 6 is ignored and fixed to 0 for A (p06_32): a region whose
     * byte address has bit 7 set would silently land 0x80 lower. */
    if ((l.table_address & 3u) != 0u || (l.table_address & 0x80u) != 0u) return Status::BadPlane;
    /* B is read at region | 0x80 whenever the mode uses it (figure 6.4). */
    const uint32_t table_span = l.parameter_mode == 0u ? kParamBytes : kParamsABBytes;
    if (l.table_address + table_span > nbg::kVramBytes) return Status::BadPlane;
    if ((nbg::bank_mask_of(l.table_address, table_span, true, true) & (l.name_banks | l.char_banks)) != 0u) {
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
    uint16_t rpmd;      /* bits 1-0 */
    uint16_t ktctl;     /* bits 12-0 */
    uint16_t ktaof;     /* bits 10-8 and 2-0 */
    uint16_t rptau, rptal;
    uint16_t prir;      /* bits 2-0 */
};

inline uint16_t ktctl_of(const Coefficients& c, uint8_t bank_shift) {
    uint16_t v = 0u;
    if (c.enabled) v = static_cast<uint16_t>(v | (1u << bank_shift));            /* RxKTE */
    if (c.one_word) v = static_cast<uint16_t>(v | (1u << (bank_shift + 1u)));     /* RxKDBS */
    v = static_cast<uint16_t>(v | ((c.mode & 3u) << (bank_shift + 2u)));         /* RxKMD */
    return v;
}

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
    r.rpmd = static_cast<uint16_t>(l.parameter_mode & 3u);
    r.ktctl = static_cast<uint16_t>(ktctl_of(l.coefficients_a, 0u) | ktctl_of(l.coefficients_b, 8u));
    r.ktaof = static_cast<uint16_t>((l.ktaof & 7u) | ((l.ktaof_b & 7u) << 8u));
    /* RPTA holds the region's word address (bits 18-1; bit 6 is ignored,
     * fixed to 0 for A and 1 for B, so the region's bit 7 is clear). */
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

/* ---- rotation parameter table (p06_30 figures 6.2, 6.3, 6.4) ----------- */

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

/* One 0x60-byte parameter set, the first 48 words (figure 6.3). */
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

/* The whole A+B region (figure 6.4): A at +0, 0x20 ignored words zeroed,
 * B at +0x80. Both sets are always written together so B can never be stale. */
inline void encode_params_ab(const Params& a, const Params& b, uint16_t out[kParamsABWords]) {
    encode_params(a, out);
    for (uint32_t i = kParamBytes / 2u; i < kParamsABBytes / 2u - kParamBytes / 2u; ++i) {
        out[i] = 0u;
    }
    encode_params(b, out + kParamsABBytes / 2u - kParamBytes / 2u);
}

/* KAst counts coefficient entries, so the unit is the entry size (table 6.3,
 * p06_43): 4 bytes for 2-word data, 2 bytes for 1-word data. The integer
 * part holds the low 16 bits of the entry index and KTAOF the 3 above them. */
inline uint32_t kast_integer(uint32_t byte_address) { return (byte_address / 4u) & 0xFFFFu; }
inline uint8_t ktaof_for(uint32_t byte_address) { return static_cast<uint8_t>(((byte_address / 4u) >> 16u) & 7u); }
inline uint32_t kast_integer_1word(uint32_t byte_address) { return (byte_address / 2u) & 0xFFFFu; }
inline uint8_t ktaof_for_1word(uint32_t byte_address) { return static_cast<uint8_t>(((byte_address / 2u) >> 16u) & 7u); }

/* A 2-word coefficient (figure 6.7): word 0 bit 15 the flag bit (the line is
 * hidden, or in parameter mode 2 the B image is shown for that line), bits
 * 14-8 line colour data (unused here, 0), bit 7 the SIGN, bits 6-0 the
 * integer magnitude; word 1 the 16-bit fraction. Sign and magnitude are
 * separate fields: `value` is 16.16 and saturates at just under +-128.
 * The value always survives: in mode 2 the entry selects the image and
 * still provides that line's coefficient. */
inline uint32_t encode_coefficient(int32_t value, bool flag) {
    const bool negative = value < 0;
    uint32_t mag = negative ? static_cast<uint32_t>(-(value + 1)) + 1u
                            : static_cast<uint32_t>(value);
    if (mag > 0x007FFFFFu) mag = 0x007FFFFFu;
    return (flag ? 0x80000000u : 0u)
         | (negative ? 0x00800000u : 0u)
         | ((mag & 0x007F0000u) << 0u)
         | (mag & 0x0000FFFFu);
}

/* A 1-word coefficient (figure 6.7): bit 15 the flag bit, bit 14 the sign,
 * bits 13-10 the integer magnitude, bits 9-0 the fraction. `value` is 16.16
 * and saturates at just under +-16. */
inline uint16_t encode_coefficient_1word(int32_t value, bool flag) {
    const bool negative = value < 0;
    uint32_t mag = negative ? static_cast<uint32_t>(-(value + 1)) + 1u
                            : static_cast<uint32_t>(value);
    if (mag > 0x000FFFFFu) mag = 0x000FFFFFu;
    return static_cast<uint16_t>((flag ? 0x8000u : 0u)
                               | (negative ? 0x4000u : 0u)
                               | ((mag >> 16u) << 10u)
                               | ((mag >> 6u) & 0x03FFu));
}

}  // namespace saturn::hal::vdp2::rot

#endif
