#include "saturn/anim3d.h"

#include "src/graphics/3d/animation/logic.hpp"

extern "C" sat_result_t sat_anim_validate(const sat_animated_model_asset* asset) {
    return saturn::core::anim3d::validate(asset);
}

extern "C" sat_result_t sat_anim_clip_validate(
    const sat_animated_model_asset* asset,
    uint16_t clip
) {
    return saturn::core::anim3d::validate_clip(asset, clip);
}

extern "C" sat_fx16_t sat_anim_clip_duration(
    const sat_animated_model_asset* asset,
    uint16_t clip
) {
    const sat_model_animation_asset* anim = saturn::core::anim3d::clip_at(asset, clip);
    if (anim == nullptr || anim->sample_rate_num == 0u || anim->sample_rate_den == 0u ||
        anim->frame_count == 0u) {
        return 0;
    }
    return saturn::core::anim3d::clip_duration_fx(anim);
}

extern "C" sat_result_t sat_anim_state_init(
    sat_anim_state_t* state,
    const sat_animated_model_asset* asset,
    uint16_t clip
) {
    return saturn::core::anim3d::state_init(state, asset, clip);
}

extern "C" sat_result_t sat_anim_set_clip(
    sat_anim_state_t* state,
    const sat_animated_model_asset* asset,
    uint16_t clip
) {
    return saturn::core::anim3d::state_init(state, asset, clip);
}

extern "C" sat_result_t sat_anim_reset(sat_anim_state_t* state) {
    if (state == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    state->frame = 0u;
    state->time = 0;
    return SAT_OK;
}

extern "C" void sat_anim_set_paused(sat_anim_state_t* state, int paused) {
    if (state == nullptr) {
        return;
    }
    if (paused != 0) {
        state->flags = static_cast<uint16_t>(state->flags | SAT_ANIM_STATE_PAUSED);
    } else {
        state->flags = static_cast<uint16_t>(
            state->flags & static_cast<uint16_t>(~SAT_ANIM_STATE_PAUSED));
    }
}

extern "C" int sat_anim_is_paused(const sat_anim_state_t* state) {
    if (state == nullptr) {
        return 0;
    }
    return (state->flags & SAT_ANIM_STATE_PAUSED) != 0u ? 1 : 0;
}

extern "C" sat_result_t sat_anim_advance(
    sat_anim_state_t* state,
    const sat_animated_model_asset* asset,
    sat_fx16_t dt
) {
    return saturn::core::anim3d::advance(state, asset, dt);
}

extern "C" sat_result_t sat_anim_decode(
    const sat_animated_model_asset* asset,
    const sat_anim_state_t* state,
    sat_vec3_t* out_vertices,
    uint16_t vertex_cap
) {
    return saturn::core::anim3d::decode(asset, state, out_vertices, vertex_cap);
}

extern "C" sat_result_t sat_anim_face_colors(
    const sat_animated_model_asset* asset,
    const sat_anim_state_t* state,
    uint16_t* out_colors,
    uint16_t color_cap
) {
    return saturn::core::anim3d::face_colors(asset, state, out_colors, color_cap);
}

extern "C" sat_result_t sat_anim_vertex_gouraud(
    const sat_animated_model_asset* asset,
    const sat_anim_state_t* state,
    uint16_t* out_gouraud,
    uint16_t gouraud_cap
) {
    return saturn::core::anim3d::vertex_gouraud(asset, state, out_gouraud, gouraud_cap);
}

namespace {

sat_result_t parallel_decode(
    const void* input, uint32_t input_size, void* output,
    uint32_t output_capacity, uint32_t* output_size) {
    if (input == nullptr || input_size != sizeof(sat_anim_decode_job_t) ||
        output == nullptr || output_size == nullptr) return SAT_ERR_INVALID_ARG;
    const sat_anim_decode_job_t* const job =
        static_cast<const sat_anim_decode_job_t*>(input);
    if (job->vertex_cap == 0u ||
        output_capacity < static_cast<uint32_t>(job->vertex_cap) * sizeof(sat_vec3_t)) {
        return SAT_ERR_CAPACITY;
    }
    const sat_result_t result = sat_anim_decode(
        job->asset, job->state, static_cast<sat_vec3_t*>(output),
        job->vertex_cap);
    if (result == SAT_OK) {
        *output_size = static_cast<uint32_t>(job->vertex_cap) * sizeof(sat_vec3_t);
    }
    return result;
}

}  // namespace

extern "C" sat_result_t sat_anim_parallel_register(void) {
    return sat_parallel_register_task(SAT_PARALLEL_TASK_ANIMATION_DECODE,
                                      &parallel_decode);
}

extern "C" sat_result_t sat_anim_decode_async(
    const sat_anim_decode_job_t* job, sat_parallel_handle_t* out_handle) {
    if (job == nullptr || out_handle == nullptr || job->output == nullptr ||
        job->vertex_cap == 0u) return SAT_ERR_INVALID_ARG;
    return sat_parallel_submit(
        SAT_PARALLEL_TASK_ANIMATION_DECODE, job,
        static_cast<uint32_t>(sizeof(*job)), job->output,
        static_cast<uint32_t>(job->vertex_cap) * sizeof(sat_vec3_t), out_handle);
}

extern "C" sat_result_t sat_anim_prepare_model_instance(
    const sat_animated_model_asset_t* asset,
    const sat_anim_state_t* state,
    const sat_mat4_t* world,
    sat_mesh_t* mesh,
    uint16_t* out_face_materials,
    uint16_t material_capacity,
    uint16_t material_count
) {
    if(asset==nullptr || state==nullptr || world==nullptr || mesh==nullptr ||
       mesh->vertices==nullptr || mesh->indices==nullptr ||
       sat_anim_clip_validate(asset,state->clip)!=SAT_OK)
        return SAT_ERR_INVALID_ARG;
    const sat_model_asset_t* model=asset->model;
    const sat_model_animation_asset_t* clip=&asset->animations[state->clip];
    if(state->frame>=clip->frame_count ||
       mesh->vertex_count!=model->vertex_count ||
       mesh->face_count!=model->face_count)
        return SAT_ERR_INVALID_ARG;
    if(mesh->vertex_cap<model->vertex_count || mesh->face_cap<model->face_count)
        return SAT_ERR_CAPACITY;
    if(out_face_materials!=nullptr) {
        if(clip->face_shades==nullptr) return SAT_ERR_UNSUPPORTED;
        if(material_capacity<model->face_count) return SAT_ERR_CAPACITY;
        if(material_count==0u) return SAT_ERR_INVALID_ARG;
        const uint8_t* const shades=&clip->face_shades[
            static_cast<uint32_t>(state->frame)*model->face_count];
        /* Preflight the complete material table before mutating pose data. */
        for(uint16_t face=0u;face<model->face_count;++face)
            if(shades[face]>=material_count) return SAT_ERR_INVALID_ARG;
    }
    SAT_TRY(sat_anim_decode(asset,state,mesh->vertices,mesh->vertex_cap));
    SAT_TRY(sat_mesh_transform(mesh,world));
    if(out_face_materials!=nullptr) {
        const uint8_t* const shades=&clip->face_shades[
            static_cast<uint32_t>(state->frame)*model->face_count];
        for(uint16_t face=0u;face<model->face_count;++face)
            out_face_materials[face]=shades[face];
    }
    return SAT_OK;
}
