/* test_vdp1_vram_region.cpp -- streamed texture VRAM (sat_vdp1_vram_*).
 *
 * Links the real VDP1 HAL (src/hal/vdp1/vdp1.cpp) with the SCU-DMA stub, so
 * the arena bookkeeping and the range checks run as they do on the Saturn;
 * only the copy itself is a no-op here.
 */
#include <cassert>
#include <cstdio>
#include <cstdint>

#include "src/hal/vdp1/vdp1.hpp"
#include "src/hal/scu/scu.hpp"

// wait_draw_end's frame clock; these tests never submit a command list.
namespace saturn::hal::scu {
uint16_t ticks_per_frame() { return 0u; }
uint64_t elapsed_ticks() { return 0u; }
}

namespace vd = saturn::hal::vdp1;

int main() {
    static uint8_t source[4096] = {};

    // A reservation is 8-byte aligned and advances the same arena uploads use:
    // an upload after it lands beyond it, never inside.
    uint32_t first = 0u, second = 0u;
    assert(vd::reserve_texture_region(1000u, &first) == SAT_OK);
    assert((first & 7u) == 0u);
    assert(vd::reserve_texture_region(13u, &second) == SAT_OK);
    assert(second == first + 1000u);  // 1000 is already a multiple of 8
    uint16_t srca = 0u;
    assert(vd::upload_texture_indexed8(source, 8u, 8u, &srca) == SAT_OK);
    assert((static_cast<uint32_t>(srca) << 3u) >= second + 16u);

    // Writes inside what was reserved succeed, anywhere and any length.
    assert(vd::write_texture_region(first, source, 1000u) == SAT_OK);
    assert(vd::write_texture_region(first + 3u, source, 5u) == SAT_OK);
    assert(vd::write_texture_region(second, source, 13u) == SAT_OK);

    // Outside the arena, before the texture base or past the cursor: refused.
    assert(vd::write_texture_region(0u, source, 16u) == SAT_ERR_INVALID_ARG);
    assert(vd::write_texture_region(first, source, 512u * 1024u) == SAT_ERR_INVALID_ARG);
    assert(vd::write_texture_region(0xFFFFFFF0u, source, 32u) == SAT_ERR_INVALID_ARG);
    assert(vd::write_texture_region(first, nullptr, 16u) == SAT_ERR_INVALID_ARG);
    assert(vd::write_texture_region(first, source, 0u) == SAT_ERR_INVALID_ARG);

    // Bad arguments and an arena that cannot fit the request.
    uint32_t unused = 0u;
    assert(vd::reserve_texture_region(0u, &unused) == SAT_ERR_INVALID_ARG);
    assert(vd::reserve_texture_region(16u, nullptr) == SAT_ERR_INVALID_ARG);
    assert(vd::reserve_texture_region(512u * 1024u, &unused) == SAT_ERR_CAPACITY);
    // A failed reservation leaves the cursor where it was.
    uint32_t after = 0u;
    assert(vd::reserve_texture_region(8u, &after) == SAT_OK);
    assert(after >= second + 16u);

    // Released texture VRAM is reused. Before this, every upload advanced the
    // arena for good: ikemen_saturn's sprite cache froze the game with
    // SAT_ERR_CAPACITY once ~512 KiB of sprites had gone through it.
    uint16_t held = 0u;
    assert(vd::upload_texture_indexed8(source, 64u, 64u, &held) == SAT_OK);
    vd::release_texture_indexed8(held, 64u, 64u);
    uint16_t again = 0u;
    assert(vd::upload_texture_indexed8(source, 64u, 64u, &again) == SAT_OK);
    assert(again == held);  // the freed span was taken back, not skipped
    vd::release_texture_indexed8(again, 64u, 64u);

    // Many times the arena's size in total traffic, with a few live textures
    // of mixed sizes, never runs out.
    uint16_t live[6] = {};
    uint16_t dims[6] = {8u, 16u, 24u, 32u, 48u, 64u};
    for (int i = 0; i < 6; ++i) {
        assert(vd::upload_texture_indexed8(source, dims[i], 8u, &live[i]) == SAT_OK);
    }
    for (int round = 0; round < 20000; ++round) {
        const int i = round % 6;
        vd::release_texture_indexed8(live[i], dims[i], 8u);
        const uint16_t w = dims[(round * 7 + 3) % 6];
        assert(vd::upload_texture_indexed8(source, w, 8u, &live[i]) == SAT_OK);
        dims[i] = w;  // keep the recorded width in step with what is live
    }

    std::printf("vdp1 vram region: OK\n");
    return 0;
}
