#include "demo.h"
#include "combat.h"
#include "fighter_assets.h"
#include "saturn/saturn.h"
#include "saturn/font.h"
#include "saturn/asset.h"
#include "saturn/vdp2.h"
#include "saturn/character3d.h"
#include "saturn/render2d.h"
#include "saturn/cd_mount.h"
#include "saturn/irq.h"
#include "examples/common/example_util.h"

#ifndef CHARACTER_DEMO_MAX_ACTORS
#define CHARACTER_DEMO_MAX_ACTORS 1
#endif
#ifndef CHARACTER_DEMO_ASSET_SLOTS
#define CHARACTER_DEMO_ASSET_SLOTS 1
#endif
#ifndef CHARACTER_DEMO_DISC_ASSETS
#define CHARACTER_DEMO_DISC_ASSETS 0
#endif
enum {
    SCREEN_WIDTH=320, SCREEN_HEIGHT=224, FONT_BANK=7, BACKGROUND_BANK=6,
    BACKGROUND_MAP_PLANE=0x3B, PALETTE_ENTRIES=256,
    SCENE_CAPACITY=2000, HUD_RESERVE=384, SHADOW_SEGMENTS=8,
    SIMULATION_HZ=COMBAT_TICK_HZ, MILLISECONDS_PER_SECOND=1000,
    RATE_DECIMAL_SCALE=10, TRANSITION_TICKS=9, PARALLEL_TIMEOUT_TICKS=60000,
    BACKGROUND_WIDTH=64, BACKGROUND_HEIGHT=64, ARENA_TILES=5,
    ACTOR_VERTEX_CAP=FIGHTER_HIGH_VERTEX_COUNT, ACTOR_FACE_CAP=FIGHTER_HIGH_FACE_COUNT,
    LOD_COUNT=3, POLICY_COUNT=3, STRESS_PRESET_COUNT=4,
    MICROSECONDS_PER_SECOND=1000000, KILOBYTE_BYTES=1024,
    CHARACTER_RAM_BUDGET_BYTES=768*KILOBYTE_BYTES,
    CHARACTER_TEXTURE_BUDGET_BYTES=32*KILOBYTE_BYTES,
    DEMO_SIGNATURE=0x43484152u
};
enum { ARENA_CAMERA_DISTANCE_Q16=SAT_FX16_ONE*7/2,
       ARENA_CAMERA_SEPARATION_DIVISOR=2, CAMERA_SMOOTHING_DIVISOR=8 };
static const unsigned kStressCounts[STRESS_PRESET_COUNT]={1,2,4,8};
static const char* const kPolicyNames[POLICY_COUNT]={"MASTER","SLAVE","AUTO"};
static const char* const kLodNames[LOD_COUNT]={"HIGH","MEDIUM","LOW"};
static const sat_model_asset_t* const kModels[LOD_COUNT]={&fighter_high_asset,&fighter_medium_asset,&fighter_low_asset};
static const sat_skin_asset_t* const kSkins[LOD_COUNT]={&fighter_high_skin,&fighter_medium_skin,&fighter_low_skin};
typedef struct {
    sat_character_asset_t view;
    uint32_t data[(FIGHTER_ARCHIVE_BYTES+sizeof(uint32_t)-1)/sizeof(uint32_t)];
    sat_skeleton_clip_t clips[FIGHTER_CLIP_COUNT];
    sat_model_texture_asset_t textures[FIGHTER_TEXTURE_COUNT];
    sat_asset_t source;
    sat_texture_t owners[FIGHTER_TEXTURE_COUNT];
    sat_vdp1_texture_t native[FIGHTER_TEXTURE_COUNT];
    sat_scene3d_material_t materials[FIGHTER_TEXTURE_COUNT];
} asset_slot_t;
typedef struct {
    sat_skeleton_instance_t skeleton;
    sat_transform3d_node_t nodes[FIGHTER_JOINT_COUNT];
    uint16_t scratch[FIGHTER_JOINT_COUNT];
    sat_skeleton_pose_joint_t pose[FIGHTER_JOINT_COUNT],fade_pose[FIGHTER_JOINT_COUNT];
    sat_mat4_t joints[FIGHTER_JOINT_COUNT],world;
    sat_pose_fade_t fade;
    sat_skel_clip_player_t player;
    sat_mesh_t mesh;
    sat_vec3_t vertices[ACTOR_VERTEX_CAP],world_vertices[ACTOR_VERTEX_CAP],normals[ACTOR_VERTEX_CAP];
    sat_projected_vertex_t projected[ACTOR_VERTEX_CAP];
    uint16_t indices[ACTOR_FACE_CAP*4],face_materials[ACTOR_FACE_CAP],gouraud[ACTOR_VERTEX_CAP];
    sat_scene3d_material_t shaded[FIGHTER_TEXTURE_COUNT];
    sat_scene3d_instance_t instance;
    sat_scene3d_instance_binding_t binding;
    sat_aabb3_t bounds;
    sat_skin_job_t job;
    sat_parallel_handle_t task;
    unsigned clip,lod,slot,flash_ticks,seen_hit;
    sat_vec3_t position,root_motion;
} actor_t;
static asset_slot_t g_assets[CHARACTER_DEMO_ASSET_SLOTS];
#if CHARACTER_DEMO_DISC_ASSETS
static sat_cd_block_t g_cd_block;
static sat_cd_device_t g_cd_device;
static sat_cdfs_volume_t g_cd_volume;
static sat_cdfs_file_source_t g_cd_sources[CHARACTER_DEMO_ASSET_SLOTS];
#endif
static unsigned g_source_registered[CHARACTER_DEMO_ASSET_SLOTS];
static actor_t g_actors[CHARACTER_DEMO_MAX_ACTORS];
static sat_scene3d_face_t g_faces[SCENE_CAPACITY];
static uint32_t g_keys[SCENE_CAPACITY];
static uint16_t g_order[SCENE_CAPACITY];
static sat_scene_t g_scene;
static sat_ascii_font_t g_font;
static sat_orbit_camera3d_t g_orbit;
static combat_round_t g_combat;
/* Post-resolution positions and accumulated invariants for emulator inspection. */
volatile struct {
    int32_t player_x,player_z,opponent_x,opponent_z;
    uint32_t checked_ticks,overlaps;
} g_character_bodies;
static character_demo_mode_t g_mode;
static unsigned g_actor_count,g_stress_preset,g_quality,g_policy=SAT_PARALLEL_AUTO;
static unsigned g_view_clip,g_paused,g_debug,g_shading=1,g_complexity,g_hud=1;
static sat_fx16_t g_speed=SAT_FX16_ONE;
static uint32_t g_last_ms,g_rate_mark,g_rate_frames,g_rate_sim,g_tick_fraction;
static uint64_t g_accumulator;
static uint8_t g_background[BACKGROUND_WIDTH*BACKGROUND_HEIGHT];
static uint16_t g_background_map[SAT_VDP2_NBG0_MAP_CELLS],g_background_palette[256];
enum { BENCHMARK_SAMPLE_CAPACITY=1024, BENCHMARK_SAMPLE_MAGIC=0x43424e31u };
typedef struct {
    uint32_t iteration,vblank,simulation_ticks,requested,accepted,lod,policy;
    uint32_t frame_ms,cpu_ms,skin_ms,preparation_ms,sort_us,emission_us,commands;
    uint32_t slave_tasks,master_tasks,wait_ticks,queue_ticks;
    uint32_t complexity;
} benchmark_sample_t;
volatile struct {
    uint32_t magic,count,capacity,row_bytes;
    benchmark_sample_t rows[BENCHMARK_SAMPLE_CAPACITY];
} g_character_benchmark __attribute__((section(".wram_l")));
/* Stable, named telemetry layout consumed by harness/tests/character_plan.py. */
typedef struct {
    uint32_t magic,frames,simulation_ticks,steps_last,loop_rate_x10,simulation_rate_x10;
    uint32_t requested,accepted,downgraded,rejected,policy,quality,clip,animation_frame;
    uint32_t joints,vertices,visible_faces,commands,command_capacity;
    uint32_t skeleton_ms,skin_ms,geometry_ms,sort_us,emit_us,frame_ms,max_frame_ms;
    uint32_t persistent_bytes,texture_bytes,slave_tasks,master_tasks,wait_ticks,queue_ticks;
    uint32_t player_health,opponent_health,hits,blocked,knockdowns,round_over;
    uint32_t events,root_travel,skin_equivalence,source_reloads,errors;
    uint32_t combat_hits_total,combat_blocks_total;
} character_metrics_t;
static character_metrics_t g_metrics;
volatile character_metrics_t g_character_demo;

