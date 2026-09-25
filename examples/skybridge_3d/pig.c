/* Skybridge 3D: the imported pig: animation decode (Master or Slave) and submission. */
#include "skybridge.h"

static sat_vec3_t g_pig_vertices[PIG_VERTEX_CAP];
static uint16_t g_pig_indices[PIG_FACE_CAP*4u];
static sat_mesh_t g_pig_mesh;
 static sat_projected_vertex_t g_pig_projected[PIG_VERTEX_CAP];
static uint16_t g_pig_face_textures[PIG_FACE_CAP];
sat_anim_state_t g_pig_anim;
sat_anim_state_t g_pig_render_anim;
/* The executor retains the complete input graph until terminal completion and
 * release. These are deliberately static: a stack descriptor would become
 * invalid as soon as submit_pig_animation() returned. */
static sat_anim_decode_job_t g_pig_anim_job;
static sat_anim_state_t g_pig_anim_job_state;
static uint8_t g_pig_anim_write_buffer;
static sat_scene3d_instance_t g_pig_instance;
static sat_vec3_t g_pig_pose[2][PIG_VERTEX_CAP];
static uint8_t g_pig_render_buffer;
static sat_parallel_handle_t g_pig_anim_handle;
static uint8_t g_pig_anim_pending;

void pig_init(void) {
    sat_example_must(sat_model_validate(&skybridge_pig_asset));
    sat_example_must(sat_anim_validate(&skybridge_pig_anim_asset));
    sat_example_must(sat_mesh_init(&g_pig_mesh,
        g_pig_vertices,PIG_VERTEX_CAP,g_pig_indices,PIG_FACE_CAP));
    sat_example_must(sat_model_copy_to_mesh(&skybridge_pig_asset,&g_pig_mesh));
    sat_example_must(sat_anim_state_init(
        &g_pig_anim,&skybridge_pig_anim_asset,1u)); /* Idle */
    {
        sat_mat4_t identity={0};
        identity.m[0]=identity.m[5]=identity.m[10]=identity.m[15]=SAT_FX16_ONE;
        sat_example_must(sat_anim_prepare_model_instance(
            &skybridge_pig_anim_asset,&g_pig_anim,&identity,&g_pig_mesh,
            g_pig_face_textures,PIG_FACE_CAP,SKYBRIDGE_PIG_SHADE_COUNT));
    }
    g_pig_render_anim=g_pig_anim;
    sat_example_must(sat_anim_decode(&skybridge_pig_anim_asset,&g_pig_anim,
                                    g_pig_pose[0],PIG_VERTEX_CAP));
    g_pig_render_buffer=0u;
}

#if SAT_SKYBRIDGE_VALIDATION
uint32_t animation_pose_hash(void) {
    uint32_t hash=2166136261u;
    const sat_vec3_t* pose=g_pig_pose[g_pig_render_buffer];
    hash=hash_word(hash,g_pig_render_anim.clip);
    hash=hash_word(hash,g_pig_render_anim.frame);
    hash=hash_word(hash,(uint32_t)g_pig_render_anim.time);
    for(uint16_t i=0u;i<PIG_VERTEX_CAP;++i) {
        hash=hash_word(hash,(uint32_t)pose[i].x);
        hash=hash_word(hash,(uint32_t)pose[i].y);
        hash=hash_word(hash,(uint32_t)pose[i].z);
    }
    return hash;
}
#endif

void submit_pig_animation(void) {
    const uint8_t write_buffer=(uint8_t)(1u-g_pig_render_buffer);
    const uint32_t slave_before=parallel_slave_task_count();
    const uint32_t master_before=parallel_master_task_count();
    if(g_pig_anim_pending || g_parallel_recovery_blocked)return;
    g_pig_anim_job_state=g_pig_anim;
    g_pig_anim_write_buffer=write_buffer;
    g_pig_anim_job=(sat_anim_decode_job_t){&skybridge_pig_anim_asset,
        &g_pig_anim_job_state,g_pig_pose[write_buffer],PIG_VERTEX_CAP,0u};
#if SAT_SKYBRIDGE_VALIDATION
    const uint16_t submit_start=sb_test_frt_counter();
#endif
    const sat_result_t submitted=
        sat_anim_decode_async(&g_pig_anim_job,&g_pig_anim_handle);
#if SAT_SKYBRIDGE_VALIDATION
    g_metrics.task_submit_frt_ticks+=(uint16_t)(
        sb_test_frt_counter()-submit_start);
#endif
    if(submitted!=SAT_OK) {
        ++g_metrics.failures;
        ++g_metrics.animation_failed;
        if(sat_anim_decode(&skybridge_pig_anim_asset,&g_pig_anim,
                           g_pig_pose[write_buffer],PIG_VERTEX_CAP)!=SAT_OK) {
            ++g_metrics.failures;
            return;
        }
        g_pig_render_buffer=write_buffer;
        g_pig_render_anim=g_pig_anim;
        return;
    }
    ++g_metrics.animation_submitted;
    record_task_dispatch(slave_before,master_before,
        &g_metrics.animation_slave_dispatches,
        &g_metrics.animation_master_dispatches);
    g_pig_anim_pending=1u;
}

