/* distance_fade_3d - Saturn-native stepped distance fade showcase.
 *
 * This example intentionally has no external assets. A small indexed8 VDP1
 * texture and a tiled NBG0 background are generated at startup so the ISO is
 * useful as a hardware/emulator regression target by itself.
 *
 * The important path is:
 *
 *   object view depth
 *       -> sat_fade3d_eval()
 *       -> VDP2 color-calc slot 0..7
 *       -> sat_scene_t textured material with the selected color-calc slot
 *       -> VDP1 Type-0 pixel carries selector bits
 *       -> VDP2 blends it with the NBG0 scene
 *
 * Controls:
 *   A          cycle NO FADE / HARD CUT / FADE 4 / FADE 8
 *   B          toggle automatic camera motion
 *   C          reset camera
 *   UP/DOWN    move camera forward/back
 *   START      toggle HUD
 */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/fade3d.h"
#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/input.h"
#include "saturn/math3d.h"
#include "saturn/scene.h"
#include "saturn/vdp1.h"
#include "saturn/vdp1_color_calc.h"
#include "saturn/vdp2.h"
#include "saturn/vdp2_color_calc.h"
#include "saturn/video.h"
#include "saturn/example_util.h"

#define SCREEN_W 320
#define SCREEN_H 224
#define FX(v) ((sat_fx16_t)((v) * 65536))

#define BG_W 64u
#define BG_H 64u
#define OBJECT_TEX_W 16u
#define OBJECT_TEX_H 16u
#define OBJECT_COUNT 12u
#define SCENE_FACE_CAPACITY 16u

#define FADE_START FX(100)
#define FADE_END   FX(220)
#define CAMERA_MIN_Z FX(0)
#define CAMERA_MAX_Z FX(80)
#define CAMERA_STEP (FX(1) / 2)

#define BG_PALETTE 0u
#define OBJECT_PALETTE 1u
#define FONT_PALETTE 2u

/* Keep the background low enough that selector-1 sprites (priority 5 when
 * normal sprites are priority 6) still have an image underneath to blend. */
#define SPRITE_PRIORITY 6u
#define BG_PRIORITY 1u

typedef enum demo_mode {
    DEMO_NO_FADE = 0,
    DEMO_HARD_CUT = 1,
    DEMO_FADE_4 = 2,
    DEMO_FADE_8 = 3,
    DEMO_MODE_COUNT = 4
} demo_mode_t;

static uint8_t g_bg_pixels[BG_W * BG_H];
static uint16_t g_bg_palette[256];
static uint16_t g_map_scratch[SAT_VDP2_NBG0_MAP_CELLS];

static uint8_t g_object_pixels[OBJECT_TEX_W * OBJECT_TEX_H];
static uint16_t g_object_palette[256];
static sat_vdp1_texture_t g_object_texture;
static sat_ascii_font_t g_font;

static sat_camera3d_t g_camera;
static sat_scene_t g_scene;
static sat_scene3d_face_t g_scene_faces[SCENE_FACE_CAPACITY];
static uint32_t g_scene_keys[SCENE_FACE_CAPACITY];
static uint16_t g_scene_order[SCENE_FACE_CAPACITY];
static sat_fx16_t g_camera_z;
static uint8_t g_auto_camera = 1u;
static uint8_t g_show_hud = 1u;
static demo_mode_t g_mode = DEMO_FADE_8;
static uint16_t g_visible_count;
static uint16_t g_culled_count;
static uint8_t g_last_slot;
/* One byte per object, remembering the slot it was last drawn at. This is
 * what sat_fade3d_slot needs to keep an object from flickering between two
 * slots while the auto-camera drifts across a transition. */
static uint8_t g_object_fade[OBJECT_COUNT];

static const int16_t kObjectX[OBJECT_COUNT] = {
    -12, 0, 12, -8, 8, -12, 0, 12, -8, 8, -12, 12
};

static const int16_t kObjectZ[OBJECT_COUNT] = {
    45, 65, 85, 105, 125, 145, 165, 185, 205, 225, 245, 265
};

