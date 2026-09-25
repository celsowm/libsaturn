/* scene_cache_occlusion -- real L3 mixed cached/dynamic painter demo.
 * A: swap between two camera views; B: change lens/eye of CURRENT view,
 * invalidating its preprojected cache. START: exit. Static indexed surfaces
 * and the moving RGB actor share ONE painter list and the same pass.
 * No extra geometry/VDP1 commands are built in application code. */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/example_util.h"
#include "saturn/font.h"
#include "saturn/input.h"
#include "saturn/math3d.h"
#include "saturn/render3d.h"
#include "saturn/scene.h"
#include "saturn/scene3d.h"
#include "saturn/vdp1.h"
#include "saturn/texture.h"
#include "saturn/surface.h"
#include "saturn/view_cache.h"

#define SCREEN_W 320u
#define SCREEN_H 224u
#define NEAR_DEPTH SAT_FX16_ONE
#define VIEW_COUNT 2u
#define CACHE_FACES 2u
#define SCENE_FACES 8u
#define OVERLAY_COMMANDS 48u

static sat_scene_t g_scene;
static sat_scene3d_face_t g_scene_faces[SCENE_FACES];
static uint32_t g_scene_keys[SCENE_FACES];
static uint16_t g_scene_order[SCENE_FACES];
/* One owner span per managed view queued in a frame. */
static sat_scene3d_owner_span_t g_owner_spans[1];
static sat_view_cache_t g_cache;
static sat_view_cache_item_t g_cache_items[VIEW_COUNT * CACHE_FACES];
static uint16_t g_cache_counts[VIEW_COUNT];
static sat_view_cache_camera_t g_cache_cameras[VIEW_COUNT];
static sat_camera3d_t g_camera;
static sat_ascii_font_t g_font;
static sat_texture_t g_checker;
static uint8_t g_checker_pixels[8u * 8u];
static uint16_t g_checker_palette[256u];
static sat_quad3_t g_static_world[CACHE_FACES];
static uint16_t g_frame;
static uint16_t g_view;
static uint8_t g_zoom;

static sat_quad3_t make_front_quad(
    sat_fx16_t cx,sat_fx16_t half_x,sat_fx16_t half_y,sat_fx16_t z) {
    sat_quad3_t q={0};
    q.v[0]=(sat_vec3_t){cx-half_x,half_y,z};
    q.v[1]=(sat_vec3_t){cx+half_x,half_y,z};
    q.v[2]=(sat_vec3_t){cx+half_x,-half_y,z};
    q.v[3]=(sat_vec3_t){cx-half_x,-half_y,z};
    return q;
}

static void update_camera(void) {
    /* Exact per-view fingerprint includes BOTH these pose changes and VP. */
    g_camera.eye.x=g_view ? 2*SAT_FX16_ONE : 0;
    g_camera.eye.z=g_zoom ? 12*SAT_FX16_ONE : 10*SAT_FX16_ONE;
    sat_example_must(sat_camera3d_update(&g_camera));
}

static void initialize_checker(void) {
    for (uint16_t y=0u;y<8u;++y)
        for (uint16_t x=0u;x<8u;++x)
            g_checker_pixels[y*8u+x]=
                ((x>>1u)^(y>>1u))&1u ? 1u : 2u;
    g_checker_palette[0]=SAT_COLOR_BLACK;
    g_checker_palette[1]=SAT_RGB555(25,29,6);
    g_checker_palette[2]=SAT_RGB555(5,19,30);
    sat_surface_t surface={0};
    sat_example_must(sat_surface_init(
        &surface,g_checker_pixels,8u,8u,8u,SAT_PIXEL_INDEX8,
        g_checker_palette,256u));
    sat_example_must(sat_texture_create_from_surface(
        &g_checker,&surface,SAT_TEXTURE_UPLOAD_ONLY));
}

static void bake_current_view(void) {
    const sat_view_cache_item_t* items=0;
    uint16_t count=0u;
    sat_result_t st=sat_view_cache_view_camera(
        &g_cache,g_view,&g_camera,NEAR_DEPTH,SCREEN_W,SCREEN_H,
        &items,&count);
    if (st==SAT_OK) return;
    sat_example_must(st==SAT_ERR_NOT_FOUND?SAT_OK:st);

    /* Projection and source geometry are computed ONCE per camera state,
     * then stored in caller-owned cache. Changing the camera makes the
     * matching view return NOT_FOUND, never stale native VDP1 coordinates. */
    sat_example_must(sat_view_cache_begin_camera(
        &g_cache,g_view,&g_camera,NEAR_DEPTH,SCREEN_W,SCREEN_H));
    for (uint16_t i=0u;i<CACHE_FACES;++i)
        sat_example_must(sat_view_cache_append_world(
            &g_cache,&g_static_world[i],0u,i));
    sat_example_must(sat_view_cache_sort(&g_cache));
}

