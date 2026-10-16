#ifndef SATURN_HAL_VDP2_RESOURCE_PLAN_LOGIC_HPP
#define SATURN_HAL_VDP2_RESOURCE_PLAN_LOGIC_HPP

/* SPDX-License-Identifier: MIT */
#include <stdint.h>

/* Pure VDP2 resource-plan policy: the semantic ledger on top of the VRAM
 * byte free list (src/hal/vdp2/allocator.cpp). It does not place bytes —
 * the free list stays the single byte owner — it records what each range
 * is FOR, checks the hardware rules for that use, and gives the
 * reserve/commit/rollback transaction its all-or-nothing behaviour.
 *
 * Rules mirrored from the existing HAL validators (VDP2 manual chapters 3,
 * 6 and 8; see the cited call sites):
 *   - rotation parameter tables and coefficient tables: byte address a
 *     multiple of 4 (src/graphics/vdp2/layers.cpp rotation and coefficient
 *     validators);
 *   - window line tables: multiple of 4 (src/hal/vdp2/compose_logic.hpp);
 *   - vertical cell scroll tables: multiple of 4 (layers.cpp vcs validator);
 *   - line scroll, back screen and line colour screen tables: even byte
 *     address (layers.cpp, vdp2_layers.h);
 *   - bitmap bases: multiple of 0x20000 (vdp2_layers.h, one bitmap per
 *     128 KiB bank);
 *   - whole-bank claims (RBG0 pattern-name/character banks, RAMCTL): the
 *     VDP2 ignores those banks' cycle patterns, so pattern-name, character
 *     and bitmap reads and the rotation parameter/coefficient tables may
 *     not live there (vdp2_layers.h RBG0 section: "can sit in any other
 *     bank").
 *
 * No register access and no globals: host tests cover this policy directly.
 * The runtime state lives once in src/hal/vdp2/plan.cpp. */
