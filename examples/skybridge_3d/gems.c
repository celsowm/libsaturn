/* Skybridge 3D: collectible gems: shared octahedron, batch preparation split across both SH-2s. */
#include "skybridge.h"

/* One immutable local-space gem mesh shared by every collectible; the game
 * updates only its instance position/bob. Renderer owns facet submission. */
static sat_vec3_t g_gem_vertices[GEM_VERTEX_CAP];
static uint16_t g_gem_indices[GEM_FACE_CAP * 4u];
static sat_mesh_t g_gem_mesh;
static uint16_t g_gem_materials[GEM_FACE_CAP];
static sat_mat4_t g_gem_world[SB_PICKUP_COUNT + 1u];
static sat_scene3d_instance_t g_gem_instances[SB_PICKUP_COUNT + 1u];
static sat_projected_vertex_t g_gem_projected[SB_PICKUP_COUNT + 1u][GEM_VERTEX_CAP];
static sat_vec3_t g_gem_world_vertices[SB_PICKUP_COUNT + 1u][GEM_VERTEX_CAP];
static sat_scene3d_prepare_item_t g_gem_batch_items[SB_PICKUP_COUNT];
static sat_scene3d_prepare_item_t g_gem_master_items[SB_PICKUP_COUNT];
static sat_scene3d_prepare_item_t g_gem_slave_items[SB_PICKUP_COUNT];
static sat_scene3d_face_t g_gem_partition_faces[SB_GEM_BATCH_CAP];
static uint32_t g_gem_partition_keys[SB_GEM_BATCH_CAP];
static sat_scene3d_face_t g_gem_master_faces[SB_GEM_BATCH_CAP];
static uint32_t g_gem_master_keys[SB_GEM_BATCH_CAP];
static sat_scene3d_face_t g_gem_slave_faces[SB_GEM_BATCH_CAP];
static uint32_t g_gem_slave_keys[SB_GEM_BATCH_CAP];
static sat_scene3d_prepare_batch_t g_gem_partition_batch;
static sat_scene3d_prepare_batch_t g_gem_master_batch;
static sat_scene3d_prepare_batch_t g_gem_slave_batch;
static sat_parallel_handle_t g_gem_slave_handle;
uint8_t g_gem_slave_pending;
static uint8_t g_gem_master_ready;
static uint8_t g_gem_slave_ready;
static uint8_t g_gem_master_merged;

/* Palette-to-material mapping is baked once at initialization. */
static const uint8_t g_gem_facet_colors[GEM_FACE_CAP]={
    SB_C_GEM_BRIGHT,SB_C_GEM_DEEP,
    SB_C_MARKER_YELLOW,SB_C_GEM_MID,
    SB_C_GEM_BRIGHT,SB_C_GEM_DEEP,
    SB_C_GEM_MID,SB_C_GEM_MID
};

void gems_init_mesh(void) {
    {
        const sat_vec3_t origin={0,0,0};
        sat_example_must(sat_mesh_init(&g_gem_mesh,g_gem_vertices,GEM_VERTEX_CAP,
                                      g_gem_indices,GEM_FACE_CAP));
        sat_example_must(sat_mesh_build_octahedron(&g_gem_mesh,&origin,
                         SB_GEM_RADIUS,SB_GEM_HALF_HEIGHT));
        for(uint8_t face=0u;face<GEM_FACE_CAP;++face)
            g_gem_materials[face]=g_gem_facet_colors[face];
    }
}

void gems_init_batches(void) {
    sat_example_must(sat_scene3d_prepare_batch_init(
        &g_gem_partition_batch,g_gem_batch_items,0u,
        g_gem_partition_faces,g_gem_partition_keys,
        SB_GEM_BATCH_CAP));
}

void gems_bind_materials(void) {
    for(uint8_t id=0u;id<=SB_PICKUP_COUNT;++id) {
        g_gem_instances[id].mesh=&g_gem_mesh;
        g_gem_instances[id].materials=g_scene_materials;
        g_gem_instances[id].material_count=g_solid_pool.count;
        g_gem_instances[id].face_materials=g_gem_materials;
        g_gem_instances[id].world=&g_gem_world[id];
        g_gem_instances[id].pass=SB_PASS_ACTOR;
        g_gem_instances[id].cull_backfaces=0u;
    }
}

