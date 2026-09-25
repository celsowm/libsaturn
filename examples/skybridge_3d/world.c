/* Skybridge 3D: the scene painter pass: decks, seesaws, lifts, insets and shadow. */
#include "skybridge.h"

static uint8_t g_active_fade_slot=SAT_INDEXED_SOLID_OPAQUE;
static uint16_t g_active_pass=SB_PASS_WORLD;
/* One byte per deck, owned here and handed to sat_fade3d_slot, which uses it
 * to keep a deck on its current slot until the camera has actually crossed
 * the transition band. */
static uint8_t g_platform_fade[SB_PLATFORM_COUNT];
/* Eight quantized levels onto the eight VDP2 sprite colour-calculation slots,
 * anything nearer than FADE_START drawn as an ordinary opaque sprite, and a
 * two-unit anti-flicker band. The library owns all of that; see the game-only
 * rule left in platform_fade_slot. */
static const sat_fade3d_slots_t g_fade_slots={
    {FADE_START, FADE_END, 8u, SAT_FADE3D_CULL_AFTER_END, 0u},
    SB_F(2), 0u, 1u, 1u, 0u
};
/* One bounded shared painter owns projection and ordering of the visible
 * faces of platforms, player and pickups in the same render pass. */
sat_scene3d_face_t g_face_items[SCENE_FACE_CAP] __attribute__((section(".wram_l")));
/* Caller-owned ordering scratch: the painter sorts these indices, so a frame
 * never moves the face records themselves. */
uint32_t g_face_keys[SCENE_FACE_CAP] __attribute__((section(".wram_l")));
uint16_t g_face_order[SCENE_FACE_CAP] __attribute__((section(".wram_l")));
sat_scene_t g_scene;
uint8_t g_world_cmd_full=0u;
/* Budget telemetry for the debug overlay. A frame that runs out of face slots
 * or VDP1 commands drops geometry SILENTLY -- world_ok() treats that as an
 * optional decoration -- so without these counters a half-drawn character
 * looks like a clipping or animation bug. Captured before the flush resets
 * the painter, and before the HUD pass spends its own reserve. */
uint16_t g_dbg_faces, g_dbg_face_cap;
uint16_t g_dbg_cmds, g_dbg_cmd_cap;
uint16_t g_dbg_pig_faces;
uint8_t g_dbg_world_full;
sat_result_t g_dbg_scene_status=SAT_OK;

void world_reset_platform_fades(void) {
    for(uint8_t i=0u;i<SB_PLATFORM_COUNT;++i)
        g_platform_fade[i]=SAT_INDEXED_SOLID_OPAQUE;
}

/* Sprite Type 0's RGB-coded pixels have different VDP2 interpretation
 * from indexed pixels. With sprite color calc enabled, mixing RGB geometry
 * and indexed faded quads made the near player's RGB body and the solid
 * platform walls disappear over the RBG0 ocean on the target emulator.
 * Keep *every* world-facing material on a single indexed palette path.
 * Ordinary sprites remain opaque at priority 7; only distant objects
 * explicitly request the faded selector (priority 6, above sea priority 5).
 * The indexed top insets keep their original patterned texture. */
/* Every world submission shares one outcome policy, so it is stated once.
 * A full command list or face queue ends the frame's world pass quietly:
 * the decorations that would follow are optional, and a partially drawn
 * frame is not an error. An unsupported face is simply skipped. Anything
 * else is a genuine bug and must not be swallowed. Returns 0 once the
 * world pass is closed, so a caller can stop early. */
uint8_t world_ok(sat_result_t st) {
    if(st==SAT_ERR_CAPACITY) {g_world_cmd_full=1u;return 0u;}
    if(st!=SAT_OK && st!=SAT_ERR_UNSUPPORTED) {
        ++g_metrics.failures;
        g_dbg_scene_status=st;
        return 0u;
    }
    return 1u;
}
/* Skybridge only selects a level material; the shared painter owns the
 * camera projection, per-face ordering, clipping and VDP1 submission. */
/* `color` is an SB_C_* palette name, i.e. the pool index the startup
 * registration produced -- no lookup, no snapping. */