namespace saturn::hal::vdp2::plan {

constexpr uint8_t  kMaxReservations = 24u;
constexpr uint32_t kVramBytes       = 0x80000u;
constexpr uint8_t  kBankCount       = 4u;    /* A0, A1, B0, B1: fixed 128 KiB quarters */
constexpr uint32_t kBankBytes       = 0x20000u;

/* The four quarters of the VDP2 byte address space. sat_vdp2_init divides
 * VRAM-A and VRAM-B in two (RAMCTL); the split only selects which cycle
 * registers time a bank, the byte space itself is always these quarters
 * (src/hal/vdp2/nbg_logic.hpp bank_of()). */
constexpr uint32_t kBankBase[4] = {0x00000u, 0x20000u, 0x40000u, 0x60000u};
constexpr uint8_t  kBankMask[4] = {0x01u, 0x02u, 0x04u, 0x08u};

/* What a reservation is for. The kind fixes the minimum alignment and
 * whether the range may live in a bank that is whole-claimed for rotation
 * (RBG0) use. A caller may always ask for a stricter alignment.
 *
 * Manual grounding (docs/sega_saturn_hardware/hard/vdp2/hon/):
 *   - p03_30 (3.3): pattern-name, character/bitmap and vertical-cell-scroll
 *     reads and CPU access are timed by the per-bank cycle pattern
 *     registers; RBG0 pattern names, characters/bitmap and per-dot
 *     coefficient tables each occupy a whole bank's cycle via the RAMCTL
 *     rotation data bank bits, and that bank's cycle pattern register is
 *     ignored. RBG1 names/chars (fixed to B1/B0) do the same.
 *   - p06_20 (6.2): RBG0 and RBG1 image data must live in separate VRAM;
 *     RBG1 names in B1, characters in B0; a per-line coefficient table may
 *     be stored in any bank, a per-dot one in its own claimed bank.
 *   - p06_30 (6.3): the rotation parameter table is one 0x60-byte set read
 *     once per line (parameter B at +0x80 when used).
 *   - p06_40 (6.4): per-line coefficient tables in VRAM need no bank claim;
 *     per-dot ones use a claimed bank (RDBS 01B) or Color RAM
 *     100800H..100FFFH (CRKTE, CRAM mode 1).
 *
 * The library contract (vdp2_layers.h rotation section) keeps rotation
 * parameter and coefficient tables outside the RBG0 image banks ("can sit
 * in any other bank"); this planner follows that stricter contract rather
 * than the manual's more permissive per-line reading, so a layout the
 * runtime has never validated cannot be planned. Revisit only with a
 * verified example that needs it. */
enum class Kind : uint8_t {
    PatternNames   = 0u,   /* NBG/RBG pattern name tables (caller alignment: the plane size) */
    Characters     = 1u,   /* character (tile) data windows */
    Bitmap         = 2u,   /* NBG0/NBG1 bitmap: 0x20000-aligned, one per bank */
    RotationParams = 3u,   /* RBG0 rotation parameter table: multiple of 4 */
    Coefficients   = 4u,   /* RBG0 coefficient tables: multiple of 4 */
    LineScroll     = 5u,   /* line scroll tables: even */
    WindowTable    = 6u,   /* window line tables: multiple of 4 */
    BackScreen     = 7u,   /* per-line back screen table: even */
    LineColor      = 8u,   /* line colour screen table: even */
    Scratch        = 9u,   /* caller-private scratch (CPU-written in blanking) */
};

constexpr uint8_t kKindCount = 10u;

enum class Status : uint8_t {
    Ok = 0u,
    NotOpen,        /* no transaction is open */
    AlreadyOpen,    /* begin() while a transaction is open */
    TableFull,      /* no free ledger slot */
    Exhausted,      /* the byte owner cannot place the request */
    BadKind,
    BadAlign,       /* below the kind's minimum alignment */
    BadRange,       /* outside VRAM or zero/wrap-around bytes */
    Overlap,        /* the range touches another reservation */
    BankClaimed,    /* the kind may not live in a whole-claimed bank */
    NotFound,       /* release of a base that is not committed */
    BanksBusy,      /* claim_banks over live NBG/rotation reservations */
};

/* Slot states. Staged ranges exist only inside an open transaction;
 * commit() turns them into committed reservations, rollback() drops them. */
enum class SlotState : uint8_t { Empty = 0u, Staged = 1u, Committed = 2u };

struct Slot {
    uint8_t  state;        /* SlotState */
    uint8_t  kind;         /* Kind */
    uint16_t pad;
    uint32_t align_bytes;  /* alignment actually applied */
    uint32_t base;         /* byte address */
    uint32_t bytes;
};

struct PlanState {
    Slot     slots[kMaxReservations];
    uint8_t  claimed_banks;   /* whole-bank claims, SAT_VDP2_BANK_* mask */
    uint32_t refusals;        /* every refused call increments this */
    Status   last_status;     /* the reason of the last refusal (Ok if none) */
    uint32_t last_conflict_base;
    uint8_t  open;            /* a transaction is open */
};

inline void init(PlanState& s) {
    for (uint8_t i = 0u; i < kMaxReservations; ++i) {
        s.slots[i] = Slot{0u, 0u, 0u, 0u, 0u, 0u};
    }
    s.claimed_banks = 0u;
    s.refusals = 0u;
    s.last_status = Status::Ok;
    s.last_conflict_base = 0u;
    s.open = 0u;
}

/* ---- kind rules --------------------------------------------------------- */

/* Minimum alignment of the kind's byte base. Pattern names, characters
 * and scratch only need an even address here; the plane-size and
 * character-window alignments are the caller's (the layer manager
 * validates them per configuration). */
inline uint32_t kind_min_align_bytes(Kind k) {
    switch (k) {
        case Kind::RotationParams:
        case Kind::Coefficients:
        case Kind::WindowTable:
            return 4u;                       /* multiple of 4 bytes */
        case Kind::Bitmap:
            return 0x20000u;                 /* one bitmap per 128 KiB bank */
        default:
            return 2u;                       /* even byte address */
    }
}

/* True when the kind's range may not live in a bank that is whole-claimed
 * for rotation use (RBG0 pattern/character banks). */
inline bool kind_needs_unclaimed_bank(Kind k) {
    switch (k) {
        case Kind::PatternNames:
        case Kind::Characters:
        case Kind::Bitmap:
        case Kind::RotationParams:
        case Kind::Coefficients:
            return true;
        default:
            return false;
    }
}

inline bool kind_valid(Kind k) {
    return static_cast<uint8_t>(k) < kKindCount;
}

/* Banks (as a SAT_VDP2_BANK_* mask) a byte range touches. Fixed quarters,
 * so no split knowledge is needed. */
inline uint8_t bank_mask_of_range(uint32_t base, uint32_t bytes) {
    uint8_t mask = 0u;
    if (bytes == 0u) return mask;
    const uint32_t last = base + bytes - 1u;
    for (uint8_t b = 0u; b < kBankCount; ++b) {
        if (base < kBankBase[b] + kBankBytes && last >= kBankBase[b]) {
            mask = static_cast<uint8_t>(mask | kBankMask[b]);
        }
    }
    return mask;
}

/* ---- ledger ------------------------------------------------------------- */

inline Slot* find_slot(PlanState& s, uint32_t base, SlotState state) {
    for (uint8_t i = 0u; i < kMaxReservations; ++i) {
        if (static_cast<SlotState>(s.slots[i].state) == state
            && s.slots[i].base == base) {
            return &s.slots[i];
        }
    }
    return nullptr;
}

inline bool ranges_overlap(uint32_t a_base, uint32_t a_bytes,
                           uint32_t b_base, uint32_t b_bytes) {
    return a_base < b_base + b_bytes && b_base < a_base + a_bytes;
}

/* Success paths never touch last_status: it is the reason of the LAST
 * REFUSAL only (see note_refusal); a successful call does not erase the
 * diagnostic of the previous one. */
inline Status open(PlanState& s) {
    if (s.open) {
        s.last_status = Status::AlreadyOpen;
        ++s.refusals;
        return s.last_status;
    }
    s.open = 1u;
    return Status::Ok;
}

/* Records a refusal the policy itself did not detect (the byte owner
 * refusing a placement, for instance). */
inline Status note_refusal(PlanState& s, Status st) {
    s.last_status = st;
    ++s.refusals;
    return st;
}

/* Drops one staged reservation without touching the byte owner; the caller
 * frees the bytes. NotFound if the base is not staged. */
inline Status unstage(PlanState& s, uint32_t base) {
    Slot* slot = find_slot(s, base, SlotState::Staged);
    if (slot == nullptr) {
        s.last_status = Status::NotFound;
        ++s.refusals;
        return s.last_status;
    }
    *slot = Slot{0u, 0u, 0u, 0u, 0u, 0u};
    return Status::Ok;
}

/* Records one reservation (already placed by the byte owner) after checking
 * the kind's rules. Nothing is recorded on failure. `align_bytes` is the
 * minimum byte alignment actually applied (0 meaning the kind's own). */
inline Status stage(PlanState& s, Kind kind, uint32_t base, uint32_t bytes,
                   uint32_t align_bytes) {
    if (!s.open) {
        s.last_status = Status::NotOpen;
        ++s.refusals;
        return s.last_status;
    }
    if (!kind_valid(kind)) {
        s.last_status = Status::BadKind;
        ++s.refusals;
        return s.last_status;
    }
    if (bytes == 0u || base >= kVramBytes
        || bytes > kVramBytes
        || base + bytes > kVramBytes) {
        s.last_status = Status::BadRange;
        ++s.refusals;
        return s.last_status;
    }
    const uint32_t want = align_bytes ? align_bytes : 2u;
    if ((want & 1u) != 0u                       /* alignments are 2-byte multiples */
        || want < kind_min_align_bytes(kind)
        || (base % kind_min_align_bytes(kind)) != 0u
        || (base % want) != 0u) {
        s.last_status = Status::BadAlign;
        ++s.refusals;
        return s.last_status;
    }
    const uint8_t banks = bank_mask_of_range(base, bytes);
    if (kind_needs_unclaimed_bank(kind) && (banks & s.claimed_banks) != 0u) {
        s.last_status = Status::BankClaimed;
        s.last_conflict_base = base;
        ++s.refusals;
        return s.last_status;
    }
    for (uint8_t i = 0u; i < kMaxReservations; ++i) {
        const Slot& other = s.slots[i];
        if (other.state == 0u) continue;
        if (ranges_overlap(base, bytes, other.base, other.bytes)) {
            s.last_status = Status::Overlap;
            s.last_conflict_base = other.base;
            ++s.refusals;
            return s.last_status;
        }
    }
    for (uint8_t i = 0u; i < kMaxReservations; ++i) {
        if (s.slots[i].state == 0u) {
            s.slots[i] = Slot{static_cast<uint8_t>(SlotState::Staged),
                              static_cast<uint8_t>(kind),
                              0u, want, base, bytes};
            return Status::Ok;
        }
    }
    s.last_status = Status::TableFull;
    ++s.refusals;
    return s.last_status;
}

inline Status commit(PlanState& s) {
    if (!s.open) {
        s.last_status = Status::NotOpen;
        ++s.refusals;
        return s.last_status;
    }
    for (uint8_t i = 0u; i < kMaxReservations; ++i) {
        if (static_cast<SlotState>(s.slots[i].state) == SlotState::Staged) {
            s.slots[i].state = static_cast<uint8_t>(SlotState::Committed);
        }
    }
    s.open = 0u;
    return Status::Ok;
}

/* The caller frees the staged ranges in the byte owner; the ledger only
 * forgets them (this function is pure, plan.cpp does the freeing through
 * `free_range(ctx, base) for every staged reservation`). */
inline Status rollback(PlanState& s, void (*free_range)(void*, uint32_t),
                       void* free_ctx) {
    if (!s.open) {
        s.last_status = Status::NotOpen;
        ++s.refusals;
        return s.last_status;
    }
    for (uint8_t i = 0u; i < kMaxReservations; ++i) {
        if (static_cast<SlotState>(s.slots[i].state) == SlotState::Staged) {
            if (free_range != nullptr) {
                free_range(free_ctx, s.slots[i].base);
            }
            s.slots[i] = Slot{0u, 0u, 0u, 0u, 0u, 0u};
        }
    }
    s.open = 0u;
    return Status::Ok;
}

/* Releases one committed reservation; plan.cpp also frees the bytes. */
inline Status release(PlanState& s, uint32_t base) {
    Slot* slot = find_slot(s, base, SlotState::Committed);
    if (slot == nullptr) {
        s.last_status = Status::NotFound;
        ++s.refusals;
        return s.last_status;
    }
    *slot = Slot{0u, 0u, 0u, 0u, 0u, 0u};
    return Status::Ok;
}

/* Whole-bank claims (rotation pattern-name/character banks). A claim that
 * would strand a live pattern-name, character, bitmap, rotation-parameter
 * or coefficient reservation is refused: nothing changes. */
inline Status claim_banks(PlanState& s, uint8_t bank_mask) {
    if (bank_mask == 0u || (bank_mask & ~0x0Fu) != 0u) {
        s.last_status = Status::BadRange;
        ++s.refusals;
        return s.last_status;
    }
    const uint8_t effective = static_cast<uint8_t>(bank_mask & ~s.claimed_banks);
    if (effective == 0u) {
        return Status::Ok;   /* idempotent re-claim */
    }
    for (uint8_t i = 0u; i < kMaxReservations; ++i) {
        const Slot& slot = s.slots[i];
        if (slot.state == 0u) continue;
        const Kind kind = static_cast<Kind>(slot.kind);
        if (!kind_needs_unclaimed_bank(kind)) continue;
        if ((bank_mask_of_range(slot.base, slot.bytes) & effective) != 0u) {
            s.last_status = Status::BanksBusy;
            s.last_conflict_base = slot.base;
            ++s.refusals;
            return s.last_status;
        }
    }
    s.claimed_banks = static_cast<uint8_t>(s.claimed_banks | effective);
    return Status::Ok;
}

inline Status release_banks(PlanState& s, uint8_t bank_mask) {
    if (bank_mask == 0u || (bank_mask & ~0x0Fu) != 0u) {
        s.last_status = Status::BadRange;
        ++s.refusals;
        return s.last_status;
    }
    s.claimed_banks = static_cast<uint8_t>(s.claimed_banks & ~bank_mask);
    return Status::Ok;
}

/* ---- telemetry ---------------------------------------------------------- */

struct Usage {
    uint32_t committed_count;
    uint32_t staged_count;
    uint32_t reserved_bytes;        /* committed, plan-managed */
    uint32_t reserved_bytes_per_bank[kBankCount];
    uint32_t claimed_banks;
    uint32_t refusals;
    uint32_t last_status;           /* plan::Status of the last refusal, 0 if none */
    uint32_t last_conflict_base;
};

inline Usage usage(const PlanState& s) {
    Usage u{};
    for (uint8_t i = 0u; i < kMaxReservations; ++i) {
        const Slot& slot = s.slots[i];
        if (slot.state == 0u) continue;
        if (static_cast<SlotState>(slot.state) == SlotState::Committed) {
            ++u.committed_count;
            u.reserved_bytes += slot.bytes;
            const uint8_t banks = bank_mask_of_range(slot.base, slot.bytes);
            for (uint8_t b = 0u; b < kBankCount; ++b) {
                if ((banks & kBankMask[b]) != 0u) {
                    /* Attribute the whole range to every bank it touches:
                     * a caller reserving across a bank edge wants to see it. */
                    u.reserved_bytes_per_bank[b] += slot.bytes;
                }
            }
        } else {
            ++u.staged_count;
        }
    }
    u.claimed_banks = s.claimed_banks;
    u.refusals = s.refusals;
    u.last_status = static_cast<uint32_t>(s.last_status);
    u.last_conflict_base = s.last_conflict_base;
    return u;
}

}  /* namespace saturn::hal::vdp2::plan */

#endif  /* SATURN_HAL_VDP2_RESOURCE_PLAN_LOGIC_HPP */
