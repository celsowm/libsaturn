/* Skybridge 3D: a playable stock-Saturn platformer, VDP1 world + VDP2 sea/sky. */
#include <stdint.h>
#include "saturn/saturn.h"
#include "saturn/asset.h"
#include "saturn/scene3d.h"
#include "saturn/fade3d.h"
#include "saturn/vdp1_color_calc.h"
#include "saturn/vdp2_color_calc.h"
#include "saturn/example_util.h"
#include "examples/vdp2_rbg0_ground/rbg0_math.h"
#include "game.h"
#include "scenery.h"

#define W 320u
#define H 224u
#define HORIZON 96u
#define SEA_WORD 0x00000u
#define ROT_WORD 0x10000u
#define COEF_WORD 0x12000u
#define SKY_W SB_SKY_W
#define SKY_H SB_SKY_H
#define FADE_START SB_FADE_START
#define FADE_END SB_FADE_END
#define VIEW_LIMIT FADE_END
#define FADE_COLOR_COUNT 36u
#define FADE_PALETTE_BANK 4u
#define FADE_OPAQUE 255u
#define FADE_CULLED 254u
#define FADE_HYSTERESIS SB_F(2)
#define SOUNDS 6u
#define SOUND_LEN 2048u
#define MUSIC_LEN 32768u
#define SCENE_OBJECT_CAP (SB_PLATFORM_COUNT + SB_PICKUP_COUNT + 1u)
#define SCENE_PASS_WORLD 0u
#define SCENE_PASS_SUPPORT 1u
#define SCENE_PASS_ACTOR 2u

static sb_game_t g_game;
static sat_ascii_font_t g_font;
static sat_vdp1_texture_t g_tile_textures[3];
static sat_vdp1_texture_t g_cloud_texture;
static uint8_t g_cloud_pixels[SB_CLOUD_W * SB_CLOUD_H];
static sat_vdp1_texture_t g_fade_textures[FADE_COLOR_COUNT];
static uint8_t g_fade_solid_pixels[16u*16u];
static uint8_t g_active_fade_slot=FADE_OPAQUE;
static uint8_t g_platform_fade[SB_PLATFORM_COUNT];
static const sat_fade3d_t g_fade_policy={
    FADE_START, FADE_END, 8u, SAT_FADE3D_CULL_AFTER_END, 0u
};
static const uint16_t g_fade_colors[FADE_COLOR_COUNT]={
    SAT_RGB555(24,23,16), SAT_RGB555(12,26,19), SAT_RGB555(27,23,16),
    SAT_RGB555(31,8,5), SAT_RGB555(31,26,5),
    SAT_RGB555(12,14,14), SAT_RGB555(6,16,15),
    SAT_RGB555(17,17,14), SAT_RGB555(8,19,18),
    SAT_RGB555(24,20,10),
    SAT_RGB555(31,26,3), SAT_RGB555(23,16,3), SAT_RGB555(31,19,2),
    SAT_RGB555(16,12,3), SAT_RGB555(27,20,4),
    SAT_RGB555(20,14,3), SAT_RGB555(27,19,4),
    SAT_RGB555(31,30,8), SAT_RGB555(31,13,3), SAT_RGB555(31,23,4),
    SAT_RGB555(31,28,7), SAT_RGB555(31,20,6), SAT_RGB555(31,12,4),
    SAT_RGB555(31,31,31), SAT_RGB555(8,10,10),
    SAT_RGB555(29,14,29), SAT_RGB555(19,5,21), SAT_RGB555(26,9,27),
    SAT_RGB555(31,20,25), SAT_RGB555(29,15,22), SAT_RGB555(24,10,17),
    SAT_RGB555(31,23,27), SAT_RGB555(31,25,27), SAT_RGB555(26,12,19),
    SAT_RGB555(17,7,12), SAT_RGB555(3,2,4)
};
#define PIG_TOP SAT_RGB555(31,20,25)
#define PIG_SIDE SAT_RGB555(29,15,22)
#define PIG_DARK SAT_RGB555(24,10,17)
#define PIG_HEAD SAT_RGB555(31,23,27)
#define PIG_SNOUT SAT_RGB555(31,25,27)
#define PIG_EAR SAT_RGB555(26,12,19)
#define PIG_HOOF SAT_RGB555(17,7,12)
#define PIG_BLACK SAT_RGB555(3,2,4)
static uint8_t g_tile_pixels[16u*16u];
static uint32_t g_last_ocean_palette_step=0xFFFFFFFFu;
static uint8_t g_sky[SKY_W * SKY_H];
static uint16_t g_sky_colors[256], g_sea_colors[256];
static uint16_t g_map[SAT_VDP2_NBG0_MAP_CELLS];
static sat_mat4_t g_vp;
static sat_vec3_t g_eye, g_target, g_camera_anchor;
/* A bounded, caller-owned scene queue: the library sorts and executes it.
 * No world-painter/actor-painter arrays or per-game depth math remain. */
static sat_scene3d_queue_item_t g_scene_items[SCENE_OBJECT_CAP];
static sat_scene3d_queue_t g_scene;
static uint8_t g_stage_ids[SB_PLATFORM_COUNT];
static uint8_t g_stage_frame_slot[SB_PLATFORM_COUNT];
static int16_t g_yaw;
static uint32_t g_prev_frame, g_frame;
static sat_sound_t g_sounds[SOUNDS];
static sat_voice_t g_music_voice;
static const char* const g_sfx_paths[SOUNDS-1u]={
    "skybridge/sfx/jump", "skybridge/sfx/land", "skybridge/sfx/pickup",
    "skybridge/sfx/fall", "skybridge/sfx/finish"
};
static int8_t g_audio[SOUNDS - 1u][SOUND_LEN];
static int8_t g_music[MUSIC_LEN];
static uint8_t g_audio_ready;
static uint8_t g_show_help=1u;
static uint8_t g_show_debug=0u;
static const rbg0_ground_config_t g_ocean = {
    512u, 256u, 160u, HORIZON, 96u, 8u, 96u, COEF_WORD
};

/* Boot is a series of completed jobs, not a pretend timed progress bar.
 * VDP1 draws this fullscreen panel independently of whether VDP2 is ready. */
static void loading_frame(const char* stage, uint8_t percent) {
    char progress[28];
    uint16_t width=(uint16_t)((uint32_t)percent*252u/100u);
    sat_example_must(sat_wait_vblank());
    sat_example_must(sat_begin_frame());
    sat_example_must(sat_draw_rect_screen(0,0,W,H,SAT_RGB555(2,7,14)));
    sat_example_must(sat_draw_rect_screen(34,64,252u,3u,SAT_RGB555(12,26,27)));
    sat_example_must(sat_ascii_font_draw_text_screen_centered_indexed8(
        &g_font,"SKYBRIDGE 3D",160,77,8,0u,0u));
    sat_example_must(sat_ascii_font_draw_text_screen_centered_indexed8(
        &g_font,stage,160,110,8,0u,0u));
    sat_example_must(sat_draw_rect_screen(34,137,252u,9u,SAT_RGB555(6,13,17)));
    if(width>0u) sat_example_must(sat_draw_rect_screen(
        34,137,width,9u,SAT_RGB555(20,29,19)));
    sat_example_must(sat_fmt_label_u32("LOADING ",percent,progress,sizeof(progress),0));
    sat_example_must(sat_ascii_font_draw_text_screen_centered_indexed8(
        &g_font,progress,160,153,8,0u,0u));
    sat_example_must(sat_end_frame());
}
static void put_text(const char* text, int x, int y) {
    (void)sat_ascii_font_draw_text_screen_indexed8(
        &g_font, text, x, y, 8, 0u, 0u);
}
static void label(const char* title, uint32_t n, int x, int y) {
    char buf[32];
    if (sat_fmt_label_u32(title, n, buf, sizeof(buf), 0) == SAT_OK) put_text(buf,x,y);
}
/* Display signed 16.16 WORLD coordinates, to one decimal place.
 * The small formatter avoids printf/libc and preserves -0.x on a fall. */
