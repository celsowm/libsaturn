#include "saturn/scene.h"
#include "src/graphics/2d/textures/runtime.hpp"

namespace {

/* The checked path consumes the existing logical texture registry. It keeps
 * all registry knowledge OUT of the L1 painter and its raw VDP1 materials. */
sat_result_t validate_managed_texture(
    void*,uint16_t owner_slot,uint16_t owner_generation,
    const sat_vdp1_texture_t* expected_native) {
    const sat_texture_t owner{owner_slot,owner_generation};
    const saturn::core::TextureSlot* current=saturn::core::texture_resolve(
        saturn::core::g_texture_registry,owner);
    return current && current->native.valid &&
           &current->native==expected_native
        ? SAT_OK : SAT_ERR_INVALID_ARG;
}

} // namespace

extern "C" sat_result_t sat_scene_bind_managed_textures(sat_scene_t* scene) {
    if (!scene || !scene->faces.entries || !scene->faces.capacity)
        return SAT_ERR_INVALID_ARG;
    return sat_scene3d_faces_bind_owner_validator(
        &scene->faces,validate_managed_texture,nullptr);
}

extern "C" sat_result_t sat_scene_queue_managed_camera_view(
    sat_scene_t* scene,sat_view_cache_t* cache,uint16_t view,
    const sat_camera3d_t* camera,sat_texture_t owner,uint16_t pass) {
    if (!scene || !scene->active || !cache || !camera ||
        scene->faces.validate_owner!=validate_managed_texture)
        return SAT_ERR_INVALID_ARG;
    const saturn::core::TextureSlot* slot=saturn::core::texture_resolve(
        saturn::core::g_texture_registry,owner);
    if (!slot || !slot->native.valid)
        return SAT_ERR_INVALID_ARG;
    const sat_scene3d_material_t material={
        SAT_SCENE3D_INDEXED_TEXTURED,0u,&slot->native,nullptr,
        SAT_INDEXED_SOLID_OPAQUE,nullptr};
    const uint16_t first=scene->faces.count;
    const sat_result_t status=sat_scene_queue_camera_view_material(
        scene,cache,view,camera,&material,pass);
    if (status!=SAT_OK)return status;
    /* The facade's whole-view admission is atomic. Stamp every newly
     * accepted face with the generation observed at submission; a recycled
     * registry slot can no longer silently substitute another native image. */
    for (uint16_t i=first;i<scene->faces.count;++i) {
        scene->faces.entries[i].owner_slot=owner.slot;
        scene->faces.entries[i].owner_generation=owner.generation;
    }
    return SAT_OK;
}