static void put_quad(const sat_quad3_t* q, uint8_t color) {
    if(g_world_cmd_full) return;
    sat_scene3d_material_t material=g_scene_materials[color];
    material.color_calc_slot=g_active_fade_slot;
    world_ok(sat_scene_submit_quad(
        &g_scene,q,&material,g_active_pass));
}
static void put_quad_lit(const sat_quad3_t* q, uint8_t c) {
    /* Gouraud RGB polygons are incompatible with this VDP2 blend path;
     * use an indexed flat-colored face so near geometry stays solid. */
    put_quad(q,c);
}
static void pquad(sat_quad3_t* q, int32_t ax,int32_t ay,int32_t az,
                  int32_t bx,int32_t by,int32_t bz,
                  int32_t cx,int32_t cy,int32_t cz,
                  int32_t dx,int32_t dy,int32_t dz) {
    q->v[0]=(sat_vec3_t){ax,ay,az};
    q->v[1]=(sat_vec3_t){bx,by,bz};
    q->v[2]=(sat_vec3_t){cx,cy,cz};
    q->v[3]=(sat_vec3_t){dx,dy,dz};
}
static void quad_rect_xz(sat_quad3_t* q,int32_t lx,int32_t rx,int32_t z0,int32_t z1,int32_t y) {
    pquad(q,lx,y,z0, rx,y,z0, rx,y,z1, lx,y,z1);
}
/* Rims, edge bands and the contact shadow are all one flat XZ rectangle
 * submitted right where it is built; building and submitting separately
 * only invites the two to drift apart. */
static void put_rect_xz(int32_t lx,int32_t rx,int32_t z0,int32_t z1,
                        int32_t y,uint8_t c) {
    sat_quad3_t q;
    quad_rect_xz(&q,lx,rx,z0,z1,y);
    put_quad(&q,c);
}
/* Draw just the top and camera-facing sides, always with consistent thickness. */
/* Level code describes a box, not its camera-facing vertices or winding.
 * The renderer owns side selection, safe near/screen clipping and materials. */
static void box3(int32_t x,int32_t y,int32_t z,int32_t hx,int32_t hy,int32_t hz,
                 uint8_t top,uint8_t xcolor,uint8_t zcolor,uint8_t trim) {
    if(g_world_cmd_full) return;
    sat_indexed_box3_t block={0};
    block.top_center=(sat_vec3_t){x,y,z};
    block.half_extents=(sat_vec3_t){hx,hy,hz};
    block.top_material=g_scene_materials[top].texture;
    block.x_material=g_scene_materials[xcolor].texture;
    block.z_material=g_scene_materials[zcolor].texture;
    if(!world_ok(sat_scene_submit_box(
            &g_scene,&block,g_active_fade_slot,g_active_pass))) return;
    /* Decorative inset remains level-owned, not a fake collision surface. */
    if(g_eye.y<=y || !trim || hx<=SB_F(4) || hz<=SB_F(4)) return;
    sat_quad3_t q;
    quad_rect_xz(&q,x-hx+SB_F(2),x+hx-SB_F(2),
                    z-hz+SB_F(2),z+hz-SB_F(2),y+SB_F(1)/32);
    if(trim==2u) {
        put_quad(&q,SB_C_AMBER_TRIM);
        return;
    }
    const uint8_t theme=trim==1u?0u:(trim==3u?1u:2u);
    world_ok(sat_scene_submit_tiled_quad(
        &g_scene,&q,&g_tile_regions[theme],g_active_fade_slot,
        g_active_pass));
}
/* The three deck zones -- start (0-3), middle (4-6) and finish (7-9) --
 * are what every platform tint keys off, so the split lives in one place
 * instead of being re-spelled at each colour decision. */
static uint8_t deck_tint(uint8_t i,uint8_t start,uint8_t mid,uint8_t end) {
    return i<4u?start:(i<7u?mid:end);
}
static uint8_t top_color(uint8_t i) {
    return deck_tint(i,SB_C_DECK_START_TOP,
                       SB_C_DECK_MID_TOP,
                       SB_C_DECK_END_TOP);
}
/* The library owns the facet painter and clipping; game logic only positions
 * an instance of the immutable octahedron mesh above its supporting deck. */
/* An actual hinged 3D deck: the two long ends use the SAME 16.16
 * surface-height function as the landing solver. It is a sloped quad,
 * not a flat box translated or rotated just for the camera. Only three
 * visible side faces and a small hinge are needed on Saturn hardware. */
