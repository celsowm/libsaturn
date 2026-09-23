#include <cstdio>
#include <cstdlib>

#include "saturn/texture.h"
#include "src/graphics/2d/palette/registry.hpp"
#include "src/core/runtime/state.hpp"
#include "src/graphics/2d/textures/runtime.hpp"
#include "src/hal/vdp1/vdp1.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace {
uint16_t g_next_srca = 0x2000u;
uint32_t g_palette_uploads = 0u;
uint32_t g_texture_uploads = 0u;
uint32_t g_texture_updates = 0u;
uint16_t g_last_pitch = 0u;
sat_result_t g_texture_upload_status = SAT_OK;
sat_result_t g_palette_upload_status = SAT_OK;
sat_result_t g_texture_update_status = SAT_OK;
uint32_t g_fail_texture_update_call = 0u;
}

namespace saturn::hal::vdp1 {

sat_result_t upload_palette(const uint16_t*, uint16_t) {
    ++g_palette_uploads;
    return g_palette_upload_status;
}

sat_result_t upload_texture_indexed8_pitched(
    const uint8_t*, uint16_t width, uint16_t height, uint16_t pitch, uint16_t* out_srca) {
    if (out_srca == nullptr || pitch < width || width == 0u || height == 0u) return SAT_ERR_INVALID_ARG;
    if (g_texture_upload_status != SAT_OK) return g_texture_upload_status;
    ++g_texture_uploads;
    g_last_pitch = pitch;
    *out_srca = g_next_srca++;
    return SAT_OK;
}

sat_result_t update_texture_indexed8_pitched(
    uint16_t, const uint8_t*, uint16_t width, uint16_t height, uint16_t pitch) {
    if (pitch < width || width == 0u || height == 0u) return SAT_ERR_INVALID_ARG;
    ++g_texture_updates;
    g_last_pitch = pitch;
    if (g_texture_update_status != SAT_OK &&
        (g_fail_texture_update_call == 0u ||
         g_texture_updates == g_fail_texture_update_call))
        return g_texture_update_status;
    return SAT_OK;
}

}  // namespace saturn::hal::vdp1

static void reset_runtime() {
    using namespace saturn::core;
    g_state = {};
    g_state.initialized = true;
    palette_registry_reset(g_palette_registry);
    texture_registry_reset(g_texture_registry);
    g_next_srca = 0x2000u;
    g_palette_uploads = 0u;
    g_texture_uploads = 0u;
    g_texture_updates = 0u;
    g_last_pitch = 0u;
    g_texture_upload_status = SAT_OK;
    g_palette_upload_status = SAT_OK;
    g_texture_update_status = SAT_OK;
    g_fail_texture_update_call = 0u;
}

static void make_palette(uint16_t* palette, uint16_t seed) {
    for (uint16_t i = 0; i < 256u; ++i) palette[i] = static_cast<uint16_t>(seed + i);
}

