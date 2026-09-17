#ifndef SATURN_CORE_RENDER2D_RUNTIME_HPP
#define SATURN_CORE_RENDER2D_RUNTIME_HPP

#include <stdint.h>

#include "saturn/render2d.h"

namespace saturn::core {

constexpr uint16_t kRender2DStackCapacity = 8u;

struct Render2DState {
    sat_camera2d_t camera;
};

struct Render2DRuntime {
    Render2DState current;
    Render2DState stack[kRender2DStackCapacity];
    uint16_t depth;
};

extern Render2DRuntime g_render2d_runtime;

inline bool render2d_camera_valid(const sat_camera2d_t& camera) {
    return camera.zoom > 0;
}

inline bool render2d_camera_is_identity(const sat_camera2d_t& camera) {
    return camera.offset_x == 0 &&
           camera.offset_y == 0 &&
           camera.target_x == 0 &&
           camera.target_y == 0 &&
           camera.rotation == 0 &&
           camera.zoom == SAT_FX16_ONE;
}

inline void render2d_runtime_reset(Render2DRuntime& runtime) {
    runtime.current.camera = sat_camera2d_default();
    runtime.depth = 0u;
}

inline sat_result_t render2d_runtime_push(Render2DRuntime& runtime) {
    if (runtime.depth >= kRender2DStackCapacity) return SAT_ERR_CAPACITY;
    runtime.stack[runtime.depth] = runtime.current;
    ++runtime.depth;
    return SAT_OK;
}

inline sat_result_t render2d_runtime_pop(Render2DRuntime& runtime) {
    if (runtime.depth == 0u) return SAT_ERR_INVALID_ARG;
    --runtime.depth;
    runtime.current = runtime.stack[runtime.depth];
    return SAT_OK;
}

inline sat_result_t render2d_runtime_set_camera(
    Render2DRuntime& runtime,
    const sat_camera2d_t& camera
) {
    if (!render2d_camera_valid(camera)) return SAT_ERR_INVALID_ARG;
    runtime.current.camera = camera;
    return SAT_OK;
}

}  // namespace saturn::core

#endif /* SATURN_CORE_RENDER2D_RUNTIME_HPP */