static uint8_t merge_gem_batch_direct(sat_scene3d_prepare_batch_t* batch) {
#if SAT_SKYBRIDGE_VALIDATION
    const uint16_t merge_start=sb_test_frt_counter();
#endif
    const uint16_t before=g_scene.faces.count;
    if(sat_scene3d_faces_merge_prepared(&g_scene.faces,batch)!=SAT_OK) {
#if SAT_SKYBRIDGE_VALIDATION
        g_metrics.geometry_merge_frt_ticks+=(uint16_t)(
            sb_test_frt_counter()-merge_start);
#endif
        ++g_metrics.failures;
        return 0u;
    }
    g_scene.submitted_faces=(uint16_t)(g_scene.submitted_faces+
                                       (g_scene.faces.count-before));
    g_scene.culled_faces=g_scene.faces.culled_faces;
    g_scene.clipped_faces=g_scene.faces.clipped_faces;
    g_metrics.prepared_faces=(uint16_t)(g_metrics.prepared_faces+
                                        batch->metrics.prepared_faces);
    record_gem_batch_merge(batch);
#if SAT_SKYBRIDGE_VALIDATION
    g_metrics.geometry_merge_frt_ticks+=(uint16_t)(
        sb_test_frt_counter()-merge_start);
#endif
    return 1u;
}

