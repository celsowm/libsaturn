#ifndef SATURN_CORE_PALETTE_REGISTRY_HPP
#define SATURN_CORE_PALETTE_REGISTRY_HPP

#include <stdint.h>

#include "saturn/core.h"

namespace saturn::core {

constexpr uint16_t kCramBankCount = 8u;
constexpr uint16_t kCramBankEntries = 256u;
constexpr uint16_t kCramWordCount = kCramBankCount * kCramBankEntries;

struct PaletteRegistry {
    uint8_t external_mask;
    uint8_t logical_mask;
    uint16_t logical_refs[kCramBankCount];
    uint16_t logical_palettes[kCramBankCount][kCramBankEntries];
    /* Bumped whenever a bank's logical palette is (re)written, so a copy
     * derived from it (a tint variant) can tell it is stale. */
    uint16_t generation[kCramBankCount];
};

extern PaletteRegistry g_palette_registry;

inline void palette_registry_reset(PaletteRegistry& state) {
    state.external_mask = 0u;
    state.logical_mask = 0u;
    for (uint16_t bank = 0; bank < kCramBankCount; ++bank) {
        state.logical_refs[bank] = 0u;
        state.generation[bank] = 0u;
    }
}

inline bool palette_equal(const uint16_t* a, const uint16_t* b) {
    if (a == nullptr || b == nullptr) return false;
    for (uint16_t i = 0; i < kCramBankEntries; ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

inline void palette_copy(uint16_t* destination, const uint16_t* source) {
    for (uint16_t i = 0; i < kCramBankEntries; ++i) destination[i] = source[i];
}

/* Low-level VDP1/VDP2 uploads permanently claim every bank they touch for the
 * current sat_init lifetime. A partial VDP2 upload conservatively claims the
 * whole bank: this costs capacity but prevents a logical texture from later
 * overwriting unrelated colors in that same bank. */
inline sat_result_t palette_claim_external(
    PaletteRegistry& state,
    uint16_t word_offset,
    uint16_t word_count
) {
    const uint32_t end = static_cast<uint32_t>(word_offset) + static_cast<uint32_t>(word_count);
    if (end > kCramWordCount) return SAT_ERR_CAPACITY;
    if (word_count == 0u) return SAT_OK;

    const uint16_t first = static_cast<uint16_t>(word_offset / kCramBankEntries);
    const uint16_t last = static_cast<uint16_t>((end - 1u) / kCramBankEntries);
    uint8_t mask = 0u;
    for (uint16_t bank = first; bank <= last; ++bank) {
        mask = static_cast<uint8_t>(mask | static_cast<uint8_t>(1u << bank));
    }
    if ((state.logical_mask & mask) != 0u) return SAT_ERR_CAPACITY;
    state.external_mask = static_cast<uint8_t>(state.external_mask | mask);
    return SAT_OK;
}

inline sat_result_t palette_acquire_logical(
    PaletteRegistry& state,
    const uint16_t* palette,
    uint16_t* out_bank,
    bool* out_needs_upload
) {
    if (palette == nullptr || out_bank == nullptr || out_needs_upload == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }

    for (uint16_t bank = 0; bank < kCramBankCount; ++bank) {
        const uint8_t bit = static_cast<uint8_t>(1u << bank);
        if ((state.logical_mask & bit) != 0u &&
            palette_equal(state.logical_palettes[bank], palette)) {
            if (state.logical_refs[bank] == 0xFFFFu) return SAT_ERR_CAPACITY;
            ++state.logical_refs[bank];
            *out_bank = bank;
            *out_needs_upload = false;
            return SAT_OK;
        }
    }

    for (uint16_t bank = 0; bank < kCramBankCount; ++bank) {
        const uint8_t bit = static_cast<uint8_t>(1u << bank);
        if (((state.logical_mask | state.external_mask) & bit) == 0u) {
            state.logical_mask = static_cast<uint8_t>(state.logical_mask | bit);
            state.logical_refs[bank] = 1u;
            palette_copy(state.logical_palettes[bank], palette);
            ++state.generation[bank];
            *out_bank = bank;
            *out_needs_upload = true;
            return SAT_OK;
        }
    }
    return SAT_ERR_CAPACITY;
}

inline sat_result_t palette_release_logical(PaletteRegistry& state, uint16_t bank) {
    if (bank >= kCramBankCount) return SAT_ERR_INVALID_ARG;
    const uint8_t bit = static_cast<uint8_t>(1u << bank);
    if ((state.logical_mask & bit) == 0u || state.logical_refs[bank] == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    --state.logical_refs[bank];
    if (state.logical_refs[bank] == 0u) {
        state.logical_mask = static_cast<uint8_t>(state.logical_mask & static_cast<uint8_t>(~bit));
    }
    return SAT_OK;
}

/* Non-mutating palette update plan. The caller must keep the source palette
 * alive and serialize prepare -> optional full CRAM upload -> commit with other
 * palette registry operations. This needs no heap and only a small stack plan.
 * A failed upload leaves logical ownership/refcounts unchanged; physical
 * CRAM may nevertheless be partially written, so the texture must be marked
 * dirty by its caller. */
struct PaletteRebindPlan {
    uint16_t old_bank;
    uint16_t target_bank;
    bool needs_upload;
};

inline sat_result_t palette_prepare_rebind(
    const PaletteRegistry& state,
    uint16_t old_bank,
    const uint16_t* palette,
    PaletteRebindPlan* out_plan
) {
    if (old_bank >= kCramBankCount || palette == nullptr || out_plan == nullptr)
        return SAT_ERR_INVALID_ARG;
    const uint8_t old_bit = static_cast<uint8_t>(1u << old_bank);
    if ((state.logical_mask & old_bit) == 0u || state.logical_refs[old_bank] == 0u)
        return SAT_ERR_INVALID_ARG;

    if (palette_equal(state.logical_palettes[old_bank], palette)) {
        *out_plan = {old_bank, old_bank, false};
        return SAT_OK;
    }

    for (uint16_t bank = 0u; bank < kCramBankCount; ++bank) {
        if (bank == old_bank) continue;
        const uint8_t bit = static_cast<uint8_t>(1u << bank);
        if ((state.logical_mask & bit) != 0u &&
            palette_equal(state.logical_palettes[bank], palette)) {
            if (state.logical_refs[bank] == 0xFFFFu) return SAT_ERR_CAPACITY;
            *out_plan = {old_bank, bank, false};
            return SAT_OK;
        }
    }

    if (state.logical_refs[old_bank] == 1u) {
        *out_plan = {old_bank, old_bank, true};
        return SAT_OK;
    }

    for (uint16_t bank = 0u; bank < kCramBankCount; ++bank) {
        const uint8_t bit = static_cast<uint8_t>(1u << bank);
        if (((state.logical_mask | state.external_mask) & bit) == 0u) {
            *out_plan = {old_bank, bank, true};
            return SAT_OK;
        }
    }
    return SAT_ERR_CAPACITY;
}

/* Commit is infallible under the serialized prepare/upload/commit contract;
 * this does not claim that a previous CRAM transfer can be rolled back. */
inline void palette_commit_rebind(
    PaletteRegistry& state,
    const PaletteRebindPlan& plan,
    const uint16_t* palette
) {
    if (plan.old_bank == plan.target_bank) {
        if (plan.needs_upload) {
            palette_copy(state.logical_palettes[plan.target_bank], palette);
            ++state.generation[plan.target_bank];
        }
        return;
    }

    const uint8_t target_bit = static_cast<uint8_t>(1u << plan.target_bank);
    if ((state.logical_mask & target_bit) != 0u) {
        ++state.logical_refs[plan.target_bank];
    } else {
        state.logical_mask = static_cast<uint8_t>(state.logical_mask | target_bit);
        state.logical_refs[plan.target_bank] = 1u;
        palette_copy(state.logical_palettes[plan.target_bank], palette);
        ++state.generation[plan.target_bank];
    }
    (void)palette_release_logical(state, plan.old_bank);
}

/* Retained for registry-only clients: the callback-free primitive performs
 * both phases synchronously. Texture updates use prepare and commit on either
 * side of the hardware upload instead. */
inline sat_result_t palette_rebind_logical(
    PaletteRegistry& state,
    uint16_t old_bank,
    const uint16_t* palette,
    uint16_t* out_bank,
    bool* out_needs_upload
) {
    if (out_bank == nullptr || out_needs_upload == nullptr)
        return SAT_ERR_INVALID_ARG;
    PaletteRebindPlan plan{};
    const sat_result_t st = palette_prepare_rebind(
        state, old_bank, palette, &plan);
    if (st != SAT_OK) return st;
    palette_commit_rebind(state, plan, palette);
    *out_bank = plan.target_bank;
    *out_needs_upload = plan.needs_upload;
    return SAT_OK;
}

}  // namespace saturn::core

#endif /* SATURN_CORE_PALETTE_REGISTRY_HPP */
