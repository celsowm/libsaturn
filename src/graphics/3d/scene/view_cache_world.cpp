#include "saturn/view_cache.h"
#include "src/graphics/3d/rendering/logic.hpp"

/* Opt-in world-geometry baking is its own link unit: the basic view-cache
 * implementation has no implicit renderer/projection dependencies. */
extern "C" sat_result_t sat_view_cache_append_world(
    sat_view_cache_t* cache,const sat_quad3_t* world,
    uint16_t color,uint16_t tag) {
    if (!cache || !cache->active || !cache->cameras || !world ||
        cache->current_view>=cache->view_count)
        return SAT_ERR_INVALID_ARG;
    const sat_view_cache_camera_t& slot=cache->cameras[cache->current_view];
    if (!slot.valid) return SAT_ERR_INVALID_ARG;
    const uint16_t count=cache->counts[cache->current_view];
    if (count>=cache->capacity_per_view) return SAT_ERR_CAPACITY;
    sat_projected_vertex_t projected[4]={};
    const sat_result_t st=sat_project_vertices(
        &slot.camera.view_proj,world->v,4u,projected);
    if (st!=SAT_OK) return st;
    sat_quad2_t screen={};
    int64_t sum=0;
    for (uint8_t i=0u;i<4u;++i) {
        const sat_projected_vertex_t& v=projected[i];
        if (v.w<slot.near_depth ||
            !saturn::core::render3d::coord_drawable(
                v.x,v.y,slot.width,slot.height))
            return SAT_ERR_UNSUPPORTED;
        screen.x[i]=v.x;screen.y[i]=v.y;
        sum+=v.w;
    }
    const sat_fx16_t camera_depth=static_cast<sat_fx16_t>(sum/4);
    const sat_result_t added=sat_view_cache_append(
        cache,&screen,static_cast<uint32_t>(camera_depth),color,tag);
    if (added!=SAT_OK) return added;
    sat_view_cache_item_t& entry=cache->storage[
        static_cast<uint32_t>(cache->current_view)*cache->capacity_per_view+count];
    entry.camera_depth=camera_depth;
    entry.camera_depth_valid=1u;
    return SAT_OK;
}