void prepare_gem_geometry(const uint8_t* deck_slot) {
    uint8_t id;
    uint16_t count=0u;
    const uint32_t start=sat_time_ms();
#if SAT_SKYBRIDGE_VALIDATION
    const uint16_t prepare_start=sb_test_frt_counter();
#endif
    if(g_parallel_recovery_blocked)return;
    g_gem_master_ready=0u;
    g_gem_slave_ready=0u;
    g_gem_master_merged=0u;
    for(id=1u;id<=SB_PICKUP_COUNT;++id) {
        const sb_platform_t* p=&sb_course_platforms(&g_game)[id];
        sat_vec3_t center;
        sat_fx16_t depth;
        if(deck_slot[id]==SAT_FADE3D_SLOT_CULLED ||
           (g_game.pickups&(1u<<(id-1u))))continue;
        center=(sat_vec3_t){sb_platform_x(&g_game,id),
            sb_platform_surface_y(&g_game,id,sb_platform_x(&g_game,id),SB_F(p->z))+
            SB_GEM_BASE_OFFSET+sat_fx16_mul(
                sat_sin_deg(SB_F((int32_t)(g_game.ticks*5u+id*33u)%360)),
                SB_F(1)/2),SB_F(p->z)};
        if(sat_scene_depth(&g_scene,&center,&depth)!=SAT_OK || depth<=0 ||
           sb_abs(center.x-g_game.x)>SB_F(160) ||
           sb_abs(center.z-g_game.z)>SB_F(180))continue;
        if(sat_mat4_translate(&g_gem_world[id],center.x,center.y,center.z)!=SAT_OK) {
            ++g_metrics.failures; continue;
        }
        g_gem_instances[id].world=&g_gem_world[id];
        g_gem_batch_items[count]=(sat_scene3d_prepare_item_t){
            &g_gem_instances[id],g_gem_projected[id],g_gem_world_vertices[id],
            deck_slot[id],0u};
        ++count;
    }
    g_metrics.visible_gems=count;
    if(count==0u) {
#if SAT_SKYBRIDGE_VALIDATION
        g_metrics.geometry_prepare_frt_ticks+=(uint16_t)(
            sb_test_frt_counter()-prepare_start);
#endif
        return;
    }
    g_gem_partition_batch.items=g_gem_batch_items;
    g_gem_partition_batch.item_count=count;
    g_gem_partition_batch.view_proj=g_scene.faces.view_proj;
    g_gem_partition_batch.eye=g_scene.faces.eye;
    g_gem_partition_batch.forward=g_scene.faces.forward;
    g_gem_partition_batch.near_depth=g_scene.faces.near_depth;
    g_gem_partition_batch.width=g_scene.faces.width;
    g_gem_partition_batch.height=g_scene.faces.height;
    {
        uint16_t split=(uint16_t)(count/2u);
#if SAT_SKYBRIDGE_FORCE_GEM_SPLIT
        const uint8_t can_split=(uint8_t)(count>=4u);
#else
        const uint8_t can_split=(sat_parallel_mode()==SAT_PARALLEL_SLAVE &&
                                 sat_parallel_slave_available()!=0u && count>=4u);
#endif
        if(!can_split)split=count;
        g_metrics.master_gem_items=split;
        g_metrics.slave_gem_items=(uint16_t)(count-split);
        if(sat_scene3d_prepare_batch_slice(&g_gem_partition_batch,0u,split,
              g_gem_master_items,g_gem_master_faces,g_gem_master_keys,
              SB_GEM_BATCH_CAP,&g_gem_master_batch)!=SAT_OK) {
            ++g_metrics.failures; return;
        }
        if(split<count) {
            const uint32_t slave_before=parallel_slave_task_count();
            const uint32_t master_before=parallel_master_task_count();
            if(sat_scene3d_prepare_batch_slice(&g_gem_partition_batch,split,
                  (uint16_t)(count-split),g_gem_slave_items,g_gem_slave_faces,
                  g_gem_slave_keys,SB_GEM_BATCH_CAP,
                  &g_gem_slave_batch)!=SAT_OK) {
                ++g_metrics.failures; return;
            }
#if SAT_SKYBRIDGE_VALIDATION
            const uint16_t submit_start=sb_test_frt_counter();
#endif
#if SAT_SKYBRIDGE_FORCE_GEM_SPLIT
            /* Exercise the actual runtime-selected Slave/AUTO path without
             * compiling Skybridge-specific policy into the generic library. */
            g_gem_slave_batch.dispatch=SAT_SCENE3D_PREPARE_DISPATCH_RUNTIME;
#endif
            const sat_result_t gem_submit=sat_scene_prepare_batch_async(
                &g_scene,&g_gem_slave_batch,&g_gem_slave_handle);
#if SAT_SKYBRIDGE_VALIDATION
            g_metrics.task_submit_frt_ticks+=(uint16_t)(
                sb_test_frt_counter()-submit_start);
#endif
#if SAT_SKYBRIDGE_VALIDATION
            g_metrics.task_input_publish_frt_ticks=
                sat_scene3d_test_input_publish_ticks();
#endif
            if(gem_submit!=SAT_OK) {
                ++g_metrics.failures;
                ++g_metrics.geometry_failed;
                /* Submission failed before a handle was accepted, so the
                 * Slave cannot own these buffers. Recover synchronously and
                 * publish readiness only after execution succeeds. */
                if(sat_scene3d_prepare_batch_execute(&g_gem_slave_batch)==SAT_OK) {
                    g_gem_slave_ready=1u;
                    g_metrics.gem_sync_fallback_items=(uint16_t)(count-split);
                }
            } else {
                ++g_metrics.geometry_submitted;
                record_task_dispatch(slave_before,master_before,
                    &g_metrics.geometry_slave_dispatches,
                    &g_metrics.geometry_master_dispatches);
                g_gem_slave_pending=1u;
            }
        }
        if(sat_scene3d_prepare_batch_execute(&g_gem_master_batch)!=SAT_OK) {
            ++g_metrics.failures;
            ++g_metrics.geometry_failed;
        } else {
            g_gem_master_ready=1u;
            g_metrics.master_gem_faces=g_gem_master_batch.metrics.prepared_faces;
        }
    }
#if SAT_SKYBRIDGE_VALIDATION
    g_metrics.geometry_prepare_frt_ticks+=(uint16_t)(
        sb_test_frt_counter()-prepare_start);
#endif
    g_metrics.geometry_ms += sat_time_ms()-start;
}

