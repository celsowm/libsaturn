#include <cstdio>

#include "saturn/view_cache.h"

static uint32_t project_calls=0u;
static sat_result_t project_status=SAT_OK;
extern "C" sat_result_t sat_project_vertices(
    const sat_mat4_t* vp,const sat_vec3_t* world,
    uint16_t count,sat_projected_vertex_t* out) {
    ++project_calls;
    if (project_status!=SAT_OK) return project_status;
    for (uint16_t i=0u;i<count;++i) {
        out[i].x=static_cast<int16_t>(world[i].x/SAT_FX16_ONE);
        out[i].y=static_cast<int16_t>(world[i].y/SAT_FX16_ONE);
        out[i].w=vp->m[15]-world[i].z;
    }
    return SAT_OK;
}

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

int main() {
    sat_view_cache_item_t storage[6] = {};
    uint16_t counts[2] = {};
    sat_view_cache_t cache{};
    sat_quad2_t quad{};
    const sat_view_cache_item_t* items = nullptr;
    uint16_t count = 0u;

    OK(sat_view_cache_init(&cache, storage, counts, 2u, 3u) == SAT_OK);
    OK(sat_view_cache_set_generation(&cache, 1u) == SAT_OK);
    OK(sat_view_cache_begin(&cache, 0u) == SAT_OK);
    OK(sat_view_cache_append(&cache, &quad, 2u, 20u, 2u) == SAT_OK);
    OK(sat_view_cache_append(&cache, &quad, 5u, 50u, 5u) == SAT_OK);
    OK(sat_view_cache_append(&cache, &quad, 5u, 51u, 6u) == SAT_OK);
    OK(sat_view_cache_append(&cache, &quad, 1u, 10u, 1u) == SAT_ERR_CAPACITY);
    OK(sat_view_cache_sort(&cache) == SAT_OK);
    OK(sat_view_cache_view(&cache, 0u, &items, &count) == SAT_OK);
    OK(count == 3u && items[0].color == 50u && items[1].color == 51u && items[2].color == 20u);
    {
        sat_view_cache_stats_t stats{};
        OK(sat_view_cache_stats(&cache, &stats) == SAT_OK);
        OK(stats.baked_entries == 3u && stats.cache_hits == 1u && stats.views_ready == 1u);
    }
    OK(sat_view_cache_begin(&cache, 1u) == SAT_OK);
    OK(sat_view_cache_append(&cache, &quad, 9u, 90u, 9u) == SAT_OK);
    OK(sat_view_cache_sort(&cache) == SAT_OK);
    OK(sat_view_cache_view(&cache, 1u, &items, &count) == SAT_OK && count == 1u && items[0].tag == 9u);
    {
        sat_view_cache_stats_t stats{};
        OK(sat_view_cache_stats(&cache, &stats) == SAT_OK);
        OK(stats.baked_entries == 4u && stats.cache_hits == 2u && stats.views_ready == 2u);
    }
    OK(sat_view_cache_set_generation(&cache, 2u) == SAT_OK);
    OK(sat_view_cache_view(&cache, 0u, &items, &count) == SAT_OK && count == 0u);
    {
        sat_view_cache_stats_t stats{};
        OK(sat_view_cache_stats(&cache, &stats) == SAT_OK);
        OK(stats.baked_entries == 0u && stats.cache_hits == 0u && stats.views_ready == 0u);
    }
    // Camera-bound caches must not return stale projected corners if any
    // camera matrix, pose, near plane, viewport or content generation changes.
    sat_view_cache_camera_t cameras[2]={};
    sat_camera3d_t camera{};
    camera.eye.z=10*SAT_FX16_ONE;
    camera.target.z=0;
    camera.up.y=SAT_FX16_ONE;
    camera.view_proj.m[0]=SAT_FX16_ONE;
    OK(sat_view_cache_bind_cameras(&cache,cameras,1u)==SAT_ERR_INVALID_ARG);
    OK(sat_view_cache_bind_cameras(&cache,cameras,2u)==SAT_OK);
    OK(sat_view_cache_view_camera(&cache,0u,&camera,SAT_FX16_ONE,
        320u,224u,&items,&count)==SAT_ERR_NOT_FOUND);
    OK(items==nullptr && count==0u);
    OK(sat_view_cache_begin_camera(&cache,0u,&camera,SAT_FX16_ONE,
        320u,224u)==SAT_OK);
    OK(sat_view_cache_append(&cache,&quad,3u,0x801Fu,5u)==SAT_OK);
    OK(sat_view_cache_sort(&cache)==SAT_OK);
    OK(sat_view_cache_view_camera(&cache,0u,&camera,SAT_FX16_ONE,
        320u,224u,&items,&count)==SAT_OK);
    OK(count==1u && items[0].tag==5u);
    camera.view_proj.m[0]+=1;
    OK(sat_view_cache_view_camera(&cache,0u,&camera,SAT_FX16_ONE,
        320u,224u,&items,&count)==SAT_ERR_NOT_FOUND);
    OK(items==nullptr && count==0u && cache.counts[0]==0u);
    camera.view_proj.m[0]-=1;
    OK(sat_view_cache_view_camera(&cache,0u,&camera,SAT_FX16_ONE,
        320u,224u,&items,&count)==SAT_ERR_NOT_FOUND);
    OK(sat_view_cache_begin_camera(&cache,0u,&camera,SAT_FX16_ONE,
        320u,224u)==SAT_OK);
    OK(sat_view_cache_append(&cache,&quad,3u,50u,5u)==SAT_OK);
    OK(sat_view_cache_sort(&cache)==SAT_OK);
    OK(sat_view_cache_view_camera(&cache,0u,&camera,2*SAT_FX16_ONE,
        320u,224u,&items,&count)==SAT_ERR_NOT_FOUND);
    OK(sat_view_cache_begin_camera(&cache,0u,&camera,SAT_FX16_ONE,
        320u,224u)==SAT_OK);
    OK(sat_view_cache_append(&cache,&quad,3u,50u,5u)==SAT_OK);
    OK(sat_view_cache_sort(&cache)==SAT_OK);
    OK(sat_view_cache_view_camera(&cache,0u,&camera,SAT_FX16_ONE,
        640u,224u,&items,&count)==SAT_ERR_NOT_FOUND);
    OK(sat_view_cache_begin_camera(&cache,0u,&camera,SAT_FX16_ONE,
        320u,224u)==SAT_OK);
    OK(sat_view_cache_append(&cache,&quad,3u,50u,5u)==SAT_OK);
    OK(sat_view_cache_sort(&cache)==SAT_OK);
    camera.eye.z+=SAT_FX16_ONE;
    OK(sat_view_cache_view_camera(&cache,0u,&camera,SAT_FX16_ONE,
        320u,224u,&items,&count)==SAT_ERR_NOT_FOUND);
    camera.eye.z-=SAT_FX16_ONE;
    OK(sat_view_cache_begin_camera(&cache,0u,&camera,SAT_FX16_ONE,
        320u,224u)==SAT_OK);
    OK(sat_view_cache_append(&cache,&quad,3u,50u,5u)==SAT_OK);
    OK(sat_view_cache_sort(&cache)==SAT_OK);
    OK(sat_view_cache_set_generation(&cache,3u)==SAT_OK);
    OK(sat_view_cache_view_camera(&cache,0u,&camera,SAT_FX16_ONE,
        320u,224u,&items,&count)==SAT_ERR_NOT_FOUND);
    OK(cache.counts[0]==0u);
    // Legacy begin cannot accidentally bless a new bake as camera-matched.
    OK(sat_view_cache_begin(&cache,0u)==SAT_OK);
    OK(sat_view_cache_append(&cache,&quad,3u,50u,5u)==SAT_OK);
    OK(sat_view_cache_sort(&cache)==SAT_OK);
    OK(sat_view_cache_view_camera(&cache,0u,&camera,SAT_FX16_ONE,
        320u,224u,&items,&count)==SAT_ERR_NOT_FOUND);

    // World-quad bake derives a linear camera depth once from projected W,
    // and stores it independently from the legacy, arbitrary depth key.
    camera.view_proj.m[15]=10*SAT_FX16_ONE;
    const sat_quad3_t world{{{-SAT_FX16_ONE,-SAT_FX16_ONE,2*SAT_FX16_ONE},
                             {SAT_FX16_ONE,-SAT_FX16_ONE,2*SAT_FX16_ONE},
                             {SAT_FX16_ONE,SAT_FX16_ONE,2*SAT_FX16_ONE},
                             {-SAT_FX16_ONE,SAT_FX16_ONE,2*SAT_FX16_ONE}}};
    OK(sat_view_cache_append_world(&cache,&world,22u,1u)==SAT_ERR_INVALID_ARG);
    OK(sat_view_cache_begin_camera(&cache,0u,&camera,SAT_FX16_ONE,
        320u,224u)==SAT_OK);
    OK(sat_view_cache_append_world(&cache,nullptr,22u,1u)==SAT_ERR_INVALID_ARG);
    OK(cache.counts[0]==0u);
    project_status=SAT_ERR_VERIFY_FAILED;
    OK(sat_view_cache_append_world(&cache,&world,22u,1u)==SAT_ERR_VERIFY_FAILED);
    project_status=SAT_OK;
    OK(cache.counts[0]==0u);
    sat_quad3_t hidden=world;
    for (uint8_t i=0u;i<4u;++i) hidden.v[i].z=10*SAT_FX16_ONE;
    OK(sat_view_cache_append_world(&cache,&hidden,22u,1u)==SAT_ERR_UNSUPPORTED);
    OK(cache.counts[0]==0u);
    sat_quad3_t offscreen=world;
    for (uint8_t i=0u;i<4u;++i) offscreen.v[i].x=1000*SAT_FX16_ONE;
    OK(sat_view_cache_append_world(&cache,&offscreen,22u,1u)==SAT_ERR_UNSUPPORTED);
    OK(cache.counts[0]==0u);
    const uint32_t before=project_calls;
    OK(sat_view_cache_append_world(&cache,&world,22u,1u)==SAT_OK);
    OK(project_calls==before+1u && cache.counts[0]==1u);
    OK(sat_view_cache_sort(&cache)==SAT_OK);
    OK(sat_view_cache_view_camera(&cache,0u,&camera,SAT_FX16_ONE,
        320u,224u,&items,&count)==SAT_OK);
    OK(project_calls==before+1u && count==1u);
    OK(items[0].camera_depth_valid==1u);
    OK(items[0].camera_depth==8*SAT_FX16_ONE);
    OK(items[0].depth==static_cast<uint32_t>(8*SAT_FX16_ONE));
    OK(items[0].quad.x[0]==-1 && items[0].quad.y[0]==-1);

    // A legacy append never confers a fabricated linear W to its item.
    OK(sat_view_cache_begin_camera(&cache,0u,&camera,SAT_FX16_ONE,
        320u,224u)==SAT_OK);
    OK(sat_view_cache_append(&cache,&quad,0xFFFFFFFFu,0u,7u)==SAT_OK);
    OK(sat_view_cache_sort(&cache)==SAT_OK);
    OK(sat_view_cache_view_camera(&cache,0u,&camera,SAT_FX16_ONE,
        320u,224u,&items,&count)==SAT_OK);
    OK(count==1u && items[0].camera_depth_valid==0u);

    std::puts("view cache: OK");
    return 0;
}