static void hud_coord(char axis,int32_t fixed,int x,int y) {
    char digits[SAT_FMT_U32_MAX];
    char out[18];
    uint16_t len=0u,i=0u;
    int64_t magnitude=fixed<0?-(int64_t)fixed:(int64_t)fixed;
    uint32_t whole=(uint32_t)(magnitude>>16u);
    uint32_t tenth=(uint32_t)(((magnitude&0xFFFFLL)*10u)>>16u);
    if(sat_fmt_u32(whole,digits,sizeof(digits),&len)!=SAT_OK)return;
    out[i++]=axis;
    out[i++]=' ';
    if(fixed<0)out[i++]='-';
    {uint16_t j;for(j=0u;j<len;++j)out[i++]=digits[j];}
    out[i++]='.';
    out[i++]=(char)('0'+tenth);
    out[i]=0;
    put_text(out,x,y);
}
/* Sprite Type 0's RGB-coded pixels have different VDP2 interpretation
 * from indexed pixels. With sprite color calc enabled, mixing RGB geometry
 * and indexed faded quads made the near player's RGB body and the solid
 * platform walls disappear over the RBG0 ocean on the target emulator.
 * Keep *every* world-facing material on a single indexed palette path.
 * Ordinary sprites remain opaque at priority 7; only distant objects
 * explicitly request the faded selector (priority 6, above sea priority 5).
 * The indexed top insets keep their original patterned texture. */
static uint8_t fade_material(uint16_t c) {
    uint8_t i, chosen=0u;
    uint32_t best=0xFFFFFFFFu;
    for (i=0u;i<FADE_COLOR_COUNT;++i) {
        uint16_t v=g_fade_colors[i];
        int32_t dr=(int32_t)(c&31u)-(int32_t)(v&31u);
        int32_t dg=(int32_t)((c>>5u)&31u)-(int32_t)((v>>5u)&31u);
        int32_t db=(int32_t)((c>>10u)&31u)-(int32_t)((v>>10u)&31u);
        uint32_t distance=(uint32_t)(dr*dr+dg*dg+db*db);
        if(distance<best) {best=distance;chosen=i;if(!distance)break;}
    }
    return chosen;
}
/* Large decks can cross the view plane when the follow camera overtakes
 * the previous platform. Projecting one near-zero-depth corner into a giant
 * distorted sprite can starve VDP1, so clip WORLD geometry before projection
 * rather than dropping the entire quad on the next frame. Solid material
 * triangles can reuse the same tiny uniform indexed texture. */