void finish_gem_geometry(void) {
    if(g_gem_master_ready) {
        g_gem_master_merged=merge_gem_batch_direct(&g_gem_master_batch);
        g_gem_master_ready=0u;
    }
    if(g_gem_slave_ready && g_gem_master_merged) {
        merge_gem_batch_direct(&g_gem_slave_batch);
        g_gem_slave_ready=0u;
    }
    if(g_gem_slave_ready && !g_gem_master_merged) {
        ++g_metrics.failures;
        g_gem_slave_ready=0u;
    }
    if(!g_gem_slave_pending)return;
    if(g_parallel_recovery_blocked)return;
    {
        const uint32_t start=sat_time_ms();
        uint8_t sync_fallback_ready=0u;
        sat_result_t st=sat_parallel_wait(g_gem_slave_handle,SB_GEM_WAIT_TIMEOUT);
        sat_parallel_task_state_t state=sat_parallel_state(g_gem_slave_handle);
        g_metrics.gem_task_state=(uint16_t)state;
        if(st!=SAT_OK && state==SAT_PARALLEL_RUNNING) {
            ++g_metrics.timeouts;
            st=sat_parallel_abort(g_gem_slave_handle,SB_PARALLEL_TIMEOUT);
            state=sat_parallel_state(g_gem_slave_handle);
            g_metrics.gem_task_state=(uint16_t)state;
        }
        if(state==SAT_PARALLEL_RUNNING) {
            parallel_recovery_failed();
            return;
        }
        if(st==SAT_OK && state==SAT_PARALLEL_COMPLETED) {
            ++g_metrics.geometry_completed;
            g_metrics.slave_gem_faces=g_gem_slave_batch.metrics.prepared_faces;
#if SAT_SKYBRIDGE_VALIDATION
            const uint16_t merge_start=sb_test_frt_counter();
#endif
            const sat_result_t merged=(!g_gem_master_merged)?SAT_ERR_BUSY:
                sat_scene_merge_prepared_batch(&g_scene,&g_gem_slave_batch,
                                               g_gem_slave_handle);
#if SAT_SKYBRIDGE_VALIDATION
            g_metrics.geometry_merge_frt_ticks+=(uint16_t)(
                sb_test_frt_counter()-merge_start);
#endif
            if(merged!=SAT_OK) {
                ++g_metrics.failures;
                ++g_metrics.geometry_failed;
            } else {
                g_metrics.prepared_faces=(uint16_t)(g_metrics.prepared_faces+
                    g_gem_slave_batch.metrics.prepared_faces);
                record_gem_batch_merge(&g_gem_slave_batch);
            }
        } else {
            ++g_metrics.failures;
            ++g_metrics.geometry_failed;
            /* A terminal worker error/aborted task no longer owns the
             * buffers. Recompute its suffix on the Master, but never do so
             * while a timed-out worker remains active. */
            if(state==SAT_PARALLEL_FAILED && g_gem_master_merged &&
               sat_scene3d_prepare_batch_execute(&g_gem_slave_batch)==SAT_OK) {
                sync_fallback_ready=1u;
                g_metrics.gem_sync_fallback_items=
                    g_gem_slave_batch.item_count;
            }
        }
#if SAT_SKYBRIDGE_VALIDATION
        const uint16_t release_start=sb_test_frt_counter();
#endif
        const sat_result_t released=sat_scene_prepare_batch_release(
            &g_scene,&g_gem_slave_batch,g_gem_slave_handle);
#if SAT_SKYBRIDGE_VALIDATION
        g_metrics.task_release_frt_ticks+=(uint16_t)(
            sb_test_frt_counter()-release_start);
#endif
        if(released!=SAT_OK) {
            parallel_recovery_failed();
            return;
        }
        g_gem_slave_pending=0u;
        if(sync_fallback_ready) {
            merge_gem_batch_direct(&g_gem_slave_batch);
        }
        g_metrics.wait_ms += sat_time_ms()-start;
    }
}
