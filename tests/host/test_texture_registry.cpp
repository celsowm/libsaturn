#include <cstdio>
#include <cstdlib>

#include "src/graphics/2d/textures/runtime.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

int main() {
    using namespace saturn::core;

    TextureRegistry registry{};
    texture_registry_reset(registry);

    sat_texture_t handles[kTextureCapacity]{};
    TextureSlot* slot = nullptr;
    for (uint16_t i = 0; i < kTextureCapacity; ++i) {
        OK(texture_allocate_slot(registry, &handles[i], &slot) == SAT_OK);
        OK(slot != nullptr);
        OK(handles[i].slot == i);
        OK(handles[i].generation != 0u);
        OK(texture_resolve(registry, handles[i]) == slot);
    }
    sat_texture_t overflow{};
    OK(texture_allocate_slot(registry, &overflow, &slot) == SAT_ERR_CAPACITY);

    const sat_texture_t stale = handles[0];
    OK(texture_release_slot(registry, handles[0]) == SAT_OK);
    OK(texture_resolve(registry, stale) == nullptr);
    OK(texture_allocate_slot(registry, &handles[0], &slot) == SAT_OK);
    OK(handles[0].slot == stale.slot);
    OK(handles[0].generation != stale.generation);

    const sat_rect_t rect{0, 0, 8, 8};
    TextureRegionRecord* region = nullptr;
    OK(texture_reserve_region(registry, handles[0], rect, &region) == SAT_OK);
    OK(region != nullptr);
    OK(texture_reserve_region(registry, handles[0], rect, &region) == SAT_OK);
    OK(texture_resolve(registry, handles[0])->region_count == 1u);

    uint16_t created = 1u;
    for (uint16_t i = 1u; i < kTextureRegionCapacity; ++i) {
        sat_rect_t r{static_cast<int16_t>(i * 8u), 0, 8, 8};
        OK(texture_reserve_region(registry, handles[0], r, &region) == SAT_OK);
        ++created;
    }
    OK(created == kTextureRegionCapacity);
    const sat_rect_t extra{0, 8, 8, 8};
    OK(texture_reserve_region(registry, handles[0], extra, &region) == SAT_ERR_CAPACITY);
    OK(texture_region_used(registry) == kTextureRegionCapacity);

    texture_invalidate_regions(registry, handles[0]);
    OK(texture_region_used(registry) == 0u);
    OK(texture_resolve(registry, handles[0])->region_count == 0u);
    OK(texture_find_region(registry, handles[0], rect) == nullptr);

    /* Hash removal must permit immediate record reuse without leaving a stale
     * bucket link (which would otherwise create a self-cycle on reinsert). */
    OK(texture_reserve_region(registry, handles[0], rect, &region) == SAT_OK);
    OK(texture_find_region(registry, handles[0], rect) == region);
    texture_cancel_region(registry, handles[0], region);
    OK(texture_find_region(registry, handles[0], rect) == nullptr);
    OK(texture_resolve(registry, handles[0])->region_count == 0u);
    OK(texture_reserve_region(registry, handles[0], rect, &region) == SAT_OK);
    OK(texture_find_region(registry, handles[0], rect) == region);
    texture_invalidate_regions(registry, handles[0]);

    const sat_texture_t before_reset = handles[1];
    texture_registry_reset(registry);
    OK(texture_resolve(registry, before_reset) == nullptr);

    std::puts("texture registry: OK");
    return 0;
}
