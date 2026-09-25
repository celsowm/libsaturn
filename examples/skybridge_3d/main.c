/* Skybridge 3D: a playable stock-Saturn platformer, VDP1 world + VDP2 sea/sky.
 *
 * main.c owns start-up order and the frame loop. The rest lives by concern:
 * world.c (scene pass), pig.c, gems.c, materials.c, background.c, audio.c,
 * hud.c, metrics.c and telemetry.c; skybridge.h is their shared contract. */
#include "skybridge.h"

sb_game_t g_game;
static sat_resource_plan_t g_resources;
static sat_resource_plan_entry_t g_resource_entries[8];
/* One camera. sat_camera3d_update derives the view-projection from it, so the
 * look_at/perspective/multiply sequence is not spelled out here. */
sat_camera3d_t g_camera;
sat_vec3_t g_eye, g_target;
static sat_follow_camera3d_t g_follow_camera;
uint32_t g_frame;
int16_t g_yaw;
static sat_step_clock_t g_step_clock;

static void start_course(uint8_t course) {
    sb_start_course(&g_game,course);
    g_yaw=0;
    world_reset_platform_fades();
    sat_example_must(sat_follow_camera3d_init(
        &g_follow_camera,&(sat_vec3_t){g_game.x,g_game.y,g_game.z},4u,6u,4u));
    sat_example_must(sat_anim_state_init(
        &g_pig_anim,&skybridge_pig_anim_asset,1u));
}

static void plan_resources(void) {
    sat_example_must(sat_resource_plan_init(
        &g_resources, g_resource_entries,
        (uint16_t)(sizeof(g_resource_entries) / sizeof(g_resource_entries[0]))));
    /* Fixed reservations are checked before uploads and scene activation. */
    sat_example_must(sat_resource_plan_set_limit(
        &g_resources, SAT_RESOURCE_WRAM, sizeof(g_face_items) +
        sizeof(g_face_keys) + sizeof(g_face_order)));
    sat_example_must(sat_resource_plan_set_limit(
        &g_resources, SAT_RESOURCE_VDP1_COMMANDS, 1024u * 4u));
    sat_example_must(sat_resource_plan_set_limit(
        &g_resources, SAT_RESOURCE_AUDIO_STAGING,
        SOUNDS * SOUND_LEN + MUSIC_LEN));
    sat_example_must(sat_resource_plan_add(
        &g_resources, SAT_RESOURCE_WRAM,
        sizeof(g_face_items) + sizeof(g_face_keys) + sizeof(g_face_order),
        2u, 1u));
    sat_example_must(sat_resource_plan_add(
        &g_resources, SAT_RESOURCE_VDP1_COMMANDS, 1024u * 4u, 4u, 1u));
    sat_example_must(sat_resource_plan_add(
        &g_resources, SAT_RESOURCE_AUDIO_STAGING,
        SOUNDS * SOUND_LEN + MUSIC_LEN, 2u, 1u));
    sat_example_must(sat_resource_plan_finalize(&g_resources));
}

