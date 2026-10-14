/* Two independently animated fighters in the supplied MMA Octagon. */
#include "saturn/saturn.h"
#include "saturn/font.h"
#include "saturn/character3d.h"
#include "saturn/orbit_camera3d.h"
#include "examples/common/example_util.h"
#include "examples/characters/combat.h"
#include "killer_clown_skeletal.h"
#include "octagon.h"
#include "environment.h"
#include "saturn/lz.h"
#include "saturn/vdp2_layers.h"
#include "audio.h"
#include "lighting.h"
#include "profiling.h"
#include "saturn/vdp2_color_calc.h"

enum {
    FIGHTERS = 2, JOINT_CAP = 66, SCREEN_W = 320, SCREEN_H = 224,
    HUD_RESERVE = 160, SHADOW_SEGMENTS = 8, MS_PER_SECOND = 1000,
    WORKER_TIMEOUT = 60000, TRANSITION_TICKS = 7,
    CAMERA_DISTANCE_Q16 = SAT_FX16_ONE * 11 / 2,
    CAMERA_HEIGHT_Q16 = SAT_FX16_ONE * 21 / 10,
    CAMERA_TARGET_HEIGHT_Q16 = SAT_FX16_ONE * 21 / 20,
    OCTAGON_FLOOR_Z_Q16 = 72738,
    CLOWN_FEET_Q16 = SAT_FX16_ONE * 143 / 100,
    OCTAGON_DIAGONAL_LIMIT_Q16 = SAT_FX16_ONE * 23 / 5,
    FOREGROUND_CAGE_HEIGHT_Q16 = SAT_FX16_ONE / 2,
    FENCE_FADE_DISTANCE_Q16 = SAT_FX16_ONE * 3 / 2,
    FENCE_MODE_COUNT = 4,
    HEALTH_BAR_W = 112, HEALTH_BAR_H = 7,
    STAGE_MATERIAL_COUNT = 6, MAT_TEXELS_PER_UNIT = 32,
    CROWD_NAME_ADDRESS = 0x20000, GROUND_CHAR_ADDRESS = 0x40000,
    GROUND_NAME_ADDRESS = 0x60000, ROTATION_TABLE_A = 0x24000,
    ROTATION_TABLE_B = 0x24100, COEFFICIENT_TABLE_A = 0x24200,
    COEFFICIENT_TABLE_B = 0x24600, MAP_CELLS = 64 * 64,
    UI_MENU = 0, UI_FIGHT = 1, UI_PAUSE = 2, UI_OPTIONS = 3, UI_CONTROLS = 4,
    LIGHT_COUNT = 2, CONE_SIDES = 8, CONE_SECTIONS = 2,
    LIGHT_FACE_RESERVE = LIGHT_COUNT * (CONE_SIDES*CONE_SECTIONS+2),
    LIGHT_SWEEP_TICKS = COMBAT_TICK_HZ * 9, LIGHT_TEXTURE_SIDE = 64,
    LIGHT_SOURCE_X_Q16 = SAT_FX16_ONE * 23 / 10,
    LIGHT_SOURCE_Y_Q16 = SAT_FX16_ONE * 16 / 5,
    LIGHT_SOURCE_Z_Q16 = SAT_FX16_ONE * 9 / 10,
    LIGHT_TORSO_FRACTION_Q16 = SAT_FX16_ONE * 2 / 3,
    LIGHT_ORBIT_X_Q16 = SAT_FX16_ONE * 3 / 10,
    LIGHT_ORBIT_Z_Q16 = SAT_FX16_ONE * 2 / 5,
    LIGHT_ORBIT_Y_Q16 = SAT_FX16_ONE / 5,
    LIGHT_POOL_FIRST_CODE = 32, LIGHT_BEAM_PEAK = 6, LIGHT_POOL_PEAK = 12,
    LIGHT_PASS = 0, GEOMETRY_PASS = 1,
    CLIP_IDLE = 0, CLIP_WALK, CLIP_PUNCH, CLIP_KICK, CLIP_HIT, CLIP_FALL, CLIP_JUMP
};
#define FACE_CAP (FIGHTERS * KILLER_CLOWN_SKELETAL_FACE_COUNT + OCTAGON_FACE_COUNT + FIGHTERS * SHADOW_SEGMENTS + LIGHT_FACE_RESERVE)
static const sat_skeleton_clip_t* const clips[] = {
    &killer_clown_skeletal_clip_Fighting_Idle, &killer_clown_skeletal_clip_Walk,
    &killer_clown_skeletal_clip_Fighting_Left_Jab, &killer_clown_skeletal_clip_Kick_Breach,
    &killer_clown_skeletal_clip_Hit_Head, &killer_clown_skeletal_clip_Hit_Knockback,
    &killer_clown_skeletal_clip_Jump_2
};
static const unsigned state_clip[] = {CLIP_IDLE,CLIP_WALK,CLIP_WALK,CLIP_JUMP,
    CLIP_PUNCH,CLIP_KICK,CLIP_IDLE,CLIP_HIT,CLIP_FALL};
typedef struct {
    sat_skeleton_instance_t skeleton;
    sat_transform3d_node_t nodes[JOINT_CAP];
    uint16_t scratch[JOINT_CAP];
    sat_skeleton_pose_joint_t pose[JOINT_CAP], from[JOINT_CAP];
    sat_mat4_t joints[JOINT_CAP], world;
    sat_skel_clip_player_t player;
    sat_pose_fade_t fade;
    sat_vec3_t vertices[KILLER_CLOWN_SKELETAL_VERTEX_COUNT];
    sat_vec3_t normals[KILLER_CLOWN_SKELETAL_VERTEX_COUNT];
    uint16_t gouraud[KILLER_CLOWN_SKELETAL_VERTEX_COUNT];
    uint32_t light_visible[LIGHT_COUNT],light_total[LIGHT_COUNT];
    sat_result_t lighting_result;
    sat_vec3_t world_vertices[KILLER_CLOWN_SKELETAL_VERTEX_COUNT];
    sat_projected_vertex_t projected[KILLER_CLOWN_SKELETAL_VERTEX_COUNT];
    uint16_t indices[KILLER_CLOWN_SKELETAL_FACE_COUNT * 4];
    sat_mesh_t mesh;
    sat_aabb3_t bounds;
    sat_scene3d_instance_t instance;
    sat_scene3d_instance_binding_t binding;
    sat_skin_job_t job;
    sat_parallel_handle_t handle;
    unsigned clip, attack_serial, hit_serial;
} fighter_t;
static fighter_t actors[FIGHTERS];
/* Includes only normals, Gouraud and per-actor results. Outputs remain private
 * to each actor; no skin/geometry task owns them during the lighting ranges. */
enum { LIGHT_OUTPUT_BYTES=offsetof(fighter_t,world_vertices)-offsetof(fighter_t,normals) };
volatile struct {uint32_t enabled,verify,updates,verified,errors;} g_clown_lighting_jobs;
#if CLOWN_VERIFY_LIGHTING
static uint8_t lighting_reference[LIGHT_OUTPUT_BYTES]
    __attribute__((section(".wram_l"),aligned(4)));
#endif
/* Geometry, LUT4 pixels and RGB CLUTs are shared; lighting is per vertex. */
static sat_vdp1_texture_t fighter_textures[FIGHTERS][KILLER_CLOWN_SKELETAL_TEXTURE_COUNT];
static sat_scene3d_material_t fighter_materials[FIGHTERS][KILLER_CLOWN_SKELETAL_TEXTURE_COUNT];
static sat_vdp1_texture_t wire_texture;
static sat_vdp1_texture_t beam_textures[LIGHT_COUNT][CONE_SECTIONS],pool_textures[LIGHT_COUNT];
static clown_spotlight_t spotlights[LIGHT_COUNT];
volatile struct {
    uint32_t updates,visible_red,visible_blue,max_visible_red,max_visible_blue;
    uint32_t red_strength,blue_strength,cone_faces,pool_faces,phase;
} g_clown_light_metrics;
static sat_scene3d_material_t stage_materials[STAGE_MATERIAL_COUNT];
static uint16_t crowd_words[CROWD_BYTES / 2] __attribute__((section(".wram_l")));
static sat_vec3_t stage_vertices[OCTAGON_VERTEX_COUNT], stage_world_vertices[OCTAGON_VERTEX_COUNT];
static uint16_t stage_indices[OCTAGON_FACE_COUNT * 4];
static uint16_t stage_face_materials[OCTAGON_FACE_COUNT];
static sat_projected_vertex_t stage_projected[OCTAGON_VERTEX_COUNT];
static sat_mesh_t stage_mesh;
static sat_scene3d_instance_t stage_instance;
static sat_mat4_t stage_world;
static uint8_t stage_foreground[OCTAGON_FACE_COUNT];
static sat_fx16_t stage_depth[OCTAGON_FACE_COUNT];
static uint16_t stage_base_material[OCTAGON_FACE_COUNT];
static uint16_t layer_names[MAP_CELLS * 2] __attribute__((section(".wram_l")));
static uint32_t ground_coefficients[SCREEN_H];
static uint32_t ground_table_index;
static sat_scene_t scene;
static sat_scene3d_face_t faces[FACE_CAP];
static uint32_t keys[FACE_CAP] __attribute__((section(".wram_l")));
static uint16_t order[FACE_CAP] __attribute__((section(".wram_l")));
/* Separate worker output; skin handles have been joined before geometry.
 * The adapter publishes immutable inputs and consumes uncached worker output. */
