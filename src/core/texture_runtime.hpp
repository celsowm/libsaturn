#ifndef SATURN_CORE_TEXTURE_RUNTIME_HPP
#define SATURN_CORE_TEXTURE_RUNTIME_HPP

#include <stdint.h>

#include "saturn/texture.h"
#include "saturn/vdp1.h"

namespace saturn::core {

constexpr uint16_t kTextureCapacity = 64u;
constexpr uint16_t kTextureRegionCapacity = 64u;
constexpr uint16_t kInvalidTextureSlot = 0xFFFFu;

struct TextureSlot {
    sat_vdp1_texture_t native;
    sat_surface_t source;
    uint16_t generation;
    uint16_t region_count;
    uint8_t palette_bank;
    uint8_t used;
    sat_texture_backing_policy_t policy;
};

struct TextureRegionRecord {
    sat_vdp1_texture_t native;
    sat_rect_t rect;
    uint16_t owner_slot;
    uint16_t owner_generation;
    uint8_t used;
};

struct TextureRegistry {
    TextureSlot slots[kTextureCapacity];
    TextureRegionRecord regions[kTextureRegionCapacity];
};

extern TextureRegistry g_texture_registry;

inline uint16_t next_texture_generation(uint16_t generation) {
    ++generation;
    return generation == 0u ? 1u : generation;
}

inline void texture_registry_reset(TextureRegistry& registry) {
    for (uint16_t i = 0; i < kTextureCapacity; ++i) {
        TextureSlot& slot = registry.slots[i];
        slot.generation = next_texture_generation(slot.generation);
        slot.used = 0u;
        slot.region_count = 0u;
        slot.palette_bank = 0u;
        slot.native = {};
        slot.source = {};
        slot.policy = SAT_TEXTURE_UPLOAD_ONLY;
    }
    for (uint16_t i = 0; i < kTextureRegionCapacity; ++i) {
        registry.regions[i] = {};
        registry.regions[i].owner_slot = kInvalidTextureSlot;
    }
}

inline TextureSlot* texture_resolve(TextureRegistry& registry, sat_texture_t texture) {
    if (texture.slot >= kTextureCapacity) return nullptr;
    TextureSlot& slot = registry.slots[texture.slot];
    if (slot.used == 0u || slot.generation != texture.generation || texture.generation == 0u) {
        return nullptr;
    }
    return &slot;
}

inline const TextureSlot* texture_resolve(const TextureRegistry& registry, sat_texture_t texture) {
    if (texture.slot >= kTextureCapacity) return nullptr;
    const TextureSlot& slot = registry.slots[texture.slot];
    if (slot.used == 0u || slot.generation != texture.generation || texture.generation == 0u) {
        return nullptr;
    }
    return &slot;
}

inline sat_result_t texture_allocate_slot(
    TextureRegistry& registry,
    sat_texture_t* out_texture,
    TextureSlot** out_slot
) {
    if (out_texture == nullptr || out_slot == nullptr) return SAT_ERR_INVALID_ARG;
    for (uint16_t i = 0; i < kTextureCapacity; ++i) {
        TextureSlot& slot = registry.slots[i];
        if (slot.used == 0u) {
            if (slot.generation == 0u) slot.generation = 1u;
            slot.used = 1u;
            slot.region_count = 0u;
            slot.native = {};
            slot.source = {};
            slot.palette_bank = 0u;
            slot.policy = SAT_TEXTURE_UPLOAD_ONLY;
            out_texture->slot = i;
            out_texture->generation = slot.generation;
            *out_slot = &slot;
            return SAT_OK;
        }
    }
    return SAT_ERR_CAPACITY;
}

inline void texture_invalidate_regions(TextureRegistry& registry, sat_texture_t owner) {
    TextureSlot* slot = texture_resolve(registry, owner);
    if (slot == nullptr) return;
    for (uint16_t i = 0; i < kTextureRegionCapacity; ++i) {
        TextureRegionRecord& record = registry.regions[i];
        if (record.used != 0u && record.owner_slot == owner.slot &&
            record.owner_generation == owner.generation) {
            record = {};
            record.owner_slot = kInvalidTextureSlot;
        }
    }
    slot->region_count = 0u;
}

inline sat_result_t texture_release_slot(TextureRegistry& registry, sat_texture_t texture) {
    TextureSlot* slot = texture_resolve(registry, texture);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    texture_invalidate_regions(registry, texture);
    slot->used = 0u;
    slot->native = {};
    slot->source = {};
    slot->region_count = 0u;
    slot->palette_bank = 0u;
    slot->policy = SAT_TEXTURE_UPLOAD_ONLY;
    slot->generation = next_texture_generation(slot->generation);
    return SAT_OK;
}

inline bool texture_rect_equal(const sat_rect_t& a, const sat_rect_t& b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

inline TextureRegionRecord* texture_find_region(
    TextureRegistry& registry,
    sat_texture_t owner,
    const sat_rect_t& rect
) {
    for (uint16_t i = 0; i < kTextureRegionCapacity; ++i) {
        TextureRegionRecord& record = registry.regions[i];
        if (record.used != 0u && record.owner_slot == owner.slot &&
            record.owner_generation == owner.generation && texture_rect_equal(record.rect, rect)) {
            return &record;
        }
    }
    return nullptr;
}

inline const TextureRegionRecord* texture_find_region(
    const TextureRegistry& registry,
    sat_texture_t owner,
    const sat_rect_t& rect
) {
    for (uint16_t i = 0; i < kTextureRegionCapacity; ++i) {
        const TextureRegionRecord& record = registry.regions[i];
        if (record.used != 0u && record.owner_slot == owner.slot &&
            record.owner_generation == owner.generation && texture_rect_equal(record.rect, rect)) {
            return &record;
        }
    }
    return nullptr;
}

inline sat_result_t texture_reserve_region(
    TextureRegistry& registry,
    sat_texture_t owner,
    const sat_rect_t& rect,
    TextureRegionRecord** out_region
) {
    if (out_region == nullptr) return SAT_ERR_INVALID_ARG;
    TextureSlot* slot = texture_resolve(registry, owner);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    if (TextureRegionRecord* existing = texture_find_region(registry, owner, rect)) {
        *out_region = existing;
        return SAT_OK;
    }
    for (uint16_t i = 0; i < kTextureRegionCapacity; ++i) {
        TextureRegionRecord& record = registry.regions[i];
        if (record.used == 0u) {
            record = {};
            record.used = 1u;
            record.owner_slot = owner.slot;
            record.owner_generation = owner.generation;
            record.rect = rect;
            ++slot->region_count;
            *out_region = &record;
            return SAT_OK;
        }
    }
    return SAT_ERR_CAPACITY;
}

inline void texture_cancel_region(
    TextureRegistry& registry,
    sat_texture_t owner,
    TextureRegionRecord* region
) {
    TextureSlot* slot = texture_resolve(registry, owner);
    if (slot == nullptr || region == nullptr || region->used == 0u) return;
    region->used = 0u;
    region->owner_slot = kInvalidTextureSlot;
    if (slot->region_count > 0u) --slot->region_count;
}

inline uint16_t texture_region_used(const TextureRegistry& registry) {
    uint16_t used = 0u;
    for (uint16_t i = 0; i < kTextureRegionCapacity; ++i) {
        if (registry.regions[i].used != 0u) ++used;
    }
    return used;
}

}  // namespace saturn::core

#endif /* SATURN_CORE_TEXTURE_RUNTIME_HPP */
