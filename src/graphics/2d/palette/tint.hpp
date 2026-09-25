#ifndef SATURN_CORE_PALETTE_TINT_HPP
#define SATURN_CORE_PALETTE_TINT_HPP

#include <stdint.h>

#include "saturn/core.h"
#include "src/graphics/2d/palette/registry.hpp"

namespace saturn::core {

/* Per-sprite RGB tint for palette textures. VDP1 Gouraud and every other
 * per-pixel colour operation need RGB pixels, and LibSaturn textures are
 * INDEX8 palette sprites, so a tint is drawn through a *variant* CRAM bank:
 * the texture's palette with every entry multiplied by the tint. Variants
 * are ordinary logical banks (deduplicated and refcounted by the palette
 * registry), cached by (source bank, tint) and rebuilt when the source
 * bank's palette changes (its generation moves on).
 *
 * CRAM is read when the frame is DISPLAYED, one frame after it is drawn, so
 * a variant is only evicted once it has been unused for two frames; a new
 * tint that finds no such room returns SAT_ERR_CAPACITY. */
constexpr uint16_t kTintVariantCapacity = 4u;

struct TintVariant {
    uint8_t used;
    uint8_t source_bank;
    uint8_t variant_bank;
    uint8_t r, g, b;
    uint16_t source_generation;
    uint32_t last_frame;
};

struct TintCache {
    TintVariant entries[kTintVariantCapacity];
    uint32_t frame;
};

extern TintCache g_tint_cache;

inline void tint_cache_reset(TintCache& cache) {
    for (TintVariant& entry : cache.entries) entry.used = 0u;
    cache.frame = 0u;
}

inline void tint_cache_begin_frame(TintCache& cache) {
    ++cache.frame;
}

inline uint16_t tint_channel(uint16_t channel, uint8_t tint) {
    return static_cast<uint16_t>((static_cast<uint32_t>(channel) * tint + 127u) / 255u);
}

/* Saturn colour words: red bits 0-4, green 5-9, blue 10-14; bit 15 kept. */
inline uint16_t tint_rgb555(uint16_t colour, uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(
        (colour & 0x8000u) |
        tint_channel(colour & 0x1Fu, r) |
        (tint_channel((colour >> 5u) & 0x1Fu, g) << 5u) |
        (tint_channel((colour >> 10u) & 0x1Fu, b) << 10u));
}

inline void tint_release_entry(TintVariant& entry, PaletteRegistry& palettes) {
    if (entry.used == 0u) return;
    (void)palette_release_logical(palettes, entry.variant_bank);
    entry.used = 0u;
}

inline void tint_release_all(TintCache& cache, PaletteRegistry& palettes) {
    for (TintVariant& entry : cache.entries) tint_release_entry(entry, palettes);
}

/* Resolves the variant bank for (source_bank, r, g, b). `scratch` holds 256
 * words; `upload(palette, bank)` writes a new variant to CRAM. */
template <typename Upload>
sat_result_t tint_acquire(
    TintCache& cache, PaletteRegistry& palettes, uint16_t source_bank,
    uint8_t r, uint8_t g, uint8_t b, uint16_t* scratch, Upload upload,
    uint16_t* out_bank) {
    if (scratch == nullptr || out_bank == nullptr || source_bank >= kCramBankCount) {
        return SAT_ERR_INVALID_ARG;
    }
    /* Only a bank the registry owns has a known palette to multiply. */
    if ((palettes.logical_mask & static_cast<uint8_t>(1u << source_bank)) == 0u) {
        return SAT_ERR_UNSUPPORTED;
    }
    const uint16_t generation = palettes.generation[source_bank];
    TintVariant* slot = nullptr;
    for (TintVariant& entry : cache.entries) {
        if (entry.used == 0u || entry.source_bank != source_bank ||
            entry.r != r || entry.g != g || entry.b != b) continue;
        if (entry.source_generation == generation) {
            entry.last_frame = cache.frame;
            *out_bank = entry.variant_bank;
            return SAT_OK;
        }
        /* The texture's palette changed since: rebuild in this entry. */
        tint_release_entry(entry, palettes);
        slot = &entry;
        break;
    }
    if (slot == nullptr) {
        for (TintVariant& entry : cache.entries) {
            if (entry.used == 0u) { slot = &entry; break; }
        }
    }
    if (slot == nullptr) {
        for (TintVariant& entry : cache.entries) {
            if (cache.frame - entry.last_frame < 2u) continue;
            if (slot == nullptr || entry.last_frame < slot->last_frame) slot = &entry;
        }
        if (slot == nullptr) return SAT_ERR_CAPACITY;
        tint_release_entry(*slot, palettes);
    }

    const uint16_t* source = palettes.logical_palettes[source_bank];
    for (uint16_t i = 0u; i < kCramBankEntries; ++i) {
        scratch[i] = tint_rgb555(source[i], r, g, b);
    }
    uint16_t bank = 0u;
    bool needs_upload = false;
    SAT_TRY(palette_acquire_logical(palettes, scratch, &bank, &needs_upload));
    if (needs_upload) {
        const sat_result_t st = upload(scratch, bank);
        if (st != SAT_OK) {
            (void)palette_release_logical(palettes, bank);
            return st;
        }
    }
    slot->used = 1u;
    slot->source_bank = static_cast<uint8_t>(source_bank);
    slot->variant_bank = static_cast<uint8_t>(bank);
    slot->r = r;
    slot->g = g;
    slot->b = b;
    slot->source_generation = generation;
    slot->last_frame = cache.frame;
    *out_bank = bank;
    return SAT_OK;
}

}  // namespace saturn::core

#endif /* SATURN_CORE_PALETTE_TINT_HPP */
