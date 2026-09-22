#include "saturn/voxel_terrain.h"

#include <stdint.h>

#include "saturn/math3d.h"
#include "src/physics/spatial/voxel_terrain_logic.hpp"

extern "C" uint32_t sat_voxel_terrain_scratch_bytes(
    uint16_t width, uint16_t view_distance) {
    if (width == 0u || width > 512u ||
        view_distance == 0u || view_distance > 512u) return 0u;
    return 4u * (static_cast<uint32_t>(view_distance) + 1u) +
           2u * static_cast<uint32_t>(width);
}

extern "C" sat_result_t sat_voxel_terrain_height_at(
    const sat_voxel_terrain_t* terrain, sat_fx16_t world_x,
    sat_fx16_t world_z, sat_fx16_t* out_height) {
    if (!saturn::core::voxel::valid_terrain(terrain) || out_height == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint32_t index = saturn::core::voxel::sample_index(
        *terrain, world_x >> 16, world_z >> 16);
    *out_height = static_cast<sat_fx16_t>(
        static_cast<uint32_t>(terrain->heights[index]) *
        terrain->height_scale * 65536u);
    return SAT_OK;
}

extern "C" sat_result_t sat_voxel_terrain_render(
    const sat_voxel_terrain_t* terrain, const sat_voxel_camera_t* camera,
    const sat_voxel_target_t* target, void* scratch,
    uint32_t scratch_bytes) {
    using namespace saturn::core::voxel;
    if (!valid_terrain(terrain) || !valid_camera(camera) ||
        !valid_target(target) || scratch == nullptr ||
        (reinterpret_cast<uintptr_t>(scratch) & 3u) != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint32_t required = sat_voxel_terrain_scratch_bytes(
        target->width, camera->view_distance);
    if (scratch_bytes < required) return SAT_ERR_CAPACITY;

    uint32_t* projection_by_distance = static_cast<uint32_t*>(scratch);
    int16_t* ceiling = reinterpret_cast<int16_t*>(
        projection_by_distance + camera->view_distance + 1u);
    projection_by_distance[0] = 0u;
    for (uint16_t d = 1u; d <= camera->view_distance; ++d) {
        projection_by_distance[d] =
            (static_cast<uint32_t>(camera->projection_scale) << 8u) / d;
    }

    /* Pixel index 0 may be transparent in the palette-sprite backend; the
     * caller selects a solid sky index and owns all presentation semantics. */
    for (uint16_t row = 0u; row < target->height; ++row) {
        uint8_t* pixels = target->pixels + static_cast<uint32_t>(row) * target->pitch;
        for (uint16_t x = 0u; x < target->width; ++x) {
            pixels[x] = target->sky_index;
        }
    }

    const int32_t horizon = static_cast<int32_t>(target->height / 2u) +
                            camera->pitch_pixels;
    const int32_t altitude = camera->y >> 16;
    const sat_fx16_t forward_x = sat_sin_deg(camera->angle);
    const sat_fx16_t forward_z = sat_cos_deg(camera->angle);
    const sat_fx16_t right_x = forward_z;
    const sat_fx16_t right_z = -forward_x;

    /* Near-to-far front-surface extraction: for each column, distant terrain
     * may fill only pixels above the closest terrain already submitted.
     * World coordinates are accumulated in 64 bits to avoid overflow at the
     * edges of the full 16.16 coordinate range. */
    for (uint16_t x = 0u; x < target->width; ++x) {
        int32_t lowest_uncovered = target->height;
        const int32_t centered = static_cast<int32_t>(2u * x + 1u) -
                                 static_cast<int32_t>(target->width);
        const sat_fx16_t lateral = static_cast<sat_fx16_t>(
            (static_cast<int64_t>(centered) * camera->half_fov) /
            target->width);
        const int32_t ray_x = static_cast<int32_t>(
            forward_x + (static_cast<int64_t>(right_x) * lateral >> 16u));
        const int32_t ray_z = static_cast<int32_t>(
            forward_z + (static_cast<int64_t>(right_z) * lateral >> 16u));

        int64_t world_x = camera->x;
        int64_t world_z = camera->z;
        for (uint16_t d = 1u; d <= camera->view_distance; ++d) {
            world_x += ray_x;
            world_z += ray_z;
            const uint32_t index = sample_index(
                *terrain,
                static_cast<int32_t>(world_x >> 16u),
                static_cast<int32_t>(world_z >> 16u));
            const int32_t elevation =
                static_cast<int32_t>(terrain->heights[index]) *
                terrain->height_scale;
            const int32_t screen_y = horizon +
                (((altitude - elevation) *
                  static_cast<int32_t>(projection_by_distance[d])) >> 8u);
            if (screen_y >= lowest_uncovered) continue;
            const int32_t top = screen_y < 0 ? 0 : screen_y;
            if (top < lowest_uncovered) {
                const uint8_t material = terrain->colors[index];
                for (int32_t y = top; y < lowest_uncovered; ++y) {
                    target->pixels[static_cast<uint32_t>(y) *
                                   target->pitch + x] = material;
                }
            }
            lowest_uncovered = top;
            if (lowest_uncovered == 0) break;
        }
        ceiling[x] = static_cast<int16_t>(lowest_uncovered);
    }
    return SAT_OK;
}
