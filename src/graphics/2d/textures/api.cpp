#include "saturn/texture.h"

#include "src/core/runtime/logic.hpp"
#include "src/graphics/2d/palette/registry.hpp"
#include "src/core/runtime/state.hpp"
#include "src/graphics/2d/textures/runtime.hpp"
#include "src/hal/vdp1/vdp1.hpp"

namespace {

using saturn::core::TextureRegionRecord;
using saturn::core::TextureSlot;

sat_result_t validate_indexed8_surface_basic(const sat_surface_t* source) {
    if (source == nullptr || source->pixels == nullptr) return SAT_ERR_INVALID_ARG;
    if (source->format != SAT_PIXEL_INDEX8) return SAT_ERR_UNSUPPORTED;
    if (source->palette_rgb555 == nullptr || source->palette_count != 256u) return SAT_ERR_INVALID_ARG;
    if (source->width == 0u || source->height == 0u || source->pitch < source->width) {
        return SAT_ERR_INVALID_ARG;
    }
    return SAT_OK;
}

sat_result_t validate_texture_source(const sat_surface_t* source) {
    const sat_result_t st = validate_indexed8_surface_basic(source);
    if (st != SAT_OK) return st;
    return saturn::core::validate_indexed8_texture_dims(source->width, source->height);
}

sat_result_t validate_policy(sat_texture_backing_policy_t policy) {
    if (policy != SAT_TEXTURE_UPLOAD_ONLY &&
        policy != SAT_TEXTURE_PERSISTENT_SOURCE &&
        policy != SAT_TEXTURE_DYNAMIC) {
        return SAT_ERR_INVALID_ARG;
    }
    return SAT_OK;
}

bool rect_inside_surface(const sat_rect_t& rect, const sat_surface_t& surface) {
    if (rect.x < 0 || rect.y < 0 || rect.width == 0u || rect.height == 0u) return false;
    const uint32_t right = static_cast<uint32_t>(static_cast<uint16_t>(rect.x)) + rect.width;
    const uint32_t bottom = static_cast<uint32_t>(static_cast<uint16_t>(rect.y)) + rect.height;
    return right <= surface.width && bottom <= surface.height;
}

const uint8_t* surface_row(const sat_surface_t& surface, uint16_t y) {
    return static_cast<const uint8_t*>(surface.pixels) + static_cast<uint32_t>(y) * surface.pitch;
}

uint8_t* surface_row(sat_surface_t& surface, uint16_t y) {
    return static_cast<uint8_t*>(surface.pixels) + static_cast<uint32_t>(y) * surface.pitch;
}

sat_result_t upload_native_from_surface(
    sat_vdp1_texture_t* out_native,
    const sat_surface_t& source,
    uint16_t palette_bank
) {
    uint16_t srca = 0u;
    const sat_result_t st = saturn::hal::vdp1::upload_texture_indexed8_pitched(
        static_cast<const uint8_t*>(source.pixels),
        source.width,
        source.height,
        source.pitch,
        &srca);
    if (st != SAT_OK) return st;
    out_native->srca = srca;
    out_native->width = source.width;
    out_native->height = source.height;
    out_native->palette = palette_bank;
    out_native->valid = 1u;
    out_native->format = SAT_VDP1_TEXTURE_INDEXED8;
    return SAT_OK;
}

/* A failed update may have written part of CRAM or VRAM. Invalidate every
 * scene-visible native descriptor, retaining the logical handle, palette
 * ownership, source and prepared-region descriptors for explicit recovery. */
void texture_needs_recovery(sat_texture_t texture, TextureSlot& slot) {
    using namespace saturn::core;
    slot.native.valid = 0u;
    for (uint16_t i = 0u; i < kTextureRegionCapacity; ++i) {
        TextureRegionRecord& record = g_texture_registry.regions[i];
        if (record.used != 0u && record.owner_slot == texture.slot &&
            record.owner_generation == texture.generation)
            record.native.valid = 0u;
    }
}

/* Only regions intersecting the updated source rectangle require a VRAM
 * refresh. The region's own dimensions/pitch determine its native address. */
sat_result_t refresh_intersecting_regions(
    sat_texture_t texture, TextureSlot& slot, const sat_rect_t& updated) {
    using namespace saturn::core;
    const uint32_t ux0 = static_cast<uint16_t>(updated.x);
    const uint32_t uy0 = static_cast<uint16_t>(updated.y);
    const uint32_t ux1 = ux0 + updated.width;
    const uint32_t uy1 = uy0 + updated.height;
    for (uint16_t i = 0u; i < kTextureRegionCapacity; ++i) {
        TextureRegionRecord& record = g_texture_registry.regions[i];
        if (record.used == 0u || record.owner_slot != texture.slot ||
            record.owner_generation != texture.generation) continue;
        const uint32_t rx0 = static_cast<uint16_t>(record.rect.x);
        const uint32_t ry0 = static_cast<uint16_t>(record.rect.y);
        const uint32_t rx1 = rx0 + record.rect.width;
        const uint32_t ry1 = ry0 + record.rect.height;
        const uint32_t x0 = ux0 > rx0 ? ux0 : rx0;
        const uint32_t y0 = uy0 > ry0 ? uy0 : ry0;
        const uint32_t x1 = ux1 < rx1 ? ux1 : rx1;
        const uint32_t y1 = uy1 < ry1 ? uy1 : ry1;
        if (x0 >= x1 || y0 >= y1) continue;
        const uint8_t* pixels = surface_row(
            slot.source, static_cast<uint16_t>(ry0)) + rx0;
        const sat_result_t st = saturn::hal::vdp1::update_texture_indexed8_rect(
            record.native.srca, pixels, record.rect.width,
            record.rect.height, slot.source.pitch,
            static_cast<uint16_t>(x0 - rx0),
            static_cast<uint16_t>(y0 - ry0),
            static_cast<uint16_t>(x1 - x0),
            static_cast<uint16_t>(y1 - y0));
        if (st != SAT_OK) return st;
    }
    return SAT_OK;
}

sat_result_t refresh_prepared_regions(sat_texture_t texture, TextureSlot& slot) {
    using namespace saturn::core;
    if (slot.region_count == 0u) return SAT_OK;
    if (slot.source.pixels == nullptr) return SAT_ERR_INVALID_ARG;

    for (uint16_t i = 0u; i < kTextureRegionCapacity; ++i) {
        TextureRegionRecord& record = g_texture_registry.regions[i];
        if (record.used == 0u || record.owner_slot != texture.slot ||
            record.owner_generation != texture.generation) {
            continue;
        }
        const uint16_t x = static_cast<uint16_t>(record.rect.x);
        const uint16_t y = static_cast<uint16_t>(record.rect.y);
        const uint8_t* pixels = surface_row(slot.source, y) + x;
        const sat_result_t st = saturn::hal::vdp1::update_texture_indexed8_pitched(
            record.native.srca,
            pixels,
            record.rect.width,
            record.rect.height,
            slot.source.pitch);
        if (st != SAT_OK) return st;
        record.native.palette = slot.palette_bank;
        record.native.valid = 1u;
    }
    return SAT_OK;
}

}  // namespace