static void build_procedural_assets(void) {
    uint32_t x;
    uint32_t y;
    uint16_t i;

    for (i = 0u; i < 256u; ++i) {
        g_bg_palette[i] = SAT_RGB555(0, 0, 0);
        g_object_palette[i] = SAT_RGB555(0, 0, 0);
    }

    /* Deliberately high-contrast second image: the blend is obvious at every
     * ratio, including on emulators with slightly different analog output. */
    g_bg_palette[1] = SAT_RGB555(4, 8, 20);
    g_bg_palette[2] = SAT_RGB555(6, 14, 27);
    g_bg_palette[3] = SAT_RGB555(10, 7, 18);
    g_bg_palette[4] = SAT_RGB555(3, 18, 18);

    for (y = 0u; y < BG_H; ++y) {
        for (x = 0u; x < BG_W; ++x) {
            const uint32_t tile = ((x >> 3u) + (y >> 3u)) & 3u;
            g_bg_pixels[(y * BG_W) + x] = (uint8_t)(1u + tile);
        }
    }

    g_object_palette[1] = SAT_RGB555(31, 28, 5);
    g_object_palette[2] = SAT_RGB555(31, 31, 31);
    g_object_palette[3] = SAT_RGB555(5, 24, 31);
    g_object_palette[4] = SAT_RGB555(31, 6, 8);

    for (y = 0u; y < OBJECT_TEX_H; ++y) {
        for (x = 0u; x < OBJECT_TEX_W; ++x) {
            uint8_t c = 1u;
            if (x == 0u || y == 0u || x == OBJECT_TEX_W - 1u || y == OBJECT_TEX_H - 1u) {
                c = 2u;
            } else if (((x >> 2u) ^ (y >> 2u)) & 1u) {
                c = 3u;
            } else if (x > 5u && x < 10u && y > 5u && y < 10u) {
                c = 4u;
            }
            g_object_pixels[(y * OBJECT_TEX_W) + x] = c;
        }
    }
}

static void init_background(void) {
    sat_vdp2_nbg0_config_t bg = {
        SAT_VDP2_CHAR_SIZE_1X1,
        SAT_VDP2_COLOR_MODE_256,
        0x3Bu,
        0u,
        0u
    };

    sat_example_must(sat_vdp2_nbg0_init(&bg));
    sat_example_must(sat_vdp2_palette_upload(g_bg_palette, 256u, BG_PALETTE * 256u));
    sat_example_must(sat_vdp2_nbg0_upload_indexed8(
        g_bg_pixels, BG_W, BG_H, BG_PALETTE, g_map_scratch));
    sat_example_must(sat_vdp2_nbg0_set_priority(BG_PRIORITY));
}

static void init_color_calc(void) {
    sat_vdp2_sprite_color_calc_config_t config = {};
    static const uint8_t ratios[8] = {0u, 4u, 8u, 12u, 16u, 20u, 24u, 28u};
    uint8_t i;

    config.enabled = 1u;
    config.normal_priority = SPRITE_PRIORITY;
    for (i = 0u; i < 8u; ++i) {
        config.ratio[i] = ratios[i];
    }
    sat_example_must(sat_vdp2_sprite_color_calc_configure(&config));
}

static void compute_camera(void) {
    sat_vec3_t eye = {0, FX(5), g_camera_z};
    sat_vec3_t target = {0, FX(5), g_camera_z + FX(100)};
    sat_vec3_t up = {0, SAT_FX16_ONE, 0};
    sat_example_must(sat_camera3d_init(
        &g_camera, &eye, &target, &up, FX(60),
        sat_fx16_div(FX(SCREEN_W), FX(SCREEN_H)), FX(1), FX(320)));
}