static void put_quad(const sat_quad3_t* q, uint16_t c) {
    sat_quad3_t pieces[4];
    uint8_t count=0u;
    sat_example_must(sat_clip_quad_near(
        q,&g_eye,&g_scene.forward,SB_F(8),pieces,&count));
    for(uint8_t piece=0u;piece<count;++piece) {
        sat_quad2_t screen,visible[6];
        uint8_t visible_count=0u;
        sat_result_t st=sat_project_quad(&g_vp,&pieces[piece],&screen);
        if(st==SAT_ERR_UNSUPPORTED)continue;
        sat_example_must(st);
        st=sat_clip_quad_screen(&screen,W,H,visible,&visible_count);
        if(st==SAT_ERR_UNSUPPORTED)continue;
        sat_example_must(st);
        /* VDP1 processes each distorted-sprite command even outside the
         * display. Clip the giant near-plane polygons BEFORE submitting:
         * an earlier platform must not consume the entire frame and starve
         * the pig, other decks and especially the HUD appended at the end. */
        for(uint8_t tri=0u;tri<visible_count;++tri) {
            sat_distorted_sprite_cmd_t cmd={0};
            for(uint8_t k=0u;k<4u;++k) {
                cmd.x[k]=visible[tri].x[k];
                cmd.y[k]=visible[tri].y[k];
            }
            cmd.texture=&g_fade_textures[fade_material(c)];
            if(g_active_fade_slot==FADE_OPAQUE)
                st=sat_draw_sprite_distorted(&cmd);
            else
                st=sat_draw_sprite_distorted_color_calc(
                    &cmd,g_active_fade_slot);
            if(st!=SAT_OK && st!=SAT_ERR_UNSUPPORTED) sat_example_must(st);
        }
    }
}
static void put_quad_lit(const sat_quad3_t* q, uint16_t c) {
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
/* Draw just the top and camera-facing sides, always with consistent thickness. */
static void box3(int32_t x,int32_t y,int32_t z,int32_t hx,int32_t hy,int32_t hz,
                 uint16_t top,uint16_t xcolor,uint16_t zcolor,uint8_t trim) {
    sat_quad3_t q;
    int32_t lx=x-hx,rx=x+hx,by=y-hy,bz=z-hz,fz=z+hz;
    if (g_eye.x>x) {
        pquad(&q,rx,y,bz,rx,y,fz,rx,by,fz,rx,by,bz);
    } else {
        pquad(&q,lx,y,fz,lx,y,bz,lx,by,bz,lx,by,fz);
    }
    put_quad(&q,xcolor);
    if (g_eye.z>z) {
        pquad(&q,rx,y,fz,lx,y,fz,lx,by,fz,rx,by,fz);
    } else {
        pquad(&q,lx,y,bz,rx,y,bz,rx,by,bz,lx,by,bz);
    }
    put_quad(&q,zcolor);
    if (g_eye.y>y) {
        quad_rect_xz(&q,lx,rx,bz,fz,y);
        put_quad_lit(&q,top);
        if (trim && hx>SB_F(4) && hz>SB_F(4)) {
            /* Indexed VDP1 distorted sprite: one tiny reusable 16x16 material. */
            quad_rect_xz(&q,lx+SB_F(2),rx-SB_F(2),bz+SB_F(2),fz-SB_F(2),y+SB_F(1)/32);
            if (trim==2u) {
                put_quad(&q,SAT_RGB555(24,20,10));
            } else {
                /* Non-uniform paving UVs need a texture clipper to split
                 * accurately. Never submit a stretched/malformed inset if
                 * the camera's near plane cuts the floor. Its SOLID base
                 * remains visible via put_quad's geometric near clipping. */
                sat_quad3_t clipped[4];
                uint8_t clipped_count=0u;
                sat_example_must(sat_clip_quad_near(
                    &q,&g_eye,&g_scene.forward,SB_F(8),clipped,&clipped_count));
                if(clipped_count==1u) {
                    uint8_t fully_in_front=1u,k;
                    sat_quad2_t projected;
                    sat_result_t st;
                    /* One clipped triangle ALSO has count=1! Check that the
                     * original 4 UV corners survived unchanged before drawing
                     * a patterned quad; otherwise the old near-plane hazard
                     * was still reachable through this inset. */
                    for(k=0u;k<4u;++k) {
                        if(clipped[0].v[k].x!=q.v[k].x ||
                           clipped[0].v[k].y!=q.v[k].y ||
                           clipped[0].v[k].z!=q.v[k].z)
                            fully_in_front=0u;
                    }
                    if(fully_in_front) {
                        st=sat_project_quad(&g_vp,&q,&projected);
                        if(st==SAT_OK) {
                            uint8_t entirely_on_screen=1u;
                            sat_distorted_sprite_cmd_t cmd={0};
                            for(k=0u;k<4u;++k) {
                                if(projected.x[k]<-(int32_t)W/2 ||
                                   projected.x[k]>=(int32_t)W/2 ||
                                   projected.y[k]<-(int32_t)H/2 ||
                                   projected.y[k]>=(int32_t)H/2)
                                    entirely_on_screen=0u;
                                cmd.x[k]=projected.x[k];
                                cmd.y[k]=projected.y[k];
                            }
                            /* This patterned sprite has no UV-aware
                             * clipping. When partially off-screen, render
                             * only the safe, clipped solid base instead. */
                            if(entirely_on_screen) {
                                cmd.texture=&g_tile_textures[
                                    trim==1u?0u:(trim==3u?1u:2u)];
                                if(g_active_fade_slot==FADE_OPAQUE)
                                    st=sat_draw_sprite_distorted(&cmd);
                                else st=sat_draw_sprite_distorted_color_calc(
                                    &cmd,g_active_fade_slot);
                                if(st!=SAT_OK && st!=SAT_ERR_UNSUPPORTED)
                                    sat_example_must(st);
                            }
                        } else if(st!=SAT_ERR_UNSUPPORTED) sat_example_must(st);
                    }
                }
            }
        }
    }
}
static uint16_t top_color(uint8_t i) {
    if (i<4u) return SAT_RGB555(24,23,16);
    if (i<7u) return SAT_RGB555(12,26,19);
    return SAT_RGB555(27,23,16);
}
/* Real 3D diamond/octahedron, not another box like the magenta avatar.
 * Four equator corners, one top tip, one bottom tip -> eight triangular
 * facets. Triangles use repeated D=C, which the quad projection supports.
 * Submit facets back-to-front: VDP1 has no depth buffer. The diamond shares
 * the game's position/radius/hover constants with the pickup collision. */
static void draw_gem(uint8_t id,int32_t x,int32_t deck_y,int32_t z) {
    static const int8_t ox[4]={1,0,-1,0};
    static const int8_t oz[4]={0,1,0,-1};
    static const uint16_t upper[4]={
        SAT_RGB555(31,30,8),SAT_RGB555(31,26,3),
        SAT_RGB555(31,30,8),SAT_RGB555(31,23,4)
    };
    static const uint16_t lower[4]={
        SAT_RGB555(31,13,3),SAT_RGB555(31,23,4),
        SAT_RGB555(31,13,3),SAT_RGB555(31,23,4)
    };
    const int32_t bob=sat_fx16_mul(
        sat_sin_deg(SB_F((int32_t)(g_game.ticks*5u+id*33u)%360)),
        SB_F(1)/2);
    const int32_t mid=deck_y+SB_GEM_BASE_OFFSET+bob;
    const int32_t tip_top=mid+SB_GEM_HALF_HEIGHT;
    const int32_t tip_bottom=mid-SB_GEM_HALF_HEIGHT;
    uint8_t order[4]={0u,1u,2u,3u};
    uint64_t distance[4];
    uint8_t i,j;
    for(i=0u;i<4u;++i) {
        uint8_t next=(uint8_t)((i+1u)&3u);
        int32_t cx=x+(ox[i]+ox[next])*SB_GEM_RADIUS/2;
        int32_t cz=z+(oz[i]+oz[next])*SB_GEM_RADIUS/2;
        int64_t dx=(int64_t)g_eye.x-cx;
        int64_t dz=(int64_t)g_eye.z-cz;
        distance[i]=(uint64_t)(dx*dx+dz*dz);
    }
    for(i=1u;i<4u;++i) {
        uint8_t index=order[i];
        j=i;
        while(j>0u && distance[order[j-1u]]<distance[index]) {
            order[j]=order[j-1u];
            --j;
        }
        order[j]=index;
    }
    for(i=0u;i<4u;++i) {
        sat_quad3_t tri;
        uint8_t side=order[i],next=(uint8_t)((side+1u)&3u);
        int32_t ax=x+ox[side]*SB_GEM_RADIUS;
        int32_t az=z+oz[side]*SB_GEM_RADIUS;
        int32_t bx=x+ox[next]*SB_GEM_RADIUS;
        int32_t bz=z+oz[next]*SB_GEM_RADIUS;
        pquad(&tri,x,tip_top,z,ax,mid,az,bx,mid,bz,bx,mid,bz);
        put_quad(&tri,upper[side]);
        pquad(&tri,ax,mid,az,x,tip_bottom,z,bx,mid,bz,bx,mid,bz);
        put_quad(&tri,lower[side]);
    }
}
static void stage_box(uint8_t i) {
    const sb_platform_t* p=&sb_course_platforms(&g_game)[i];
    int32_t x=sb_platform_x(&g_game,i), z=SB_F(p->z);
    int32_t top_y=sb_platform_y(&g_game,i);
    uint16_t t=top_color(i);
    if(p->surface==SB_SURFACE_SLICK)t=SAT_RGB555(9,26,30);
    if(p->surface==SB_SURFACE_GRIP)t=SAT_RGB555(31,20,7);
    if(p->kind==SB_LIFT)
        t=SAT_RGB555(31,26,3);
    if (p->kind==SB_COLLAPSING && g_game.collapse_ticks>0u &&
        g_game.collapse_ticks<=30u)
        t=(g_game.ticks&4u)?SAT_RGB555(31,8,5):SAT_RGB555(31,26,5);
    box3(x,top_y,z,SB_F(p->half_x),SB_F(5),SB_F(p->half_z),
         t, p->kind==SB_LIFT?SAT_RGB555(24,20,10):
             (i<4u ? SAT_RGB555(12,14,14):SAT_RGB555(6,16,15)),
         p->kind==SB_LIFT?SAT_RGB555(31,19,2):
             (i<4u ? SAT_RGB555(17,17,14):SAT_RGB555(8,19,18)),
         p->kind==SB_COLLAPSING?2u:
             (p->kind==SB_LIFT?4u:(i<4u?1u:(i<7u?3u:4u))));
    /* Distinct corner braces and inset deck rails make the floating decks
     * read as engineered 3D structures rather than untextured slabs.
     * All braces are outside the traversable deck and are decorative only. */
    if(i==0u || i==3u || i==4u || i==6u || i==9u) {
        int32_t rim_z=z+(g_eye.z<z?-SB_F(p->half_z-3):
                                      SB_F(p->half_z-3));
        uint16_t metal=(i<4u)?SAT_RGB555(17,17,14):
            (i<7u?SAT_RGB555(8,19,18):SAT_RGB555(24,20,10));
        box3(x-SB_F(p->half_x-4),top_y-SB_F(6),rim_z,
             SB_F(2),SB_F(3),SB_F(2),metal,metal,metal,0u);
        box3(x+SB_F(p->half_x-4),top_y-SB_F(6),rim_z,
             SB_F(2),SB_F(3),SB_F(2),metal,metal,metal,0u);
    }
    /* A thin raised, contrasting edge band makes platform boundaries
     * legible at speed without adding collision-changing obstacles. */
    if(g_eye.y>top_y && p->half_x>7 && p->half_z>7) {
        sat_quad3_t edge;
        int32_t sy=top_y+SB_F(1)/24;
        int32_t lx=x-SB_F(p->half_x-1),rx=x+SB_F(p->half_x-1);
        int32_t bz=z-SB_F(p->half_z-1),fz=z+SB_F(p->half_z-1);
        uint16_t edge_color=(i<4u)?SAT_RGB555(24,20,10):
            (i<7u?SAT_RGB555(12,26,19):SAT_RGB555(31,26,3));
        quad_rect_xz(&edge,lx,rx,bz,bz+SB_F(1),sy);
        put_quad(&edge,edge_color);
        quad_rect_xz(&edge,lx,rx,fz-SB_F(1),fz,sy);
        put_quad(&edge,edge_color);
    }
    /* Elevators have a visible shaft below the deck: its length
     * changes with the actual collision top, never a separate animation. */
    if(p->kind==SB_LIFT) {
        box3(x,top_y-SB_F(9),z,SB_F(2),SB_F(4),SB_F(2),
             SAT_RGB555(24,20,10),SAT_RGB555(12,14,14),
             SAT_RGB555(17,17,14),0u);
    }
    /* Checkpoints and finish are physically marked, not just HUD text. */
    if (i==3u || i==6u) {
        box3(x-SB_F(7),top_y+SB_F(5),z+SB_F(5),SB_F(1),SB_F(5),SB_F(1),
             SAT_RGB555(31,26,3),SAT_RGB555(23,16,3),SAT_RGB555(31,19,2),0u);
    }
    if (i==9u) {
        box3(x-SB_F(10),top_y+SB_F(9),z+SB_F(5),SB_F(1),SB_F(9),SB_F(1),
             SAT_RGB555(31,26,3),SAT_RGB555(16,12,3),SAT_RGB555(27,20,4),0u);
        box3(x+SB_F(10),top_y+SB_F(9),z+SB_F(5),SB_F(1),SB_F(9),SB_F(1),
             SAT_RGB555(31,26,3),SAT_RGB555(16,12,3),SAT_RGB555(27,20,4),0u);
        box3(x,top_y+SB_F(19),z+SB_F(5),SB_F(11),SB_F(1),SB_F(1),
             SAT_RGB555(31,26,3),SAT_RGB555(20,14,3),SAT_RGB555(27,19,4),0u);
    }
}
/* Model only: the pig is contained inside the original 4x4x5 fixed-point
 * collision volume. Course 1/2 movement, coyote time, gem contact and
 * elevator carry remain owned by game.h. Its geometry uses indexed VDP1
 * distorted sprites through box3()/put_quad(), never RGB faces that break
 * the existing VDP2 distance color-calculation setup. */
static void pig_transform(int32_t x,int32_t z,int8_t fx,int8_t fz,
                          int32_t side,int32_t forward,
                          int32_t* out_x,int32_t* out_z) {
    /* Local forward follows the persistent cardinal snout direction.
     * Local side and forward are 16.16; facing is exactly -1, 0 or 1. */
    *out_x=x+(int32_t)fz*side+(int32_t)fx*forward;
    *out_z=z-(int32_t)fx*side+(int32_t)fz*forward;
}
static void pig_box(int32_t x,int32_t z,int32_t feet,
                    int8_t fx,int8_t fz,
                    int32_t side,int32_t up,int32_t forward,
                    int32_t half_side,int32_t half_up,int32_t half_forward,
                    uint16_t top,uint16_t lateral,uint16_t front) {
    int32_t wx,wz;
    int32_t hx=fz!=0?half_side:half_forward;
    int32_t hz=fz!=0?half_forward:half_side;
    pig_transform(x,z,fx,fz,side,forward,&wx,&wz);
    box3(wx,feet+up,wz,hx,half_up,hz,top,lateral,front,0u);
}
static void pig_mark(int32_t px,int32_t pz,int32_t feet,
                     int8_t fx,int8_t fz,
                     int32_t side,int32_t y,int32_t forward,
                     int32_t half_width,int32_t half_height,
                     uint16_t color) {
    sat_quad3_t q;
    int32_t ax,az,bx,bz;
    pig_transform(px,pz,fx,fz,side-half_width,forward,&ax,&az);
    pig_transform(px,pz,fx,fz,side+half_width,forward,&bx,&bz);
    pquad(&q,ax,feet+y+half_height,az,
          bx,feet+y+half_height,bz,
          bx,feet+y-half_height,bz,
          ax,feet+y-half_height,az);
    put_quad(&q,color);
}
static void pig_ear(int32_t px,int32_t pz,int32_t feet,
                    int8_t fx,int8_t fz,int32_t side) {
    sat_quad3_t q;
    int32_t ax,az,bx,bz,tx,tz;
    const int32_t spread=SB_F(1)/4;
    const int32_t base_f=SB_F(1)/4;
    /* Flat, pointed low-poly ear: pink outer triangle + inset shading. */
    pig_transform(px,pz,fx,fz,side-spread,base_f,&ax,&az);
    pig_transform(px,pz,fx,fz,side+spread,base_f,&bx,&bz);
    pig_transform(px,pz,fx,fz,side,-SB_F(1)/12,&tx,&tz);
    pquad(&q,ax,feet+SB_F(4)+SB_F(1)/20,az,
          bx,feet+SB_F(4)+SB_F(1)/20,bz,
          tx,feet+SB_F(4)+SB_F(4)/5,tz,
          tx,feet+SB_F(4)+SB_F(4)/5,tz);
    put_quad(&q,PIG_TOP);
    pig_transform(px,pz,fx,fz,side-spread/2,base_f+SB_F(1)/64,&ax,&az);
    pig_transform(px,pz,fx,fz,side+spread/2,base_f+SB_F(1)/64,&bx,&bz);
    pig_transform(px,pz,fx,fz,side,0,&tx,&tz);
    pquad(&q,ax,feet+SB_F(4)+SB_F(1)/5,az,
          bx,feet+SB_F(4)+SB_F(1)/5,bz,
          tx,feet+SB_F(4)+SB_F(3)/5,tz,
          tx,feet+SB_F(4)+SB_F(3)/5,tz);
    put_quad(&q,PIG_EAR);
}
static void pig_head(int32_t px,int32_t pz,int32_t feet,
                     int8_t fx,int8_t fz,int32_t bob,
                     uint8_t show_face) {
    const int32_t f=feet+bob;
    pig_box(px,pz,f,fx,fz,0,SB_F(16)/5,SB_HALF,
            SB_F(1),SB_F(19)/20,SB_F(9)/10,
            PIG_HEAD,PIG_SIDE,PIG_TOP);
    /* Ears extend to 4.8 units and never exceed the original 5-unit
     * player collider even at the peak of the little running bounce. */
    pig_ear(px,pz,f,fx,fz,-SB_F(7)/10);
    pig_ear(px,pz,f,fx,fz, SB_F(7)/10);
    pig_box(px,pz,f,fx,fz,0,SB_F(29)/10,SB_F(3)/2,
            SB_F(11)/20,SB_F(9)/20,SB_F(2)/5,
            PIG_SNOUT,PIG_SNOUT,PIG_SIDE);
    if(show_face) {
        /* Eyes and two dark nostrils on the *real forward face*.
         * No camera-facing billboard; rotation now exposes a recognizable
         * profile, front and back instead of an identically painted cube. */
        pig_mark(px,pz,f,fx,fz,-SB_F(2)/5,SB_F(18)/5,SB_F(3)/2,
                 SB_F(3)/20,SB_F(3)/20,SAT_RGB555(31,31,31));
        pig_mark(px,pz,f,fx,fz, SB_F(2)/5,SB_F(18)/5,SB_F(3)/2,
                 SB_F(3)/20,SB_F(3)/20,SAT_RGB555(31,31,31));
        pig_mark(px,pz,f,fx,fz,-SB_F(2)/5,SB_F(18)/5,
                 SB_F(3)/2+SB_F(1)/64,
                 SB_F(1)/16,SB_F(1)/16,PIG_BLACK);
        pig_mark(px,pz,f,fx,fz, SB_F(2)/5,SB_F(18)/5,
                 SB_F(3)/2+SB_F(1)/64,
                 SB_F(1)/16,SB_F(1)/16,PIG_BLACK);
        pig_mark(px,pz,f,fx,fz,-SB_F(1)/5,SB_F(29)/10,
                 SB_F(19)/10+SB_F(1)/64,
                 SB_F(1)/12,SB_F(1)/9,PIG_DARK);
        pig_mark(px,pz,f,fx,fz, SB_F(1)/5,SB_F(29)/10,
                 SB_F(19)/10+SB_F(1)/64,
                 SB_F(1)/12,SB_F(1)/9,PIG_DARK);
    }
}
static void pig_tail(int32_t px,int32_t pz,int32_t feet,
                     int8_t fx,int8_t fz,int32_t bob) {
    const int32_t f=feet+bob;
    pig_box(px,pz,f,fx,fz,0,SB_F(5)/2,-SB_F(8)/5,
            SB_F(1)/6,SB_F(1)/6,SB_F(1)/5,
            PIG_DARK,PIG_DARK,PIG_DARK);
    pig_box(px,pz,f,fx,fz,SB_F(1)/5,SB_F(11)/4,-SB_F(9)/5,
            SB_F(1)/7,SB_F(1)/7,SB_F(1)/7,
            PIG_TOP,PIG_SIDE,PIG_DARK);
}
static void player_pig(void) {
    const int32_t px=g_game.x,pz=g_game.z,feet=g_game.y;
    const int8_t fx=g_game.facing_x,fz=g_game.facing_z;
    int32_t bob=0,step=0;
    int64_t camera_side;
    uint8_t from_front;
    uint8_t i;
    if(g_game.support>=0) {
        sat_quad3_t shadow;
        int32_t sy=sb_platform_y(&g_game,(uint8_t)g_game.support)+SB_F(1)/16;
        quad_rect_xz(&shadow,px-SB_F(2),px+SB_F(2),
                     pz-SB_F(2),pz+SB_F(2),sy);
        put_quad(&shadow,SAT_RGB555(8,10,10));
    }
    if(g_game.support>=0 &&
       sb_abs(g_game.vx)+sb_abs(g_game.vz)>SB_F(1)/3) {
        int32_t cycle=sat_sin_deg(SB_F((int32_t)(g_game.ticks*12u)%360));
        bob=sb_mul(cycle,SB_F(1)/10);
        step=sb_mul(cycle,SB_F(1)/5);
    }
    /* All four hooves are within X/Z +/-2 and touch the deck when idle. */
    for(i=0u;i<4u;++i) {
        int32_t side=(i&1u)?SB_F(4)/5:-SB_F(4)/5;
        int32_t forward=(i&2u)?SB_F(4)/5:-SB_F(4)/5;
        int32_t lift=((i&1u)==((i>>1u)&1u)?step:-step);
        if(lift<0)lift=0;
        pig_box(px,pz,feet,fx,fz,side,SB_F(13)/20+lift,forward,
                SB_F(3)/10,SB_F(11)/20,SB_F(3)/10,
                PIG_SIDE,PIG_HOOF,PIG_DARK);
    }
    camera_side=(int64_t)(g_eye.x-px)*fx+(int64_t)(g_eye.z-pz)*fz;
    from_front=(uint8_t)(camera_side>=0);
    if(from_front) {
        pig_tail(px,pz,feet,fx,fz,bob);
    } else {
        pig_head(px,pz,feet,fx,fz,bob,0u);
    }
    pig_box(px,pz,feet+bob,fx,fz,0,SB_F(47)/20,-SB_F(1)/5,
            SB_F(6)/5,SB_F(11)/10,SB_F(13)/10,
            PIG_TOP,PIG_SIDE,PIG_DARK);
    if(from_front) {
        pig_head(px,pz,feet,fx,fz,bob,1u);
    } else {
        pig_tail(px,pz,feet,fx,fz,bob);
    }
}
/* Each platform owns its previous quantized level. Once within two units
 * of a transition, keep the old state until the camera actually crosses
 * the hysteresis band; don't toggle one object twice as the chase camera
 * eases across a color-calc slot or at the culling boundary. */
static uint8_t platform_fade_slot(uint8_t id,int32_t depth) {
    sat_fade3d_result_t result={0};
    uint8_t prev=g_platform_fade[id],target;
    int32_t band=FADE_HYSTERESIS;
    if(g_game.support==(int8_t)id) {
        g_platform_fade[id]=FADE_OPAQUE;
        return FADE_OPAQUE;
    }
    if(sat_fade3d_eval(&g_fade_policy,depth,&result)!=SAT_OK)
        return FADE_CULLED;
    target=result.culled?FADE_CULLED:
        (depth<=FADE_START?FADE_OPAQUE:result.level);
    if(prev==FADE_OPAQUE && depth<=FADE_START+band) return prev;
    if(prev==FADE_CULLED && depth>=FADE_END-band) return prev;
    if(prev<8u) {
        int32_t span=(FADE_END-FADE_START)/8;
        int32_t lo=FADE_START+(int32_t)prev*span-band;
        int32_t hi=FADE_START+(int32_t)(prev+1u)*span+band;
        if(depth>=lo && depth<=hi)return prev;
    }
    g_platform_fade[id]=target;
    return target;
}
/* Procedural shapes remain Skybridge-owned; camera depth and painter order
 * are reusable LibSaturn scene-queue responsibilities.  Stage callbacks are
 * invoked only after all entries and fades have been computed this frame. */
static sat_result_t draw_stage_item(void* user,const sat_camera3d_t* camera) {
    uint8_t id=*(const uint8_t*)user;
    (void)camera;
    g_active_fade_slot=g_stage_frame_slot[id];
    stage_box(id);
    return SAT_OK;
}
static sat_result_t draw_pig_item(void* user,const sat_camera3d_t* camera) {
    (void)user;
    (void)camera;
    g_active_fade_slot=FADE_OPAQUE;
    player_pig();
    return SAT_OK;
}
static sat_result_t draw_gem_item(void* user,const sat_camera3d_t* camera) {
    uint8_t id=*(const uint8_t*)user;
    (void)camera;
    g_active_fade_slot=FADE_OPAQUE;
    draw_gem(id,sb_platform_x(&g_game,id),sb_platform_y(&g_game,id),
             SB_F(sb_course_platforms(&g_game)[id].z));
    return SAT_OK;
}
static void draw_world(void) {
    sat_camera3d_t camera={0};
    uint8_t visible_decks[SB_PLATFORM_COUNT]={0};
    uint8_t i;
    camera.eye=g_eye;
    camera.target=g_target;
    camera.up=(sat_vec3_t){0,SB_F(1),0};
    camera.fov_y=SB_F(55);
    camera.aspect=(sat_fx16_t)((W*65536u)/H);
    camera.near_z=SB_F(2);
    camera.far_z=SB_F(250);
    camera.view_proj=g_vp;  /* already calculated once in the frame loop */
    sat_example_must(sat_scene3d_queue_begin(&g_scene,&camera));

    /* The large deck geometry still uses an explicit painter pass, with the
     * supporting platform drawn after other decks: subdividing intersecting
     * decks for exact visibility is outside this small Saturn sample.
     * Pig and gems share pass 2, where the queue sorts them by actual 3D
     * camera-space depth and the gem can correctly appear IN FRONT. */
    for(i=0u;i<SB_PLATFORM_COUNT;++i) {
        const sb_platform_t* p=&sb_course_platforms(&g_game)[i];
        sat_vec3_t center;
        sat_fx16_t depth;
        uint8_t slot;
        if(!sb_platform_active(&g_game,i))continue;
        center=(sat_vec3_t){
            sb_platform_x(&g_game,i),sb_platform_y(&g_game,i),SB_F(p->z)
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
        sat_example_must(sat_scene3d_queue_depth(&g_scene,&center,&depth));
        if(g_game.support!=(int8_t)i &&
           (depth < -SB_F(9) ||
            sb_abs(center.x-g_game.x)>SB_F(160) ||
            sb_abs(center.z-g_game.z)>SB_F(180)))continue;
        slot=platform_fade_slot(i,depth);
        if(slot==FADE_CULLED)continue;
        g_stage_frame_slot[i]=slot;
        sat_example_must(sat_scene3d_queue_submit_draw(
            &g_scene,&center,
            g_game.support==(int8_t)i?SCENE_PASS_SUPPORT:SCENE_PASS_WORLD,
            draw_stage_item,&g_stage_ids[i]));
        visible_decks[i]=1u;
    }
    {
        sat_vec3_t center={
            g_game.x,g_game.y+SB_PLAYER_HEIGHT/2,g_game.z
        };
        sat_example_must(sat_scene3d_queue_submit_draw(
            &g_scene,&center,SCENE_PASS_ACTOR,draw_pig_item,0));
    }
    for(i=1u;i<=SB_PICKUP_COUNT;++i) {
        sat_vec3_t center;
        sat_fx16_t depth;
        if(!visible_decks[i] || (g_game.pickups&(1u<<(i-1u))))continue;
        center=(sat_vec3_t){
            sb_platform_x(&g_game,i),
            sb_platform_y(&g_game,i)+SB_GEM_BASE_OFFSET,
            SB_F(sb_course_platforms(&g_game)[i].z)
        };
        sat_example_must(sat_scene3d_queue_depth(&g_scene,&center,&depth));
        if(depth<=0 || sb_abs(center.x-g_game.x)>SB_F(160) ||
           sb_abs(center.z-g_game.z)>SB_F(180))continue;
        sat_example_must(sat_scene3d_queue_submit_draw(
            &g_scene,&center,SCENE_PASS_ACTOR,draw_gem_item,&g_stage_ids[i]));
    }
    sat_example_must(sat_scene3d_queue_flush(&g_scene));
}
static void init_tile_texture(void) {
    static const uint8_t banks[3]={3u,5u,6u};
    static const uint8_t colors[3][3][3]={
        {{9u,23u,18u},{17u,28u,22u},{26u,30u,27u}},
        {{8u,18u,24u},{13u,25u,28u},{23u,29u,30u}},
        {{21u,17u,9u},{27u,23u,14u},{31u,29u,22u}}
    };
    uint16_t palette[256];
    uint16_t x,y;
    uint8_t theme;
    /* Broad 4x4 paving motifs, a quiet border and sparse highlights:
     * distinguish the three regions without a noisy repeating checker. */
    for (y=0u;y<16u;++y) for (x=0u;x<16u;++x) {
        uint8_t cell=(uint8_t)(((x>>2u)+(y>>2u))&1u);
        uint8_t grout=(uint8_t)((x&7u)==0u || (y&7u)==0u);
        uint8_t glint=(uint8_t)((x==5u && y==4u)||(x==13u && y==12u));
        g_tile_pixels[y*16u+x]=glint?3u:(grout?2u:(uint8_t)(1u+cell));
    }
    for(theme=0u;theme<3u;++theme) {
        for(x=0u;x<256u;++x)palette[x]=SAT_BGR555(0,0,0);
        for(x=0u;x<3u;++x)
            palette[x+1u]=SAT_RGB555(colors[theme][x][0],
                                     colors[theme][x][1],
                                     colors[theme][x][2]);
        sat_example_must(sat_tex_upload_indexed8(
            &g_tile_textures[theme],g_tile_pixels,16u,16u,palette,banks[theme]));
    }
}
static void init_cloud_texture(void) {
    uint16_t palette[256];
    uint16_t x,y;
    for(x=0u;x<256u;++x)palette[x]=SAT_BGR555(0,0,0);
    palette[1u]=SAT_RGB555(31,31,31);
    palette[2u]=SAT_RGB555(28,30,31);
    palette[3u]=SAT_RGB555(23,27,30);
    palette[4u]=SAT_RGB555(18,24,28);
    for(y=0u;y<SB_CLOUD_H;++y)for(x=0u;x<SB_CLOUD_W;++x)
        g_cloud_pixels[y*SB_CLOUD_W+x]=sb_scenery_cloud_pixel(x,y);
    /* CRAM bank 7 is deliberately separate from sea 0, sky 1,
     * font 2, stage 3/5/6 and the fade-material bank 4. */
    sat_example_must(sat_tex_upload_indexed8(
        &g_cloud_texture,g_cloud_pixels,SB_CLOUD_W,SB_CLOUD_H,palette,7u));
}
static void draw_clouds(void) {
    static const uint16_t base_x[6]={18u,102u,206u,315u,405u,488u};
    static const uint8_t y[6]={21u,42u,27u,15u,48u,32u};
    static const uint8_t width[6]={68u,52u,79u,60u,55u,72u};
    static const uint8_t height[6]={15u,12u,18u,14u,12u,16u};
    uint8_t i;
    for(i=0u;i<6u;++i) {
        int32_t x=sb_scenery_cloud_x(base_x[i],(uint16_t)g_yaw,g_frame);
        int32_t left=x-(int32_t)width[i]/2;
        int32_t right=x+(int32_t)width[i]/2;
        if(right>0 && left<(int32_t)W)
            sat_example_must(sat_draw_sprite_scaled_screen(
                &g_cloud_texture,(int16_t)x,(int16_t)y[i],
                width[i],height[i],0u));
        /* Draw the periodic copy when a cloud crosses the 512px seam.
         * Do not wrap every cloud at 320px: that visibly tiles the sky. */
        x-=512;
        left=x-(int32_t)width[i]/2;
        right=x+(int32_t)width[i]/2;
        if(right>0 && left<(int32_t)W)
            sat_example_must(sat_draw_sprite_scaled_screen(
                &g_cloud_texture,(int16_t)x,(int16_t)y[i],
                width[i],height[i],0u));
    }
}
static void init_fade_materials(void) {
    uint16_t palette[256]={0};
    uint8_t i;
    for(i=0u;i<FADE_COLOR_COUNT;++i)
        palette[i+1u]=g_fade_colors[i]; /* Match the RGB-coded CRAM format proven by distance_fade_3d. */
    sat_example_must(sat_vdp1_set_erase_transparent());
    sat_example_must(sat_palette_upload_indexed8(palette,FADE_PALETTE_BANK));
    for(i=0u;i<FADE_COLOR_COUNT;++i) {
        uint16_t n;
        for(n=0u;n<256u;++n)g_fade_solid_pixels[n]=(uint8_t)(i+1u);
        sat_example_must(sat_tex_upload_indexed8_pixels(
            &g_fade_textures[i],g_fade_solid_pixels,16u,16u,FADE_PALETTE_BANK));
    }
}
static void init_sky(void) {
    uint32_t x,y;
    for(x=0u;x<256u;++x)g_sky_colors[x]=sb_scenery_sky_color(x);
    for(y=0u;y<SKY_H;++y)for(x=0u;x<SKY_W;++x)
        g_sky[y*SKY_W+x]=sb_scenery_sky_pixel(x,y);
}
static void init_sea(void) {
    volatile uint16_t* vram=(volatile uint16_t*)0x25E00000u;
    uint32_t x,y,at=SEA_WORD;
    for(x=0u;x<256u;++x)
        g_sea_colors[x]=sb_scenery_sea_color(x&63u);
    for(y=0u;y<SB_SEA_H;++y) {
        for(x=0u;x<SB_SEA_W;x+=2u) {
            uint8_t a=sb_scenery_sea_pixel(x,y);
            uint8_t b=sb_scenery_sea_pixel(x+1u,y);
            vram[at++]=(uint16_t)(((uint16_t)a<<8u)|b);
        }
        if((y&15u)==15u)
            loading_frame("GENERATING OCEAN",
                (uint8_t)(15u+((y+1u)*60u/SB_SEA_H)));
    }
}
static void animate_sea_palette(uint32_t tick) {
    /* Palette modulation touches only eight highlight colors: 16 bytes
     * every 8 display frames, never a 128 KiB bitmap or the fade registers.
     * The RBG0 scroll supplies flow and this supplies subtle foam shimmer. */
    uint32_t phase=(tick>>3u)&31u;
    uint32_t light=phase<16u?phase:31u-phase;
    uint32_t i;
    if(g_last_ocean_palette_step==phase)return;
    g_last_ocean_palette_step=phase;
    for(i=0u;i<8u;++i) {
        uint32_t t=i+(light>>2u);
        g_sea_colors[48u+i]=sb_scenery_rgb(6u+t/3u,18u+t/2u,
                                           24u+t/4u);
    }
    sat_example_must(sat_vdp2_palette_upload(
        &g_sea_colors[48u],8u,48u));
}
static void update_rotation(int32_t fx,int32_t fz) {
    uint16_t p[48];
    /* Keep the sea under a fixed 96px horizon, rotate/scroll sample plane. */
    /* Two slow, non-identical currents slide the textured water plane
     * underneath a fixed world horizon without any per-frame bitmap upload. */
    rbg0_ground_build_params(&g_ocean, (g_game.x>>16)+(int32_t)(g_frame/9u),
                              (g_game.z>>16)+(int32_t)(g_frame/17u),p);
    p[15]=(uint16_t)((uint32_t)fz&0xFFFFu);
    p[17]=(uint16_t)((uint32_t)fx&0xFFFFu);
    p[21]=(uint16_t)((uint32_t)(-fx)&0xFFFFu);
    p[23]=(uint16_t)((uint32_t)fz&0xFFFFu);
    /* The parameter table uses signed 16.16 A/B/D/E, including high words. */
    p[14]=(uint16_t)(fz>>16);
    p[16]=(uint16_t)(fx>>16);
    p[20]=(uint16_t)((-fx)>>16);
    p[22]=(uint16_t)(fz>>16);
    sat_example_must(sat_vdp2_vram_write_words(ROT_WORD,p,48u));
}
static void init_background(void) {
    uint16_t coef[H*2u],p[48];
    const sat_vdp2_nbg0_config_t sky = {
        SAT_VDP2_CHAR_SIZE_1X1,SAT_VDP2_COLOR_MODE_256,0x3Bu,0u,0u
    };
    const sat_vdp2_rbg0_mode7_config_t sea = {
        SAT_VDP2_RBG0_BITMAP_512x256,SAT_VDP2_COLOR_MODE_256,
        SEA_WORD,ROT_WORD,SAT_COLOR_BLACK,5u,7u
    };
    uint32_t y;
    for(y=0u;y<H;++y) rbg0_ground_encode_coefficient(&g_ocean,y,
                      &coef[y*2u],&coef[y*2u+1u]);
    rbg0_ground_build_params(&g_ocean,0,0,p);
    sat_example_must(sat_vdp2_palette_upload(g_sea_colors,256u,0u));
    sat_example_must(sat_vdp2_palette_upload(g_sky_colors,256u,256u));
    sat_example_must(sat_vdp2_nbg0_init(&sky));
    sat_example_must(sat_vdp2_nbg0_upload_indexed8(g_sky,SKY_W,SKY_H,1u,g_map));
    sat_example_must(sat_vdp2_vram_write_words(COEF_WORD,coef,H*2u));
    sat_example_must(sat_vdp2_vram_write_words(ROT_WORD,p,48u));
    sat_example_must(sat_vdp2_rbg0_mode7_init(&sea));
    sat_example_must(sat_vdp2_nbg0_set_priority(2u));
    sat_example_must(sat_vdp2_rbg0_set_priority(5u));
    sat_example_must(sat_vdp2_sprite_set_priority(7u));
    sat_example_must(sat_vdp2_sprite_color_calc_configure_alpha(7u));
    loading_frame("COMPOSING VDP2",82u);
}
static void init_audio(void) {
    uint32_t i;
    uint8_t j;
    const uint8_t tone_step[SOUNDS-1u]={3u,5u,7u,2u,9u};
    sat_sound_play_params_t music_params={76u,0,1u,0u,SB_F(1)};
    sat_example_must(sat_audio_init());
    sat_example_must(sat_audio_set_master_volume(160u));
    for(j=0u;j<SOUNDS-1u;++j) {
        for(i=0u;i<SOUND_LEN;++i) {
            int32_t wave=(int32_t)((i*tone_step[j])&63u)-32;
            int32_t env=(int32_t)((SOUND_LEN-i)*55u/SOUND_LEN);
            if (j==3u) wave=(int32_t)(((i*73u)^(i>>2u))&63u)-32;
            g_audio[j][i]=(int8_t)(wave*env/32);
        }
        {
            sat_asset_desc_t asset={0};
            sat_asset_t asset_handle={0};
            asset.logical_path=g_sfx_paths[j];
            asset.kind=SAT_ASSET_SOUND;
            asset.data=g_audio[j];
            asset.size=SOUND_LEN;
            asset.sample_rate=11025u;
            asset.sample_count=SOUND_LEN;
            asset.channels=1u;
            asset.format=SAT_AUDIO_PCM_S8;
            sat_example_must(sat_asset_register(&asset,&asset_handle));
            sat_example_must(sat_sound_load(g_sfx_paths[j],&g_sounds[j]));
        }
        loading_frame("REGISTERING AUDIO",(uint8_t)(82u+(j+1u)*2u));
    }
    {
        static const uint8_t periods[8]={50u,45u,40u,38u,34u,38u,40u,45u};
        for (i=0u;i<MUSIC_LEN;++i) {
            uint32_t note=i/4096u, local=i%4096u;
            uint32_t period=periods[note];
            int32_t phase=(int32_t)((local%period)*128u/period);
            int32_t wave=(phase<64 ? phase : 128-phase)-32;
            int32_t envelope=(int32_t)(local<160u ? local :
                (local>3935u ? 4095u-local : 160u));
            int32_t bass=(int32_t)((i%128u)/2u);
            bass=(bass<32 ? bass : 64-bass)-16;
            /* 8-note, three-second arpeggio. Fade notes to zero at boundaries. */
            g_music[i]=(int8_t)((wave*envelope)/320+bass/4);
            if ((i&4095u)==4095u)
                loading_frame("GENERATING MUSIC",(uint8_t)(92u+(i+1u)*6u/MUSIC_LEN));
        }
    }
    {
        sat_sound_desc_t desc={0};
        desc.samples=g_music;desc.sample_count=MUSIC_LEN;
        desc.sample_rate=11025u;desc.format=SAT_AUDIO_PCM_S8;
        desc.loop=1u;
        sat_example_must(sat_sound_create(&g_sounds[5u],&desc));
        sat_example_must(sat_sound_play(g_sounds[5u],&music_params,&g_music_voice));
    }
    g_audio_ready=1u;
    loading_frame("READY",100u);
}
static void sound_event(uint16_t event) {
    uint8_t id;
    sat_sound_play_params_t p={96u,0,8u,0u,SB_F(1)};
    if (!g_audio_ready || !event) return;
    if (event&SB_EVENT_WIN) id=4u;
    else if (event&SB_EVENT_FALL) id=3u;
    else if (event&SB_EVENT_CHECKPOINT) id=2u;
    else if (event&SB_EVENT_PICKUP) id=2u;
    else if (event&SB_EVENT_JUMP) id=0u;
    else if (event&SB_EVENT_LAND) id=1u;
    else if (event&SB_EVENT_WARNING) id=1u;
    else return;
    (void)sat_sound_play(g_sounds[id],&p,0);
}
static void start_course(uint8_t course) {
    uint8_t i;
    sb_start_course(&g_game,course);
    g_yaw=0;
    for(i=0u;i<SB_PLATFORM_COUNT;++i)g_platform_fade[i]=FADE_OPAQUE;
    g_camera_anchor=(sat_vec3_t){g_game.x,g_game.y,g_game.z};
}
static void hud(void) {
    uint8_t i,count=0u;
    for(i=0u;i<SB_PICKUP_COUNT;++i) if(g_game.pickups&(1u<<i)) ++count;
    (void)sat_draw_rect_screen(0,0,W,16u,SAT_RGB555(3,8,15));
    put_text("SKYBRIDGE",7,4);
    label("C",g_game.course+1u,104,4);
    label("GEMS ",count,144,4);
    put_text("/8",192,4);
    label("TIME ",g_game.ticks/60u,224,4);
    /* Always show player position, not the smoothed camera anchor. */
    (void)sat_draw_rect_screen(0,17,W,13u,SAT_RGB555(3,8,15));
    hud_coord('X',g_game.x,7,19);
    hud_coord('Y',g_game.y,112,19);
    hud_coord('Z',g_game.z,217,19);
    if(g_show_debug) {
        /* Y toggles the extra camera-orbit diagnostic below the XYZ row. */
        uint8_t surface=sb_ground_surface(&g_game,g_game.support);
        (void)sat_draw_rect_screen(0,31,W,13u,SAT_RGB555(3,8,15));
        label("DECK ",g_game.support<0?0u:(uint32_t)g_game.support+1u,7,33);
        label("YAW ",(uint32_t)g_yaw,101,33);
        put_text(surface==SB_SURFACE_SLICK?"ICE":
                 surface==SB_SURFACE_GRIP?"GRIP":
                 surface==SB_SURFACE_AIR?"AIR":"NORMAL",197,33);
    }
    if (g_game.finished) {
        (void)sat_draw_rect_screen(46,76,228u,75u,SAT_RGB555(2,13,16));
        put_text(g_game.course==0u?"COURSE 1 COMPLETE":
                                     "COURSE 2 COMPLETE",66,83);
        label("GEMS ",count,116,104);
        put_text("/8",164,104);
        put_text(g_game.course==0u?"START: COURSE 2":
                                     "START: REPLAY",82,128);
    } else if (g_game.paused) {
        put_text("PAUSED - START RESUMES",64,92);
        put_text("X: SWITCH COURSE",80,108);
    } else if(g_show_help && g_game.ticks<480u) {
        put_text("GEMS OPTIONAL  Z BRAKE",8,192);
        put_text("D-PAD MOVE  A JUMP",8,204);
        put_text("B/C CAMERA  START PAUSE",8,215);
    }
}
int main(void) {
    const sat_video_config_t video={W,H,1u,0u};
    const sat_vec3_t up={0,SB_F(1),0};
    sat_pad_state_t pad={0};
    sat_vdp2_scroll_t sky_scroll={0u,0u,31u,0u};
    sat_example_must(sat_init(&video));
    sb_init(&g_game);
    sat_example_must(sat_scene3d_queue_init(
        &g_scene,g_scene_items,SCENE_OBJECT_CAP));
    {uint8_t i;for(i=0u;i<SB_PLATFORM_COUNT;++i) {
        g_stage_ids[i]=i;
        g_platform_fade[i]=FADE_OPAQUE;
    }}
    g_camera_anchor=(sat_vec3_t){g_game.x,g_game.y,g_game.z};
    /* Load the first drawable font before expensive procedural generation. */
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font,SAT_COLOR_WHITE,SAT_COLOR_BLACK,2u));
    sat_example_must(sat_vdp1_set_erase_transparent());
    loading_frame("INITIALIZING WORLD",5u);
    init_tile_texture();
    init_cloud_texture();
    init_fade_materials();
    loading_frame("BUILDING SKY",10u);
    init_sky();
    loading_frame("BUILDING SEA",15u);
    init_sea();
    init_background();
    init_audio();
    g_prev_frame=sat_frame_count();
    for(;;) {
        uint32_t now,steps;
        uint16_t pressed,events=0u;
        int32_t fx,fz,rx,rz;
        sat_example_must(sat_wait_vblank());
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
        sat_example_must(sat_pad_poll(&pad));
        steps=now-g_prev_frame;
        g_prev_frame=now;
        if(steps>3u) steps=3u; /* Drop excess catch-up, preserve responsive input. */
        if (steps==0u) steps=1u;
        /* Pause + X is a deliberate course selector for playing/testing
         * Course 2 without finishing all ten decks of Course 1 first. */
        if(g_game.paused && (pad.pressed&SAT_PAD_X)) {
            start_course((uint8_t)(g_game.course+1u));
        } else if (pad.pressed&SAT_PAD_START) {
            if(g_game.finished)
                start_course((uint8_t)(g_game.course+1u));
            else g_game.paused=(uint8_t)!g_game.paused;
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
        }
        sound_event(events);
        if (events & SB_EVENT_FALL) {
            g_camera_anchor=(sat_vec3_t){g_game.x,g_game.y,g_game.z};
        } else {
            g_camera_anchor.x+=(g_game.x-g_camera_anchor.x)/4;
            g_camera_anchor.y+=(g_game.y-g_camera_anchor.y)/6;
            g_camera_anchor.z+=(g_game.z-g_camera_anchor.z)/4;
        }
        {
            int32_t ex,ez,lx,lz;
            sb_camera_offset(fx,fz,&ex,&ez,&lx,&lz);
            g_eye=(sat_vec3_t){g_camera_anchor.x+ex,
                g_camera_anchor.y+SB_F(29),g_camera_anchor.z+ez};
            g_target=(sat_vec3_t){g_camera_anchor.x+lx,
                g_camera_anchor.y+SB_F(5),g_camera_anchor.z+lz};
        }
        {
            sat_mat4_t view,projection;
            sat_example_must(sat_mat4_look_at(&view,&g_eye,&g_target,&up));
            sat_example_must(sat_mat4_perspective(&projection,SB_F(55),
                sat_fx16_div(SB_F(W),SB_F(H)),SB_F(2),SB_F(250)));
            sat_example_must(sat_mat4_multiply(&g_vp,&projection,&view));
        }
        /* Sprite color calculation was configured at startup. The generic
         * VBlank layer replay now preserves both of its priority selectors. */
        sat_example_must(sat_vdp1_set_erase_transparent());
        sat_example_must(sat_begin_frame());
        draw_clouds();
        draw_world();
        hud();
        sat_example_must(sat_end_frame());
        sat_example_must(sat_audio_update());
    }
}