static uint32_t ticks_us(uint32_t ticks) {
    const uint32_t frequency=sat_time_frequency_hz();
    return frequency?(uint32_t)((uint64_t)ticks*MICROSECONDS_PER_SECOND/frequency):0;
}
static sat_fx16_t tick_delta(void) { return SAT_FX16_ONE/SIMULATION_HZ; }
static void select_clip(actor_t* actor,unsigned clip) {
    if(actor->clip==clip && actor->player.clip)return;
    sat_example_must(sat_pose_fade_begin(&actor->fade,actor->pose,tick_delta()*TRANSITION_TICKS));
    sat_example_must(sat_skel_clip_player_set_clip(&actor->player,&g_assets[actor->slot].clips[clip],0));
    actor->clip=clip;
}
static void load_assets(unsigned slot) {
    asset_slot_t* asset=&g_assets[slot];
    const char* path=slot?"characters/red":"characters/blue";
    const char* source=slot?"embedded/red.ska":"embedded/blue.ska";
#if CHARACTER_DEMO_DISC_ASSETS
    source=slot?"disc/red.ska":"disc/blue.ska";
#endif
    if(!g_source_registered[slot]) {
#if !CHARACTER_DEMO_DISC_ASSETS
        sat_example_must(sat_file_register_blob(source,fighter_archive,FIGHTER_ARCHIVE_BYTES));
#endif
        g_source_registered[slot]=1;
    }
    sat_asset_desc_t desc={0}; desc.logical_path=path; desc.source_path=source;
    desc.kind=SAT_ASSET_DATA; desc.size=FIGHTER_ARCHIVE_BYTES;
    sat_example_must(sat_asset_register(&desc,&asset->source));
    sat_asset_prefetch_t prefetch;
    sat_example_must(sat_asset_prefetch_submit(path,0,FIGHTER_ARCHIVE_BYTES,&prefetch));
    sat_asset_prefetch_state_t state=SAT_ASSET_PREFETCH_PENDING;
    while(state==SAT_ASSET_PREFETCH_PENDING) {
        sat_result_t result=SAT_OK;uint32_t cached=0;
        sat_example_must(sat_asset_prefetch_update());
        sat_example_must(sat_asset_prefetch_status(prefetch,&state,&result,&cached));
        sat_example_must(result);
    }
    sat_example_must(sat_asset_prefetch_cancel(prefetch));
    sat_example_must(sat_character_asset_load(path,asset->data,sizeof(asset->data),asset->clips,
        FIGHTER_CLIP_COUNT,asset->textures,FIGHTER_TEXTURE_COUNT,&asset->view));
    if(slot)asset->view.model.palettes_rgb555=fighter_red_palette;
    sat_example_must(sat_character_materials_upload(&asset->view.model,asset->owners,
        asset->native,asset->materials,FIGHTER_TEXTURE_COUNT));
    ++g_metrics.source_reloads;
}
static void release_assets(unsigned slot) {
    asset_slot_t* asset=&g_assets[slot];
    sat_example_must(sat_character_materials_release(asset->owners,FIGHTER_TEXTURE_COUNT));
    sat_character_asset_release(&asset->view);
    sat_example_must(sat_asset_close(asset->source));
}
static void bind_lod(actor_t* actor,unsigned level) {
    const sat_model_asset_t* model=kModels[level];
    sat_example_must(sat_mesh_init(&actor->mesh,actor->vertices,ACTOR_VERTEX_CAP,actor->indices,ACTOR_FACE_CAP));
    sat_example_must(sat_model_copy_to_mesh(model,&actor->mesh));
    __builtin_memcpy(actor->face_materials,model->face_texture_indices,model->face_count*sizeof(uint16_t));
    actor->instance=(sat_scene3d_instance_t){0}; actor->instance.mesh=&actor->mesh;
    actor->instance.materials=g_assets[actor->slot].materials;
    actor->instance.material_count=FIGHTER_TEXTURE_COUNT;
    actor->instance.face_materials=actor->face_materials; actor->instance.cull_backfaces=1;
    actor->instance.world=&actor->world; actor->instance.bounds=&actor->bounds;
    sat_example_must(sat_scene3d_instance_binding_init(&actor->binding,&actor->mesh,
        actor->instance.materials,FIGHTER_TEXTURE_COUNT,actor->face_materials));
    actor->lod=level;
    sat_example_must(sat_skin_validate(kSkins[level]));
    sat_example_must(sat_skin_validate_mesh(kSkins[level],&actor->mesh));
}
static void init_actors(void) {
    for(unsigned i=0;i<CHARACTER_DEMO_MAX_ACTORS;++i) {
        actor_t* a=&g_actors[i]; __builtin_memset(a,0,sizeof(*a)); a->slot=i%CHARACTER_DEMO_ASSET_SLOTS;
        sat_example_must(sat_skeleton_instance_init(&a->skeleton,&g_assets[a->slot].view.skeleton,
            a->nodes,FIGHTER_JOINT_COUNT,a->scratch,FIGHTER_JOINT_COUNT));
        sat_example_must(sat_skeleton_rest_pose(a->skeleton.skeleton,a->pose,FIGHTER_JOINT_COUNT));
        sat_example_must(sat_pose_fade_init(&a->fade,a->fade_pose,FIGHTER_JOINT_COUNT));
        sat_example_must(sat_skel_clip_player_init(&a->player,&g_assets[a->slot].clips[FIGHTER_CLIP_IDLE]));
        a->clip=FIGHTER_CLIP_IDLE; bind_lod(a,g_quality);
    }
}
static void init_background(void) {
    for(unsigned y=0;y<BACKGROUND_HEIGHT;++y)for(unsigned x=0;x<BACKGROUND_WIDTH;++x) {
        unsigned skyline=((x/8)*7)%24+18;
        g_background[y*BACKGROUND_WIDTH+x]=(y>skyline)?(x%8==3 && y%8==4?3:2):1;
    }
    g_background_palette[1]=SAT_RGB555(3,5,10); g_background_palette[2]=SAT_RGB555(4,7,12);
    g_background_palette[3]=SAT_RGB555(13,15,18);
    sat_vdp2_nbg0_config_t config={SAT_VDP2_CHAR_SIZE_1X1,SAT_VDP2_COLOR_MODE_256,BACKGROUND_MAP_PLANE,0,0};
    sat_example_must(sat_vdp2_nbg0_init(&config));
    sat_example_must(sat_vdp2_palette_upload(g_background_palette,PALETTE_ENTRIES,
        BACKGROUND_BANK*PALETTE_ENTRIES));
    sat_example_must(sat_vdp2_nbg0_upload_indexed8(g_background,BACKGROUND_WIDTH,BACKGROUND_HEIGHT,
        BACKGROUND_BANK,g_background_map));
    sat_example_must(sat_vdp2_nbg0_set_priority(1));
    sat_example_must(sat_vdp2_nbg0_set_enabled(1));
}
static void fit_camera(void) {
    sat_vec3_t min={-SAT_FX16_ONE,-SAT_FX16_ONE/8,-SAT_FX16_ONE};
    sat_vec3_t max={SAT_FX16_ONE,SAT_FX16_ONE*5/2,SAT_FX16_ONE};
    if(g_mode==CHARACTER_DEMO_STRESS) { min.x=-SAT_FX16_ONE*3;max.x=SAT_FX16_ONE*3; }
    sat_orbit_camera3d_fit_t fit={0}; fit.min_extent=SAT_FX16_ONE/16;
    fit.initial_distance_factor=SAT_FX16_ONE*3/2; fit.min_distance_factor=SAT_FX16_ONE/2;
    fit.max_distance_factor=SAT_FX16_ONE*4; fit.near_plane_factor=SAT_FX16_ONE/16;
    fit.near_plane_floor=1;fit.fov_y=SAT_FX16_ONE*45;
    fit.aspect=sat_fx16_div(SAT_FX16_ONE*SCREEN_WIDTH,SAT_FX16_ONE*SCREEN_HEIGHT);
    fit.far_z=SAT_FX16_ONE*32;fit.pitch_min_deg=-30;fit.pitch_max_deg=60;
    sat_example_must(sat_orbit_camera3d_fit_bounds(&g_orbit,&min,&max,&fit));
    g_orbit.pitch_deg=12;g_orbit.yaw_deg=g_mode==CHARACTER_DEMO_ARENA?10:0;
    sat_example_must(sat_orbit_camera3d_update(&g_orbit));
}
static void input(const sat_pad_state_t* pad) {
    if(g_mode==CHARACTER_DEMO_ARENA) {
        if(pad->pressed&SAT_PAD_START) { combat_reset(&g_combat);
            // At a frame boundary all skin jobs and the prior VDP1 list are done.
            for(unsigned s=0;s<CHARACTER_DEMO_ASSET_SLOTS;++s){release_assets(s);load_assets(s);}
            init_actors();
        }
        if(pad->pressed&SAT_PAD_Z)g_debug=!g_debug;
        return;
    }
    if(pad->pressed&SAT_PAD_START)g_paused=!g_paused;
    if(pad->pressed&SAT_PAD_Z)g_policy=(g_policy+1)%POLICY_COUNT;
    if(pad->pressed&SAT_PAD_C)g_shading=!g_shading;
    if(pad->pressed&SAT_PAD_Y)g_debug=!g_debug;
    if(g_mode==CHARACTER_DEMO_STRESS) {
        if(pad->pressed&SAT_PAD_A)g_stress_preset=(g_stress_preset+1)%STRESS_PRESET_COUNT;
        g_actor_count=kStressCounts[g_stress_preset];
        if(pad->pressed&SAT_PAD_B)g_quality=(g_quality+1)%LOD_COUNT;
        if(pad->pressed&SAT_PAD_X)g_complexity=!g_complexity;
    } else {
        if(pad->pressed&SAT_PAD_A)g_view_clip=(g_view_clip+1)%FIGHTER_CLIP_COUNT;
        if(pad->pressed&SAT_PAD_B)g_view_clip=(g_view_clip+FIGHTER_CLIP_COUNT-1)%FIGHTER_CLIP_COUNT;
        if(pad->pressed&SAT_PAD_X)g_hud=!g_hud;
        if(pad->pressed&SAT_PAD_L)g_speed=g_speed>SAT_FX16_ONE/4?g_speed-SAT_FX16_ONE/4:SAT_FX16_ONE/4;
        if(pad->pressed&SAT_PAD_R)g_speed=g_speed<SAT_FX16_ONE*2?g_speed+SAT_FX16_ONE/4:SAT_FX16_ONE*2;
    }
    sat_pad_state_t camera=*pad;
    camera.pressed&=(uint16_t)~SAT_PAD_B;
    if(g_mode==CHARACTER_DEMO_VIEWER) {
        camera.held&=(uint16_t)~(SAT_PAD_L|SAT_PAD_R);
        if((pad->held&SAT_PAD_UP) && (pad->held&SAT_PAD_X))camera.held|=SAT_PAD_R;
        if((pad->held&SAT_PAD_DOWN) && (pad->held&SAT_PAD_X))camera.held|=SAT_PAD_L;
        if(pad->held&SAT_PAD_X)camera.held&=(uint16_t)~(SAT_PAD_UP|SAT_PAD_DOWN);
    }
    // No extra toggle here: C is shading; automatic path belongs to stress.
    camera.pressed&=(uint16_t)~SAT_PAD_C;
    sat_example_must(sat_orbit_camera3d_apply_pad(&g_orbit,&camera,SAT_PAD_C));
}
static void simulate(uint16_t buttons) {
    g_tick_fraction+=SAT_FX16_ONE;
    const sat_fx16_t delta=(sat_fx16_t)(g_tick_fraction/SIMULATION_HZ);
    g_tick_fraction%=SIMULATION_HZ;
    if(g_mode==CHARACTER_DEMO_ARENA) {
        unsigned hits=g_combat.hits,blocks=g_combat.blocked;
        combat_tick(&g_combat,buttons);
        const sat_vec3_t p=g_combat.fighters[0].position,q=g_combat.fighters[1].position;
        const int64_t dx=(int64_t)q.x-p.x,dz=(int64_t)q.z-p.z;
        const int64_t diameter=COMBAT_BODY_DIAMETER_Q16;
        g_character_bodies.player_x=p.x;g_character_bodies.player_z=p.z;
        g_character_bodies.opponent_x=q.x;g_character_bodies.opponent_z=q.z;
        ++g_character_bodies.checked_ticks;
        if(dx*dx+dz*dz<diameter*diameter)++g_character_bodies.overlaps;
        g_metrics.combat_hits_total+=g_combat.hits-hits;
        g_metrics.combat_blocks_total+=g_combat.blocked-blocks;
    }
    for(unsigned i=0;i<g_actor_count;++i) {
        actor_t* a=&g_actors[i]; unsigned clip=g_view_clip;
        if(a->flash_ticks)--a->flash_ticks;
        if(g_mode==CHARACTER_DEMO_ARENA && a->seen_hit!=g_combat.fighters[i].hit_serial) {
            a->seen_hit=g_combat.fighters[i].hit_serial;a->flash_ticks=SIMULATION_HZ/8;
        }
        if(g_mode==CHARACTER_DEMO_ARENA)clip=(unsigned)g_combat.fighters[i].state;
        else if(g_mode==CHARACTER_DEMO_STRESS)clip=g_complexity?(i+g_metrics.simulation_ticks/SIMULATION_HZ)%FIGHTER_CLIP_COUNT:FIGHTER_CLIP_WALK;
        select_clip(a,clip);
        sat_example_must(sat_skel_clip_player_set_speed(&a->player,g_speed));
        sat_clip_event_t events[4];uint16_t count=0;sat_vec3_t root={0};
        sat_example_must(sat_skel_clip_player_advance(&a->player,delta,events,
            sizeof(events)/sizeof(events[0]),&count,&root));
        a->root_motion.x+=root.x;a->root_motion.z+=root.z;
        g_metrics.events+=count; g_metrics.root_travel+=(uint32_t)(root.z<0?-root.z:root.z);
        sat_pose_fade_advance(&a->fade,delta);
    }
    ++g_metrics.simulation_ticks;
}
static void draw_floor(void) {
    sat_scene3d_material_t mat={0};mat.kind=SAT_SCENE3D_RGB;mat.color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
    for(int z=-ARENA_TILES/2;z<=ARENA_TILES/2;++z)for(int x=-ARENA_TILES/2;x<=ARENA_TILES/2;++x) {
        sat_fx16_t left=(x*2-1)*SAT_FX16_ONE,front=(z*2-1)*SAT_FX16_ONE;
        sat_quad3_t quad={{{left,0,front},{left+2*SAT_FX16_ONE,0,front},
            {left+2*SAT_FX16_ONE,0,front+2*SAT_FX16_ONE},{left,0,front+2*SAT_FX16_ONE}}};
        mat.rgb555=(x+z)&1?SAT_RGB555(10,11,13):SAT_RGB555(12,13,15);
        sat_example_must(sat_scene_submit_quad(&g_scene,&quad,&mat,0));
    }
}
static unsigned choose_lod(void) {
    const unsigned stage=ARENA_TILES*ARENA_TILES+g_actor_count*SHADOW_SEGMENTS;
    const unsigned quota=(SCENE_CAPACITY-HUD_RESERVE-stage)/g_actor_count;
    for(unsigned level=g_quality;level<LOD_COUNT;++level)if(kModels[level]->face_count<=quota)return level;
    return LOD_COUNT;
}
static void evaluate_actors(unsigned level) {
    g_metrics.skeleton_ms=0;g_metrics.skin_ms=0;
    for(unsigned i=0;i<g_actor_count;++i) {
        actor_t* a=&g_actors[i];if(a->lod!=level)bind_lod(a,level);
        uint32_t start=sat_time_ms();
        sat_example_must(sat_skel_clip_player_sample(&a->player,a->pose,FIGHTER_JOINT_COUNT));
        sat_example_must(sat_pose_fade_apply(&a->fade,a->pose));
        sat_example_must(sat_skeleton_evaluate(&a->skeleton,a->pose,FIGHTER_JOINT_COUNT));
        sat_example_must(sat_skeleton_world_matrices(&a->skeleton,a->joints,FIGHTER_JOINT_COUNT));
        g_metrics.skeleton_ms+=sat_time_ms()-start;
    }
    uint32_t start=sat_time_ms();
    for(unsigned i=0;i<g_actor_count;++i) {
        actor_t* a=&g_actors[i]; a->job=(sat_skin_job_t){kSkins[level],a->joints,a->vertices,
            ACTOR_VERTEX_CAP,0,sat_skin_vertex_count(kSkins[level]),FIGHTER_JOINT_COUNT,0};
        sat_parallel_mode_t policy=(sat_parallel_mode_t)g_policy;
        if(policy==SAT_PARALLEL_AUTO && i%2)policy=SAT_PARALLEL_MASTER;
        sat_example_must(sat_skin_job_submit(&a->job,policy,&a->task));
        // Bound outstanding work to two jobs: Master evaluates the next while
        // Slave owns this output; no per-frame allocation or descriptor reuse.
        if(i%2 || i+1==g_actor_count) {
            unsigned first=i%2?i-1:i;
            for(unsigned j=first;j<=i;++j) {
                uint32_t bytes=0;
                sat_example_must(sat_parallel_wait(g_actors[j].task,PARALLEL_TIMEOUT_TICKS));
                sat_example_must(sat_parallel_result(g_actors[j].task,&bytes));
                sat_example_must(sat_parallel_release(g_actors[j].task));g_actors[j].task=0;
                g_actors[j].mesh.vertex_count=(uint16_t)sat_skin_vertex_count(kSkins[level]);
            }
        }
    }
    g_metrics.skin_ms=sat_time_ms()-start;
    // Compare one real parallel pose to the canonical serial range every mode
    // change. This verification is separately labelled and not benchmark work.
    if(g_metrics.skin_equivalence!=g_policy+1) {
        actor_t* a=&g_actors[0];sat_example_must(sat_skin_deform(kSkins[level],a->joints,
            FIGHTER_JOINT_COUNT,a->world_vertices,ACTOR_VERTEX_CAP));
        for(unsigned v=0;v<a->mesh.vertex_count;++v) {
            if(a->vertices[v].x!=a->world_vertices[v].x ||
               a->vertices[v].y!=a->world_vertices[v].y || a->vertices[v].z!=a->world_vertices[v].z) {
                ++g_metrics.errors;sat_example_must(SAT_ERR_VERIFY_FAILED);
            }
        }
        g_metrics.skin_equivalence=g_policy+1;
    }
}
static void place(actor_t* a,unsigned i) {
    if(g_mode==CHARACTER_DEMO_ARENA)a->position=g_combat.fighters[i].position;
    else if(g_mode==CHARACTER_DEMO_STRESS) {
        a->position.x=((int)(i%4)*2-3)*SAT_FX16_ONE;
        a->position.z=((int)(i/4)*2-1)*SAT_FX16_ONE;
    } else a->position=(sat_vec3_t){0,0,0};
    sat_example_must(sat_mat4_translate(&a->world,a->position.x,a->position.y,a->position.z));
    if(g_mode==CHARACTER_DEMO_ARENA) {
        sat_mat4_t rotation,combined;
        sat_example_must(sat_mat4_rotate_y(&rotation,g_combat.fighters[i].facing*SAT_FX16_ONE*90));
        sat_example_must(sat_mat4_multiply(&combined,&a->world,&rotation));a->world=combined;
    }
    sat_example_must(sat_skin_bounds(a->vertices,a->mesh.vertex_count,SAT_FX16_ONE/32,&a->bounds));
}
static void character_materials(actor_t* a) {
    /* Flash and Gouraud tables are mutable; only resident indexed materials
       satisfy the existing immutable binding contract. LOD/reload rebuild it. */
    a->instance.validated_binding=NULL;
    if(a->flash_ticks) {
        for(unsigned m=0;m<FIGHTER_TEXTURE_COUNT;++m) {
            a->shaded[m]=(sat_scene3d_material_t){0};a->shaded[m].kind=SAT_SCENE3D_RGB;
            a->shaded[m].rgb555=SAT_RGB555(31,29,22);a->shaded[m].color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
        }
        a->instance.materials=a->shaded;return;
    }
    if(g_shading) {
        a->instance.materials=g_assets[a->slot].materials;
        a->instance.validated_binding=&a->binding;return;
    }
    const sat_vec3_t light={-SAT_FX16_ONE/2,SAT_FX16_ONE,SAT_FX16_ONE};
    sat_example_must(sat_mesh_vertex_normals(&a->mesh,a->normals,ACTOR_VERTEX_CAP));
    sat_vec3_t unit_light; sat_vec3_normalize(&unit_light,&light);
    sat_example_must(sat_gouraud_lambert(a->normals,a->mesh.vertex_count,&unit_light,
        SAT_FX16_ONE*3/4,a->gouraud));
    for(unsigned m=0;m<FIGHTER_TEXTURE_COUNT;++m) {
        a->shaded[m]=(sat_scene3d_material_t){0};a->shaded[m].kind=SAT_SCENE3D_RGB;
        a->shaded[m].rgb555=g_assets[a->slot].materials[m].rgb555;
        a->shaded[m].color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;a->shaded[m].vertex_gouraud=a->gouraud;
    }
    a->instance.materials=a->shaded;
}
static void camera(void) {
    if(g_mode==CHARACTER_DEMO_ARENA) {
        sat_fx16_t target_x=(g_combat.fighters[0].position.x+g_combat.fighters[1].position.x)/2;
        sat_fx16_t target_z=(g_combat.fighters[0].position.z+g_combat.fighters[1].position.z)/2;
        sat_fx16_t separation=g_combat.fighters[0].position.x-g_combat.fighters[1].position.x;
        if(separation<0)separation=-separation;
        sat_fx16_t desired=ARENA_CAMERA_DISTANCE_Q16+separation/ARENA_CAMERA_SEPARATION_DIVISOR;
        g_orbit.target.x+=(target_x-g_orbit.target.x)/CAMERA_SMOOTHING_DIVISOR;
        g_orbit.target.z+=(target_z-g_orbit.target.z)/CAMERA_SMOOTHING_DIVISOR;
        g_orbit.distance+=(desired-g_orbit.distance)/CAMERA_SMOOTHING_DIVISOR;
        sat_example_must(sat_orbit_camera3d_update(&g_orbit));
    }
}
static void text(const char* message,int x,int y) {
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&g_font,message,x,y,8,0,0));
}
static void value(const char* label,uint32_t number,int x,int y) {
    char buffer[64];sat_example_must(sat_fmt_label_u32(label,number,buffer,sizeof(buffer),0));text(buffer,x,y);
}
static void rate(const char* label,uint32_t number,int x,int y) {
    char buffer[64];sat_example_must(sat_fmt_label_u32(label,number/RATE_DECIMAL_SCALE,buffer,sizeof(buffer),0));
    unsigned len=0;while(buffer[len])++len;
    buffer[len++]='.';buffer[len++]=(char)('0'+number%RATE_DECIMAL_SCALE);buffer[len]=0;text(buffer,x,y);
}
static void debug_skeleton(actor_t* a) {
    sat_vec3_t points[FIGHTER_JOINT_COUNT];sat_projected_vertex_t screen[FIGHTER_JOINT_COUNT];
    for(unsigned j=0;j<FIGHTER_JOINT_COUNT;++j) {
        sat_mat4_t matrix;sat_example_must(sat_skeleton_attachment(&a->skeleton,j,&a->world,NULL,&matrix));
        points[j]=(sat_vec3_t){matrix.m[3],matrix.m[7],matrix.m[11]};
    }
    sat_example_must(sat_project_vertices(&g_orbit.view_proj,points,FIGHTER_JOINT_COUNT,screen));
    for(unsigned j=0;j<FIGHTER_JOINT_COUNT;++j) {
        unsigned parent=a->skeleton.skeleton->joints[j].parent;if(parent==SAT_TRANSFORM3D_ROOT)continue;
        if(screen[j].w>g_orbit.near_z && screen[parent].w>g_orbit.near_z) {
            sat_point_t p={screen[j].x+SCREEN_WIDTH/2,screen[j].y+SCREEN_HEIGHT/2};
            sat_point_t q={screen[parent].x+SCREEN_WIDTH/2,screen[parent].y+SCREEN_HEIGHT/2};
            sat_example_must(sat_draw_line(p,q,sat_color_rgba(255,220,70,255)));
        }
    }
}
static void debug_collision(actor_t* a) {
    sat_vec3_t points[8];sat_projected_vertex_t screen[8];
    const sat_fx16_t radius=COMBAT_BODY_RADIUS_Q16, top=SAT_FX16_ONE*9/5;
    for(unsigned c=0;c<8;++c)points[c]=(sat_vec3_t){a->position.x+(c&1?radius:-radius),
        c&2?top:SAT_FX16_ONE/8,a->position.z+(c&4?radius:-radius)};
    sat_example_must(sat_project_vertices(&g_orbit.view_proj,points,8,screen));
    for(unsigned c=0;c<8;++c)for(unsigned bit=1;bit<8;bit*=2) {
        unsigned other=c^bit;if(other<c)continue;
        if(screen[c].w<=g_orbit.near_z || screen[other].w<=g_orbit.near_z)continue;
        sat_point_t p={screen[c].x+SCREEN_WIDTH/2,screen[c].y+SCREEN_HEIGHT/2};
        sat_point_t q={screen[other].x+SCREEN_WIDTH/2,screen[other].y+SCREEN_HEIGHT/2};
        sat_example_must(sat_draw_line(p,q,sat_color_rgba(100,245,130,255)));
    }
}
static void hud(void) {
    uint32_t now=sat_time_ms(),span=now-g_rate_mark;
    ++g_rate_frames;
    if(span>=MILLISECONDS_PER_SECOND) {
        g_metrics.loop_rate_x10=(g_rate_frames*MILLISECONDS_PER_SECOND*RATE_DECIMAL_SCALE+span/2)/span;
        g_metrics.simulation_rate_x10=((g_metrics.simulation_ticks-g_rate_sim)*MILLISECONDS_PER_SECOND*RATE_DECIMAL_SCALE+span/2)/span;
        g_rate_frames=0;g_rate_mark=now;g_rate_sim=g_metrics.simulation_ticks;
    }
    if(g_debug)for(unsigned i=0;i<g_actor_count;++i){debug_skeleton(&g_actors[i]);if(g_mode==CHARACTER_DEMO_ARENA)debug_collision(&g_actors[i]);}
    if(g_mode==CHARACTER_DEMO_ARENA) {
        text("VECTOR ARENA",112,8);
        sat_rect_t left={8,24,(uint16_t)(g_combat.fighters[0].health*120/COMBAT_HEALTH),8};
        sat_rect_t right={192,24,(uint16_t)(g_combat.fighters[1].health*120/COMBAT_HEALTH),8};
        if(left.width)sat_example_must(sat_fill_rect(&left,sat_color_rgba(60,170,245,255)));
        if(right.width)sat_example_must(sat_fill_rect(&right,sat_color_rgba(235,80,65,255)));
        value("",g_combat.round_ticks/SIMULATION_HZ,152,24);
        rate("SUB/S ",g_metrics.loop_rate_x10,8,40);
        text(g_combat.round_over?"ROUND OVER / START TO RESTART":"D-PAD MOVE  A PUNCH  C KICK",8,192);
        text("B GUARD  X JUMP  Z BONES  START RESET",8,208);
        if(g_debug){value("HITS ",g_combat.hits,8,56);value("BLOCK ",g_combat.blocked,8,72);}
        return;
    }
    text(g_mode==CHARACTER_DEMO_STRESS?"SKINNING / STOCK RAM BENCHMARK":"SKELETAL / ORIGINAL HUMANOID",8,8);
    rate("SUB/S ",g_metrics.loop_rate_x10,8,24);rate("SIM/S ",g_metrics.simulation_rate_x10,176,24);
    if(g_hud) {
        text(sat_character_clip_name(&g_assets[0].view,g_actors[0].clip),8,40);
        text(kPolicyNames[g_policy],176,40);text(kLodNames[g_quality],256,40);
        value("BONES ",FIGHTER_JOINT_COUNT,8,56);value("VERTS ",g_metrics.vertices,176,56);
        value("FACES ",g_metrics.visible_faces,8,72);value("CMDS ",g_metrics.commands,176,72);
        value("SKIN MS ",g_metrics.skin_ms,8,88);value("PREP MS ",g_metrics.geometry_ms,176,88);
        value("FRAME ",g_metrics.animation_frame,8,104);value("OWN RAM KB ",g_metrics.persistent_bytes/KILOBYTE_BYTES,176,104);
        if(g_mode==CHARACTER_DEMO_STRESS) {
            value("ACTORS ",g_actor_count,8,120);value("DOWNGRADED ",g_metrics.downgraded,176,120);
            value("SORT US ",g_metrics.sort_us,8,136);value("EMIT US ",g_metrics.emit_us,176,136);
            value("MAX MS ",g_metrics.max_frame_ms,8,152);value("REJECTED ",g_metrics.rejected,176,152);
        }
    }
    text(g_paused?"PAUSED":"RUNNING",256,176);
    text(g_mode==CHARACTER_DEMO_STRESS?"A COUNT B LOD X COMPLEX Z CPU":"A/B CLIP L/R SPEED X+UP/DOWN ZOOM",8,192);
    text("D-PAD CAMERA  C LIGHT  Y BONES  START",8,208);
}
int character_demo_run(character_demo_mode_t mode) {
    g_mode=mode;g_actor_count=mode==CHARACTER_DEMO_ARENA?2:1;
    sat_video_config_t video={SCREEN_WIDTH,SCREEN_HEIGHT,SAT_VIDEO_AUTO,0};sat_example_must(sat_init(&video));
    sat_parallel_config_t parallel={SAT_PARALLEL_AUTO,NULL,0,0,PARALLEL_TIMEOUT_TICKS};
    sat_example_must(sat_parallel_init(&parallel));sat_example_must(sat_skin_parallel_register());
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&g_font,SAT_COLOR_WHITE,SAT_COLOR_BLACK,FONT_BANK));
    text("LOADING ORIGINAL CHARACTERS",8,8);