static void draw_seesaw(uint8_t id,const sb_platform_t* p) {
    const int32_t x=sb_platform_x(&g_game,id),z=SB_F(p->z);
    const int32_t lx=x-SB_F(p->half_x),rx=x+SB_F(p->half_x);
    const int32_t bz=z-SB_F(p->half_z),fz=z+SB_F(p->half_z);
    const int32_t y0=sb_platform_surface_y(&g_game,id,x,bz);
    const int32_t y1=sb_platform_surface_y(&g_game,id,x,fz);
    const int32_t bottom=sb_platform_y(&g_game,id)-SB_F(5);
    const uint16_t top=(id&1u)?SB_C_SEESAW_TOP_ODD:
                                  SB_C_SEESAW_TOP_EVEN;
    const uint16_t side=(id&1u)?SB_C_SEESAW_SIDE_ODD:
                                   SB_C_SEESAW_SIDE_EVEN;
    sat_quad3_t q;
    if(g_eye.x>x) {
        pquad(&q,rx,y0,bz,rx,y1,fz,rx,bottom,fz,rx,bottom,bz);
    } else {
        pquad(&q,lx,y1,fz,lx,y0,bz,lx,bottom,bz,lx,bottom,fz);
    }
    put_quad(&q,side);
    if(g_eye.z>z)
        pquad(&q,rx,y1,fz,lx,y1,fz,lx,bottom,fz,rx,bottom,fz);
    else
        pquad(&q,lx,y0,bz,rx,y0,bz,rx,bottom,bz,lx,bottom,bz);
    put_quad(&q,side);
    if(g_eye.y>bottom) {
        pquad(&q,lx,y0,bz,rx,y0,bz,rx,y1,fz,lx,y1,fz);
        put_quad_lit(&q,top);
        /* The short crossbar remains on the hinge, identifying the pivot
         * while the opposite ends visibly rise and fall. */
        pquad(&q,lx,sb_platform_y(&g_game,id)+SB_F(1)/20,z-SB_F(1)/3,
              rx,sb_platform_y(&g_game,id)+SB_F(1)/20,z-SB_F(1)/3,
              rx,sb_platform_y(&g_game,id)+SB_F(1)/20,z+SB_F(1)/3,
              lx,sb_platform_y(&g_game,id)+SB_F(1)/20,z+SB_F(1)/3);
        put_quad(&q,SB_C_SEESAW_HINGE);
    }
    box3(x,bottom-SB_F(3),z,SB_F(2),SB_F(3),SB_F(2),
         SB_C_SEESAW_PIVOT_TOP,SB_C_STEEL_DARK,
         SB_C_SEESAW_PIVOT_FRONT,0u);
}
static void stage_box(uint8_t i) {
    const sb_platform_t* p=&sb_course_platforms(&g_game)[i];
    if(p->kind==SB_SEESAW) {
        draw_seesaw(i,p);
        return;
    }
    int32_t x=sb_platform_x(&g_game,i), z=SB_F(p->z);
    int32_t top_y=sb_platform_y(&g_game,i);
    uint16_t t=top_color(i);
    if(p->surface==SB_SURFACE_SLICK)t=SB_C_DECK_ICE_TOP;
    if(p->surface==SB_SURFACE_GRIP)t=SB_C_DECK_GRIP_TOP;
    if(p->kind==SB_LIFT)
        t=SB_C_MARKER_YELLOW;
    if (p->kind==SB_COLLAPSING && g_game.collapse_ticks>0u &&
        g_game.collapse_ticks<=30u)
        t=(g_game.ticks&4u)?SB_C_COLLAPSE_WARN_A:SB_C_COLLAPSE_WARN_B;
    {
        /* Course 3 has actual missing geometry, not a dark quad drawn over
         * an intact floor. Use the SAME shared solid slices as ground
         * collision, leaving the rectangular opening open to the ocean. */
        const sb_hole_t* hole=sb_platform_hole(&g_game,i);
        sb_deck_slice_t slabs[4];
        uint8_t pieces=sb_deck_slices(&g_game,i,slabs);
        uint16_t side=p->kind==SB_LIFT?SB_C_AMBER_TRIM:
            (i<4u?SB_C_STEEL_DARK:SB_C_TEAL_DEEP);
        uint16_t front=p->kind==SB_LIFT?SB_C_LIFT_FRONT:
            (i<4u?SB_C_STEEL_LIGHT:SB_C_TEAL_DARK);
        for(uint8_t part=0u;part<pieces;++part) {
            const sb_deck_slice_t* s=&slabs[part];
            int32_t half_x=(s->max_x-s->min_x)/2;
            int32_t half_z=(s->max_z-s->min_z)/2;
            box3(s->min_x+half_x,top_y,s->min_z+half_z,
                 half_x,SB_F(5),half_z,t,side,front,
                 hole?0u:(p->kind==SB_COLLAPSING?2u:
                     (p->kind==SB_LIFT?4u:(i<4u?1u:(i<7u?3u:4u)))));
        }
        if(hole && g_eye.y>top_y) {
            /* Bright narrow rim is drawn OUTSIDE the void. These four
             * strips never cover the aperture or create phantom flooring. */
            const int32_t hl=x+SB_F(hole->x_offset-hole->half_x);
            const int32_t hr=x+SB_F(hole->x_offset+hole->half_x);
            const int32_t hb=z+SB_F(hole->z_offset-hole->half_z);
            const int32_t hf=z+SB_F(hole->z_offset+hole->half_z);
            const int32_t y=top_y+SB_F(1)/24;
            const uint16_t warning=SB_C_HOLE_RIM;
            put_rect_xz(hl-SB_F(1),hl,hb,hf,y,warning);
            put_rect_xz(hr,hr+SB_F(1),hb,hf,y,warning);
            put_rect_xz(hl,hr,hb-SB_F(1),hb,y,warning);
            put_rect_xz(hl,hr,hf,hf+SB_F(1),y,warning);
        }
    }
    /* Distinct corner braces and inset deck rails make the floating decks
     * read as engineered 3D structures rather than untextured slabs.
     * All braces are outside the traversable deck and are decorative only. */
    if(i==0u || i==3u || i==4u || i==6u || i==9u) {
        int32_t rim_z=z+(g_eye.z<z?-SB_F(p->half_z-3):
                                      SB_F(p->half_z-3));
        uint16_t metal=deck_tint(i,SB_C_STEEL_LIGHT,
                                   SB_C_TEAL_DARK,
                                   SB_C_AMBER_TRIM);
        box3(x-SB_F(p->half_x-4),top_y-SB_F(6),rim_z,
             SB_F(2),SB_F(3),SB_F(2),metal,metal,metal,0u);
        box3(x+SB_F(p->half_x-4),top_y-SB_F(6),rim_z,
             SB_F(2),SB_F(3),SB_F(2),metal,metal,metal,0u);
    }
    /* A thin raised, contrasting edge band makes platform boundaries
     * legible at speed without adding collision-changing obstacles. */
    if(g_eye.y>top_y && p->half_x>7 && p->half_z>7) {
        int32_t sy=top_y+SB_F(1)/24;
        int32_t lx=x-SB_F(p->half_x-1),rx=x+SB_F(p->half_x-1);
        int32_t bz=z-SB_F(p->half_z-1),fz=z+SB_F(p->half_z-1);
        uint16_t edge_color=deck_tint(i,SB_C_AMBER_TRIM,
                                        SB_C_DECK_MID_TOP,
                                        SB_C_MARKER_YELLOW);
        put_rect_xz(lx,rx,bz,bz+SB_F(1),sy,edge_color);
        put_rect_xz(lx,rx,fz-SB_F(1),fz,sy,edge_color);
    }
    /* Elevators have a visible shaft below the deck: its length
     * changes with the actual collision top, never a separate animation. */
    if(p->kind==SB_LIFT) {
        box3(x,top_y-SB_F(9),z,SB_F(2),SB_F(4),SB_F(2),
             SB_C_AMBER_TRIM,SB_C_STEEL_DARK,
             SB_C_STEEL_LIGHT,0u);
    }
    /* Checkpoints and finish are physically marked, not just HUD text. */
    if (i==3u || i==6u) {
        box3(x-SB_F(7),top_y+SB_F(5),z+SB_F(5),SB_F(1),SB_F(5),SB_F(1),
             SB_C_MARKER_YELLOW,SB_C_CHECKPOINT_SIDE,SB_C_LIFT_FRONT,0u);
    }
    if (i==9u) {
        box3(x-SB_F(10),top_y+SB_F(9),z+SB_F(5),SB_F(1),SB_F(9),SB_F(1),
             SB_C_MARKER_YELLOW,SB_C_FINISH_POST_SIDE,SB_C_FINISH_POST_FRONT,0u);
        box3(x+SB_F(10),top_y+SB_F(9),z+SB_F(5),SB_F(1),SB_F(9),SB_F(1),
             SB_C_MARKER_YELLOW,SB_C_FINISH_POST_SIDE,SB_C_FINISH_POST_FRONT,0u);
        box3(x,top_y+SB_F(19),z+SB_F(5),SB_F(11),SB_F(1),SB_F(1),
             SB_C_MARKER_YELLOW,SB_C_FINISH_BAR_SIDE,SB_C_FINISH_BAR_FRONT,0u);
    }
}
/* Model only: the pig is contained inside the original 4x4x5 fixed-point
 * collision volume. Course 1/2 movement, coyote time, gem contact and
 * elevator carry remain owned by game.h. Its geometry uses indexed VDP1
 * distorted sprites through box3()/put_quad(), never RGB faces that break
 * the existing VDP2 distance color-calculation setup. */