extern "C" uint16_t sat_texture_capacity(void) {
    return saturn::core::kTextureCapacity;
}

extern "C" uint16_t sat_texture_region_capacity(void) {
    return saturn::core::kTextureRegionCapacity;
}

extern "C" sat_result_t sat_texture_create_from_surface(
    sat_texture_t* out_texture,
    const sat_surface_t* source,
    sat_texture_backing_policy_t backing_policy
) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    if (out_texture == nullptr) return SAT_ERR_INVALID_ARG;
    st = validate_policy(backing_policy);
    if (st != SAT_OK) return st;
    st = validate_texture_source(source);
    if (st != SAT_OK) return st;

    sat_texture_t handle{};
    TextureSlot* slot = nullptr;
    st = texture_allocate_slot(g_texture_registry, &handle, &slot);
    if (st != SAT_OK) return st;

    uint16_t palette_bank = 0u;
    bool needs_palette_upload = false;
    st = palette_acquire_logical(
        g_palette_registry, source->palette_rgb555, &palette_bank, &needs_palette_upload);
    if (st != SAT_OK) {
        (void)texture_release_slot(g_texture_registry, handle);
        return st;
    }

    if (needs_palette_upload) {
        st = saturn::hal::vdp1::upload_palette(source->palette_rgb555, palette_bank);
        if (st != SAT_OK) {
            (void)palette_release_logical(g_palette_registry, palette_bank);
            (void)texture_release_slot(g_texture_registry, handle);
            return st;
        }
    }

    st = upload_native_from_surface(&slot->native, *source, palette_bank);
    if (st != SAT_OK) {
        (void)palette_release_logical(g_palette_registry, palette_bank);
        (void)texture_release_slot(g_texture_registry, handle);
        return st;
    }

    slot->palette_bank = static_cast<uint8_t>(palette_bank);
    slot->policy = backing_policy;
    if (backing_policy != SAT_TEXTURE_UPLOAD_ONLY) slot->source = *source;
    *out_texture = handle;
    return SAT_OK;
}

