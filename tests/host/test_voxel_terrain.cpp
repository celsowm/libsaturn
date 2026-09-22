#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "saturn/voxel_terrain.h"
#include "src/physics/spatial/voxel_terrain_logic.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

int main() {
    using saturn::core::voxel::coordinate;
    OK(coordinate(-1, 8u, SAT_VOXEL_EDGE_WRAP) == 7u);
    OK(coordinate(9, 8u, SAT_VOXEL_EDGE_WRAP) == 1u);
    OK(coordinate(-1, 3u, SAT_VOXEL_EDGE_WRAP) == 2u);
    OK(coordinate(4, 3u, SAT_VOXEL_EDGE_WRAP) == 1u);
    OK(coordinate(-5, 3u, SAT_VOXEL_EDGE_CLAMP) == 0u);
    OK(coordinate(9, 3u, SAT_VOXEL_EDGE_CLAMP) == 2u);

    uint8_t height[64]{};
    uint8_t color[64]{};
    for (uint32_t i = 0u; i < 64u; ++i) color[i] = 3u;
    sat_voxel_terrain_t terrain{height, color, 8u, 8u, 8u, 1u, SAT_VOXEL_EDGE_WRAP};
    sat_fx16_t ground = -1;
    height[7u + 7u * 8u] = 5u;
    OK(sat_voxel_terrain_height_at(&terrain, -65536, -65536, &ground) == SAT_OK);
    OK(ground == 5 * SAT_FX16_ONE);
    terrain.edge_mode = SAT_VOXEL_EDGE_CLAMP;
    OK(sat_voxel_terrain_height_at(&terrain, -65536, -65536, &ground) == SAT_OK);
    OK(ground == 0);
    terrain.edge_mode = SAT_VOXEL_EDGE_WRAP;
    terrain.height_scale = 3u;
    OK(sat_voxel_terrain_height_at(&terrain, -65536, -65536, &ground) == SAT_OK);
    OK(ground == 15 * SAT_FX16_ONE);
    terrain.height_scale = 1u;
    height[63] = 0u;

    uint8_t buffer[8u * 10u];
    std::memset(buffer, 0xEE, sizeof(buffer));
    sat_voxel_target_t target{buffer, 8u, 8u, 10u, 1u};
    sat_voxel_camera_t camera{};
    camera.x = 4 * SAT_FX16_ONE;
    camera.y = 2 * SAT_FX16_ONE;
    camera.z = 4 * SAT_FX16_ONE;
    camera.half_fov = SAT_FX16_ONE / 2;
    camera.projection_scale = 4u;
    camera.view_distance = 8u;
    alignas(4) uint8_t scratch[4u * 513u + 2u * 512u]{};
    const uint32_t needed = sat_voxel_terrain_scratch_bytes(8u, 8u);
    OK(needed == 52u);
    OK(sat_voxel_terrain_render(&terrain, &camera, &target,
                                scratch, needed - 1u) == SAT_ERR_CAPACITY);
    OK(buffer[0] == 0xEE);
    OK(sat_voxel_terrain_render(&terrain, &camera, &target,
                                scratch, needed) == SAT_OK);
    for (uint16_t x = 0u; x < target.width; ++x) {
        OK(buffer[x] == 1u);
        OK(buffer[5u * target.pitch + x] == 3u);
        OK(buffer[7u * target.pitch + x] == 3u);
    }
    for (uint16_t row = 0u; row < target.height; ++row) {
        OK(buffer[static_cast<uint32_t>(row) * target.pitch + 8u] == 0xEE);
        OK(buffer[static_cast<uint32_t>(row) * target.pitch + 9u] == 0xEE);
    }
    camera.pitch_pixels = -3;
    OK(sat_voxel_terrain_render(&terrain, &camera, &target,
                                scratch, needed) == SAT_OK);
    OK(buffer[2u * target.pitch + 4u] == 3u);
    camera.pitch_pixels = 0;
    target.width = 513u;
    OK(sat_voxel_terrain_render(&terrain, &camera, &target,
                                scratch, sizeof(scratch)) == SAT_ERR_INVALID_ARG);
    target.width = 8u;
    camera.view_distance = 0u;
    OK(sat_voxel_terrain_render(&terrain, &camera, &target,
                                scratch, sizeof(scratch)) == SAT_ERR_INVALID_ARG);
    camera.view_distance = 8u;
    terrain.pitch = 7u;
    OK(sat_voxel_terrain_render(&terrain, &camera, &target,
                                scratch, sizeof(scratch)) == SAT_ERR_INVALID_ARG);
    terrain.pitch = 8u;
    terrain.edge_mode = 2u;
    OK(sat_voxel_terrain_height_at(&terrain, 0, 0, &ground) == SAT_ERR_INVALID_ARG);
    std::puts("voxel terrain: OK");
    return 0;
}