/* Preserve the existing collision/contact shadow, not the old procedural
 * pig body. The visible pig now comes exclusively from the user-modified
 * CC BY 4.0 GLB converted by the stock importer at build time. */
static void pig_shadow(const uint8_t* deck_slot) {
    if(g_game.support>=0 &&
       deck_slot[(uint8_t)g_game.support]!=SAT_FADE3D_SLOT_CULLED) {
        const int32_t px=g_game.x,pz=g_game.z;
        const int32_t sy=sb_platform_surface_y(
            &g_game,(uint8_t)g_game.support,px,pz)+SB_F(1)/16;
        /* The shadow is a mark ON the deck, so it takes the deck's slot and
         * is skipped entirely when the deck itself was not drawn -- a shadow
         * floating over open sea reads as a bug. */
        const uint8_t previous=g_active_fade_slot;
        g_active_fade_slot=deck_slot[(uint8_t)g_game.support];
        put_rect_xz(px-SB_F(2),px+SB_F(2),
                    pz-SB_F(2),pz+SB_F(2),sy,SB_C_PIG_SHADOW);
        g_active_fade_slot=previous;
    }
}
/* The one fade rule that really is the GAME's: whatever the distance says,
 * the deck under the player's feet stays fully readable. Quantization, the
 * level-to-slot mapping and the anti-flicker band are the library's, and
 * g_platform_fade[id] is the per-deck byte sat_fade3d_slot keeps them in. */