extern "C" sat_result_t sat_texture_destroy(sat_texture_t texture) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    TextureSlot* slot = texture_resolve(g_texture_registry, texture);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    const uint16_t palette_bank = slot->palette_bank;
    st = palette_release_logical(g_palette_registry, palette_bank);
    if (st != SAT_OK) return st;
    return texture_release_slot(g_texture_registry, texture);
}

extern "C" sat_result_t sat_texture_info(sat_texture_t texture, sat_texture_info_t* out_info) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    if (out_info == nullptr) return SAT_ERR_INVALID_ARG;
    const TextureSlot* slot = texture_resolve(g_texture_registry, texture);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    out_info->width = slot->native.width;
    out_info->height = slot->native.height;
    out_info->format = SAT_PIXEL_INDEX8;
    out_info->backing_policy = slot->policy;
    out_info->prepared_region_count = slot->region_count;
    out_info->health = slot->native.valid != 0u
        ? SAT_TEXTURE_READY : SAT_TEXTURE_NEEDS_RECOVERY;
    return SAT_OK;
}

extern "C" sat_result_t sat_texture_update(sat_texture_t texture, const sat_surface_t* source) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    st = validate_texture_source(source);
    if (st != SAT_OK) return st;
    TextureSlot* slot = texture_resolve(g_texture_registry, texture);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    if (source->width != slot->native.width || source->height != slot->native.height) {
        return SAT_ERR_INVALID_ARG;
    }

    PaletteRebindPlan palette_plan{};
    st = palette_prepare_rebind(
        g_palette_registry, slot->palette_bank, source->palette_rgb555,
        &palette_plan);
    if (st != SAT_OK) return st;

    // Upload BEFORE changing logical palette ownership. An unsuccessful CRAM
    // transfer may still be partial, so invalidate the texture and all of its
    // prepared regions while preserving the previous logical binding.
    const bool recovering = slot->native.valid == 0u;
    if (palette_plan.needs_upload || recovering) {
        st = saturn::hal::vdp1::upload_palette(
            source->palette_rgb555, palette_plan.target_bank);
        if (st != SAT_OK) {
            texture_needs_recovery(texture, *slot);
            return st;
        }
    }
    // Serialized, infallible commit after the complete palette transfer.
    palette_commit_rebind(g_palette_registry, palette_plan,
                          source->palette_rgb555);
    const uint16_t palette_bank = palette_plan.target_bank;
    slot->palette_bank = static_cast<uint8_t>(palette_bank);

    st = saturn::hal::vdp1::update_texture_indexed8_pitched(
        slot->native.srca,
        static_cast<const uint8_t*>(source->pixels),
        source->width,
        source->height,
        source->pitch);
    if (st != SAT_OK) {
        texture_needs_recovery(texture, *slot);
        return st;
    }

    slot->native.palette = palette_bank;
    if (slot->policy != SAT_TEXTURE_UPLOAD_ONLY) {
        // The full source is retained only after the parent upload succeeds.
        // If a region refresh fails, the new source remains the recovery input.
        slot->source = *source;
        st = refresh_prepared_regions(texture, *slot);
        if (st != SAT_OK) {
            texture_needs_recovery(texture, *slot);
            return st;
        }
    }
    slot->native.valid = 1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_texture_update_rect(
    sat_texture_t texture,
    const sat_rect_t* destination_rect,
    const sat_surface_t* source
) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    if (destination_rect == nullptr) return SAT_ERR_INVALID_ARG;
    st = validate_indexed8_surface_basic(source);
    if (st != SAT_OK) return st;
    TextureSlot* slot = texture_resolve(g_texture_registry, texture);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    if (slot->policy != SAT_TEXTURE_DYNAMIC || slot->source.pixels == nullptr) {
        return SAT_ERR_UNSUPPORTED;
    }
    if (slot->native.valid == 0u) return SAT_ERR_BUSY;
    if (!rect_inside_surface(*destination_rect, slot->source)) return SAT_ERR_INVALID_ARG;
    if (source->width != destination_rect->width || source->height != destination_rect->height) {
        return SAT_ERR_INVALID_ARG;
    }
    if (!palette_equal(
            g_palette_registry.logical_palettes[slot->palette_bank],
            source->palette_rgb555)) {
        return SAT_ERR_INVALID_ARG;
    }

    const uint16_t dst_x = static_cast<uint16_t>(destination_rect->x);
    const uint16_t dst_y = static_cast<uint16_t>(destination_rect->y);
    for (uint16_t y = 0; y < destination_rect->height; ++y) {
        uint8_t* dst = surface_row(slot->source, static_cast<uint16_t>(dst_y + y)) + dst_x;
        const uint8_t* src = surface_row(*source, y);
        for (uint16_t x = 0; x < destination_rect->width; ++x) dst[x] = src[x];
    }

    st = saturn::hal::vdp1::update_texture_indexed8_rect(
        slot->native.srca,
        static_cast<const uint8_t*>(slot->source.pixels),
        slot->source.width,
        slot->source.height,
        slot->source.pitch,
        dst_x, dst_y, destination_rect->width, destination_rect->height);
    if (st != SAT_OK) {
        texture_needs_recovery(texture, *slot);
        return st;
    }
    st = refresh_intersecting_regions(texture, *slot, *destination_rect);
    if (st != SAT_OK) texture_needs_recovery(texture, *slot);
    return st;
}