#if CLOWN_PARALLEL_GEOMETRY
static sat_scene3d_face_t geometry_faces[KILLER_CLOWN_SKELETAL_FACE_COUNT]
    __attribute__((section(".wram_l")));
static uint32_t geometry_keys[KILLER_CLOWN_SKELETAL_FACE_COUNT]
    __attribute__((section(".wram_l")));
static sat_scene3d_prepare_item_t geometry_item;
static sat_scene3d_prepare_batch_t geometry_batch;
#endif
volatile struct {uint32_t enabled,verify,pairs,verified,errors,serial;} g_clown_geometry;
static sat_ascii_font_t font;
static sat_ascii_font_t title_font;
static sat_camera3d_t camera;
static sat_fx16_t near_depth;
combat_round_t g_clown_round;
volatile struct {
    uint32_t frames, simulation_ticks, hits, blocks, knockdowns, body_overlaps;
    uint32_t stage_faces, actor_faces, commands, draw_errors, slave_jobs, round_resets;
    uint32_t two_player, paused, submissions_tenths, pose_changes;
    uint32_t fence_mode, fence_mesh_frames, fence_hidden_frames, fence_solid_frames;
    uint32_t second_pad_buttons;
    uint32_t draw_timeouts, late_swaps;
    uint32_t ui_state, menu_selection, menu_visits, options_visits, controls_visits;
    uint32_t ground_updates;
    uint32_t lights_enabled, light_updates;
} g_clown_metrics;
static uint64_t accumulator;
static uint32_t previous_ms, rate_mark, rate_frames;
static uint32_t submenu_selection;
static sat_fx16_t camera_focal;

