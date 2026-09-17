#include "src/core/render2d_runtime.hpp"

#include "src/core/render2d_logic.hpp"
#include "src/hal/vdp1.hpp"

namespace saturn::core {

Render2DRuntime g_render2d_runtime{};

sat_result_t render2d_emit_clip_state(
    const Render2DState& state,
    uint16_t screen_width,
    uint16_t screen_height
) {
    if (state.clip_enabled == 0u) return SAT_OK;
    Render2DClip clip{};
    const sat_result_t st =
        resolve_render2d_clip(&state.clip, screen_width, screen_height, &clip);
    if (st != SAT_OK) return st;

    saturn::hal::vdp1::UserClipRequest request{};
    request.x0 = clip.x0;
    request.y0 = clip.y0;
    request.x1 = clip.x1;
    request.y1 = clip.y1;
    return saturn::hal::vdp1::push_user_clip(request);
}

sat_result_t render2d_ensure_clip(uint16_t screen_width, uint16_t screen_height) {
    if (g_render2d_runtime.current.clip_enabled == 0u ||
        g_render2d_runtime.clip_dirty == 0u) {
        return SAT_OK;
    }
    const sat_result_t st = render2d_emit_clip_state(
        g_render2d_runtime.current, screen_width, screen_height);
    if (st == SAT_OK) g_render2d_runtime.clip_dirty = 0u;
    return st;
}

}  // namespace saturn::core