static void update_input(const sat_pad_state_t* pad) {
    if ((pad->pressed & SAT_PAD_A) != 0u) {
        g_mode = (demo_mode_t)(((uint8_t)g_mode + 1u) % (uint8_t)DEMO_MODE_COUNT);
        /* The slot stride changes with the mode, so a slot remembered under
         * the previous mapping no longer means anything. */
        for (uint16_t o = 0u; o < OBJECT_COUNT; ++o) {
            g_object_fade[o] = SAT_INDEXED_SOLID_OPAQUE;
        }
    }
    if ((pad->pressed & SAT_PAD_B) != 0u) {
        g_auto_camera = (uint8_t)!g_auto_camera;
    }
    if ((pad->pressed & SAT_PAD_C) != 0u) {
        g_camera_z = CAMERA_MIN_Z;
    }
    if ((pad->pressed & SAT_PAD_START) != 0u) {
        g_show_hud = (uint8_t)!g_show_hud;
    }

    if ((pad->held & SAT_PAD_UP) != 0u) {
        g_camera_z += CAMERA_STEP;
    }
    if ((pad->held & SAT_PAD_DOWN) != 0u) {
        g_camera_z -= CAMERA_STEP;
    }
    if (g_auto_camera) {
        g_camera_z += FX(1) / 8;
    }

    if (g_camera_z > CAMERA_MAX_Z) {
        g_camera_z = CAMERA_MIN_Z;
    }
    if (g_camera_z < CAMERA_MIN_Z) {
        g_camera_z = CAMERA_MIN_Z;
    }
}

/* Four levels are spread across the eight hardware slots (0,2,4,6) so the
 * coarser mode still reaches full transparency; eight levels map one to one.
 * The stride, the quantization and the anti-flicker band are all the
 * library's, so this only describes the demo's intent. */
static sat_fade3d_slots_t fade_config(void) {
    sat_fade3d_slots_t cfg = {};
    cfg.policy.start = FADE_START;
    cfg.policy.end = FADE_END;
    cfg.policy.levels = (g_mode == DEMO_FADE_4) ? 4u : 8u;
    cfg.policy.flags = SAT_FADE3D_CULL_AFTER_END;
    cfg.hysteresis = FX(4);
    cfg.base_slot = 0u;
    cfg.slot_stride = (g_mode == DEMO_FADE_4) ? 2u : 1u;
    cfg.opaque_before_start = 1u;
    return cfg;
}

static void draw_object(uint16_t index) {
    const sat_fx16_t x = FX(kObjectX[index]);
    const sat_fx16_t z = FX(kObjectZ[index]);
    const sat_vec3_t center = {x, FX(5), z};
    sat_quad3_t world;
    sat_scene3d_material_t material = {};
    sat_fx16_t depth;
    sat_result_t st;
    uint8_t use_color_calc = 0u;
    uint8_t slot = 0u;

    st = sat_scene_depth(&g_scene, &center, &depth);
    if (st != SAT_OK || depth <= 0) {
        ++g_culled_count;
        return;
    }

    if (g_mode == DEMO_HARD_CUT) {
        if (depth >= FADE_END) {
            ++g_culled_count;
            return;
        }
    } else if (g_mode == DEMO_FADE_4 || g_mode == DEMO_FADE_8) {
        const sat_fade3d_slots_t cfg = fade_config();
        uint8_t resolved = SAT_INDEXED_SOLID_OPAQUE;
        st = sat_fade3d_slot(&cfg, depth,
                             &g_object_fade[index], &resolved);
        if (st != SAT_OK || resolved == SAT_FADE3D_SLOT_CULLED) {
            ++g_culled_count;
            return;
        }
        if (resolved != SAT_INDEXED_SOLID_OPAQUE) {
            slot = resolved;
            use_color_calc = 1u;
            g_last_slot = slot;
        }
    }

    sat_quad3_billboard(&world, x, z, SAT_FX16_ONE, 0, FX(5), FX(10));
    material.kind = SAT_SCENE3D_INDEXED_TEXTURED;
    material.texture = &g_object_texture;
    material.color_calc_slot = use_color_calc != 0u
        ? slot : SAT_INDEXED_SOLID_OPAQUE;
    st = sat_scene_submit_quad(&g_scene, &world, &material, 0u);

    if (st == SAT_OK) {
        ++g_visible_count;
    }
}

static void draw_text(const char* text, int x, int y) {
    (void)sat_ascii_font_draw_text_screen_indexed8(&g_font, text, x, y, 8, 0u, 0u);
}