extern "C" sat_result_t sat_texture_prepare_region(sat_texture_t texture, const sat_rect_t* region) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    if (region == nullptr) return SAT_ERR_INVALID_ARG;
    TextureSlot* slot = texture_resolve(g_texture_registry, texture);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    if (slot->policy == SAT_TEXTURE_UPLOAD_ONLY || slot->source.pixels == nullptr) {
        return SAT_ERR_UNSUPPORTED;
    }
    if (slot->native.valid == 0u) return SAT_ERR_BUSY;
    if (!rect_inside_surface(*region, slot->source) || (region->width & 7u) != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (texture_find_region(g_texture_registry, texture, *region) != nullptr) return SAT_OK;

    TextureRegionRecord* record = nullptr;
    st = texture_reserve_region(g_texture_registry, texture, *region, &record);
    if (st != SAT_OK) return st;

    const uint16_t x = static_cast<uint16_t>(region->x);
    const uint16_t y = static_cast<uint16_t>(region->y);
    const uint8_t* pixels = surface_row(slot->source, y) + x;
    sat_surface_t region_surface{
        const_cast<uint8_t*>(pixels),
        region->width,
        region->height,
        slot->source.pitch,
        SAT_PIXEL_INDEX8,
        slot->source.palette_rgb555,
        slot->source.palette_count};
    st = upload_native_from_surface(&record->native, region_surface, slot->palette_bank);
    if (st != SAT_OK) {
        texture_cancel_region(g_texture_registry, texture, record);
        return st;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_texture_region_stats(
    sat_texture_t texture,
    sat_texture_region_stats_t* out_stats
) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    if (out_stats == nullptr) return SAT_ERR_INVALID_ARG;
    const TextureSlot* slot = texture_resolve(g_texture_registry, texture);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    out_stats->used = slot->region_count;
    out_stats->capacity = kTextureRegionCapacity;
    return SAT_OK;
}