void prepare_pig_animation_master(void) {
    const uint8_t write_buffer=(uint8_t)(1u-g_pig_render_buffer);
    sat_anim_state_t frame_state=g_pig_anim;
    if(sat_anim_decode(&skybridge_pig_anim_asset,&frame_state,
                       g_pig_pose[write_buffer],PIG_VERTEX_CAP)!=SAT_OK) {
        ++g_metrics.failures;
        return;
    }
    g_pig_render_buffer=write_buffer;
    g_pig_render_anim=frame_state;
}

void finish_pig_animation(void) {
    const uint32_t start=sat_time_ms();
    if(!g_pig_anim_pending)return;
    if(g_parallel_recovery_blocked)return;
    {
        sat_result_t st=sat_parallel_wait(g_pig_anim_handle,SB_PARALLEL_TIMEOUT);
        sat_parallel_task_state_t state=sat_parallel_state(g_pig_anim_handle);
        if(st!=SAT_OK && state==SAT_PARALLEL_RUNNING) {
            ++g_metrics.timeouts;
            st=sat_parallel_abort(g_pig_anim_handle,SB_PARALLEL_TIMEOUT);
            state=sat_parallel_state(g_pig_anim_handle);
        }
        if(state==SAT_PARALLEL_RUNNING) {
            /* Neither timeout nor a failed abort returns ownership. Keep the
             * descriptor, state snapshot and output buffer live. */
            parallel_recovery_failed();
            return;
        }
        if(st==SAT_OK && state==SAT_PARALLEL_COMPLETED) {
            g_pig_render_buffer=g_pig_anim_write_buffer;
            g_pig_render_anim=g_pig_anim_job_state;
            ++g_metrics.animation_completed;
        } else {
            ++g_metrics.failures;
            ++g_metrics.animation_failed;
        }
#if SAT_SKYBRIDGE_VALIDATION
        const uint16_t release_start=sb_test_frt_counter();
#endif
        const sat_result_t released=sat_parallel_release(g_pig_anim_handle);
#if SAT_SKYBRIDGE_VALIDATION
        g_metrics.task_release_frt_ticks+=(uint16_t)(
            sb_test_frt_counter()-release_start);
#endif
        if(released!=SAT_OK) {
            parallel_recovery_failed();
            return;
        }
        g_pig_anim_pending=0u;
    }
    g_metrics.wait_ms += sat_time_ms()-start;
}

/* The VDP2 fade setup intentionally makes only indexed Sprite Type 0
 * materials opaque at priority 7. The converter's RGB shade palette is
 * therefore uploaded into the spare entries of the fade bank and each
 * animated face selects a tiny indexed solid texture. This keeps the pig
 * opaque and pink instead of blending it with the RBG0 sea. */
void player_pig(void) {
    if(g_world_cmd_full) return;
    sat_model_transform3d_t pose;
    sat_mat4_t world;
    sat_model_transform3d_identity(&pose);
    pose.position=(sat_vec3_t){g_game.x,
        g_game.y-SB_F(1)/2,g_game.z};
    /* glTF local +Z points towards the snout. The game controller stores
     * a persistent CARDINAL forward independent of camera yaw. */
    pose.rotation_deg.y=g_game.facing_z<0?SB_F(180):
        (g_game.facing_x>0?SB_F(90):
         (g_game.facing_x<0?SB_F(270):0));
    sat_example_must(sat_model_transform3d_matrix(&pose,&world));
    /* The Slave only decodes into the alternate local-pose buffer. The
     * Master owns this mesh and applies the world transform after completion,
     * so no transformed vertex range is concurrently recycled. */
    for(uint16_t vertex=0u;vertex<SKYBRIDGE_PIG_VERTEX_COUNT;++vertex)
        g_pig_mesh.vertices[vertex]=g_pig_pose[g_pig_render_buffer][vertex];
    if(sat_mesh_transform(&g_pig_mesh,&world)!=SAT_OK ||
       sat_anim_face_materials(&skybridge_pig_anim_asset,
        &g_pig_render_anim,g_pig_face_textures,PIG_FACE_CAP,
        SKYBRIDGE_PIG_SHADE_COUNT)!=SAT_OK) {
        ++g_metrics.failures;
        return;
    }
    /* Animated pose already carries the world transform; do not apply it
     * twice when submitting the pig through the canonical instance path. */
    g_pig_instance.mesh=&g_pig_mesh;
    g_pig_instance.materials=g_pig_materials;
    g_pig_instance.material_count=SKYBRIDGE_PIG_SHADE_COUNT;
    g_pig_instance.face_materials=g_pig_face_textures;
    g_pig_instance.world=0;
    g_pig_instance.pass=SB_PASS_ACTOR;
    g_pig_instance.cull_backfaces=1u;
    /* Deliberately SLOT_INHERIT, not a fade slot: the player's character is
     * the one thing that must stay readable at any camera distance. The gems
     * around it do fade, through the same parameter. */
    {
        const uint16_t before=g_scene.faces.count;
        world_ok(sat_scene_submit_instance(
            &g_scene,&g_pig_instance,SAT_SCENE3D_SLOT_INHERIT,g_pig_projected,0));
        g_dbg_pig_faces=(uint16_t)(g_scene.faces.count-before);
    }
}