static uint8_t platform_fade_slot(uint8_t id,int32_t depth) {
    uint8_t slot=SAT_INDEXED_SOLID_OPAQUE;
    if(g_game.support==(int8_t)id) {
        g_platform_fade[id]=SAT_INDEXED_SOLID_OPAQUE;
        return SAT_INDEXED_SOLID_OPAQUE;
    }
    if(sat_fade3d_slot(&g_fade_slots,depth,
                       &g_platform_fade[id],&slot)!=SAT_OK)
        return SAT_FADE3D_SLOT_CULLED;
    return slot;
}
void draw_world(void) {
    /* Each deck's chosen slot, doubling as the visibility flag: CULLED means
     * the deck was not drawn this frame. Anything sitting ON a deck -- its
     * gem, the player's contact shadow -- has to fade WITH it, so the slot
     * has to outlive the loop that picked it. */
    uint8_t deck_slot[SB_PLATFORM_COUNT];
    uint8_t i;
    for(i=0u;i<SB_PLATFORM_COUNT;++i)deck_slot[i]=SAT_FADE3D_SLOT_CULLED;
    g_dbg_scene_status=sat_scene_begin(
        &g_scene,&g_camera,SB_F(8),W,H,SB_HUD_COMMAND_RESERVE);
    if(g_dbg_scene_status!=SAT_OK) {
        sat_vdp1_command_stats_t stats={0};
        sat_vdp1_command_stats(&stats);
        put_text("SCENE BEGIN ERROR",48,84);
        put_label("ERR",(uint32_t)(-g_dbg_scene_status),48,100);
        put_label("CMD",stats.used,48,116);
        put_label("CAP",stats.capacity,48,132);
        put_label("RES",stats.overlay_reserved,48,148);
        return;
    }
    g_active_pass=SB_PASS_WORLD;

    /* All visible faces share render pass zero, independent of which object
     * contributed them. Gameplay visibility/fade still belongs to the game. */
    for(i=0u;i<SB_PLATFORM_COUNT;++i) {
        const sb_platform_t* p=&sb_course_platforms(&g_game)[i];
        sat_vec3_t center;
        sat_fx16_t depth;
        uint8_t slot;
        if(!sb_platform_active(&g_game,i))continue;
        center=(sat_vec3_t){
            sb_platform_x(&g_game,i),
            sb_platform_surface_y(&g_game,i,sb_platform_x(&g_game,i),SB_F(p->z)),
            SB_F(p->z)
        };
        /* Camera-penetration guard for an already-passed platform. At the
         * reported C1 position X~7 Z~30, the chase camera (42 units behind
         * the pig) sits inside the previous pier's XZ footprint. Drawing
         * the pier right around the camera adds several nearly full-screen
         * VDP1 raster commands, even after geometric clipping. That pier
         * is NOT the supporting deck; omit it while the eye is inside its
         * footprint, without touching world/collision/collectibles.
         * Apply to ALL courses and decks; no hardcoded stage index. */
        if(g_game.support!=(int8_t)i &&
           g_eye.y>center.y+SB_F(6) &&
           sb_abs(g_eye.x-center.x)<SB_F(p->half_x) &&
           sb_abs(g_eye.z-center.z)<SB_F(p->half_z))
            continue;
        sat_example_must(sat_scene_depth(&g_scene,&center,&depth));
        if(g_game.support!=(int8_t)i &&
           (depth < -SB_F(9) ||
            sb_abs(center.x-g_game.x)>SB_F(160) ||
            sb_abs(center.z-g_game.z)>SB_F(180)))continue;
        slot=platform_fade_slot(i,depth);
        if(slot==SAT_FADE3D_SLOT_CULLED)continue;
        g_active_fade_slot=slot;
        g_active_pass=(g_game.support==(int8_t)i)?SB_PASS_SUPPORT:SB_PASS_WORLD;
        stage_box(i);
        deck_slot[i]=slot;
    }
    g_active_fade_slot=SAT_INDEXED_SOLID_OPAQUE;
    g_active_pass=SB_PASS_ACTOR;
    /* Gema instances are independent of the pig pose. Dispatch their
     * suffix before the Master finishes the actor preparation. */
    prepare_gem_geometry(deck_slot);
#if SAT_PARALLEL_TEST_FAULT == SAT_PARALLEL_TEST_FAULT_TIMEOUT_ABORT
    /* Keep the injected timeout adjacent to submission, while its real Slave
     * worker is known to be active. Production and other validation profiles
     * retain the normal frame schedule below. */
    finish_gem_geometry();
#endif
    /* The pig must advance on every gameplay frame. If the Slave owns the gem
     * suffix, decode this frame's immutable animation snapshot on the Master;
     * otherwise the Slave may decode it asynchronously. */
    if(g_gem_slave_pending) prepare_pig_animation_master();
    else submit_pig_animation();
    finish_pig_animation();
    pig_shadow(deck_slot);
    player_pig();
    finish_gem_geometry();
    /* World, pig and gem faces are ordered together before the protected HUD. */
    g_dbg_faces=g_scene.faces.count;
    g_dbg_face_cap=g_scene.faces.capacity;
    if(sat_scene_flush(&g_scene)!=SAT_OK) {
        ++g_metrics.failures;
        g_dbg_scene_status=SAT_ERR_VERIFY_FAILED;
    }
    {
        /* AFTER the flush: that is where the queued faces actually become
         * VDP1 commands, so sampling before it always reported an empty list
         * and hid exactly the exhaustion this row exists to show. Still before
         * the HUD pass, which spends its own reserved quota. */
        sat_vdp1_command_stats_t cmd={0};
        if(sat_vdp1_command_stats(&cmd)==SAT_OK) {
            g_dbg_cmds=cmd.used;
            g_dbg_cmd_cap=(uint16_t)(cmd.capacity-cmd.overlay_reserved);
        }
        g_dbg_world_full=g_world_cmd_full;
    }
}
