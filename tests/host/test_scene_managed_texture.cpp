#include <cassert>
#include <cstdio>
#include "saturn/scene.h"
#include "src/graphics/2d/textures/runtime.hpp"

namespace saturn::core {TextureRegistry g_texture_registry={};}
static sat_result_t submit_status=SAT_OK;

extern "C" sat_result_t sat_scene3d_faces_bind_owner_validator(
    sat_scene3d_faces_t* faces,sat_scene3d_owner_validate_fn fn,void* ctx,
    sat_scene3d_owner_span_t* spans,uint16_t capacity) {
    if(!faces || faces->active)return SAT_ERR_INVALID_ARG;
    faces->validate_owner=fn;
    faces->owner_context=ctx;
    faces->owner_spans=spans;
    faces->owner_span_capacity=capacity;
    faces->owner_span_count=0u;
    return SAT_OK;
}
extern "C" uint8_t sat_scene3d_faces_owner_span_available(
    const sat_scene3d_faces_t* faces) {
    return faces->validate_owner &&
           faces->owner_span_count<faces->owner_span_capacity ? 1u : 0u;
}
extern "C" sat_result_t sat_scene3d_faces_mark_owner(
    sat_scene3d_faces_t* faces,uint16_t first,uint16_t count,
    uint16_t slot,uint16_t generation,const sat_vdp1_texture_t* native) {
    if(faces->owner_span_count>=faces->owner_span_capacity)
        return SAT_ERR_CAPACITY;
    faces->owner_spans[faces->owner_span_count++]=
        sat_scene3d_owner_span_t{first,count,slot,generation,native};
    return SAT_OK;
}
extern "C" sat_result_t sat_scene_queue_camera_view_material(
    sat_scene_t* scene,sat_view_cache_t*,uint16_t,
    const sat_camera3d_t*,const sat_scene3d_material_t* material,
    uint16_t) {
    if(submit_status!=SAT_OK)return submit_status;
    for(uint16_t i=0u;i<2u;++i) {
        if(scene->faces.count>=scene->faces.capacity)return SAT_ERR_CAPACITY;
        scene->faces.entries[scene->faces.count].material=*material;
        ++scene->faces.count;
    }
    return SAT_OK;
}

int main() {
    using namespace saturn::core;
    texture_registry_reset(g_texture_registry);
    sat_texture_t owner{};
    TextureSlot* slot=nullptr;
    assert(texture_allocate_slot(g_texture_registry,&owner,&slot)==SAT_OK);
    slot->native.valid=1u;
    slot->native.srca=34u;

    sat_scene3d_face_t storage[2]{};
    sat_scene_t scene{};
    scene.faces.entries=storage;
    scene.faces.capacity=2u;
    sat_view_cache_t cache{};
    sat_camera3d_t camera{};
    assert(sat_scene_queue_managed_camera_view(
        &scene,&cache,0u,&camera,owner,0u)==SAT_ERR_INVALID_ARG);
    sat_scene3d_owner_span_t spans[2]{};
    assert(sat_scene_bind_managed_textures(&scene,nullptr,2u)==SAT_ERR_INVALID_ARG);
    assert(sat_scene_bind_managed_textures(&scene,spans,0u)==SAT_ERR_INVALID_ARG);
    assert(sat_scene_bind_managed_textures(&scene,spans,2u)==SAT_OK);
    assert(scene.faces.validate_owner!=nullptr);
    scene.active=1u;scene.faces.active=1u;
    assert(sat_scene_bind_managed_textures(&scene,spans,2u)==SAT_ERR_INVALID_ARG);
    assert(sat_scene_queue_managed_camera_view(
        &scene,&cache,0u,&camera,owner,0u)==SAT_OK);
    assert(scene.faces.count==2u);
    /* One span for the whole view instead of an owner stamp per face. */
    assert(scene.faces.owner_span_count==1u);
    const sat_scene3d_owner_span_t& span=spans[0];
    assert(span.first==0u && span.count==2u);
    assert(span.slot==owner.slot && span.generation==owner.generation);
    assert(span.native==&slot->native);
    for(uint16_t i=0u;i<2u;++i)
        assert(storage[i].material.texture==&slot->native);
    assert(scene.faces.validate_owner(
        scene.faces.owner_context,span.slot,span.generation,
        span.native)==SAT_OK);
    /* Recycled slot is still valid, but it belongs to a NEW generation. */
    const sat_vdp1_texture_t* borrowed=&slot->native;
    assert(texture_release_slot(g_texture_registry,owner)==SAT_OK);
    sat_texture_t replacement{};
    TextureSlot* new_slot=nullptr;
    assert(texture_allocate_slot(
        g_texture_registry,&replacement,&new_slot)==SAT_OK);
    new_slot->native.valid=1u;
    new_slot->native.srca=77u;
    assert(replacement.slot==owner.slot &&
           replacement.generation!=owner.generation);
    assert(&new_slot->native==borrowed);
    assert(scene.faces.validate_owner(
        scene.faces.owner_context,owner.slot,owner.generation,
        borrowed)==SAT_ERR_INVALID_ARG);
    assert(scene.faces.validate_owner(
        scene.faces.owner_context,replacement.slot,replacement.generation,
        borrowed)==SAT_OK);
    /* Failed view admission must not record any additional owner span. */
    scene.faces.count=0u;
    submit_status=SAT_ERR_CAPACITY;
    assert(sat_scene_queue_managed_camera_view(
        &scene,&cache,0u,&camera,replacement,0u)==SAT_ERR_CAPACITY);
    assert(scene.faces.count==0u && scene.faces.owner_span_count==1u);
    /* A full span table refuses the view BEFORE admitting any face. */
    submit_status=SAT_OK;
    scene.faces.owner_span_count=2u;
    assert(sat_scene_queue_managed_camera_view(
        &scene,&cache,0u,&camera,replacement,0u)==SAT_ERR_CAPACITY);
    assert(scene.faces.count==0u);
    std::puts("managed texture ownership bridge: OK");
}