static const char* mode_name(void) {
    switch (g_mode) {
    case DEMO_NO_FADE: return "NO FADE";
    case DEMO_HARD_CUT: return "HARD CUT";
    case DEMO_FADE_4: return "FADE 4";
    case DEMO_FADE_8: return "FADE 8";
    default: return "?";
    }
}

static void draw_hud(void) {
    char line[32];

    if (!g_show_hud) {
        return;
    }

    draw_text("DISTANCE FADE 3D", 4, 2);
    draw_text(mode_name(), 4, 12);
    draw_text("A MODE  B AUTO  C RESET", 4, 22);
    draw_text("UP/DOWN CAMERA  START HUD", 4, 32);

    if (sat_fmt_label_u32("CAM Z ", (uint32_t)sat_fx16_to_int(g_camera_z),
                          line, sizeof(line), 0) == SAT_OK) {
        draw_text(line, 4, 190);
    }
    if (sat_fmt_label_u32("VISIBLE ", g_visible_count,
                          line, sizeof(line), 0) == SAT_OK) {
        draw_text(line, 100, 190);
    }
    if (sat_fmt_label_u32("CULLED ", g_culled_count,
                          line, sizeof(line), 0) == SAT_OK) {
        draw_text(line, 200, 190);
    }
    if (g_mode == DEMO_FADE_4 || g_mode == DEMO_FADE_8) {
        if (sat_fmt_label_u32("LAST SLOT ", g_last_slot,
                              line, sizeof(line), 0) == SAT_OK) {
            draw_text(line, 4, 202);
        }
    }
    if (g_auto_camera) {
        draw_text("AUTO", 180, 202);
    }
}

int main(void) {
    sat_video_config_t video = {SCREEN_W, SCREEN_H, 1u, 0u};

    sat_example_must(sat_init(&video));
    build_procedural_assets();
    init_background();

    sat_example_must(sat_tex_upload_indexed8(
        &g_object_texture,
        g_object_pixels,
        OBJECT_TEX_W,
        OBJECT_TEX_H,
        g_object_palette,
        OBJECT_PALETTE));
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, FONT_PALETTE));
    sat_example_must(sat_scene_init(
        &g_scene, g_scene_faces, g_scene_keys, g_scene_order,
        SCENE_FACE_CAPACITY));
    sat_example_must(sat_vdp1_set_erase_transparent());
    init_color_calc();
    /* Every object starts opaque; sat_fade3d_slot reads this back as the
     * previous slot when deciding whether a transition has been crossed. */
    for (uint16_t o = 0u; o < OBJECT_COUNT; ++o) {
        g_object_fade[o] = SAT_INDEXED_SOLID_OPAQUE;
    }

    for (;;) {
        sat_pad_state_t pad = {0};
        sat_vdp2_scroll_t scroll = {0};
        int i;

        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_pad_poll(&pad));
        update_input(&pad);

        /* Move the second image slightly with the camera so the blend cannot
         * be mistaken for simple palette darkening. */
        scroll.x_integer = (uint16_t)((uint32_t)(g_camera_z >> 18) & 0x07FFu);
        scroll.y_integer = (uint16_t)((uint32_t)(g_camera_z >> 19) & 0x07FFu);
        sat_example_must(sat_vdp2_nbg0_set_scroll(&scroll));
        sat_example_must(sat_vdp2_layers_commit());
        /* The generic layer shadow now preserves both color-calc sprite
         * priorities. Recommitting the color-calc registers later in the
         * frame could miss VBlank and produce alternating blend frames. */

        sat_example_must(sat_vdp1_set_erase_transparent());
        sat_example_must(sat_begin_frame());
        compute_camera();

        g_visible_count = 0u;
        g_culled_count = 0u;
        g_last_slot = 0u;

        sat_example_must(sat_scene_begin(
            &g_scene, &g_camera, FX(1), SCREEN_W, SCREEN_H, 64u));
        for (i = (int)OBJECT_COUNT - 1; i >= 0; --i) {
            draw_object((uint16_t)i);
        }
        sat_example_must(sat_scene_flush(&g_scene));
        draw_hud();

        sat_example_must(sat_end_frame());
    }

    return 0;
}
