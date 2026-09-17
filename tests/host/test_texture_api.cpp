#include <cstdio>
#include <cstdlib>

#include "saturn/texture.h"
#include "src/core/palette_registry.hpp"
#include "src/core/runtime_state.hpp"
#include "src/core/texture_runtime.hpp"
#include "src/hal/vdp1.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace {
uint16_t g_next_srca = 0x2000u;
uint32_t g_palette_uploads = 0u;
uint32_t g_texture_uploads = 0u;
uint32_t g_texture_updates = 0u;
uint16_t g_last_pitch = 0u;
}

namespace saturn::hal::vdp1 {

sat_result_t upload_palette(const uint16_t*, uint16_t) {
    ++g_palette_uploads;
    return SAT_OK;
}

sat_result_t upload_texture_indexed8_pitched(
    const uint8_t*, uint16_t width, uint16_t height, uint16_t pitch, uint16_t* out_srca) {
    if (out_srca == nullptr || pitch < width || width == 0u || height == 0u) return SAT_ERR_INVALID_ARG;
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
}

static void make_palette(uint16_t* palette, uint16_t seed) {
    for (uint16_t i = 0; i < 256u; ++i) palette[i] = static_cast<uint16_t>(seed + i);
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

    uint8_t patch_pixels[2u * 8u]{};
    patch_pixels[0] = 7u;
    patch_pixels[1] = 8u;
    patch_pixels[8] = 9u;
    patch_pixels[9] = 10u;
    sat_surface_t patch{patch_pixels, 2u, 2u, 8u, SAT_PIXEL_INDEX8, palette, 256u};
    const sat_rect_t dst{3, 1, 2, 2};
    OK(sat_texture_update_rect(dynamic, &dst, &patch) == SAT_OK);
    OK(g_texture_updates == 3u);
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
    OK(palette_claim_external(g_palette_registry, 0u, kCramWordCount) == SAT_OK);
    OK(sat_texture_create_from_surface(&invalid, &surface, SAT_TEXTURE_UPLOAD_ONLY) == SAT_ERR_CAPACITY);

    std::puts("texture api: OK");
    return 0;
}