static void text(const char* message,int x,int y) {
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&font,message,x,y,8,0,0));
}
static void number(const char* label,unsigned value,int x,int y) {
    char line[48];sat_example_must(sat_fmt_label_u32(label,value,line,sizeof(line),0));text(line,x,y);
}
static void reset_round(void) {
    combat_reset(&g_clown_round);
    accumulator=0;
    for(unsigned i=0;i<FIGHTERS;++i) {
        actors[i].clip=CLIP_IDLE;actors[i].attack_serial=actors[i].hit_serial=0;
        sat_example_must(sat_skel_clip_player_init(&actors[i].player,clips[CLIP_IDLE]));
        sat_example_must(sat_pose_fade_init(&actors[i].fade,actors[i].from,JOINT_CAP));
        sat_example_must(sat_skel_clip_player_sample(&actors[i].player,actors[i].pose,JOINT_CAP));
    }
    ++g_clown_metrics.round_resets;
}
static sat_fx16_t absolute(sat_fx16_t value) {return value<0?-value:value;}
static void constrain_octagon(void) {
    combat_constrain_octagon(&g_clown_round,OCTAGON_DIAGONAL_LIMIT_Q16);
}
static void simulate(const sat_pad_state_t* p1,const sat_pad_state_t* p2) {
    unsigned hits=g_clown_round.hits,blocks=g_clown_round.blocked,kos=g_clown_round.knockdowns;
    unsigned round_over=g_clown_round.round_over;
    if(g_clown_metrics.two_player)combat_tick_dual(&g_clown_round,p1->held,p2->held);
    else combat_tick(&g_clown_round,p1->held);
    constrain_octagon();
    g_clown_metrics.hits+=g_clown_round.hits-hits;
    g_clown_metrics.blocks+=g_clown_round.blocked-blocks;
    g_clown_metrics.knockdowns+=g_clown_round.knockdowns-kos;
    if(!round_over && g_clown_round.round_over)clown_audio_play(CLOWN_SOUND_BELL,0);
    int64_t dx=(int64_t)g_clown_round.fighters[1].position.x-g_clown_round.fighters[0].position.x;
    int64_t dz=(int64_t)g_clown_round.fighters[1].position.z-g_clown_round.fighters[0].position.z;
    if(dx*dx+dz*dz<(int64_t)COMBAT_BODY_DIAMETER_Q16*COMBAT_BODY_DIAMETER_Q16)
        ++g_clown_metrics.body_overlaps;
    const sat_fx16_t dt=SAT_FX16_ONE/COMBAT_TICK_HZ;
    for(unsigned i=0;i<FIGHTERS;++i) {
        fighter_t* a=&actors[i];const combat_fighter_t* f=&g_clown_round.fighters[i];
        if(f->attack_serial!=a->attack_serial)
            clown_audio_play(f->state==COMBAT_KICK?CLOWN_SOUND_KICK:CLOWN_SOUND_PUNCH,f->position.x);
        if(f->hit_serial!=a->hit_serial)clown_audio_play(CLOWN_SOUND_DAMAGE,f->position.x);
        unsigned clip=state_clip[f->state];
        if(clip!=a->clip || f->attack_serial!=a->attack_serial || f->hit_serial!=a->hit_serial) {
            sat_example_must(sat_pose_fade_begin(&a->fade,a->pose,dt*TRANSITION_TICKS));
            sat_example_must(sat_skel_clip_player_init(&a->player,clips[clip]));
            unsigned action_ticks=f->state==COMBAT_PUNCH?COMBAT_PUNCH_TICKS:
                f->state==COMBAT_KICK?COMBAT_KICK_TICKS:f->state==COMBAT_HIT?COMBAT_HIT_TICKS:
                f->state==COMBAT_KNOCKDOWN?COMBAT_KNOCKDOWN_TICKS:f->state==COMBAT_JUMP?COMBAT_JUMP_TICKS:0;
            if(action_ticks)sat_example_must(sat_skel_clip_player_set_speed(&a->player,
                sat_fx16_div(sat_clip_duration(clips[clip]),SAT_FX16_ONE*action_ticks/COMBAT_TICK_HZ)));
            a->clip=clip;a->attack_serial=f->attack_serial;a->hit_serial=f->hit_serial;
            ++g_clown_metrics.pose_changes;
        }
        if(f->state!=COMBAT_GUARD && !g_clown_round.round_over)
            sat_example_must(sat_skel_clip_player_advance(&a->player,dt,NULL,0,NULL,NULL));
        sat_pose_fade_advance(&a->fade,dt);
    }
    ++g_clown_metrics.simulation_ticks;
}
static void evaluate_fighters(void) {
    for(unsigned i=0;i<FIGHTERS;++i) {
        fighter_t* a=&actors[i];
        profile_start(PROFILE_POSE);
        sat_example_must(sat_skel_clip_player_sample(&a->player,a->pose,JOINT_CAP));
        sat_example_must(sat_pose_fade_apply(&a->fade,a->pose));
        sat_example_must(sat_skeleton_evaluate(&a->skeleton,a->pose,JOINT_CAP));
        sat_example_must(sat_skeleton_world_matrices(&a->skeleton,a->joints,JOINT_CAP));
        a->job=(sat_skin_job_t){&killer_clown_skeletal_skin,a->joints,a->vertices,
            KILLER_CLOWN_SKELETAL_VERTEX_COUNT,0,KILLER_CLOWN_SKELETAL_VERTEX_COUNT,JOINT_CAP,0};
        sat_example_must(sat_skin_job_submit(&a->job,i?SAT_PARALLEL_MASTER:SAT_PARALLEL_AUTO,&a->handle));
        profile_stop(PROFILE_POSE);
    }
    for(unsigned i=0;i<FIGHTERS;++i) {
        fighter_t* a=&actors[i];uint32_t bytes=0;
        profile_start(PROFILE_SKIN_JOIN);
        sat_example_must(sat_parallel_wait(a->handle,WORKER_TIMEOUT));
        sat_example_must(sat_parallel_result(a->handle,&bytes));
        sat_example_must(sat_parallel_release(a->handle));a->handle=0;
        profile_stop(PROFILE_SKIN_JOIN);
        profile_start(PROFILE_BOUNDS);
        a->mesh.vertex_count=KILLER_CLOWN_SKELETAL_VERTEX_COUNT;
        const combat_fighter_t* f=&g_clown_round.fighters[i];
        sat_mat4_t translation,rotation;
        sat_example_must(sat_mat4_translate(&translation,f->position.x,-CLOWN_FEET_Q16,f->position.z));
        sat_example_must(sat_mat4_rotate_y(&rotation,f->facing*SAT_FX16_ONE*90));
        sat_example_must(sat_mat4_multiply(&a->world,&translation,&rotation));
        sat_example_must(sat_skin_bounds(a->vertices,a->mesh.vertex_count,SAT_FX16_ONE/32,&a->bounds));
        profile_stop(PROFILE_BOUNDS);
    }
    if(sat_parallel_slave_available())++g_clown_metrics.slave_jobs;
}
#if CLOWN_PARALLEL_GEOMETRY && CLOWN_VERIFY_GEOMETRY
/* Compare semantic fields only: safe faces leave world storage unspecified. */
static int geometry_face_equal(const sat_scene3d_face_t* a,const sat_scene3d_face_t* b) {
    if(a->projected_safe!=b->projected_safe || a->cached_projected!=b->cached_projected ||
       a->gouraud_valid!=b->gouraud_valid || a->material.kind!=b->material.kind ||
       a->material.rgb555!=b->material.rgb555 || a->material.texture!=b->material.texture ||
       a->material.tiled!=b->material.tiled ||
       a->material.color_calc_slot!=b->material.color_calc_slot ||
       a->material.vertex_gouraud!=b->material.vertex_gouraud ||
       a->material.sprite_flags!=b->material.sprite_flags)return 0;
    for(unsigned corner=0;corner<4;++corner) {
        if(a->projected.x[corner]!=b->projected.x[corner] ||
           a->projected.y[corner]!=b->projected.y[corner] ||
           (a->gouraud_valid && a->gouraud[corner]!=b->gouraud[corner]))return 0;
        if(!a->projected_safe && (a->world.v[corner].x!=b->world.v[corner].x ||
           a->world.v[corner].y!=b->world.v[corner].y ||
           a->world.v[corner].z!=b->world.v[corner].z))return 0;
    }
    return 1;
}
static void verify_geometry(const sat_scene3d_faces_t* prefix) {
    sat_scene3d_faces_t reference=*prefix;
    /* The worker is joined and its output is already merged. Reuse its
     * buffer for the serial reference, preserving the actual merged faces. */
    reference.entries=geometry_faces;reference.keys=geometry_keys;
    reference.count=0;reference.capacity=KILLER_CLOWN_SKELETAL_FACE_COUNT;
    reference.culled_faces=reference.clipped_faces=0;
    fighter_t* actor=&actors[FIGHTERS-1];
    sat_example_must(sat_scene3d_faces_submit_instance(&reference,&actor->instance,
        SAT_SCENE3D_SLOT_INHERIT,actor->projected,actor->world_vertices));
    int equal=reference.count==geometry_batch.metrics.prepared_faces &&
        reference.culled_faces==geometry_batch.metrics.culled_faces &&
        reference.clipped_faces==geometry_batch.metrics.clipped_faces;
    for(unsigned face=0;equal && face<geometry_batch.metrics.prepared_faces;++face) {
        unsigned index=prefix->count+face;
        equal=scene.faces.keys[index]==geometry_keys[face] &&
            geometry_face_equal(&scene.faces.entries[index],&geometry_faces[face]);
    }
    if(!equal){++g_clown_geometry.errors;sat_example_must(SAT_ERR_VERIFY_FAILED);}
    ++g_clown_geometry.verified;
}
#endif
static void prepare_fighters(void) {
#if CLOWN_PARALLEL_GEOMETRY
    sat_parallel_handle_t handle=0;
    int paired=sat_parallel_slave_available();
    if(paired) {
        fighter_t* actor=&actors[FIGHTERS-1];
        geometry_item=(sat_scene3d_prepare_item_t){&actor->instance,actor->projected,
            actor->world_vertices,SAT_SCENE3D_SLOT_INHERIT,0};
        sat_example_must(sat_scene3d_prepare_batch_init(&geometry_batch,&geometry_item,1,
            geometry_faces,geometry_keys,KILLER_CLOWN_SKELETAL_FACE_COUNT));
        geometry_batch.dispatch=SAT_SCENE3D_PREPARE_DISPATCH_RUNTIME;
        sat_example_must(sat_scene_prepare_batch_async(&scene,&geometry_batch,&handle));
    }
#endif
    for(unsigned i=0;i<FIGHTERS;++i) {
        sat_example_must(sat_character_shadow(&scene,&g_clown_round.fighters[i].position,SAT_FX16_ONE/128,
            SAT_FX16_ONE/3,SAT_FX16_ONE/4,SAT_RGB555(3,3,4),GEOMETRY_PASS,SHADOW_SEGMENTS));
#if CLOWN_PARALLEL_GEOMETRY
        if(paired && i==FIGHTERS-1) {
            sat_example_must(sat_parallel_wait(handle,WORKER_TIMEOUT));
#if CLOWN_VERIFY_GEOMETRY
            const sat_scene3d_faces_t prefix=scene.faces;
#endif
            sat_example_must(sat_scene_merge_prepared_batch(&scene,&geometry_batch,handle));
#if CLOWN_VERIFY_GEOMETRY
            verify_geometry(&prefix);
#endif
            sat_example_must(sat_scene_prepare_batch_release(&scene,&geometry_batch,handle));
            ++g_clown_geometry.pairs;
            continue;
        }
#endif
        fighter_t* actor=&actors[i];
        sat_result_t result=sat_scene_submit_instance(&scene,&actor->instance,
            SAT_SCENE3D_SLOT_INHERIT,actor->projected,actor->world_vertices);
        if(result!=SAT_OK)++g_clown_metrics.draw_errors;
        sat_example_must(result);++g_clown_geometry.serial;
    }
}
static void update_camera(void) {
    sat_vec3_t a=g_clown_round.fighters[0].position,b=g_clown_round.fighters[1].position;
    sat_fx16_t midpoint_x=(a.x+b.x)/2,midpoint_z=(a.z+b.z)/2;
    camera.target=(sat_vec3_t){midpoint_x,CAMERA_TARGET_HEIGHT_Q16,midpoint_z};
    camera.eye=(sat_vec3_t){midpoint_x,CAMERA_HEIGHT_Q16,
        midpoint_z+CAMERA_DISTANCE_Q16+absolute(a.x-b.x)/3};
    camera.up=(sat_vec3_t){0,SAT_FX16_ONE,0};
    sat_mat4_t view,projection;
    sat_example_must(sat_mat4_look_at(&view,&camera.eye,&camera.target,&camera.up));
    sat_example_must(sat_mat4_perspective(&projection,SAT_FX16_ONE*50,
        SAT_FX16_ONE*SCREEN_W/SCREEN_H,near_depth,SAT_FX16_ONE*32));
    sat_example_must(sat_mat4_multiply(&camera.view_proj,&projection,&view));
    camera_focal=projection.m[5]*(SCREEN_H/2)/SAT_FX16_ONE;
}
static void panel(int x,int y,unsigned width,unsigned height,sat_color_t color);
static void draw_hud(void) {
    panel(0,0,SCREEN_W,40,(sat_color_t){5,9,16,255});
    text("CLOWN OCTAGON",8,8);
    text(g_clown_metrics.two_player?"P1 VS P2":"P1 VS CPU",232,8);
    for(unsigned i=0;i<FIGHTERS;++i) {
        int x=i?200:8;
        sat_rect_t border={x-1,25,HEALTH_BAR_W+2,HEALTH_BAR_H+2};
        sat_rect_t fill={x,26,HEALTH_BAR_W*g_clown_round.fighters[i].health/COMBAT_HEALTH,HEALTH_BAR_H};
        sat_example_must(sat_fill_rect(&border,(sat_color_t){24,24,27,255}));
        if(fill.width)sat_example_must(sat_fill_rect(&fill,i?(sat_color_t){220,70,75,255}:(sat_color_t){70,145,235,255}));
    }
    number("",(g_clown_round.round_ticks+COMBAT_TICK_HZ-1)/COMBAT_TICK_HZ,152,26);
    if(g_clown_round.round_over) {
        unsigned a=g_clown_round.fighters[0].health,b=g_clown_round.fighters[1].health;
        text(a==b?"DRAW":a>b?"PLAYER 1 WINS":g_clown_metrics.two_player?"PLAYER 2 WINS":"CPU WINS",104,64);
        text("START REMATCH",104,80);
    } else if(g_clown_metrics.paused)text("PAUSED",136,64);
    panel(0,208,SCREEN_W,16,(sat_color_t){5,9,16,255});
    text("START PAUSE   L+R MENU",72,212);
}
static void panel(int x,int y,unsigned width,unsigned height,sat_color_t color) {
    sat_rect_t rectangle={(int16_t)x,(int16_t)y,(uint16_t)width,(uint16_t)height};
    sat_example_must(sat_fill_rect(&rectangle,color));
}
static void centered(const char* message,int y) {
    text(message,(SCREEN_W-(int)__builtin_strlen(message)*8)/2,y);
}
static void open_menu(void) {
    g_clown_metrics.ui_state=UI_MENU;g_clown_metrics.paused=1;
    g_clown_metrics.menu_selection=0;++g_clown_metrics.menu_visits;
}
static void start_match(unsigned versus) {
    g_clown_metrics.two_player=versus;reset_round();g_clown_metrics.paused=0;
    g_clown_metrics.ui_state=UI_FIGHT;
    clown_audio_play(CLOWN_SOUND_BELL,0);
}
static void menu_input(const sat_pad_state_t* pad) {
    unsigned state=g_clown_metrics.ui_state;
    if(state==UI_FIGHT) {
        if(pad->pressed&SAT_PAD_START) {
            if(g_clown_round.round_over)start_match(g_clown_metrics.two_player);
            else {g_clown_metrics.ui_state=UI_PAUSE;g_clown_metrics.paused=1;submenu_selection=0;}
        } else if((pad->held&(SAT_PAD_L|SAT_PAD_R))==(SAT_PAD_L|SAT_PAD_R))open_menu();
        else {
            if(pad->pressed&SAT_PAD_Y)start_match(!g_clown_metrics.two_player);
            if(pad->pressed&SAT_PAD_Z)g_clown_metrics.fence_mode=(g_clown_metrics.fence_mode+1)%FENCE_MODE_COUNT;
        }
        return;
    }
    if(state==UI_OPTIONS || state==UI_CONTROLS) {
        if(pad->pressed&(SAT_PAD_B|SAT_PAD_START)) {open_menu();return;}
        if(state==UI_OPTIONS) {
            if(pad->pressed&(SAT_PAD_UP|SAT_PAD_DOWN))submenu_selection=!submenu_selection;
            if(pad->pressed&(SAT_PAD_A|SAT_PAD_LEFT|SAT_PAD_RIGHT)) {
                if(submenu_selection)g_clown_metrics.lights_enabled=!g_clown_metrics.lights_enabled;
                else g_clown_metrics.fence_mode=(g_clown_metrics.fence_mode+((pad->pressed&SAT_PAD_LEFT)?FENCE_MODE_COUNT-1:1))%FENCE_MODE_COUNT;
            }
        }
        return;
    }
    unsigned count=state==UI_MENU?4:3;
    volatile uint32_t* selection=state==UI_MENU?&g_clown_metrics.menu_selection:&submenu_selection;
    if(pad->pressed&SAT_PAD_UP)*selection=(*selection+count-1)%count;
    if(pad->pressed&SAT_PAD_DOWN)*selection=(*selection+1)%count;
    if(state==UI_PAUSE && (pad->pressed&SAT_PAD_B)) {
        g_clown_metrics.ui_state=UI_FIGHT;g_clown_metrics.paused=0;return;
    }
    if(pad->pressed&(SAT_PAD_A|SAT_PAD_START)) {
        if(state==UI_MENU) {
            if(*selection<2)start_match(*selection);
            else if(*selection==2){g_clown_metrics.ui_state=UI_OPTIONS;submenu_selection=0;++g_clown_metrics.options_visits;}
            else {g_clown_metrics.ui_state=UI_CONTROLS;++g_clown_metrics.controls_visits;}
        } else if(*selection==0) {g_clown_metrics.ui_state=UI_FIGHT;g_clown_metrics.paused=0;}
        else if(*selection==1)start_match(g_clown_metrics.two_player);
        else open_menu();
    }
}
static void draw_menu(void) {
    static const char* const fence_names[] = {"AUTO / PROXIMITY", "MESH COVERAGE", "SOLID CAGE", "CUTAWAY"};
    static const char* const main_items[] = {"ARCADE FIGHT", "VERSUS / 2 PLAYERS", "OPTIONS", "HOW TO PLAY"};
    static const char* const pause_items[] = {"RESUME", "RESTART ROUND", "MAIN MENU"};
    unsigned state=g_clown_metrics.ui_state;
    panel(36,20,248,190,(sat_color_t){8,13,23,255});
    panel(36,20,248,3,(sat_color_t){66,125,193,255});
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&title_font,
        state==UI_PAUSE?"PAUSED":"CLOWN CAGE",state==UI_PAUSE?112:80,36,16,0,0));
    centered(state==UI_MENU?"OCTAGON FIGHTING":"",62);
    if(state==UI_MENU || state==UI_PAUSE) {
        unsigned count=state==UI_MENU?4:3;
        unsigned selected=state==UI_MENU?g_clown_metrics.menu_selection:submenu_selection;
        const char* const* items=state==UI_MENU?main_items:pause_items;
        for(unsigned i=0;i<count;++i) {
            int y=88+(int)i*24;
            if(i==selected) {
                panel(54,y-6,212,22,(sat_color_t){22,48,79,255});
                panel(54,y-6,3,22,(sat_color_t){90,174,250,255});
                text(">",64,y);
            }
            text(items[i],80,y);
        }
        centered("D-PAD SELECT   A/START ENTER",188);
    } else if(state==UI_OPTIONS) {
        text(submenu_selection==0?"> FENCE":"  FENCE",56,90);
        text(fence_names[g_clown_metrics.fence_mode],72,108);
        text(submenu_selection==1?"> SPOTLIGHTS":"  SPOTLIGHTS",56,136);
        text(g_clown_metrics.lights_enabled?"BLUE + RED / ON":"OFF",72,154);
        centered("LEFT/RIGHT CHANGE  B BACK",184);
    } else {
        text("D-PAD  MOVE / SIDESTEP",56,84);text("A JAB     C KICK",56,104);
        text("B GUARD   X JUMP",56,124);text("START PAUSE   Z FENCE",56,144);
        text("L+R RETURN TO MENU",56,164);centered("B / START BACK",192);
    }
}
static void initialize_crowd(void) {
    uint32_t decoded=0;
    sat_example_must(sat_lz_decode(crowd_packed,CROWD_PACKED_BYTES,crowd_words,sizeof(crowd_words),&decoded));
    if(decoded!=sizeof(crowd_words))sat_example_must(SAT_ERR_VERIFY_FAILED);
    sat_example_must(sat_vdp2_vram_write_words(0,crowd_words,CROWD_BYTES/2));
    sat_vdp2_layer_config_t config;
    sat_vdp2_layer_config_default(SAT_VDP2_NBG0,&config);
    /* RGB555 cells in A0, two-word pattern names in A1. */
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x) {
        unsigned cell=y*64+x;
        layer_names[cell*2]=0;
        layer_names[cell*2+1]=(uint16_t)(((y%32)*32+(x%32))*4);
    }
    sat_example_must(sat_vdp2_vram_write_words(CROWD_NAME_ADDRESS/2,layer_names,MAP_CELLS*2));
    config.bitmap=0;config.pattern_name_words=2;config.char_bank_mask=SAT_VDP2_BANK_A0;
    config.color_mode=SAT_VDP2_COLOR_MODE_32768;config.priority=1;config.transparent=0;
    for(unsigned plane=0;plane<4;++plane)config.plane_address[plane]=CROWD_NAME_ADDRESS;
    sat_example_must(sat_vdp2_layer_configure(&config));
    sat_example_must(sat_vdp2_layer_set_zoom(SAT_VDP2_NBG0,SAT_FX16_ONE*2,SAT_FX16_ONE));
    sat_example_must(sat_lz_decode(mat_packed,MAT_PACKED_BYTES,crowd_words,sizeof(crowd_words),&decoded));
    if(decoded!=MAT_BYTES)sat_example_must(SAT_ERR_VERIFY_FAILED);
    sat_example_must(sat_vdp2_vram_write_words(GROUND_CHAR_ADDRESS/2,crowd_words,MAT_BYTES/2));
    sat_example_must(sat_vdp2_palette_upload(mat_palette,256,4*256));
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x) {
        unsigned tile=0;
        if(x>=16 && x<48 && y>=16 && y<48)tile=1+(y-16)*32+x-16;
        unsigned cell=y*64+x;
        layer_names[cell*2]=4<<4;
        layer_names[cell*2+1]=(uint16_t)(GROUND_CHAR_ADDRESS/32+tile*2);
    }
    sat_example_must(sat_vdp2_vram_write_words(GROUND_NAME_ADDRESS/2,layer_names,MAP_CELLS*2));
    sat_vdp2_rotation_config_t rotation;
    sat_vdp2_rotation_config_default(&rotation);
    rotation.pattern_name_words=2;rotation.name_bank_mask=SAT_VDP2_BANK_B1;
    rotation.char_bank_mask=SAT_VDP2_BANK_B0;rotation.priority=3;
    rotation.coefficients=1;rotation.over=SAT_VDP2_ROTATION_OVER_512;
    rotation.table_address=ROTATION_TABLE_A;
    for(unsigned plane=0;plane<16;++plane)rotation.plane_address[plane]=GROUND_NAME_ADDRESS;
    sat_example_must(sat_vdp2_rotation_configure(&rotation));
    sat_example_must(sat_vdp2_sprite_set_priority(6));
}
static void update_ground(void) {
    sat_vec3_t forward={camera.target.x-camera.eye.x,camera.target.y-camera.eye.y,camera.target.z-camera.eye.z};
    sat_vec3_normalize(&forward,&forward);
    sat_fx16_t sine=-forward.y,cosine=-forward.z;
    uint32_t table=ground_table_index?ROTATION_TABLE_B:ROTATION_TABLE_A;
    uint32_t coefficients=ground_table_index?COEFFICIENT_TABLE_B:COEFFICIENT_TABLE_A;
    for(int y=0;y<SCREEN_H;++y) {
        int64_t denominator=(int64_t)camera_focal*sine+(y-SCREEN_H/2)*(int64_t)cosine;
        int32_t scale=0;uint8_t hidden=denominator<=0;
        if(!hidden) {
            int64_t value=(int64_t)MAT_TEXELS_PER_UNIT*camera.eye.y*SAT_FX16_ONE/denominator;
            if(value>SAT_FX16_ONE*127)hidden=1;else scale=(int32_t)value;
        }
        ground_coefficients[y]=sat_vdp2_rotation_coefficient(scale,hidden);
    }
    sat_example_must(sat_vdp2_rotation_write_coefficients(coefficients,ground_coefficients,SCREEN_H));
    sat_vdp2_rotation_params_t parameters={0};
    parameters.dx=SAT_FX16_ONE;parameters.dyst=-sine;
    parameters.yst=camera_focal*cosine+(SCREEN_H/2)*sine;
    parameters.matrix[0]=parameters.matrix[4]=SAT_FX16_ONE;
    parameters.px=SCREEN_W/2;
    parameters.mx=(256-SCREEN_W/2)*SAT_FX16_ONE+camera.eye.x*MAT_TEXELS_PER_UNIT;
    parameters.my=256*SAT_FX16_ONE-camera.eye.z*MAT_TEXELS_PER_UNIT;
    parameters.kx=parameters.ky=SAT_FX16_ONE;
    parameters.coefficient_address=coefficients;parameters.coefficient_step=SAT_FX16_ONE;
    sat_example_must(sat_vdp2_rotation_write_params(table,&parameters));
    ground_table_index=!ground_table_index;++g_clown_metrics.ground_updates;
}
static void update_fence(void) {
    unsigned visible=0,mesh=0,hidden=0,solid=0;
    for(unsigned face=0;face<OCTAGON_FACE_COUNT;++face) {
        unsigned fade=0,hide=0;
        if(stage_foreground[face]) {
            if(g_clown_metrics.fence_mode==3)hide=1;
            else if(g_clown_metrics.fence_mode!=2) {
                fade=g_clown_metrics.fence_mode==1;
                if(g_clown_metrics.fence_mode==0) {
                    for(unsigned i=0;i<FIGHTERS;++i)
                        if(stage_depth[face]<camera.eye.z &&
                           stage_depth[face]>g_clown_round.fighters[i].position.z-FENCE_FADE_DISTANCE_Q16)
                            hide=1;
                }
            }
            if(hide)++hidden;else if(fade)++mesh;else ++solid;
        }
        if(hide)continue;
        for(unsigned corner=0;corner<4;++corner)
            stage_indices[visible*4+corner]=octagon_asset.indices[face*4+corner];
        stage_face_materials[visible]=fade?5:stage_base_material[face];
        ++visible;
    }
    stage_mesh.face_count=(uint16_t)visible;
    g_clown_metrics.stage_faces=visible;
    if(mesh)++g_clown_metrics.fence_mesh_frames;
    if(hidden)++g_clown_metrics.fence_hidden_frames;
    if(solid)++g_clown_metrics.fence_solid_frames;
}
static void initialize_lights(void) {
    /* The decoded crowd staging buffer is free after the VDP2 upload. */
    uint8_t* beam=(uint8_t*)crowd_words;
    uint8_t* pool=beam+LIGHT_TEXTURE_SIDE*LIGHT_TEXTURE_SIDE*CONE_SECTIONS;
    uint16_t palettes[LIGHT_COUNT][256]={{0}};
    for(unsigned y=0;y<LIGHT_TEXTURE_SIDE*CONE_SECTIONS;++y)for(unsigned x=0;x<LIGHT_TEXTURE_SIDE;++x) {
        int edge=(int)x-LIGHT_TEXTURE_SIDE/2;if(edge<0)edge=-edge;
        unsigned radial=(LIGHT_TEXTURE_SIDE/2-edge)*31/(LIGHT_TEXTURE_SIDE/2);
        unsigned axial=31-16*y/(LIGHT_TEXTURE_SIDE*CONE_SECTIONS);
        beam[y*LIGHT_TEXTURE_SIDE+x]=(uint8_t)(radial*axial/31);
    }
    for(unsigned y=0;y<LIGHT_TEXTURE_SIDE;++y)for(unsigned x=0;x<LIGHT_TEXTURE_SIDE;++x) {
        int dx=(int)x-LIGHT_TEXTURE_SIDE/2,dy=(int)y-LIGHT_TEXTURE_SIDE/2;
        unsigned square=(unsigned)(dx*dx+dy*dy),radius=LIGHT_TEXTURE_SIDE*LIGHT_TEXTURE_SIDE/4;
        pool[y*LIGHT_TEXTURE_SIDE+x]=square>=radius?0:(uint8_t)(LIGHT_POOL_FIRST_CODE+31*(radius-square)/radius);
    }
    for(unsigned i=1;i<32;++i) {
        unsigned intensity=i*LIGHT_BEAM_PEAK/31;
        palettes[0][i]=SAT_RGB555(intensity,intensity/4,intensity/6);
        palettes[1][i]=SAT_RGB555(intensity/6,intensity/3,intensity);
    }
    for(unsigned i=0;i<32;++i) {
        unsigned intensity=i*LIGHT_POOL_PEAK/31;
        palettes[0][LIGHT_POOL_FIRST_CODE+i]=SAT_RGB555(intensity,intensity/4,intensity/6);
        palettes[1][LIGHT_POOL_FIRST_CODE+i]=SAT_RGB555(intensity/6,intensity/3,intensity);
    }
    sat_vdp1_texture_t full;
    sat_example_must(sat_tex_upload_indexed8(&full,beam,LIGHT_TEXTURE_SIDE,
        LIGHT_TEXTURE_SIDE*CONE_SECTIONS,palettes[0],5));
    for(unsigned light=0;light<LIGHT_COUNT;++light)for(unsigned section=0;section<CONE_SECTIONS;++section) {
        beam_textures[light][section]=full;
        beam_textures[light][section].height=LIGHT_TEXTURE_SIDE;
        beam_textures[light][section].srca+=section*LIGHT_TEXTURE_SIDE*LIGHT_TEXTURE_SIDE/8;
        beam_textures[light][section].palette=5+light;
    }
    sat_example_must(sat_palette_upload_indexed8(palettes[1],6));
    sat_example_must(sat_tex_upload_indexed8_pixels(&pool_textures[0],pool,LIGHT_TEXTURE_SIDE,LIGHT_TEXTURE_SIDE,5));
    pool_textures[1]=pool_textures[0];pool_textures[1].palette=6;
    /* Only marked palette sprites add to the mat. RGB fighters and HUD
     * retain priority selector zero and are excluded from color calculation. */
    sat_example_must(sat_vdp2_sprite_color_calc_configure_alpha(7));
    g_clown_metrics.lights_enabled=1;
}
static int color_channel(int channel) {return channel<0?0:channel>31?31:channel;}
static void light_normals_range(void* context,uint32_t begin,uint32_t end) {
    (void)context;
    for(unsigned actor=begin;actor<end;++actor) {
        fighter_t* a=&actors[actor];
        a->lighting_result=sat_mesh_vertex_normals_scaled(&a->mesh,a->normals,
            KILLER_CLOWN_SKELETAL_VERTEX_COUNT);
    }
}
static void light_shading_range(void* context,uint32_t begin,uint32_t end) {
    (void)context;
    for(unsigned actor=begin;actor<end;++actor) {
        fighter_t* a=&actors[actor];const combat_fighter_t* fighter=&g_clown_round.fighters[actor];
        for(unsigned light=0;light<LIGHT_COUNT;++light) {
            a->light_visible[light]=0;a->light_total[light]=0;
        }
        for(unsigned vertex=0;vertex<KILLER_CLOWN_SKELETAL_VERTEX_COUNT;++vertex) {
            sat_vec3_t n=clown_light_normalize(a->normals[vertex]),v=a->vertices[vertex];
            sat_vec3_t normal={fighter->facing*n.z,n.y,-fighter->facing*n.x};
            sat_vec3_t point={fighter->position.x+fighter->facing*v.z,
                v.y-CLOWN_FEET_Q16,fighter->position.z-fighter->facing*v.x};
            unsigned strength[LIGHT_COUNT]={0};
            int64_t facing=(int64_t)normal.x*(camera.eye.x-point.x)+
                (int64_t)normal.y*(camera.eye.y-point.y)+(int64_t)normal.z*(camera.eye.z-point.z);
            for(unsigned light=0;light<LIGHT_COUNT;++light) {
                strength[light]=clown_spotlight_diffuse(&spotlights[light],point,normal);
                a->light_total[light]+=strength[light];
                if(strength[light]>16 && facing>0)++a->light_visible[light];
            }
            int red=(actor?16:14)+(int)(strength[0]*15/256)-(int)(strength[1]*2/256);
            int green=14+(int)(strength[0]*3/256+strength[1]*5/256);
            int blue=(actor?14:16)+(int)(strength[1]*15/256)-(int)(strength[0]*3/256);
            a->gouraud[vertex]=sat_gouraud_rgb(color_channel(red)-16,color_channel(green)-16,color_channel(blue)-16);
        }
    }
}
static void light_range(sat_parallel_range_fn fn) {
#if CLOWN_PARALLEL_LIGHTING
    /* Range execution purges Slave's input cache and joins before returning.
     * All immutable pose, camera, light and reciprocal-table inputs have
     * been written by Master. Consume precisely the second actor's output. */
    sat_example_must(sat_parallel_for(0,FIGHTERS,1,fn,NULL,
        actors[FIGHTERS-1].normals,LIGHT_OUTPUT_BYTES));
#else
    fn(NULL,0,FIGHTERS);
#endif
}
static void update_lights(void) {
    unsigned phase=g_clown_metrics.simulation_ticks%LIGHT_SWEEP_TICKS;
    sat_fx16_t angle=(sat_fx16_t)((int64_t)phase*360*SAT_FX16_ONE/LIGHT_SWEEP_TICKS);
    if(!g_clown_metrics.lights_enabled) {
        for(unsigned actor=0;actor<FIGHTERS;++actor) {
            uint16_t ambient=sat_gouraud_rgb(actor?0:-2,-2,actor?-2:0);
            for(unsigned vertex=0;vertex<KILLER_CLOWN_SKELETAL_VERTEX_COUNT;++vertex)
                actors[actor].gouraud[vertex]=ambient;
        }
        g_clown_light_metrics.visible_red=g_clown_light_metrics.visible_blue=0;
        g_clown_light_metrics.red_strength=g_clown_light_metrics.blue_strength=0;
        g_clown_light_metrics.phase=(uint32_t)(angle/SAT_FX16_ONE);
        ++g_clown_light_metrics.updates;++g_clown_metrics.light_updates;
        return;
    }
    for(unsigned light=0;light<LIGHT_COUNT;++light) {
        unsigned actor=light?0:1;
        const combat_fighter_t* fighter=&g_clown_round.fighters[actor];
        sat_fx16_t orbit=angle+light*90*SAT_FX16_ONE;
        sat_vec3_t source={light?-LIGHT_SOURCE_X_Q16:LIGHT_SOURCE_X_Q16,LIGHT_SOURCE_Y_Q16,LIGHT_SOURCE_Z_Q16};
        const sat_aabb3_t* bounds=&actors[actor].bounds;
        sat_fx16_t torso=bounds->center.y+sat_fx16_mul(bounds->half.y,
            2*LIGHT_TORSO_FRACTION_Q16-SAT_FX16_ONE)-CLOWN_FEET_Q16;
        if(torso<SAT_FX16_ONE/4)torso=SAT_FX16_ONE/4;
        if(torso>source.y-SAT_FX16_ONE/2)torso=source.y-SAT_FX16_ONE/2;
        sat_vec3_t target={fighter->position.x+sat_fx16_mul(sat_cos_deg(orbit),LIGHT_ORBIT_X_Q16),
            torso+sat_fx16_mul(sat_sin_deg(orbit*2),LIGHT_ORBIT_Y_Q16),
            fighter->position.z+sat_fx16_mul(sat_sin_deg(orbit),LIGHT_ORBIT_Z_Q16)};
        clown_spotlight_aim(&spotlights[light],source,target);
    }
    profile_start(PROFILE_NORMALS);
    light_range(light_normals_range);
    profile_stop(PROFILE_NORMALS);
    for(unsigned actor=0;actor<FIGHTERS;++actor)sat_example_must(actors[actor].lighting_result);
    light_range(light_shading_range);
    ++g_clown_lighting_jobs.updates;
#if CLOWN_VERIFY_LIGHTING
    /* Both ranges have joined. Preserve actual output before recomputing the
     * second actor on Master at exactly the same pose, camera and lights. */
    const uint8_t* actual=(const uint8_t*)actors[FIGHTERS-1].normals;
    for(unsigned byte=0;byte<LIGHT_OUTPUT_BYTES;++byte)lighting_reference[byte]=actual[byte];
    light_normals_range(NULL,FIGHTERS-1,FIGHTERS);
    light_shading_range(NULL,FIGHTERS-1,FIGHTERS);
    for(unsigned byte=0;byte<LIGHT_OUTPUT_BYTES;++byte)if(lighting_reference[byte]!=actual[byte]) {
        ++g_clown_lighting_jobs.errors;sat_example_must(SAT_ERR_VERIFY_FAILED);
    }
    ++g_clown_lighting_jobs.verified;
#endif
    unsigned visible[LIGHT_COUNT]={0},total[LIGHT_COUNT]={0};
    for(unsigned actor=0;actor<FIGHTERS;++actor)for(unsigned light=0;light<LIGHT_COUNT;++light) {
        visible[light]+=actors[actor].light_visible[light];
        total[light]+=actors[actor].light_total[light];
    }
    g_clown_light_metrics.visible_red=visible[0];g_clown_light_metrics.visible_blue=visible[1];
    if(visible[0]>g_clown_light_metrics.max_visible_red)g_clown_light_metrics.max_visible_red=visible[0];
    if(visible[1]>g_clown_light_metrics.max_visible_blue)g_clown_light_metrics.max_visible_blue=visible[1];
    g_clown_light_metrics.red_strength=total[0];g_clown_light_metrics.blue_strength=total[1];
    g_clown_light_metrics.phase=(uint32_t)(angle/SAT_FX16_ONE);
    ++g_clown_light_metrics.updates;++g_clown_metrics.light_updates;
}
static sat_vec3_t beam_point(sat_vec3_t source,sat_vec3_t end,unsigned ring) {
    /* Two depth slices make front/back parts enter the painter separately. */
    sat_fx16_t fraction=ring==0?SAT_FX16_ONE/32:ring==1?SAT_FX16_ONE/2:SAT_FX16_ONE;
    return (sat_vec3_t){source.x+sat_fx16_mul(end.x-source.x,fraction),
        source.y+sat_fx16_mul(end.y-source.y,fraction),source.z+sat_fx16_mul(end.z-source.z,fraction)};
}
static void draw_lights(void) {
    g_clown_light_metrics.cone_faces=g_clown_light_metrics.pool_faces=0;
    if(!g_clown_metrics.lights_enabled)return;
    for(unsigned light=0;light<LIGHT_COUNT;++light) {
        const clown_spotlight_t* spot=&spotlights[light];
        sat_vec3_t rings[CONE_SECTIONS+1][CONE_SIDES];unsigned ground=0;
        for(unsigned side=0;side<CONE_SIDES;++side) {
            sat_vec3_t end;
            ground+=clown_spotlight_ground(spot,side*360*SAT_FX16_ONE/CONE_SIDES,&end);
            for(unsigned ring=0;ring<=CONE_SECTIONS;++ring)rings[ring][side]=beam_point(spot->source,end,ring);
        }
        if(ground==CONE_SIDES) {
            const sat_vec3_t* rim=rings[CONE_SECTIONS];
            sat_vec3_t center={(rim[0].x+rim[2].x+rim[4].x+rim[6].x)/4,SAT_FX16_ONE/256,
                (rim[0].z+rim[2].z+rim[4].z+rim[6].z)/4};
            sat_vec3_t u={(rim[0].x-rim[4].x)/2,0,(rim[0].z-rim[4].z)/2};
            sat_vec3_t v={(rim[2].x-rim[6].x)/2,0,(rim[2].z-rim[6].z)/2};
            sat_quad3_t pool={{{center.x-u.x-v.x,center.y,center.z-u.z-v.z},
                {center.x+u.x-v.x,center.y,center.z+u.z-v.z},
                {center.x+u.x+v.x,center.y,center.z+u.z+v.z},
                {center.x-u.x+v.x,center.y,center.z-u.z+v.z}}};
            sat_scene3d_material_t material={0};material.kind=SAT_SCENE3D_INDEXED_TEXTURED;
            material.texture=&pool_textures[light];material.color_calc_slot=SAT_VDP2_SPRITE_ADD;
            sat_example_must(sat_scene_submit_quad(&scene,&pool,&material,LIGHT_PASS));
            ++g_clown_light_metrics.pool_faces;
        }
        for(unsigned section=0;section<CONE_SECTIONS;++section)for(unsigned side=0;side<CONE_SIDES;++side) {
            unsigned next=(side+1)%CONE_SIDES;
            sat_quad3_t shell={{rings[section][side],rings[section][next],rings[section+1][next],rings[section+1][side]}};
            sat_vec3_t edge_a,edge_b,normal,toward_camera;
            sat_vec3_sub(&edge_a,&shell.v[1],&shell.v[0]);
            sat_vec3_sub(&edge_b,&shell.v[2],&shell.v[0]);
            sat_vec3_cross(&normal,&edge_a,&edge_b);
            sat_vec3_sub(&toward_camera,&camera.eye,&shell.v[0]);
            if(sat_vec3_dot(&normal,&toward_camera)<=0)continue;
            sat_scene3d_material_t material={0};material.kind=SAT_SCENE3D_INDEXED_TEXTURED;
            material.texture=&beam_textures[light][section];
            material.color_calc_slot=SAT_VDP2_SPRITE_ADD;
            sat_example_must(sat_scene_submit_quad(&scene,&shell,&material,LIGHT_PASS));
            ++g_clown_light_metrics.cone_faces;
        }
        const sat_vec3_t source=spot->source;
        sat_quad3_t lamp={{{source.x-SAT_FX16_ONE/8,source.y-SAT_FX16_ONE/8,source.z},
            {source.x+SAT_FX16_ONE/8,source.y-SAT_FX16_ONE/8,source.z},
            {source.x+SAT_FX16_ONE/8,source.y+SAT_FX16_ONE/8,source.z},
            {source.x-SAT_FX16_ONE/8,source.y+SAT_FX16_ONE/8,source.z}}};
        uint16_t gouraud[4]={sat_gouraud_grey(-8),light?sat_gouraud_rgb(-2,2,15):sat_gouraud_rgb(15,0,-2),
            light?sat_gouraud_rgb(0,6,15):sat_gouraud_rgb(15,6,2),sat_gouraud_grey(-8)};
        sat_scene3d_material_t material={0};material.kind=SAT_SCENE3D_RGB;
        material.rgb555=SAT_RGB555(16,16,16);material.vertex_gouraud=gouraud;
        material.color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
        sat_example_must(sat_scene_submit_quad(&scene,&lamp,&material,GEOMETRY_PASS));
    }
}
static void initialize_assets(void) {
    sat_example_must(sat_model_upload_textures(&killer_clown_skeletal_asset,fighter_textures[0],KILLER_CLOWN_SKELETAL_TEXTURE_COUNT));
    for(unsigned i=0;i<FIGHTERS;++i) {
        fighter_t* a=&actors[i];
        for(unsigned t=0;t<KILLER_CLOWN_SKELETAL_TEXTURE_COUNT;++t) {
            fighter_textures[i][t]=fighter_textures[0][t];
            fighter_materials[i][t]=(sat_scene3d_material_t){0};
            fighter_materials[i][t].kind=SAT_SCENE3D_INDEXED_TEXTURED;
            fighter_materials[i][t].texture=&fighter_textures[i][t];
            fighter_materials[i][t].vertex_gouraud=a->gouraud;
            fighter_materials[i][t].color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
        }
        sat_example_must(sat_mesh_init(&a->mesh,a->vertices,KILLER_CLOWN_SKELETAL_VERTEX_COUNT,a->indices,KILLER_CLOWN_SKELETAL_FACE_COUNT));
        sat_example_must(sat_model_copy_to_mesh(&killer_clown_skeletal_asset,&a->mesh));
        sat_example_must(sat_skeleton_instance_init(&a->skeleton,&killer_clown_skeletal_skeleton,a->nodes,JOINT_CAP,a->scratch,JOINT_CAP));
        a->instance=(sat_scene3d_instance_t){0};a->instance.mesh=&a->mesh;
        a->instance.materials=fighter_materials[i];a->instance.material_count=KILLER_CLOWN_SKELETAL_TEXTURE_COUNT;
        a->instance.face_materials=killer_clown_skeletal_asset.face_texture_indices;
        a->instance.world=&a->world;a->instance.bounds=&a->bounds;a->instance.cull_backfaces=1;
        a->instance.pass=GEOMETRY_PASS;
        sat_example_must(sat_scene3d_instance_binding_init(&a->binding,&a->mesh,
            fighter_materials[i],KILLER_CLOWN_SKELETAL_TEXTURE_COUNT,a->instance.face_materials));
        a->instance.validated_binding=&a->binding;
    }
    uint8_t wire_pixels[64*64/2]={0};uint16_t wire_palette[16]={0};
    wire_palette[1]=SAT_RGB555(2,3,4);
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x) {
        unsigned code=((x+y)%16==0 || (x+64-y)%16==0)?1:0;
        wire_pixels[(y*64+x)/2]|=(uint8_t)(code<<(x%2?0:4));
    }
    uint16_t wire_lut;sat_example_must(sat_vdp1_upload_lut(wire_palette,&wire_lut));
    sat_example_must(sat_tex_upload_lut4_pixels(&wire_texture,wire_pixels,64,64,wire_lut));
    for(unsigned m=0;m<4;++m) {
        stage_materials[m]=(sat_scene3d_material_t){0};stage_materials[m].kind=SAT_SCENE3D_RGB;
        stage_materials[m].rgb555=SAT_RGB555(3+m,4+m,6+m);
        stage_materials[m].color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
    }
    stage_materials[4]=(sat_scene3d_material_t){0};stage_materials[4].kind=SAT_SCENE3D_INDEXED_TEXTURED;
    stage_materials[4].texture=&wire_texture;stage_materials[4].color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
    stage_materials[5]=stage_materials[4];stage_materials[5].sprite_flags=SAT_SPRITE_FLAG_MESH;
    sat_example_must(sat_mesh_init(&stage_mesh,stage_vertices,OCTAGON_VERTEX_COUNT,stage_indices,OCTAGON_FACE_COUNT));
    sat_example_must(sat_model_copy_to_mesh(&octagon_asset,&stage_mesh));
    /* Fixed broadcast camera: omit tall camera-side cage faces, retaining
       floor/base, rear fence and rear columns in the global scene painter. */
    for(unsigned face=0;face<OCTAGON_FACE_COUNT;++face) {
        int64_t center_y=0;sat_fx16_t top=0,min_x=INT32_MAX,max_x=INT32_MIN,min_y=INT32_MAX,max_y=INT32_MIN;
        for(unsigned corner=0;corner<4;++corner) {
            sat_vec3_t point=stage_vertices[octagon_asset.indices[face*4+corner]];
            center_y+=point.y;if(point.z>top)top=point.z;
            if(point.x<min_x)min_x=point.x;
            if(point.x>max_x)max_x=point.x;
            if(point.y<min_y)min_y=point.y;
            if(point.y>max_y)max_y=point.y;
        }
        unsigned fence=(max_x-min_x> SAT_FX16_ONE || max_y-min_y>SAT_FX16_ONE);
        stage_base_material[face]=fence?4:(face%4);
        stage_foreground[face]=(fence && center_y<0);
        stage_depth[face]=(sat_fx16_t)(-center_y/4);
    }
    /* The source is Z-up; bake its floor down to game Y=0 in the instance. */
    sat_mat4_t rotation,translation;
    sat_example_must(sat_mat4_rotate_x(&rotation,-SAT_FX16_ONE*90));
    sat_example_must(sat_mat4_translate(&translation,0,-OCTAGON_FLOOR_Z_Q16,0));
    sat_example_must(sat_mat4_multiply(&stage_world,&translation,&rotation));
    stage_instance=(sat_scene3d_instance_t){0};stage_instance.mesh=&stage_mesh;
    stage_instance.pass=GEOMETRY_PASS;
    stage_instance.materials=stage_materials;stage_instance.material_count=STAGE_MATERIAL_COUNT;
    stage_instance.face_materials=stage_face_materials;stage_instance.world=&stage_world;
    /* Dynamic topology/material selectors are validated on each submission. */
}
int main(void) {
    sat_video_config_t video={SCREEN_W,SCREEN_H,SAT_VIDEO_AUTO,0};
    sat_example_must(sat_init(&video));
    sat_example_must(sat_vdp1_set_frame_mode(SAT_VDP1_FRAME_ON_SUBMIT));
    sat_parallel_config_t parallel={SAT_PARALLEL_AUTO,NULL,0,0,WORKER_TIMEOUT};
    sat_example_must(sat_parallel_init(&parallel));sat_example_must(sat_skin_parallel_register());
    sat_example_must(sat_vdp2_back_color_set(SAT_BGR555(2,3,6)));
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&font,SAT_COLOR_WHITE,SAT_COLOR_BLACK,0));
    sat_example_must(sat_ascii_font_init_scaled_indexed8(&title_font,SAT_COLOR_WHITE,SAT_COLOR_BLACK,0,2));
    text("LOADING CLOWN OCTAGON",8,8);
    initialize_crowd();initialize_assets();initialize_lights();clown_audio_init();reset_round();near_depth=SAT_FX16_ONE/8;
    open_menu();
    sat_example_must(sat_scene_init(&scene,faces,keys,order,FACE_CAP));
    g_clown_metrics.stage_faces=stage_mesh.face_count;g_clown_metrics.actor_faces=KILLER_CLOWN_SKELETAL_FACE_COUNT;
    previous_ms=rate_mark=sat_time_ms();
    g_clown_lighting_jobs.enabled=CLOWN_PARALLEL_LIGHTING;
    g_clown_lighting_jobs.verify=CLOWN_VERIFY_LIGHTING;
    g_clown_geometry.enabled=CLOWN_PARALLEL_GEOMETRY;
    g_clown_geometry.verify=CLOWN_VERIFY_GEOMETRY;
    g_clown_profile.magic=CLOWN_PROFILE_MAGIC;
    g_clown_profile.capacity=CLOWN_PROFILE_CAPACITY;
    g_clown_profile.row_bytes=sizeof(clown_profile_row_t);
    g_clown_profile.frequency=sat_time_frequency_hz();
    while(1) {
#if CLOWN_PROFILE
        profile_row=(clown_profile_row_t){0};
        uint64_t profile_begin=sat_time_ticks();
#endif
        profile_start(PROFILE_WAIT);
        sat_example_must(sat_wait_vblank());sat_example_must(sat_begin_frame());
        profile_stop(PROFILE_WAIT);
        profile_start(PROFILE_INPUT);
        sat_pad_state_t p1={0},p2={0};
        sat_example_must(sat_input_poll());
        sat_example_must(sat_pad_tap_state(0,0,&p1));
        sat_example_must(sat_pad_tap_state(1,0,&p2));
        if(g_clown_metrics.two_player)g_clown_metrics.second_pad_buttons|=p2.held;
        menu_input(&p1);
        uint32_t now=sat_time_ms(),elapsed=now-previous_ms;previous_ms=now;
        if(!g_clown_metrics.paused) {
            accumulator+=(uint64_t)elapsed*COMBAT_TICK_HZ;
            while(accumulator>=MS_PER_SECOND){simulate(&p1,&p2);accumulator-=MS_PER_SECOND;}
        }
        clown_audio_update();
        profile_stop(PROFILE_INPUT);
        profile_start(PROFILE_EVALUATE);evaluate_fighters();profile_stop(PROFILE_EVALUATE);
        profile_start(PROFILE_ENVIRONMENT);
        update_camera();update_fence();update_ground();
        profile_stop(PROFILE_ENVIRONMENT);
        profile_start(PROFILE_LIGHTS);update_lights();profile_stop(PROFILE_LIGHTS);
        sat_example_must(sat_vdp2_layer_set_scroll(SAT_VDP2_NBG0,
            (camera.target.x*12/SAT_FX16_ONE)*SAT_FX16_ONE,0));
        profile_start(PROFILE_GEOMETRY);
        sat_example_must(sat_scene_begin(&scene,&camera,near_depth,SCREEN_W,SCREEN_H,HUD_RESERVE));
        draw_lights();
        sat_result_t result=sat_scene_submit_instance(&scene,&stage_instance,SAT_SCENE3D_SLOT_INHERIT,stage_projected,stage_world_vertices);
        if(result!=SAT_OK)++g_clown_metrics.draw_errors;
        sat_example_must(result);
        prepare_fighters();
        profile_stop(PROFILE_GEOMETRY);profile_start(PROFILE_FLUSH);
        result=sat_scene_flush(&scene);if(result!=SAT_OK)++g_clown_metrics.draw_errors;sat_example_must(result);
        profile_stop(PROFILE_FLUSH);
#if CLOWN_PROFILE
        sat_scene3d_profile_t scene_profile={0};
        sat_example_must(sat_scene3d_profile_get(&scene_profile));
        profile_row.painter=scene_profile.painter_ticks;
        profile_row.emission=scene_profile.emission_ticks;
#endif
        g_clown_metrics.commands=scene.commands_used;
        profile_start(PROFILE_HUD);
        if(g_clown_metrics.ui_state==UI_FIGHT)draw_hud();else draw_menu();
        profile_stop(PROFILE_HUD);profile_start(PROFILE_END);
        sat_example_must(sat_end_frame());profile_stop(PROFILE_END);++g_clown_metrics.frames;++rate_frames;
        sat_vdp1_frame_stats_t frame_stats;
        sat_example_must(sat_vdp1_frame_stats(&frame_stats));
        g_clown_metrics.draw_timeouts=frame_stats.draw_timeouts;
        g_clown_metrics.late_swaps=frame_stats.late_swaps;
#if CLOWN_PROFILE
        profile_row.frame=g_clown_metrics.frames;
        profile_row.fields=sat_irq_count(SAT_IRQ_VBLANK_IN);
        profile_row.simulation=g_clown_metrics.simulation_ticks;
        profile_row.ui=g_clown_metrics.ui_state;
        profile_row.lights=g_clown_metrics.lights_enabled;
        profile_row.commands=scene.commands_used;
        profile_row.late_swaps=frame_stats.late_swaps;
        profile_row.elapsed=(uint32_t)(sat_time_ticks()-profile_begin);
        g_clown_profile.rows[g_clown_profile.count%CLOWN_PROFILE_CAPACITY]=profile_row;
        ++g_clown_profile.count;
        if(g_clown_profile.count>CLOWN_PROFILE_CAPACITY)++g_clown_profile.overwritten;
#endif
        now=sat_time_ms();
        if(now-rate_mark>=MS_PER_SECOND) {
            g_clown_metrics.submissions_tenths=rate_frames*MS_PER_SECOND*10/(now-rate_mark);
            rate_frames=0;rate_mark=now;
        }
    }
}
