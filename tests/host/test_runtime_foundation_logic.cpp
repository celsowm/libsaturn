#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "saturn/color.h"
#include "saturn/geometry2d.h"
#include "saturn/memory.h"
#include "src/core/memory_logic.hpp"
#include "src/core/time_logic.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

int main() {
    sat_rect_t a{10, 10, 20, 20};
    sat_rect_t b{25, 5, 20, 20};
    sat_rect_t overlap{};
    OK(sat_rect_intersect(&a, &b, &overlap) == 1);
    OK(overlap.x == 25 && overlap.y == 10 && overlap.width == 5 && overlap.height == 15);
    OK(sat_rect_contains_point(&a, sat_point_t{10, 10}) == 1);
    OK(sat_rect_contains_point(&a, sat_point_t{30, 30}) == 0);

    const sat_color_t c = sat_color_rgba(1, 2, 3, 4);
    OK(c.r == 1 && c.g == 2 && c.b == 3 && c.a == 4);
    OK(SAT_RGB555(31, 0, 0) == 0x801Fu);

    alignas(16) uint8_t arena_bytes[64]{};
    sat_arena_t arena{};
    OK(saturn::core::memory_logic::arena_init(&arena, arena_bytes, sizeof(arena_bytes)) == SAT_OK);
    void* p1 = saturn::core::memory_logic::arena_alloc(&arena, 3, 1);
    void* p2 = saturn::core::memory_logic::arena_alloc(&arena, 8, 8);
    OK(p1 != nullptr && p2 != nullptr);
    OK((reinterpret_cast<uintptr_t>(p2) & 7u) == 0u);
    OK(arena.high_water >= arena.offset && arena.offset <= sizeof(arena_bytes));
    OK(saturn::core::memory_logic::arena_alloc(&arena, 128, 1) == nullptr);
    const size_t high_water = arena.high_water;
    saturn::core::memory_logic::arena_reset(&arena);
    OK(arena.offset == 0u && arena.high_water == high_water);

    struct Item { uint32_t value; };
    alignas(8) uint8_t pool_bytes[3 * 8]{};
    sat_pool_slot_t slots[3]{};
    sat_pool_t pool{};
    OK(saturn::core::memory_logic::pool_init(&pool, pool_bytes, sizeof(Item), 8, 3, slots) == SAT_OK);
    sat_pool_handle_t h[3]{};
    void* ptr = nullptr;
    for (auto& handle : h) {
        OK(saturn::core::memory_logic::pool_acquire(&pool, &handle, &ptr) == SAT_OK);
        OK(ptr != nullptr);
    }
    sat_pool_handle_t extra{};
    OK(saturn::core::memory_logic::pool_acquire(&pool, &extra, &ptr) == SAT_ERR_CAPACITY);
    const sat_pool_handle_t stale = h[1];
    OK(saturn::core::memory_logic::pool_release(&pool, h[1]) == SAT_OK);
    OK(saturn::core::memory_logic::pool_get(&pool, stale) == nullptr);
    OK(saturn::core::memory_logic::pool_acquire(&pool, &h[1], &ptr) == SAT_OK);
    OK(h[1].generation != stale.generation);
    OK(pool.high_water == 3u);

    using saturn::core::time_logic::elapsed_ms;
    using saturn::core::time_logic::ticks_to_ms;
    OK(elapsed_ms(5u, 0xFFFFFFF0u) == 21u);
    OK(ticks_to_ms(60000u, 1000u, 60u) == 1000u);
    OK(ticks_to_ms(500u, 1000u, 60u) == 8u);

    std::puts("runtime foundation logic: ok");
    return 0;
}
