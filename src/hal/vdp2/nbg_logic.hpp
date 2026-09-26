#ifndef SATURN_HAL_VDP2_NBG_LOGIC_HPP
#define SATURN_HAL_VDP2_NBG_LOGIC_HPP

#include <stdint.h>

/* Pure VDP2 normal-scroll-screen (NBG0..NBG3) policy: VRAM geometry, register
 * composition and the VRAM cycle-pattern allocator (VDP2 manual chapters 3, 4
 * and 5). No register access, so host tests cover it. */
namespace saturn::hal::vdp2::nbg {

constexpr uint8_t kLayerCount = 4u;
constexpr uint32_t kVramBytes = 0x80000u;
constexpr uint8_t kBankCount = 4u;          /* A0, A1, B0, B1 */
constexpr uint8_t kSlots = 8u;              /* T0..T7 in normal (non hi-res) mode */

/* Access commands of the VRAM cycle pattern registers (table 3.5). */
enum Cmd : uint8_t {
    kCmdPn0 = 0x0, kCmdPn1 = 0x1, kCmdPn2 = 0x2, kCmdPn3 = 0x3,
    kCmdCg0 = 0x4, kCmdCg1 = 0x5, kCmdCg2 = 0x6, kCmdCg3 = 0x7,
    kCmdVcs0 = 0xC, kCmdVcs1 = 0xD, kCmdCpu = 0xE, kCmdNone = 0xF
};

enum Bank : uint8_t { kBankA0 = 0, kBankA1 = 1, kBankB0 = 2, kBankB1 = 3 };
constexpr uint8_t kAllBanks = 0x0Fu;

enum class Colors : uint8_t { C16 = 0, C256 = 1, C2048 = 2, C32768 = 3, C16M = 4 };

struct Layer {
    bool enabled;
    bool bitmap;               /* NBG0 and NBG1 only */
    Colors colors;
    bool char_2x2;             /* cell format */
    bool pn_one_word;          /* cell format: 1-word pattern names */
    uint8_t pages_x, pages_y;  /* cell format plane size: 1 or 2 pages */
    uint8_t bitmap_size;       /* bitmap format: 0 512x256, 1 512x512, 2 1024x256, 3 1024x512 */
    uint8_t reduction;         /* horizontal reduction: 0 1x, 1 1/2, 2 1/4 (NBG0, NBG1) */
    bool vcs;                  /* vertical cell scroll (NBG0, NBG1) */
    uint8_t priority;          /* 0..7 */
    bool transparent;          /* dot code 0 is transparent */
    uint8_t palette;           /* bitmap palette number bits 6-4 */
    uint8_t char_number_supp;  /* PNCN auxiliary character number bits (5) */
    uint8_t palette_supp;      /* PNCN auxiliary palette number bits 6-4 (3) */
    bool char_number_mode1;    /* 12-bit character numbers, no inversion (CNSM) */
    uint32_t plane_address[4]; /* cell: pattern name tables A..D; bitmap: [0] = bitmap base */
    uint8_t char_banks;        /* banks holding character data (cell format) */
    uint32_t vcs_address;      /* vertical cell scroll table (byte address) */
};

enum class Status : uint8_t {
    Ok,
    BadLayer,
    BadFormat,       /* bitmap on NBG2/NBG3, 2048+ colours on NBG2/NBG3, ... */
    BadPlane,        /* misaligned, past VRAM, or planes that cannot share the map offset */
    BadReduction,
    NoCyclePattern   /* the layers cannot all be fetched within one cycle */
};

/* ---- geometry (table 4.4, 4.8, 4.11) ----------------------------------- */

inline uint32_t page_bytes(bool one_word, bool char_2x2) {
    if (one_word) return char_2x2 ? 0x800u : 0x2000u;
    return char_2x2 ? 0x1000u : 0x4000u;
}

inline uint32_t plane_bytes(bool one_word, bool char_2x2, uint8_t pages_x, uint8_t pages_y) {
    return page_bytes(one_word, char_2x2) * pages_x * pages_y;
}

inline uint8_t bits_per_dot(Colors c) {
    switch (c) {
        case Colors::C16: return 4u;
        case Colors::C256: return 8u;
        case Colors::C2048:
        case Colors::C32768: return 16u;
        default: return 32u;
    }
}

inline uint32_t bitmap_bytes(uint8_t bitmap_size, Colors c) {
    const uint32_t w = (bitmap_size & 2u) != 0u ? 1024u : 512u;
    const uint32_t h = (bitmap_size & 1u) != 0u ? 512u : 256u;
    return w * h * bits_per_dot(c) / 8u;
}

/* Bank a VRAM byte address belongs to, given how VRAM-A and VRAM-B are split
 * (RAMCTL VRAMD / VRBMD). An undivided half uses its first bank's registers. */
inline uint8_t bank_of(uint32_t address, bool split_a, bool split_b) {
    if (address < 0x40000u) return (split_a && address >= 0x20000u) ? kBankA1 : kBankA0;
    return (split_b && address >= 0x60000u) ? kBankB1 : kBankB0;
}

/* Banks (as a mask) a byte range touches. */
inline uint8_t bank_mask_of(uint32_t address, uint32_t bytes, bool split_a, bool split_b) {
    if (bytes == 0u) return 0u;
    uint8_t mask = 0u;
    const uint32_t last = address + bytes - 1u;
    /* Walk the 128 KiB regions the range covers. */
    for (uint32_t region = address & ~0x1FFFFu; region <= last && region < kVramBytes;
         region += 0x20000u) {
        mask = static_cast<uint8_t>(mask | (1u << bank_of(region > address ? region : address,
                                                          split_a, split_b)));
    }
    return mask;
}

/* Register number of a plane address (its address in plane-size units). */
inline uint32_t map_value(uint32_t address, uint32_t plane_size_bytes) {
    return address / plane_size_bytes;
}

inline uint32_t reads_for_pattern_names(uint8_t reduction) {
    return 1u << reduction;   /* 1, 2, 4 */
}

/* Character pattern reads at 1x per pattern name read (table 3.3). */
inline uint32_t reads_for_characters_1x(Colors c) {
    switch (c) {
        case Colors::C16: return 1u;
        case Colors::C256: return 2u;
        case Colors::C2048:
        case Colors::C32768: return 4u;
        default: return 8u;
    }
}

/* Total character (or bitmap) pattern reads in one cycle. */
inline uint32_t reads_for_characters(Colors c, uint8_t reduction) {
    return reads_for_characters_1x(c) << reduction;
}

/* ---- validation --------------------------------------------------------- */

inline Status validate(uint8_t index, const Layer& l, bool split_a, bool split_b) {
    (void)split_a; (void)split_b;
    if (index >= kLayerCount) return Status::BadLayer;
    if (l.priority > 7u) return Status::BadFormat;
    if (index >= 2u) {
        if (l.bitmap || l.reduction != 0u || l.vcs) return Status::BadFormat;
        if (l.colors != Colors::C16 && l.colors != Colors::C256) return Status::BadFormat;
    }
    if (l.reduction > 2u) return Status::BadReduction;
    if (l.bitmap) {
        if (l.bitmap_size > 3u) return Status::BadFormat;
        const uint32_t bytes = bitmap_bytes(l.bitmap_size, l.colors);
        const uint32_t base = l.plane_address[0];
        if ((base & 0x1FFFFu) != 0u || base >= kVramBytes) return Status::BadPlane;
        (void)bytes;   /* bitmaps larger than VRAM repeat vertically (manual 4.9) */
        return Status::Ok;
    }
    if (l.pages_x < 1u || l.pages_x > 2u || l.pages_y < 1u || l.pages_y > 2u) return Status::BadPlane;
    if (l.reduction == 2u && l.pages_x == 2u && l.pages_y == 2u) return Status::BadReduction;
    const uint32_t size = plane_bytes(l.pn_one_word, l.char_2x2, l.pages_x, l.pages_y);
    uint32_t offset_bits = 0xFFFFFFFFu;
    for (uint8_t p = 0u; p < 4u; ++p) {
        const uint32_t a = l.plane_address[p];
        if (a % size != 0u || a + size > kVramBytes) return Status::BadPlane;
        /* The three map-offset bits are shared by the four planes. */
        const uint32_t high = map_value(a, size) >> 6u;
        if (offset_bits == 0xFFFFFFFFu) offset_bits = high;
        else if (offset_bits != high) return Status::BadPlane;
    }
    if (offset_bits > 7u) return Status::BadPlane;
    if (l.char_banks == 0u) return Status::BadFormat;
    return Status::Ok;
}

/* ---- register composition ---------------------------------------------- */

struct Registers {
    uint16_t bgon_on;          /* screen display enable bits (0..3) */
    uint16_t bgon_opaque_zero; /* BGON bits 8-11: set = dot code 0 is drawn, clear = transparent */
    uint16_t chctla, chctlb;
    uint16_t bmpna;
    uint16_t pncn[4];
    uint16_t plsz;
    uint16_t mpofn;
    uint16_t mpab[4], mpcd[4];
    uint16_t prina, prinb;
    uint16_t zmctl;
    uint16_t scrctl;
};

inline uint16_t char_color_field(Colors c) {
    /* NBG0: 3 bits; NBG1: 2 bits (no 16.77 M colours); NBG2/3: 1 bit. */
    return static_cast<uint16_t>(c);
}

/* Composes the registers this set of layers needs. Unused layers contribute
 * nothing (their fields stay zero). */
inline Registers compose(const Layer layers[kLayerCount]) {
    Registers r{};
    for (uint8_t i = 0u; i < kLayerCount; ++i) {
        const Layer& l = layers[i];
        if (!l.enabled) continue;
        r.bgon_on = static_cast<uint16_t>(r.bgon_on | (1u << i));
        if (!l.transparent) r.bgon_opaque_zero = static_cast<uint16_t>(r.bgon_opaque_zero | (1u << (8u + i)));
        const uint16_t chcn = char_color_field(l.colors);
        const uint16_t chsz = l.char_2x2 ? 1u : 0u;
        if (i == 0u) {
            r.chctla = static_cast<uint16_t>(r.chctla | chsz | ((l.bitmap ? 1u : 0u) << 1u) |
                                             ((l.bitmap_size & 3u) << 2u) | ((chcn & 7u) << 4u));
        } else if (i == 1u) {
            r.chctla = static_cast<uint16_t>(r.chctla | (chsz << 8u) | ((l.bitmap ? 1u : 0u) << 9u) |
                                             ((l.bitmap_size & 3u) << 10u) | ((chcn & 3u) << 12u));
        } else if (i == 2u) {
            r.chctlb = static_cast<uint16_t>(r.chctlb | chsz | ((chcn & 1u) << 1u));
        } else {
            r.chctlb = static_cast<uint16_t>(r.chctlb | (chsz << 4u) | ((chcn & 1u) << 5u));
        }
        if (l.bitmap) {
            const uint16_t bm = static_cast<uint16_t>(l.palette & 7u);
            if (i == 0u) r.bmpna = static_cast<uint16_t>(r.bmpna | bm);
            else r.bmpna = static_cast<uint16_t>(r.bmpna | (bm << 8u));
            /* The bitmap base sits on 128 KiB boundaries: map offset bits 8-6
             * hold base / 0x20000, and the map registers are unused. */
            r.mpofn = static_cast<uint16_t>(r.mpofn |
                      (((l.plane_address[0] / 0x20000u) & 7u) << (4u * i)));
            continue;
        }
        r.pncn[i] = static_cast<uint16_t>(((l.pn_one_word ? 1u : 0u) << 15u) |
                                          ((l.char_number_mode1 ? 1u : 0u) << 14u) |
                                          ((l.palette_supp & 7u) << 5u) | (l.char_number_supp & 0x1Fu));
        const uint32_t size = plane_bytes(l.pn_one_word, l.char_2x2, l.pages_x, l.pages_y);
        const uint16_t plsz = static_cast<uint16_t>((l.pages_x == 2u && l.pages_y == 2u) ? 3u
                                                    : (l.pages_x == 2u ? 1u : 0u));
        r.plsz = static_cast<uint16_t>(r.plsz | (plsz << (2u * i)));
        uint16_t v[4];
        for (uint8_t p = 0u; p < 4u; ++p) v[p] = static_cast<uint16_t>(map_value(l.plane_address[p], size));
        r.mpab[i] = static_cast<uint16_t>((v[0] & 0x3Fu) | ((v[1] & 0x3Fu) << 8u));
        r.mpcd[i] = static_cast<uint16_t>((v[2] & 0x3Fu) | ((v[3] & 0x3Fu) << 8u));
        r.mpofn = static_cast<uint16_t>(r.mpofn | (((v[0] >> 6u) & 7u) << (4u * i)));
    }
    /* Priorities (the detailed register tables, not the quick-reference
     * diagram, which reads the other way round): PRINA holds NBG0 in bits 2-0
     * and NBG1 in bits 10-8; PRINB holds NBG2 in bits 2-0 and NBG3 in 10-8. */
    r.prina = static_cast<uint16_t>((layers[0].priority & 7u) | ((layers[1].priority & 7u) << 8u));
    r.prinb = static_cast<uint16_t>((layers[2].priority & 7u) | ((layers[3].priority & 7u) << 8u));
    /* Horizontal reduction enables: N0ZMHF bit 0 (1/2), N0ZMQT bit 1 (1/4); NBG1 at 8, 9. */
    for (uint8_t i = 0u; i < 2u; ++i) {
        if (!layers[i].enabled) continue;
        if (layers[i].reduction == 1u) r.zmctl = static_cast<uint16_t>(r.zmctl | (1u << (8u * i)));
        if (layers[i].reduction == 2u) r.zmctl = static_cast<uint16_t>(r.zmctl | (2u << (8u * i)));
    }
    if (layers[0].enabled && layers[0].vcs) r.scrctl = static_cast<uint16_t>(r.scrctl | 0x0001u);
    if (layers[1].enabled && layers[1].vcs) r.scrctl = static_cast<uint16_t>(r.scrctl | 0x0100u);
    return r;
}

/* ---- cycle-pattern allocator ------------------------------------------- */

/* Character reads allowed at each timing, per pattern-name timing (table
 * 3.4, normal mode). Bit t of entry p: a character read at Tt may follow a
 * pattern-name read at Tp. */
constexpr uint8_t kCgAllowed[kSlots] = {
    0xF7u,  /* PN T0: T0-T2, T4-T7 */
    0xEFu,  /* PN T1: T0-T3, T5-T7 */
    0xCFu,  /* PN T2: T0-T3, T6-T7 */
    0x8Fu,  /* PN T3: T0-T3, T7 */
    0x0Fu,  /* PN T4: T0-T3 */
    0x0Eu,  /* PN T5: T1-T3 */
    0x0Cu,  /* PN T6: T2, T3 */
    0x08u   /* PN T7: T3 */
};

/* Slots (per bank) a caller already spends: a layer managed elsewhere. */
struct Reserved {
    uint8_t mask[kBankCount];             /* bit t: slot Tt is taken */
    uint8_t cmd[kBankCount][kSlots];      /* what it holds */
};

struct Plan {
    Status status;
    /* Access command per bank and timing. */
    uint8_t cmd[kBankCount][kSlots];
    /* The same as the 8 cycle pattern registers: A0L A0U A1L A1U B0L B0U B1L B1U. */
    uint16_t cyc[8];
};

namespace detail {

constexpr uint8_t kMaxRequests = 48u;

/* One access to place: `banks` all get `cmd` at the chosen timing. */
struct Request {
    uint8_t banks;
    uint8_t cmd;
    uint8_t allowed;          /* timings the request may use (bit t) */
    int8_t pn_dep;            /* index of the pattern-name request whose timing limits this one, or -1 */
    bool unrestricted_at_t0;  /* NBG0/1: pattern names at T0 free the character timings */
    int8_t after;             /* must come later than this request, or -1 (canonical order / VCS order) */
};

struct State {
    uint8_t taken[kBankCount];
    uint8_t cmd[kBankCount][kSlots];
};

inline bool fits(const State& s, uint8_t banks, uint32_t slot) {
    for (uint8_t b = 0u; b < kBankCount; ++b) {
        if ((banks & (1u << b)) != 0u && (s.taken[b] & (1u << slot)) != 0u) return false;
    }
    return true;
}

inline void put(State& s, uint8_t banks, uint32_t slot, uint8_t cmd) {
    for (uint8_t b = 0u; b < kBankCount; ++b) {
        if ((banks & (1u << b)) == 0u) continue;
        s.taken[b] = static_cast<uint8_t>(s.taken[b] | (1u << slot));
        s.cmd[b][slot] = cmd;
    }
}

/* Depth-first search in request order, timings ascending. */
inline bool solve(const State& state, const Request* req, uint8_t count, uint8_t index,
                  int8_t* chosen, State* result) {
    if (index == count) {
        *result = state;
        return true;
    }
    const Request& r = req[index];
    uint32_t allowed = r.allowed;
    if (r.pn_dep >= 0) {
        const uint8_t pn_slot = static_cast<uint8_t>(chosen[r.pn_dep]);
        if (!(r.unrestricted_at_t0 && pn_slot == 0u)) allowed &= kCgAllowed[pn_slot];
    }
    for (uint32_t slot = 0u; slot < kSlots; ++slot) {
        if ((allowed & (1u << slot)) == 0u) continue;
        if (r.after >= 0 && static_cast<int8_t>(slot) <= chosen[r.after]) continue;
        if (!fits(state, r.banks, slot)) continue;
        State next = state;
        put(next, r.banks, slot, r.cmd);
        chosen[index] = static_cast<int8_t>(slot);
        if (solve(next, req, count, static_cast<uint8_t>(index + 1u), chosen, result)) return true;
    }
    return false;
}

/* Free slots become CPU access where the manual allows it and no-access
 * elsewhere: in a divided VRAM half a CPU command needs both banks free at
 * that timing and a free timing right before it (manual 3.3). */
inline void fill_free(State& s, bool split_a, bool split_b) {
    for (uint8_t half = 0u; half < 2u; ++half) {
        const bool split = half == 0u ? split_a : split_b;
        const uint8_t b0 = static_cast<uint8_t>(half * 2u);
        const uint8_t b1 = static_cast<uint8_t>(b0 + 1u);
        for (uint32_t t = 0u; t < kSlots; ++t) {
            const bool f0 = (s.taken[b0] & (1u << t)) == 0u;
            const bool f1 = (s.taken[b1] & (1u << t)) == 0u;
            if (!split) {
                if (f0) s.cmd[b0][t] = kCmdCpu;
                if (f1) s.cmd[b1][t] = kCmdNone;   /* the second bank is unused when undivided */
                continue;
            }
            const bool prev_free = t > 0u && (s.taken[b0] & (1u << (t - 1u))) == 0u &&
                                   (s.taken[b1] & (1u << (t - 1u))) == 0u;
            const bool cpu = f0 && f1 && prev_free;
            if (f0) s.cmd[b0][t] = cpu ? kCmdCpu : kCmdNone;
            if (f1) s.cmd[b1][t] = cpu ? kCmdCpu : kCmdNone;
        }
    }
}

}  // namespace detail

/* Places every enabled layer's VRAM reads into cycle patterns, or reports
 * NoCyclePattern. `split_a` / `split_b` say whether VRAM-A / VRAM-B is divided
 * into two banks (RAMCTL). Nothing here reads the hardware. */
inline Plan plan_cycles(const Layer layers[kLayerCount], bool split_a, bool split_b,
                        const Reserved* reserved = nullptr) {
    using namespace detail;
    Plan plan{};
    Request req[kMaxRequests];
    uint8_t count = 0u;
    int8_t vcs_first = -1;
    for (uint8_t i = 0u; i < kLayerCount; ++i) {
        const Layer& l = layers[i];
        if (!l.enabled) continue;
        const Status st = validate(i, l, split_a, split_b);
        if (st != Status::Ok) { plan.status = st; return plan; }
        uint8_t cg_banks = l.char_banks;
        if (l.bitmap) {
            cg_banks = bank_mask_of(l.plane_address[0], bitmap_bytes(l.bitmap_size, l.colors),
                                    split_a, split_b);
        }
        const uint8_t cg_cmd = static_cast<uint8_t>(kCmdCg0 + i);
        const uint8_t pn_cmd = static_cast<uint8_t>(kCmdPn0 + i);
        if (l.bitmap) {
            const uint32_t reads = reads_for_characters(l.colors, l.reduction);
            int8_t prev = -1;
            for (uint32_t k = 0u; k < reads; ++k) {
                if (count >= kMaxRequests) { plan.status = Status::NoCyclePattern; return plan; }
                req[count] = {cg_banks, cg_cmd, 0xFFu, -1, false, prev};
                prev = static_cast<int8_t>(count++);
            }
        } else {
            uint8_t pn_banks = 0u;
            const uint32_t size = plane_bytes(l.pn_one_word, l.char_2x2, l.pages_x, l.pages_y);
            for (uint8_t p = 0u; p < 4u; ++p) {
                pn_banks = static_cast<uint8_t>(pn_banks |
                    bank_mask_of(l.plane_address[p], size, split_a, split_b));
            }
            /* Pattern names can only sit in one of A0/B0 and one of A1/B1. */
            if ((pn_banks & 0x5u) == 0x5u || (pn_banks & 0xAu) == 0xAu) {
                plan.status = Status::BadPlane;
                return plan;
            }
            const uint32_t pn_reads = reads_for_pattern_names(l.reduction);
            const uint32_t per_pn = reads_for_characters_1x(l.colors);
            int8_t prev_pn = -1;
            for (uint32_t k = 0u; k < pn_reads; ++k) {
                if (count >= kMaxRequests) { plan.status = Status::NoCyclePattern; return plan; }
                req[count] = {pn_banks, pn_cmd, 0xFFu, -1, false, prev_pn};
                const int8_t pn_index = static_cast<int8_t>(count++);
                prev_pn = pn_index;
                int8_t prev_cg = -1;
                for (uint32_t c = 0u; c < per_pn; ++c) {
                    if (count >= kMaxRequests) { plan.status = Status::NoCyclePattern; return plan; }
                    req[count] = {cg_banks, cg_cmd, 0xFFu, pn_index, i <= 1u, prev_cg};
                    prev_cg = static_cast<int8_t>(count++);
                }
            }
        }
        if (l.vcs) {
            if (count >= kMaxRequests) { plan.status = Status::NoCyclePattern; return plan; }
            const uint8_t vcs_banks = bank_mask_of(l.vcs_address, 4u, split_a, split_b);
            /* NBG0's read is at T0 or T1, NBG1's at T0..T2, NBG0 first. */
            const uint8_t allowed = i == 0u ? 0x03u : 0x07u;
            req[count] = {vcs_banks, static_cast<uint8_t>(kCmdVcs0 + i), allowed, -1, false,
                          i == 1u ? vcs_first : static_cast<int8_t>(-1)};
            if (i == 0u) vcs_first = static_cast<int8_t>(count);
            ++count;
        }
    }
    State start{};
    for (uint8_t b = 0u; b < kBankCount; ++b) {
        for (uint32_t t = 0u; t < kSlots; ++t) start.cmd[b][t] = kCmdNone;
        if (reserved != nullptr) {
            start.taken[b] = reserved->mask[b];
            for (uint32_t t = 0u; t < kSlots; ++t) {
                if ((reserved->mask[b] & (1u << t)) != 0u) start.cmd[b][t] = reserved->cmd[b][t];
            }
        }
    }
    State done{};
    int8_t chosen[kMaxRequests];
    for (uint8_t i = 0u; i < kMaxRequests; ++i) chosen[i] = -1;
    if (!solve(start, req, count, 0u, chosen, &done)) {
        plan.status = Status::NoCyclePattern;
        return plan;
    }
    fill_free(done, split_a, split_b);
    for (uint8_t b = 0u; b < kBankCount; ++b) {
        for (uint32_t t = 0u; t < kSlots; ++t) plan.cmd[b][t] = done.cmd[b][t];
        for (uint32_t half = 0u; half < 2u; ++half) {
            uint16_t word = 0u;
            for (uint32_t t = 0u; t < 4u; ++t) {
                word = static_cast<uint16_t>(word | (plan.cmd[b][half * 4u + t] << (12u - 4u * t)));
            }
            plan.cyc[b * 2u + half] = word;
        }
    }
    plan.status = Status::Ok;
    return plan;
}


/* ---- checking a finished pattern ---------------------------------------- */

namespace detail {

inline uint32_t popcount8(uint32_t v) {
    uint32_t n = 0u;
    for (; v != 0u; v >>= 1u) n += v & 1u;
    return n;
}

inline uint8_t slots_of(const uint8_t cmd[kBankCount][kSlots], uint8_t bank, uint8_t command) {
    uint8_t m = 0u;
    for (uint32_t t = 0u; t < kSlots; ++t) {
        if (cmd[bank][t] == command) m = static_cast<uint8_t>(m | (1u << t));
    }
    return m;
}

/* The commands of one kind must sit at the same timings in every bank that
 * needs them, and nowhere else. Returns that timing set, or 0xFFFF on error. */
inline uint16_t common_slots(const uint8_t cmd[kBankCount][kSlots], uint8_t banks, uint8_t command) {
    uint8_t seen = 0u;
    bool first = true;
    for (uint8_t b = 0u; b < kBankCount; ++b) {
        const uint8_t m = slots_of(cmd, b, command);
        if ((banks & (1u << b)) == 0u) {
            if (m != 0u) return 0xFFFFu;
            continue;
        }
        if (first) { seen = m; first = false; }
        else if (m != seen) return 0xFFFFu;
    }
    return seen;
}

/* Can `groups` pattern-name reads each take `per_group` timings from `pool`,
 * every timing inside that group's allowed set and used once? */
inline bool assign_groups(const uint8_t allowed[], uint32_t groups, uint32_t per_group, uint8_t pool) {
    if (groups == 0u) return true;
    const uint8_t usable = static_cast<uint8_t>(allowed[groups - 1u] & pool);
    /* Try every subset of `usable` with exactly per_group members. */
    for (uint32_t subset = 0u; subset < 256u; ++subset) {
        if ((subset & ~static_cast<uint32_t>(usable)) != 0u) continue;
        uint32_t members = 0u;
        for (uint32_t v = subset; v != 0u; v >>= 1u) members += v & 1u;
        if (members != per_group) continue;
        if (assign_groups(allowed, groups - 1u, per_group,
                          static_cast<uint8_t>(pool & ~static_cast<uint8_t>(subset)))) return true;
    }
    return false;
}

}  // namespace detail

/* Verifies a cycle pattern (as `cmd[bank][timing]`) gives every enabled layer
 * exactly the reads the manual requires within the manual's restrictions:
 * this is the specification, independent of how plan_cycles finds patterns. */
inline bool check_pattern(const Layer layers[kLayerCount], bool split_a, bool split_b,
                          const uint8_t cmd[kBankCount][kSlots]) {
    using namespace detail;
    for (uint8_t i = 0u; i < kLayerCount; ++i) {
        const Layer& l = layers[i];
        const uint8_t pn_cmd = static_cast<uint8_t>(kCmdPn0 + i);
        const uint8_t cg_cmd = static_cast<uint8_t>(kCmdCg0 + i);
        const uint8_t vcs_cmd = static_cast<uint8_t>(kCmdVcs0 + i);
        if (!l.enabled) {
            for (uint8_t b = 0u; b < kBankCount; ++b) {
                if (slots_of(cmd, b, pn_cmd) != 0u || slots_of(cmd, b, cg_cmd) != 0u) return false;
            }
            continue;
        }
        uint8_t cg_banks = l.char_banks;
        if (l.bitmap) cg_banks = bank_mask_of(l.plane_address[0], bitmap_bytes(l.bitmap_size, l.colors),
                                              split_a, split_b);
        const uint16_t cg = common_slots(cmd, cg_banks, cg_cmd);
        if (cg == 0xFFFFu) return false;
        if (l.bitmap) {
            if (popcount8(cg) != reads_for_characters(l.colors, l.reduction)) return false;
        } else {
            uint8_t pn_banks = 0u;
            const uint32_t size = plane_bytes(l.pn_one_word, l.char_2x2, l.pages_x, l.pages_y);
            for (uint8_t p = 0u; p < 4u; ++p) {
                pn_banks = static_cast<uint8_t>(pn_banks | bank_mask_of(l.plane_address[p], size, split_a, split_b));
            }
            const uint16_t pn = common_slots(cmd, pn_banks, pn_cmd);
            if (pn == 0xFFFFu || popcount8(pn) != reads_for_pattern_names(l.reduction)) return false;
            if (popcount8(cg) != reads_for_characters(l.colors, l.reduction)) return false;
            uint8_t allowed[8];
            uint32_t groups = 0u;
            for (uint32_t t = 0u; t < kSlots; ++t) {
                if ((pn & (1u << t)) == 0u) continue;
                allowed[groups++] = (i <= 1u && t == 0u) ? 0xFFu : kCgAllowed[t];
            }
            if (!assign_groups(allowed, groups, reads_for_characters_1x(l.colors), static_cast<uint8_t>(cg))) {
                return false;
            }
        }
        if (l.vcs) {
            const uint16_t v = common_slots(cmd, bank_mask_of(l.vcs_address, 4u, split_a, split_b), vcs_cmd);
            if (v == 0xFFFFu || popcount8(v) != 1u) return false;
            const uint8_t allowed_vcs = i == 0u ? 0x03u : 0x07u;
            if ((v & ~static_cast<uint16_t>(allowed_vcs)) != 0u) return false;
        }
    }
    return true;
}

}  // namespace saturn::hal::vdp2::nbg

#endif