int main(void) {
    const sat_video_config_t video={W,H,1u,0u};
    const sat_vec3_t up={0,SB_F(1),0};
    const sat_vec3_t g_game_origin={0,0,0};
    const sat_vec3_t g_game_origin_ahead={0,0,SB_F(1)};
    sat_pad_state_t pad={0};
    sat_vdp2_scroll_t sky_scroll={0u,0u,31u,0u};
#if SAT_SKYBRIDGE_VALIDATION
    initialize_test_telemetry();
#endif
    /* Keep immutable asset preparation ahead of SSHON. */
    gems_init_mesh();
    pig_init();
    gems_init_batches();
    sat_example_must(sat_init(&video));
    {
        const sat_parallel_config_t parallel_config={
            (sat_parallel_mode_t)SAT_SKYBRIDGE_PARALLEL_MODE,0,0u,0u,
            SB_PARALLEL_TIMEOUT};
        sat_example_must(sat_anim_parallel_register());
        sat_example_must(sat_scene3d_prepare_parallel_register());
        sat_example_must(sat_parallel_init(&parallel_config));
    }
    plan_resources();
    sb_init(&g_game);
    sat_example_must(sat_camera3d_init(
        &g_camera,&g_game_origin,&g_game_origin_ahead,&up,SB_F(55),
        sat_fx16_div(SB_F(W),SB_F(H)),SB_F(2),SB_F(250)));
    sat_example_must(sat_scene_init(
        &g_scene,g_face_items,g_face_keys,g_face_order,SCENE_FACE_CAP));
    world_reset_platform_fades();
    sat_example_must(sat_follow_camera3d_init(
        &g_follow_camera,&(sat_vec3_t){g_game.x,g_game.y,g_game.z},4u,6u,4u));
    /* Load the first drawable font before expensive procedural generation. */
    hud_font_init();
    sat_example_must(sat_vdp1_set_erase_transparent());
    loading_frame("INITIALIZING WORLD",5u);
    init_tile_texture();
    init_cloud_texture();
    init_scene_materials();
    gems_bind_materials();
    loading_frame("IMPORTING PIG",10u);
    loading_frame("BUILDING SKY",13u);
    init_sky();
    loading_frame("BUILDING SEA",15u);
    init_sea();
    init_background();
    init_audio();
    sat_step_clock_init(&g_step_clock);
    for(;;) {
        uint32_t now;
        uint16_t steps;
        uint16_t pressed,events=0u;
        int32_t fx,fz,rx,rz;
#if SAT_SKYBRIDGE_VALIDATION
        uint16_t frame_start_frt;
#endif
        sat_example_must(sat_wait_vblank());
        g_metrics=(sb_frame_metrics_t){0};
        g_metrics.begin_ms=sat_time_ms();
#if SAT_SKYBRIDGE_VALIDATION
        frame_start_frt=sb_test_frt_counter();
#endif
        /* VDP2's register latch is at VBlank. Apply BOTH layer and sprite
         * priority configuration before the comparatively slow SMPC pad poll,
         * math, audio and VDP1 submissions. The color-calc PRISA selector
         * now lives in the layer shadow, so it cannot alternate per frame. */
        now=sat_frame_count();
        g_frame=now;
        sky_scroll.x_integer=sb_scenery_sky_scroll(
            (uint16_t)g_yaw,g_frame);
        sat_example_must(sat_vdp2_nbg0_set_scroll(&sky_scroll));
        animate_sea_palette(g_frame);
        update_rotation(sat_sin_deg(SB_F(g_yaw)),
                        sat_cos_deg(SB_F(g_yaw)));
        sat_example_must(sat_vdp2_layers_commit());
        {
            const uint32_t stage=sat_time_ms();
        sat_example_must(sat_pad_poll(&pad));
        /* Drop excess catch-up, preserve responsive input. sat_step_clock_steps
         * returns the raw elapsed count, so it can be 0 on a frame that beat
         * the display; this game always advances at least one tick rather than
         * freezing the simulation for that frame. */
#if SAT_SKYBRIDGE_VALIDATION
        /* Profile timing varies with the chosen execution policy under an
         * emulator. Keep test-route game state comparable while leaving the
         * production fixed-step catch-up policy unchanged. */
        steps=1u;
#else
        steps=sat_step_clock_steps(&g_step_clock,3u);
        if(steps==0u) steps=1u;
#endif
        /* Pause + X is a deliberate course selector for playing/testing
         * Course 2 without finishing all ten decks of Course 1 first. */
        if(g_game.paused && (pad.pressed&SAT_PAD_X)) {
            start_course((uint8_t)(g_game.course+1u));
            /* Keep the course picker open: with four courses, three
             * consecutive X presses select Course 4 without requiring
             * START/X/START/X/START/X to revisit the pause menu. */
            g_game.paused=1u;
        } else if (pad.pressed&SAT_PAD_START) {
            if(g_game.finished)
                start_course((uint8_t)(g_game.course+1u));
            else g_game.paused=(uint8_t)!g_game.paused;
        }
            g_metrics.input_ms=sat_time_ms()-stage;
        }
        if(pad.pressed&SAT_PAD_Y)g_show_debug=(uint8_t)!g_show_debug;
        if(pad.pressed&SAT_PAD_B) g_yaw-=15;
        if(pad.pressed&SAT_PAD_C) g_yaw+=15;
        if(g_yaw>=360)g_yaw-=360;
        if(g_yaw<0)g_yaw+=360;
        fx=sat_sin_deg(SB_F(g_yaw));
        fz=sat_cos_deg(SB_F(g_yaw));
        /* View basis: looking along +Z, screen-right is world -X. */
        sb_camera_right(fx,fz,&rx,&rz);
        pressed=0u;
        if (pad.pressed&SAT_PAD_A) pressed|=SB_JUMP;
        {
            uint16_t held=0u;
            const uint32_t physics_stage=sat_time_ms();
            if(pad.held&SAT_PAD_UP) held|=SB_UP;
            if(pad.held&SAT_PAD_DOWN) held|=SB_DOWN;
            if(pad.held&SAT_PAD_LEFT) held|=SB_LEFT;
            if(pad.held&SAT_PAD_RIGHT) held|=SB_RIGHT;
            if(pad.held&SAT_PAD_A) held|=SB_JUMP;
            if(pad.held&SAT_PAD_Z) held|=SB_BRAKE;
            while(steps--) {
                events|=sb_tick(&g_game,held,pressed,fx,fz,rx,rz);
                pressed=0u; /* A pressed edge is delivered once, never per catch-up step. */
            }
            g_metrics.physics_ms=sat_time_ms()-physics_stage;
        }
        /* Jump=2, Walk=0, Idle=1 in the selected converter clip order.
         * Use actual movement/grounding, not camera yaw, to choose poses. */
        {
            const uint32_t stage=sat_time_ms();
            uint16_t clip=g_game.support<0?2u:
                (sb_abs(g_game.vx)+sb_abs(g_game.vz)>SB_F(1)/3?0u:1u);
            if(clip!=g_pig_anim.clip)
                sat_example_must(sat_anim_set_clip(
                    &g_pig_anim,&skybridge_pig_anim_asset,clip));
            if(!g_game.paused && !g_game.finished)
                sat_example_must(sat_anim_advance(
                    &g_pig_anim,&skybridge_pig_anim_asset,SB_F(1)/60));
            g_metrics.animation_ms=sat_time_ms()-stage;
        }
        sound_event(events);
        {
            const uint32_t stage=sat_time_ms();
            int32_t ex,ez,lx,lz;
            const sat_vec3_t desired={g_game.x,g_game.y,g_game.z};
            sat_vec3_t eye_offset,target_offset;
            sb_camera_offset(fx,fz,&ex,&ez,&lx,&lz);
            eye_offset=(sat_vec3_t){ex,SB_F(29),ez};
            target_offset=(sat_vec3_t){lx,SB_F(5),lz};
            sat_example_must(sat_follow_camera3d_set_offsets(
                &g_follow_camera,&eye_offset,&target_offset));
            sat_example_must(sat_follow_camera3d_step(
                &g_follow_camera,&desired,(events&SB_EVENT_FALL)?1u:0u,
                &g_eye,&g_target));
            g_metrics.camera_ms=sat_time_ms()-stage;
        }
        g_camera.eye=g_eye;
        g_camera.target=g_target;
        sat_example_must(sat_camera3d_update(&g_camera));
        /* Sprite color calculation was configured at startup. The generic
         * VBlank layer replay now preserves both of its priority selectors. */
        sat_example_must(sat_vdp1_set_erase_transparent());
        sat_example_must(sat_begin_frame());
        g_world_cmd_full=0u;
        g_dbg_scene_status=SAT_OK;
        {
            const uint32_t submission_stage=sat_time_ms();
        draw_clouds();
        draw_world();
            g_metrics.submission_ms=sat_time_ms()-submission_stage;
        }
        record_parallel_snapshot();
        /* The HUD reports this completed CPU-side preparation interval. It is
         * intentionally sampled before HUD/VBlank work, so it describes the
         * frame whose diagnostics are on screen rather than a half-built
         * value from the next frame. */
        g_metrics.frame_cpu_ms=sat_time_ms()-g_metrics.begin_ms;
#if SAT_SKYBRIDGE_VALIDATION
        g_metrics.frame_cpu_frt_ticks=sb_test_frt_delta(frame_start_frt);
#endif
        sat_example_must(sat_vdp1_overlay_begin());
        {
            const uint32_t hud_stage=sat_time_ms();
        draw_hud();
            g_metrics.hud_ms=sat_time_ms()-hud_stage;
        }
        sat_example_must(sat_end_frame());
        sat_example_must(sat_audio_update());
        g_metrics.frame_ms=sat_time_ms()-g_metrics.begin_ms;
#if SAT_SKYBRIDGE_VALIDATION
        g_metrics.frame_frt_ticks=sb_test_frt_delta(frame_start_frt);
#endif
        g_metrics.over_budget=(g_metrics.frame_ms>17u)?1u:0u;
#if SAT_SKYBRIDGE_VALIDATION
        write_test_telemetry(&pad);
#endif
    }
}
