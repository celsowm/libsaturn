/* Regression coverage for the caller-owned animation async contract. */

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "saturn/anim3d.h"

extern "C" sat_result_t sat_draw_quad2_sprite(
    const sat_quad2_t*, const sat_vdp1_texture_t*, uint16_t, uint16_t) {
    return SAT_OK;
}
extern "C" sat_result_t sat_draw_quad2_polygon(
    const sat_quad2_t*, uint16_t) { return SAT_OK; }
extern "C" sat_result_t sat_draw_quad2_polygon_gouraud(
    const sat_quad2_t*, uint16_t, const uint16_t*) { return SAT_OK; }
extern "C" sat_result_t sat_draw_world_polygon(
    const sat_mat4_t*, const sat_quad3_t*, uint16_t) { return SAT_OK; }
extern "C" sat_result_t sat_draw_world_polygon_gouraud(
    const sat_mat4_t*, const sat_quad3_t*, uint16_t, const uint16_t*) {
    return SAT_OK;
}
extern "C" sat_result_t sat_draw_world_sprite(
    const sat_mat4_t*, const sat_quad3_t*, const sat_vdp1_texture_t*,
    uint16_t, uint16_t) { return SAT_OK; }
extern "C" sat_result_t sat_project_vertices(
    const sat_mat4_t*, const sat_vec3_t*, uint16_t, sat_projected_vertex_t*) {
    return SAT_ERR_UNSUPPORTED;
}
extern "C" sat_result_t sat_palette_upload_indexed8(
    const uint16_t*, uint16_t) { return SAT_OK; }
extern "C" sat_result_t sat_tex_upload_indexed8_pixels(
    sat_vdp1_texture_t* out, const uint8_t*, uint16_t width, uint16_t height,
    uint16_t palette) {
    out->srca=1u; out->width=width; out->height=height;
    out->palette=palette; out->valid=1u; out->format=SAT_VDP1_TEXTURE_INDEXED8;
    return SAT_OK;
}

static const void* captured_input;
static void* captured_output;
static uint32_t captured_output_capacity;

extern "C" sat_result_t sat_parallel_register_task(
    sat_parallel_task_type_t, sat_parallel_process_fn) {
    return SAT_OK;
}

extern "C" sat_result_t sat_parallel_submit(
    sat_parallel_task_type_t, const void* input, uint32_t input_size,
    void* output, uint32_t output_capacity,
    sat_parallel_handle_t* out_handle) {
    assert(input_size == sizeof(sat_anim_decode_job_t));
    captured_input=input;
    captured_output=output;
    captured_output_capacity=output_capacity;
    *out_handle=0x1234u;
    return SAT_OK;
}

extern "C" sat_result_t sat_parallel_cache_sync_range(
    const void*, uint32_t) {
    return SAT_OK;
}

extern "C" void* sat_parallel_uncached_address(const void* address) {
    return const_cast<void*>(address);
}

namespace {

sat_vec3_t vertices[2]={{0,0,0},{SAT_FX16_ONE,SAT_FX16_ONE,SAT_FX16_ONE}};
uint16_t indices[4]={0,1,1,0};
sat_model_asset_t model={vertices,2u,indices,1u,nullptr,nullptr,0u,
                         nullptr,0u,1u,0u,nullptr,0u,nullptr};
int16_t positions[12]={
    0,0,0, 1000,1000,1000,
    2000,3000,4000, 5000,6000,7000};
sat_model_animation_asset_t clip={positions,2u,2u,30u,1u,
    SAT_ANIM_FLAG_LOOP,0u,{0,0,0,SAT_FX16_ONE,SAT_FX16_ONE,SAT_FX16_ONE},
    nullptr,nullptr};
sat_animated_model_asset_t asset={&model,&clip,1u,0u};

void stack_activity() {
    volatile uint32_t scratch[128]={};
    for(uint32_t i=0u;i<128u;++i) scratch[i]=0xA5A50000u+i;
    (void)scratch[127];
}

}  // namespace

int main() {
    sat_anim_state_t state={};
    assert(sat_anim_state_init(&state,&asset,0u)==SAT_OK);
    state.frame=1u;
    sat_vec3_t expected[2]={};
    assert(sat_anim_decode(&asset,&state,expected,2u)==SAT_OK);

    /* This job is caller-owned and remains valid while the fake worker is
     * intentionally delayed until after the submitting scope has done more
     * stack work. The real Skybridge job follows the same contract with static
     * storage. */
    sat_vec3_t output[2]={};
    sat_anim_decode_job_t job={&asset,&state,output,2u,0u};
    sat_parallel_handle_t handle=0u;
    assert(sat_anim_decode_async(&job,&handle)==SAT_OK);
    stack_activity();
    const sat_anim_decode_job_t* delayed=
        static_cast<const sat_anim_decode_job_t*>(captured_input);
    assert(delayed==&job);
    assert(delayed->asset==&asset && delayed->state==&state);
    assert(captured_output==output &&
           captured_output_capacity==sizeof(output));
    assert(sat_anim_decode(delayed->asset,delayed->state,
                           delayed->output,delayed->vertex_cap)==SAT_OK);
    for(uint16_t i=0u;i<2u;++i) {
        assert(output[i].x==expected[i].x);
        assert(output[i].y==expected[i].y);
        assert(output[i].z==expected[i].z);
    }

    std::puts("PASS: test_anim3d_async_lifetime.cpp");
    return 0;
}