static void update_failure_and_recovery() {
    using namespace saturn::core;
    reset_runtime();

    uint16_t original_palette[256]{}, new_palette[256]{}, final_palette[256]{};
    make_palette(original_palette, 0x100u);
    make_palette(new_palette, 0x200u);
    make_palette(final_palette, 0x300u);
    uint8_t original_pixels[32u]{}, new_pixels[32u]{}, final_pixels[32u]{};
    sat_surface_t original{original_pixels, 8u, 4u, 8u,
                           SAT_PIXEL_INDEX8, original_palette, 256u};
    sat_surface_t replacement{new_pixels, 8u, 4u, 8u,
                              SAT_PIXEL_INDEX8, new_palette, 256u};
    sat_surface_t final_source{final_pixels, 8u, 4u, 8u,
                               SAT_PIXEL_INDEX8, final_palette, 256u};
    sat_texture_t handle{};
    sat_texture_info_t info{};
    const sat_rect_t region{0, 0, 8u, 2u};
    OK(sat_texture_create_from_surface(
        &handle, &original, SAT_TEXTURE_DYNAMIC) == SAT_OK);
    OK(sat_texture_prepare_region(handle, &region) == SAT_OK);
    OK(sat_texture_info(handle, &info) == SAT_OK &&
       info.health == SAT_TEXTURE_READY);
    auto* slot = texture_resolve(g_texture_registry, handle);
    OK(slot != nullptr);
    auto* prepared = texture_find_region(g_texture_registry, handle, region);
    OK(prepared != nullptr);

    // Palette rebinding is logical before the hardware upload: its failure
    // must invalidate every native descriptor without leaking its new bank.
    g_palette_upload_status = SAT_ERR_IO;
    OK(sat_texture_update(handle, &replacement) == SAT_ERR_IO);
    OK(sat_texture_info(handle, &info) == SAT_OK &&
       info.health == SAT_TEXTURE_NEEDS_RECOVERY);
    OK(slot->native.valid == 0u && prepared->native.valid == 0u);
    OK(palette_equal(g_palette_registry.logical_palettes[slot->palette_bank],
                     new_palette));
    OK(slot->source.pixels == original_pixels);
    uint8_t patch_pixels[8u]{};
    sat_surface_t patch{patch_pixels, 8u, 1u, 8u,
                        SAT_PIXEL_INDEX8, new_palette, 256u};
    const sat_rect_t patch_rect{0, 0, 8u, 1u};
    OK(sat_texture_update_rect(handle, &patch_rect, &patch) == SAT_ERR_BUSY);
    OK(sat_texture_prepare_region(handle, &region) == SAT_ERR_BUSY);
    const uint32_t palette_calls = g_palette_uploads;
    g_palette_upload_status = SAT_OK;
    OK(sat_texture_update(handle, &replacement) == SAT_OK);
    OK(g_palette_uploads == palette_calls + 1u); // Force palette repair.
    OK(sat_texture_info(handle, &info) == SAT_OK &&
       info.health == SAT_TEXTURE_READY);
    OK(slot->native.valid == 1u && prepared->native.valid == 1u);
    OK(slot->source.pixels == new_pixels);

    // Parent VRAM transfer can fail after palette ownership has changed.
    g_texture_update_status = SAT_ERR_IO;
    g_fail_texture_update_call = g_texture_updates + 1u;
    OK(sat_texture_update(handle, &final_source) == SAT_ERR_IO);
    OK(slot->native.valid == 0u && prepared->native.valid == 0u);
    OK(palette_equal(g_palette_registry.logical_palettes[slot->palette_bank],
                     final_palette));
    OK(slot->source.pixels == new_pixels);
    g_texture_update_status = SAT_OK;
    g_fail_texture_update_call = 0u;
    OK(sat_texture_update(handle, &final_source) == SAT_OK);
    OK(slot->native.valid == 1u && prepared->native.valid == 1u);
    OK(slot->source.pixels == final_pixels);

    // A prepared-region refresh can fail AFTER parent VRAM was rewritten.
    g_texture_update_status = SAT_ERR_IO;
    g_fail_texture_update_call = g_texture_updates + 2u;
    OK(sat_texture_update(handle, &final_source) == SAT_ERR_IO);
    OK(slot->native.valid == 0u && prepared->native.valid == 0u);
    OK(slot->source.pixels == final_pixels); // Valid recovery source retained.
    g_texture_update_status = SAT_OK;
    g_fail_texture_update_call = 0u;
    OK(sat_texture_update(handle, &final_source) == SAT_OK);
    OK(slot->native.valid == 1u && prepared->native.valid == 1u);

    // Rect writes mutate caller-owned CPU memory first. A failed parent
    // upload explicitly invalidates the GPU copy until a full refresh.
    patch.palette_rgb555 = final_palette;
    patch_pixels[0] = 42u;
    g_texture_update_status = SAT_ERR_IO;
    g_fail_texture_update_call = g_texture_updates + 1u;
    OK(sat_texture_update_rect(handle, &patch_rect, &patch) == SAT_ERR_IO);
    OK(final_pixels[0] == 42u && slot->native.valid == 0u &&
       prepared->native.valid == 0u);
    g_texture_update_status = SAT_OK;
    g_fail_texture_update_call = 0u;
    OK(sat_texture_update(handle, &final_source) == SAT_OK);
    OK(sat_texture_info(handle, &info) == SAT_OK &&
       info.health == SAT_TEXTURE_READY);
    OK(slot->native.valid == 1u && prepared->native.valid == 1u);
    OK(sat_texture_destroy(handle) == SAT_OK);
}

