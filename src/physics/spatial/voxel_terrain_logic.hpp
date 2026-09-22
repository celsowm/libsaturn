#ifndef SATURN_CORE_VOXEL_TERRAIN_LOGIC_HPP
#define SATURN_CORE_VOXEL_TERRAIN_LOGIC_HPP

#include <stdint.h>
#include "saturn/voxel_terrain.h"

namespace saturn::core::voxel {

inline bool valid_terrain(const sat_voxel_terrain_t* terrain) {
    return terrain != nullptr && terrain->heights != nullptr &&
        terrain->colors != nullptr && terrain->width != 0u &&
        terrain->height != 0u && terrain->pitch >= terrain->width &&
        terrain->height_scale >= 1u && terrain->height_scale <= 16u &&
        terrain->edge_mode <= SAT_VOXEL_EDGE_WRAP;
}

/* A power-of-two map uses a mask even for negative world coordinates.
 * Non-power-of-two maps use signed remainder, corrected to [0,size). */
inline uint16_t coordinate(int32_t value, uint16_t size, uint8_t edge_mode) {
    if (edge_mode == SAT_VOXEL_EDGE_CLAMP) {
        if (value < 0) return 0u;
        if (value >= static_cast<int32_t>(size)) return static_cast<uint16_t>(size - 1u);
        return static_cast<uint16_t>(value);
    }
    if ((size & static_cast<uint16_t>(size - 1u)) == 0u) {
        return static_cast<uint16_t>(static_cast<uint32_t>(value) & (size - 1u));
    }
    int32_t index = value % static_cast<int32_t>(size);
    if (index < 0) index += size;
    return static_cast<uint16_t>(index);
}

inline uint32_t sample_index(
    const sat_voxel_terrain_t& terrain, int32_t x, int32_t z) {
    const uint16_t mx = coordinate(x, terrain.width, terrain.edge_mode);
    const uint16_t mz = coordinate(z, terrain.height, terrain.edge_mode);
    return static_cast<uint32_t>(mz) * terrain.pitch + mx;
}

inline bool valid_camera(const sat_voxel_camera_t* camera) {
    return camera != nullptr && camera->half_fov > 0 &&
        camera->half_fov <= SAT_FX16_ONE &&
        camera->projection_scale >= 1u && camera->projection_scale <= 512u &&
        camera->view_distance >= 1u && camera->view_distance <= 512u &&
        camera->y >= 0 && camera->y <= (4096 * SAT_FX16_ONE);
}

inline bool valid_target(const sat_voxel_target_t* target) {
    return target != nullptr && target->pixels != nullptr &&
        target->width >= 1u && target->width <= 512u &&
        target->height >= 1u && target->height <= 256u &&
        target->pitch >= target->width;
}
}  // namespace saturn::core::voxel
#endif /* SATURN_CORE_VOXEL_TERRAIN_LOGIC_HPP */