static void draw_cached_static(void) {
    /* The logical owner is revalidated when the painter flushes. A recycled
     * texture slot cannot silently render a different wall image. */
    sat_example_must(sat_scene_queue_managed_camera_view(
        &g_scene,&g_cache,g_view,&g_camera,g_checker,0u));
}

static void draw_actor(void) {
    /* Crossing the static near wall changes WHO occludes whom. Never force
     * the actor in front by submitting it after cached drawing. */
    const sat_fx16_t z=((g_frame/80u)&1u)
        ? 2*SAT_FX16_ONE : -2*SAT_FX16_ONE;
    const sat_fx16_t x=SAT_FX16_ONE/2;
    const sat_quad3_t actor=make_front_quad(
        x,SAT_FX16_ONE/2,SAT_FX16_ONE,z);
    const sat_scene3d_material_t actor_material={
        SAT_SCENE3D_RGB,SAT_RGB555(30,6,9),0,0,
        SAT_INDEXED_SOLID_OPAQUE,0};
    sat_example_must(sat_scene_submit_quad(
        &g_scene,&actor,&actor_material,0u));
}

static void draw_hud(void) {
    sat_example_must(sat_vdp1_overlay_begin());
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font,"CACHED TEXTURE + MOVING RGB",6,5,8,0u,0u));
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font,"A CAMERA  B ZOOM  START EXIT",6,17,8,0u,0u));
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font,"NEAR WALL HIDES RED WHEN BEHIND",6,202,8,0u,0u));
}

int main(void) {
    const sat_video_config_t video={SCREEN_W,SCREEN_H,1u,0u};
    const sat_vec3_t eye={0,0,10*SAT_FX16_ONE};
    const sat_vec3_t target={0,0,0};
    const sat_vec3_t up={0,SAT_FX16_ONE,0};
    sat_example_must(sat_init(&video));
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font,SAT_COLOR_WHITE,SAT_COLOR_BLACK,0u));
    initialize_checker();
    sat_example_must(sat_scene_init(
        &g_scene,g_scene_faces,g_scene_keys,g_scene_order,SCENE_FACES));
    sat_example_must(sat_scene_bind_managed_textures(
        &g_scene,g_owner_spans,1u));
    sat_example_must(sat_view_cache_init(
        &g_cache,g_cache_items,g_cache_counts,VIEW_COUNT,CACHE_FACES));
    sat_example_must(sat_view_cache_bind_cameras(
        &g_cache,g_cache_cameras,VIEW_COUNT));
    sat_example_must(sat_camera3d_init(
        &g_camera,&eye,&target,&up,55*SAT_FX16_ONE,
        sat_fx16_div(SCREEN_W*SAT_FX16_ONE,SCREEN_H*SAT_FX16_ONE),
        NEAR_DEPTH,100*SAT_FX16_ONE));

    /* Wide textured wall in back; narrower wall in front of the actor. */
    g_static_world[0]=make_front_quad(
        -SAT_FX16_ONE/2,2*SAT_FX16_ONE,
        (3*SAT_FX16_ONE)/2,-SAT_FX16_ONE);
    g_static_world[1]=make_front_quad(
        SAT_FX16_ONE,3*SAT_FX16_ONE/4,
        SAT_FX16_ONE,3*SAT_FX16_ONE/2);

    for (;;) {
        sat_pad_state_t pad={0};
        sat_example_must(sat_app_frame_begin(
            SAT_COLOR_BLACK,SAT_COLOR_BLACK,&pad));
        if ((pad.pressed & SAT_PAD_START)!=0u) break;
        if ((pad.pressed & SAT_PAD_A)!=0u) g_view^=1u;
        if ((pad.pressed & SAT_PAD_B)!=0u) g_zoom^=1u;
        update_camera();
        bake_current_view();
        sat_example_must(sat_scene_begin(
            &g_scene,&g_camera,NEAR_DEPTH,SCREEN_W,SCREEN_H,
            OVERLAY_COMMANDS));
        draw_cached_static();
        draw_actor();
        sat_example_must(sat_scene_flush(&g_scene));
        draw_hud();
        sat_example_must(sat_app_frame_end());
        ++g_frame;
    }
    sat_example_must(sat_app_frame_end());
    sat_example_must(sat_shutdown());
    return 0;
}