int main() {
    using namespace saturn::core;
    reset_runtime();

    uint16_t palette[256]{};
    make_palette(palette, 0x100u);
    uint8_t pixels[4u * 16u]{};
    sat_surface_t surface{pixels, 8u, 4u, 16u, SAT_PIXEL_INDEX8, palette, 256u};

    sat_texture_t upload_only{};
    OK(sat_texture_create_from_surface(&upload_only, &surface, SAT_TEXTURE_UPLOAD_ONLY) == SAT_OK);
    OK(g_palette_uploads == 1u);
    OK(g_texture_uploads == 1u);
    OK(g_last_pitch == 16u);

    sat_texture_info_t info{};
    OK(sat_texture_info(upload_only, &info) == SAT_OK);
    OK(info.width == 8u && info.height == 4u && info.format == SAT_PIXEL_INDEX8);
    OK(info.backing_policy == SAT_TEXTURE_UPLOAD_ONLY);
    const sat_rect_t region{0, 0, 8, 2};
    OK(sat_texture_prepare_region(upload_only, &region) == SAT_ERR_UNSUPPORTED);

    sat_texture_t persistent{};
    OK(sat_texture_create_from_surface(&persistent, &surface, SAT_TEXTURE_PERSISTENT_SOURCE) == SAT_OK);
    OK(g_palette_uploads == 1u);
    OK(sat_texture_prepare_region(persistent, &region) == SAT_OK);
    OK(g_texture_uploads == 3u);
    OK(sat_texture_prepare_region(persistent, &region) == SAT_OK);
    OK(g_texture_uploads == 3u);
    sat_texture_region_stats_t stats{};
    OK(sat_texture_region_stats(persistent, &stats) == SAT_OK);
    OK(stats.used == 1u && stats.capacity == kTextureRegionCapacity);

    OK(sat_texture_update(persistent, &surface) == SAT_OK);
    OK(g_texture_updates == 2u); /* parent + prepared region, both in place */
    OK(g_texture_uploads == 3u); /* no new VRAM allocation */
    OK(sat_texture_region_stats(persistent, &stats) == SAT_OK && stats.used == 1u);

    uint8_t dynamic_pixels[4u * 12u]{};
    sat_surface_t dynamic_surface{dynamic_pixels, 8u, 4u, 12u, SAT_PIXEL_INDEX8, palette, 256u};
    sat_texture_t dynamic{};
    OK(sat_texture_create_from_surface(&dynamic, &dynamic_surface, SAT_TEXTURE_DYNAMIC) == SAT_OK);
    const sat_rect_t dynamic_region{0, 0, 8u, 4u};
    OK(sat_texture_prepare_region(dynamic, &dynamic_region) == SAT_OK);
    OK(sat_texture_region_stats(dynamic, &stats) == SAT_OK && stats.used == 1u);

    uint8_t patch_pixels[2u * 8u]{};
    patch_pixels[0] = 7u;
    patch_pixels[1] = 8u;
    patch_pixels[8] = 9u;
    patch_pixels[9] = 10u;
    sat_surface_t patch{patch_pixels, 2u, 2u, 8u, SAT_PIXEL_INDEX8, palette, 256u};
    const sat_rect_t dst{3, 1, 2, 2};
    OK(sat_texture_update_rect(dynamic, &dst, &patch) == SAT_OK);
    OK(g_texture_updates == 4u); /* parent + persistent region + parent + dynamic region */
    OK(dynamic_pixels[15u] == 7u && dynamic_pixels[16u] == 8u);
    OK(dynamic_pixels[27u] == 9u && dynamic_pixels[28u] == 10u);

    const sat_texture_t stale = upload_only;
    OK(sat_texture_destroy(upload_only) == SAT_OK);
    OK(sat_texture_info(stale, &info) == SAT_ERR_INVALID_ARG);
    OK(sat_texture_destroy(stale) == SAT_ERR_INVALID_ARG);

    sat_surface_t rgba{pixels, 8u, 4u, 32u, SAT_PIXEL_RGBA8888, nullptr, 0u};
    sat_texture_t invalid{};
    OK(sat_texture_create_from_surface(&invalid, &rgba, SAT_TEXTURE_UPLOAD_ONLY) == SAT_ERR_UNSUPPORTED);

    reset_runtime();
    OK(sat_texture_capacity() == kTextureCapacity);
    sat_texture_t handles[kTextureCapacity]{};
    for (uint16_t i = 0u; i < kTextureCapacity; ++i) {
        OK(sat_texture_create_from_surface(&handles[i], &surface, SAT_TEXTURE_UPLOAD_ONLY) == SAT_OK);
    }
    sat_texture_t overflow{};
    OK(sat_texture_create_from_surface(&overflow, &surface, SAT_TEXTURE_UPLOAD_ONLY) == SAT_ERR_CAPACITY);

    constexpr uint16_t recycled_index = 17u;
    const sat_texture_t recycled_stale = handles[recycled_index];
    const uint16_t recycled_slot = recycled_stale.slot;
    const uint16_t recycled_generation = recycled_stale.generation;
    OK(sat_texture_destroy(handles[recycled_index]) == SAT_OK);

    sat_texture_t recycled{};
    OK(sat_texture_create_from_surface(&recycled, &surface, SAT_TEXTURE_UPLOAD_ONLY) == SAT_OK);
    OK(recycled.slot == recycled_slot);
    OK(recycled.generation != recycled_generation);
    OK(sat_texture_info(recycled_stale, &info) == SAT_ERR_INVALID_ARG);
    handles[recycled_index] = recycled;

    for (uint16_t i = 0u; i < kTextureCapacity; ++i) {
        OK(sat_texture_destroy(handles[i]) == SAT_OK);
    }

    reset_runtime();

    uint8_t cache_pixels[64u * 64u]{};
    sat_surface_t cache_surface{
        cache_pixels, 64u, 64u, 64u, SAT_PIXEL_INDEX8, palette, 256u};
    sat_texture_t cache_owner{};
    OK(sat_texture_create_from_surface(
        &cache_owner, &cache_surface, SAT_TEXTURE_PERSISTENT_SOURCE) == SAT_OK);
    const sat_texture_t cache_owner_stale = cache_owner;

    for (uint16_t y = 0u; y < 8u; ++y) {
        for (uint16_t x = 0u; x < 8u; ++x) {
            const sat_rect_t tile{
                static_cast<int16_t>(x * 8u),
                static_cast<int16_t>(y * 8u),
                8u,
                8u};
            OK(sat_texture_prepare_region(cache_owner, &tile) == SAT_OK);
        }
    }
    OK(texture_region_used(g_texture_registry) == kTextureRegionCapacity);
    OK(sat_texture_region_stats(cache_owner, &stats) == SAT_OK);
    OK(stats.used == kTextureRegionCapacity);

    sat_texture_t cache_other{};
    OK(sat_texture_create_from_surface(
        &cache_other, &cache_surface, SAT_TEXTURE_PERSISTENT_SOURCE) == SAT_OK);
    const sat_rect_t cache_other_region{0, 0, 16u, 8u};
    OK(sat_texture_prepare_region(cache_other, &cache_other_region) == SAT_ERR_CAPACITY);
    OK(sat_texture_region_stats(cache_other, &stats) == SAT_OK && stats.used == 0u);

    const sat_rect_t first_tile{0, 0, 8u, 8u};
    OK(texture_find_region(g_texture_registry, cache_owner, first_tile) != nullptr);
    OK(texture_find_region(g_texture_registry, cache_other, first_tile) == nullptr);

    OK(sat_texture_destroy(cache_owner) == SAT_OK);
    OK(texture_region_used(g_texture_registry) == 0u);
    OK(texture_find_region(g_texture_registry, cache_owner_stale, first_tile) == nullptr);
    OK(sat_texture_prepare_region(cache_other, &cache_other_region) == SAT_OK);
    OK(sat_texture_region_stats(cache_other, &stats) == SAT_OK && stats.used == 1u);

    sat_texture_t cache_reused{};
    OK(sat_texture_create_from_surface(
        &cache_reused, &cache_surface, SAT_TEXTURE_PERSISTENT_SOURCE) == SAT_OK);
    OK(cache_reused.slot == cache_owner_stale.slot);
    OK(cache_reused.generation != cache_owner_stale.generation);
    OK(sat_texture_prepare_region(cache_reused, &first_tile) == SAT_OK);
    OK(texture_find_region(g_texture_registry, cache_reused, first_tile) != nullptr);
    OK(texture_find_region(g_texture_registry, cache_owner_stale, first_tile) == nullptr);

    reset_runtime();
    sat_texture_t vram_limited{};
    OK(sat_texture_create_from_surface(
        &vram_limited, &cache_surface, SAT_TEXTURE_PERSISTENT_SOURCE) == SAT_OK);
    g_texture_upload_status = SAT_ERR_CAPACITY;
    OK(sat_texture_prepare_region(vram_limited, &first_tile) == SAT_ERR_CAPACITY);
    OK(sat_texture_region_stats(vram_limited, &stats) == SAT_OK && stats.used == 0u);
    OK(texture_region_used(g_texture_registry) == 0u);
    g_texture_upload_status = SAT_OK;
    OK(sat_texture_prepare_region(vram_limited, &first_tile) == SAT_OK);
    OK(sat_texture_region_stats(vram_limited, &stats) == SAT_OK && stats.used == 1u);

    update_failure_and_recovery();

    reset_runtime();
    OK(palette_claim_external(g_palette_registry, 0u, kCramWordCount) == SAT_OK);
    OK(sat_texture_create_from_surface(&invalid, &surface, SAT_TEXTURE_UPLOAD_ONLY) == SAT_ERR_CAPACITY);

    std::puts("texture api: OK");
    return 0;
}
