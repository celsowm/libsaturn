#include "saturn/view_cache.h"
#include "src/graphics/3d/rendering/logic.hpp"

namespace {
/* Fieldwise equality avoids struct padding, hashing collisions and unrelated
 * byte values. This is an exact contract, not epsilon camera tracking. */
bool same_vec(const sat_vec3_t& a,const sat_vec3_t& b) {
    return a.x==b.x && a.y==b.y && a.z==b.z;
}
bool same_camera(const sat_camera3d_t& a,const sat_camera3d_t& b) {
    if (!same_vec(a.eye,b.eye) || !same_vec(a.target,b.target) ||
        !same_vec(a.up,b.up) || a.fov_y!=b.fov_y || a.aspect!=b.aspect ||
        a.near_z!=b.near_z || a.far_z!=b.far_z) return false;
    for (uint8_t i=0u;i<16u;++i)
        if (a.view_proj.m[i]!=b.view_proj.m[i]) return false;
    return true;
}
bool valid_camera_request(const sat_camera3d_t* camera,
                          sat_fx16_t near_depth,uint16_t width,uint16_t height) {
    return camera && near_depth>0 && width>=2u && height>=2u &&
        width<=2048u && height<=2048u;
}
}


extern "C" sat_result_t sat_view_cache_init(sat_view_cache_t* cache,
    sat_view_cache_item_t* storage, uint16_t* counts,
    uint16_t view_count, uint16_t capacity_per_view) {
    if (!cache || !storage || !counts || !view_count || !capacity_per_view) {
        return SAT_ERR_INVALID_ARG;
    }
    cache->storage = storage;
    cache->cameras = nullptr;
    cache->counts = counts;
    cache->view_count = view_count;
    cache->capacity_per_view = capacity_per_view;
    cache->current_view = 0u;
    cache->generation = 0u;
    cache->baked_entries = 0u;
    cache->cache_hits = 0u;
    cache->active = 0u;
    for (uint16_t i = 0u; i < view_count; ++i) counts[i] = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_view_cache_bind_cameras(
    sat_view_cache_t* cache,sat_view_cache_camera_t* slots,
    uint16_t slot_count) {
    if (!cache || !cache->storage || !cache->counts || cache->active ||
        !slots || slot_count<cache->view_count)
        return SAT_ERR_INVALID_ARG;
    cache->cameras=slots;
    cache->baked_entries=cache->cache_hits=0u;
    for (uint16_t i=0u;i<cache->view_count;++i) {
        cache->counts[i]=0u;
        cache->cameras[i].valid=0u;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_view_cache_begin_camera(
    sat_view_cache_t* cache,uint16_t view,const sat_camera3d_t* camera,
    sat_fx16_t near_depth,uint16_t width,uint16_t height) {
    if (!cache || !cache->cameras || !valid_camera_request(
            camera,near_depth,width,height))
        return SAT_ERR_INVALID_ARG;
    const sat_result_t st=sat_view_cache_begin(cache,view);
    if (st!=SAT_OK) return st;
    sat_view_cache_camera_t& slot=cache->cameras[view];
    slot.camera=*camera;
    slot.near_depth=near_depth;
    slot.width=width;
    slot.height=height;
    slot.valid=1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_view_cache_view_camera(
    sat_view_cache_t* cache,uint16_t view,const sat_camera3d_t* camera,
    sat_fx16_t near_depth,uint16_t width,uint16_t height,
    const sat_view_cache_item_t** out_items,uint16_t* out_count) {
    if (!cache || !cache->storage || !cache->counts || !cache->cameras ||
        !out_items || !out_count || cache->active ||
        view>=cache->view_count || !valid_camera_request(
            camera,near_depth,width,height))
        return SAT_ERR_INVALID_ARG;
    *out_items=nullptr;
    *out_count=0u;
    sat_view_cache_camera_t& slot=cache->cameras[view];
    if (!slot.valid || slot.near_depth!=near_depth ||
        slot.width!=width || slot.height!=height ||
        !same_camera(slot.camera,*camera)) {
        slot.valid=0u;
        cache->counts[view]=0u;
        return SAT_ERR_NOT_FOUND;
    }
    return sat_view_cache_view(cache,view,out_items,out_count);
}

extern "C" sat_result_t sat_view_cache_set_generation(sat_view_cache_t* cache,
    uint32_t generation) {
    if (!cache || !cache->storage || cache->active) return SAT_ERR_INVALID_ARG;
    if (cache->generation == generation) return SAT_OK;
    cache->generation = generation;
    cache->baked_entries = 0u;
    cache->cache_hits = 0u;
    for (uint16_t i = 0u; i < cache->view_count; ++i) {
        cache->counts[i] = 0u;
        if (cache->cameras) cache->cameras[i].valid=0u;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_view_cache_begin(sat_view_cache_t* cache, uint16_t view) {
    if (!cache || !cache->storage || view >= cache->view_count) return SAT_ERR_INVALID_ARG;
    cache->current_view = view;
    cache->counts[view] = 0u;
    if (cache->cameras) cache->cameras[view].valid=0u;
    cache->active = 1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_view_cache_append(sat_view_cache_t* cache,
    const sat_quad2_t* quad, uint32_t depth, uint16_t color, uint16_t tag) {
    if (!cache || !cache->active || !quad) return SAT_ERR_INVALID_ARG;
    uint16_t& count = cache->counts[cache->current_view];
    if (count >= cache->capacity_per_view) return SAT_ERR_CAPACITY;
    sat_view_cache_item_t& item = cache->storage[
        static_cast<uint32_t>(cache->current_view) * cache->capacity_per_view + count];
    item.quad = *quad;
    item.depth = depth;
    item.color = color;
    item.tag = tag;
    item.camera_depth=0;
    item.camera_depth_valid=0u;
    ++count;
    ++cache->baked_entries;
    return SAT_OK;
}

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

extern "C" sat_result_t sat_view_cache_sort(sat_view_cache_t* cache) {
    if (!cache || !cache->active) return SAT_ERR_INVALID_ARG;
    sat_view_cache_item_t* list = cache->storage +
        static_cast<uint32_t>(cache->current_view) * cache->capacity_per_view;
    const uint16_t count = cache->counts[cache->current_view];
    for (uint16_t i = 1u; i < count; ++i) {
        const sat_view_cache_item_t value = list[i];
        int j = static_cast<int>(i) - 1;
        while (j >= 0 && list[j].depth < value.depth) {
            list[j + 1] = list[j];
            --j;
        }
        list[j + 1] = value;
    }
    cache->active = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_view_cache_view(sat_view_cache_t* cache, uint16_t view,
    const sat_view_cache_item_t** out_items, uint16_t* out_count) {
    if (!cache || !out_items || !out_count || view >= cache->view_count || cache->active) {
        return SAT_ERR_INVALID_ARG;
    }
    *out_items = cache->storage + static_cast<uint32_t>(view) * cache->capacity_per_view;
    *out_count = cache->counts[view];
    if (cache->counts[view] != 0u) ++cache->cache_hits;
    return SAT_OK;
}

extern "C" sat_result_t sat_view_cache_stats(
    const sat_view_cache_t* cache, sat_view_cache_stats_t* out) {
    if (!cache || !out || !cache->storage || !cache->counts)
        return SAT_ERR_INVALID_ARG;
    out->baked_entries = cache->baked_entries;
    out->cache_hits = cache->cache_hits;
    out->views_ready = 0u;
    for (uint16_t i = 0u; i < cache->view_count; ++i) {
        if (cache->counts[i] != 0u) ++out->views_ready;
    }
    return SAT_OK;
}
