/* Reuse the established renderer stubs; exercise the actual staged VDP1 HAL
 * and painter together instead of mocking the traversal being checked. */
#define main ordinary_scene_regressions /* not run here */
#define SAT_TEST_REAL_VDP1_HAL 1
#include "test_scene3d_faces_api.cpp"
#undef main
#include "src/hal/vdp1/vdp1.hpp"

namespace vd=saturn::hal::vdp1;
extern "C" sat_result_t sat_vdp1_command_batch_link(sat_vdp1_command_batch_t* b,uint16_t i) {
    return vd::command_batch_link(b,i);
}
extern "C" sat_result_t sat_vdp1_command_batch_finish(sat_vdp1_command_batch_t* b) {
    return vd::command_batch_finish(b);
}
extern "C" sat_result_t sat_vdp1_command_batch_cancel(sat_vdp1_command_batch_t* b) {
    return vd::command_batch_cancel(b);
}
extern "C" sat_result_t sat_vdp1_command_stats(sat_vdp1_command_stats_t* out) {
    bool overlay=false;
    vd::command_stats(out->used,out->capacity,out->overlay_reserved,overlay);
    out->overlay_pass=overlay?1u:0u;
    return SAT_OK;
}

int main() {
    /* ordinary_scene_regressions needs the program spy (its draw counters);
     * this binary links the real HAL, and test_scene3d_faces_api runs them. */
    vd::Command commands[32]{};
    sat_scene3d_faces_t scene{};
    sat_scene3d_face_t cold[8]{};
    sat_scene3d_projected_face_t hot[8]{};
    uint32_t keys[8]{};uint16_t order[8]{};
    sat_mat4_t vp{};vp.m[0]=SAT_FX16_ONE;
    sat_vec3_t eye{},forward{0,0,SAT_FX16_ONE};
    assert(sat_scene3d_faces_init(&scene,cold,keys,order,8)==SAT_OK);
    assert(sat_scene3d_faces_use_projected_storage(&scene,hot,8)==SAT_OK);
    vd::begin_frame(commands,32);
    assert(vd::reserve_overlay_commands(4)==SAT_OK);
    sat_vdp1_command_batch_t batch{};
    assert(vd::command_batch_reserve(8,&batch)==SAT_OK);
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,SAT_FX16_ONE,320,224)==SAT_OK);
    assert(sat_scene3d_faces_bind_native_commands(&scene,&batch)==SAT_OK);
    sat_scene3d_material_t red{};red.kind=SAT_SCENE3D_RGB;red.rgb555=0x801f;
    red.color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
    const auto near=quad(2),far=quad(4);
    assert(sat_scene3d_faces_submit_quad(&scene,&near,&red,0)==SAT_OK);
    red.rgb555=0x83e0;
    assert(sat_scene3d_faces_submit_quad(&scene,&far,&red,0)==SAT_OK);
    red.rgb555=0xfc00;
    assert(sat_scene3d_faces_submit_quad(&scene,&far,&red,0)==SAT_OK);
    assert(hot[0].reserved==1u && hot[1].reserved==1u && hot[2].reserved==1u);
    sat_scene3d_face_t expanded{};
    assert(sat_scene3d_faces_get(&scene,0,&expanded)==SAT_OK);
    assert(expanded.projected.x[0]==-1 && expanded.projected.y[2]==1);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    assert(!batch.active && !vd::command_batch_pending());
    // Equal-depth source order remains green, blue, followed by near red.
    assert(commands[1].link==3u*4u);
    assert(commands[3].colr==0x83e0 && commands[3].link==4u*4u);
    assert(commands[4].colr==0xfc00 && commands[4].link==2u*4u);
    assert(commands[2].colr==0x801f && commands[2].link==10u*4u);
    assert(scene.emitted_faces==3);
    assert(vd::begin_overlay_pass()==SAT_OK);

    // Borrowed residency failing at flush cancels the entire arena and
    // leaves the prefix/HUD budget intact. No native draw becomes reachable.
    vd::begin_frame(commands,32);
    assert(vd::reserve_overlay_commands(4)==SAT_OK);
    assert(vd::command_batch_reserve(8,&batch)==SAT_OK);
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,SAT_FX16_ONE,320,224)==SAT_OK);
    assert(sat_scene3d_faces_bind_native_commands(&scene,&batch)==SAT_OK);
    sat_vdp1_texture_t texture{30,8,8,0,1,SAT_VDP1_TEXTURE_INDEXED8};
    sat_scene3d_material_t material{};material.kind=SAT_SCENE3D_INDEXED_TEXTURED;
    material.texture=&texture;material.color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
    assert(sat_scene3d_faces_submit_quad(&scene,&far,&material,0)==SAT_OK);
    texture.valid=0;
    assert(sat_scene3d_faces_flush(&scene)==SAT_ERR_INVALID_ARG);
    assert(!vd::command_batch_pending() && !batch.active);
    uint16_t used=0,capacity=0,reserved=0;bool overlay=false;
    vd::command_stats(used,capacity,reserved,overlay);
    assert(used==2 && reserved==4 && (commands[1].ctrl&0x7000u)==0);
    puts("Native staged scene: stable painter links, coordinates, residency and rollback OK");
}