#if CHARACTER_DEMO_DISC_ASSETS
    sat_example_must(sat_cd_mount_default(&g_cd_block,NULL,NULL,&g_cd_device,0,&g_cd_volume));
    const sat_cdfs_source_desc_t sources[2]={{"high_blue.ska","disc/blue.ska",FIGHTER_ARCHIVE_BYTES},
        {"high_red.ska","disc/red.ska",FIGHTER_ARCHIVE_BYTES}};
    sat_example_must(sat_cdfs_register_source_manifest(&g_cd_volume,sources,
        CHARACTER_DEMO_ASSET_SLOTS,g_cd_sources));
#endif
    for(unsigned slot=0;slot<CHARACTER_DEMO_ASSET_SLOTS;++slot)load_assets(slot);
    init_actors();init_background();fit_camera();combat_reset(&g_combat);
    sat_example_must(sat_scene_init(&g_scene,g_faces,g_keys,g_order,SCENE_CAPACITY));
    g_metrics.magic=DEMO_SIGNATURE;
    g_character_benchmark.magic=BENCHMARK_SAMPLE_MAGIC;
    g_character_benchmark.capacity=BENCHMARK_SAMPLE_CAPACITY;
    g_character_benchmark.row_bytes=sizeof(benchmark_sample_t);
    g_metrics.persistent_bytes=sizeof(g_actors)+sizeof(g_assets)+sizeof(g_faces)+sizeof(g_keys)+sizeof(g_order);
    for(unsigned t=0;t<FIGHTER_TEXTURE_COUNT;++t)
        g_metrics.texture_bytes+=g_assets[0].textures[t].pixel_count*CHARACTER_DEMO_ASSET_SLOTS;
    sat_resource_plan_t resources; sat_resource_plan_entry_t entries[3];
    sat_example_must(sat_resource_plan_init(&resources,entries,sizeof(entries)/sizeof(entries[0])));
    sat_example_must(sat_resource_plan_set_limit(&resources,SAT_RESOURCE_MAIN_RAM,CHARACTER_RAM_BUDGET_BYTES));
    sat_example_must(sat_resource_plan_set_limit(&resources,SAT_RESOURCE_VDP1_VRAM,CHARACTER_TEXTURE_BUDGET_BYTES));
    sat_example_must(sat_resource_plan_set_limit(&resources,SAT_RESOURCE_CRAM,
        CHARACTER_DEMO_ASSET_SLOTS*PALETTE_ENTRIES*sizeof(uint16_t)));
    sat_example_must(sat_resource_plan_add(&resources,SAT_RESOURCE_MAIN_RAM,g_metrics.persistent_bytes,
        sizeof(uint32_t),1));
    sat_example_must(sat_resource_plan_add(&resources,SAT_RESOURCE_VDP1_VRAM,g_metrics.texture_bytes,
        sizeof(uint64_t),1));
    sat_example_must(sat_resource_plan_add(&resources,SAT_RESOURCE_CRAM,
        CHARACTER_DEMO_ASSET_SLOTS*PALETTE_ENTRIES*sizeof(uint16_t),sizeof(uint16_t),1));
    sat_example_must(sat_resource_plan_finalize(&resources));
    g_last_ms=sat_time_ms();g_rate_mark=g_last_ms;
    while(1) {
        uint32_t frame_start=sat_time_ms();
        sat_example_must(sat_wait_vblank());sat_example_must(sat_begin_frame());
        uint32_t cpu_start=sat_time_ms();
        sat_pad_state_t pad={0};sat_example_must(sat_pad_poll(&pad));input(&pad);
        uint32_t now=sat_time_ms(),elapsed=now-g_last_ms;g_last_ms=now;
        unsigned steps=0;
        if(!g_paused) {
            g_accumulator+=(uint64_t)elapsed*SIMULATION_HZ;
            while(g_accumulator>=MILLISECONDS_PER_SECOND) {
                simulate(pad.held);
                g_accumulator-=MILLISECONDS_PER_SECOND;++steps;
            }
        }
        g_metrics.steps_last=steps;g_metrics.requested=g_actor_count;
        g_metrics.policy=g_policy;g_metrics.quality=g_quality;
        unsigned level=choose_lod();
        g_metrics.accepted=level<LOD_COUNT?g_actor_count:0;
        g_metrics.rejected=level<LOD_COUNT?0:g_actor_count;
        g_metrics.downgraded=level>g_quality && level<LOD_COUNT?g_actor_count:0;
        if(level<LOD_COUNT)evaluate_actors(level);
        camera();sat_camera3d_t camera_state={0};camera_state.eye=g_orbit.eye;camera_state.target=g_orbit.target;
        camera_state.up=(sat_vec3_t){0,SAT_FX16_ONE,0};camera_state.view_proj=g_orbit.view_proj;
        sat_example_must(sat_scene_begin(&g_scene,&camera_state,g_orbit.near_z,SCREEN_WIDTH,SCREEN_HEIGHT,HUD_RESERVE));
        draw_floor();g_metrics.geometry_ms=0;g_metrics.vertices=0;
        for(unsigned i=0;i<g_metrics.accepted;++i) {
            actor_t* a=&g_actors[i];place(a,i);character_materials(a);
            sat_example_must(sat_character_shadow(&g_scene,&a->position,SAT_FX16_ONE/128,
                SAT_FX16_ONE/3,SAT_FX16_ONE/4,SAT_RGB555(3,4,6),1,SHADOW_SEGMENTS));
            a->instance.pass=2;
            uint32_t stamp=sat_time_ms();
            sat_example_must(sat_scene_submit_instance(&g_scene,&a->instance,SAT_SCENE3D_SLOT_INHERIT,
                a->projected,a->world_vertices));
            g_metrics.geometry_ms+=sat_time_ms()-stamp;g_metrics.vertices+=a->mesh.vertex_count;
        }
        sat_result_t flushed=sat_scene_flush(&g_scene);
        if(flushed==SAT_ERR_CAPACITY) {
            g_metrics.rejected=g_metrics.accepted;
            g_metrics.accepted=0;
        } else sat_example_must(flushed);
        sat_scene_stats_t scene={0};sat_example_must(sat_scene_stats(&g_scene,&scene));
        g_metrics.visible_faces=scene.flushed_faces;g_metrics.commands=scene.commands_used;
        g_metrics.command_capacity=scene.commands_capacity;
        sat_scene3d_profile_t profile={0};
        if(sat_scene3d_profile_get(&profile)==SAT_OK){g_metrics.sort_us=ticks_us(profile.painter_ticks);g_metrics.emit_us=ticks_us(profile.emission_ticks);}
        sat_parallel_stats_t tasks={0};sat_example_must(sat_parallel_stats(&tasks));
        g_metrics.slave_tasks=tasks.slave_tasks;g_metrics.master_tasks=tasks.master_tasks;
        g_metrics.wait_ticks=tasks.master_wait_ticks;g_metrics.queue_ticks=tasks.submission_ticks+tasks.completion_ticks;
        g_metrics.clip=g_actors[0].clip;
        const sat_skeleton_clip_t* clip=g_actors[0].player.clip;
        g_metrics.animation_frame=(uint32_t)(((uint64_t)g_actors[0].player.time*clip->rate_num)/(SAT_FX16_ONE*clip->rate_den));
        g_metrics.joints=FIGHTER_JOINT_COUNT;g_metrics.player_health=g_combat.fighters[0].health;
        g_metrics.opponent_health=g_combat.fighters[1].health;g_metrics.hits=g_combat.hits;
        g_metrics.blocked=g_combat.blocked;g_metrics.knockdowns=g_combat.knockdowns;g_metrics.round_over=g_combat.round_over;
        hud();sat_vdp1_command_stats_t commands={0};sat_example_must(sat_vdp1_command_stats(&commands));
        g_metrics.commands=commands.used;
        uint32_t cpu_ms=sat_time_ms()-cpu_start;
        sat_example_must(sat_end_frame());g_metrics.frame_ms=sat_time_ms()-frame_start;
        if(g_metrics.frame_ms>g_metrics.max_frame_ms)g_metrics.max_frame_ms=g_metrics.frame_ms;
        ++g_metrics.frames;
        if(g_mode==CHARACTER_DEMO_STRESS && g_character_benchmark.count<BENCHMARK_SAMPLE_CAPACITY) {
            benchmark_sample_t row={g_metrics.frames,sat_irq_count(SAT_IRQ_VBLANK_IN),
                g_metrics.simulation_ticks,g_actor_count,g_metrics.accepted,level,g_policy,
                g_metrics.frame_ms,cpu_ms,g_metrics.skin_ms,g_metrics.geometry_ms,
                g_metrics.sort_us,g_metrics.emit_us,g_metrics.commands,
                g_metrics.slave_tasks,g_metrics.master_tasks,g_metrics.wait_ticks,g_metrics.queue_ticks,g_complexity};
            g_character_benchmark.rows[g_character_benchmark.count]=row;++g_character_benchmark.count;
        }
        g_character_demo=g_metrics;
    }
    return 0;
}
