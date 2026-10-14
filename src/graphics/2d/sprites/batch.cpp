#include "saturn/sprite_batch.h"

#include "src/core/runtime/logic.hpp"
#include "src/hal/vdp1/vdp1.hpp"

/* Frame preparation and the public entry of the bulk sprite path. The per
 * instance work is in hal::vdp1::push_sprite_batch. */

namespace {
/* What a batch frame may carry: the flags that change only command words
 * the frame holds. Tint, blend, camera, scale, rotation and user clip need
 * the general sat_draw_texture path. */
constexpr uint16_t kBatchFlags = SAT_SPRITE_FLAG_OPAQUE | SAT_SPRITE_FLAG_MESH |
                                 SAT_SPRITE_FLAG_FLIP_H | SAT_SPRITE_FLAG_FLIP_V;
}  // namespace

extern "C" sat_result_t sat_sprite_frame_init_raw(
    sat_sprite_frame_t* frame, uint16_t srca, uint16_t width, uint16_t height,
    uint16_t format, uint16_t palette, uint16_t flags,
    int16_t pivot_x, int16_t pivot_y) {
    if (frame == nullptr) return SAT_ERR_INVALID_ARG;
    if (width == 0u || height == 0u || (width & 7u) != 0u || height > 255u) return SAT_ERR_INVALID_ARG;
    if (format != SAT_VDP1_TEXTURE_INDEXED8 && format != SAT_VDP1_TEXTURE_LUT4) return SAT_ERR_INVALID_ARG;
    if ((flags & ~kBatchFlags) != 0u) return SAT_ERR_UNSUPPORTED;
    if (pivot_x < 0 || pivot_y < 0 || pivot_x >= static_cast<int32_t>(width) ||
        pivot_y >= static_cast<int32_t>(height)) {
        return SAT_ERR_INVALID_ARG;
    }
    frame->ctrl = saturn::core::compose_sprite_dir(flags);
    frame->pmod = saturn::core::compose_sprite_pmod(flags, format);
    frame->colr = saturn::core::compose_sprite_colr(palette, format);
    frame->srca = srca;
    frame->size = static_cast<uint16_t>(((width / 8u) << 8u) | height);
    frame->width = width;
    frame->height = height;
    frame->pivot_x = pivot_x;
    frame->pivot_y = pivot_y;
    return SAT_OK;
}

extern "C" sat_result_t sat_sprite_frame_init(
    sat_sprite_frame_t* frame, const sat_vdp1_texture_t* texture, uint16_t flags,
    int16_t pivot_x, int16_t pivot_y) {
    if (texture == nullptr || texture->valid == 0u) return SAT_ERR_INVALID_ARG;
    return sat_sprite_frame_init_raw(frame, texture->srca, texture->width, texture->height,
                                     texture->format, texture->palette, flags, pivot_x, pivot_y);
}

extern "C" sat_result_t sat_sprite_batch_draw(
    const sat_sprite_frame_t* frames, uint32_t frame_count,
    const sat_sprite_instance_t* instances, uint32_t count,
    uint32_t options, sat_sprite_batch_stats_t* stats) {
    if (stats != nullptr) *stats = sat_sprite_batch_stats_t{};
    if ((options & ~(SAT_SPRITE_BATCH_KEEP_PRECLIP | SAT_SPRITE_BATCH_NO_CULL)) != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    return saturn::hal::vdp1::push_sprite_batch(frames, frame_count, instances, count, options, stats);
}
