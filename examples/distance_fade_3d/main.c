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
 *       -> sat_draw_sprite_distorted_color_calc()
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
#include "saturn/render3d.h"
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

static sat_mat4_t g_view_proj;
static sat_fx16_t g_camera_z;
static uint8_t g_auto_camera = 1u;
static uint8_t g_show_hud = 1u;
static demo_mode_t g_mode = DEMO_FADE_8;
static uint16_t g_visible_count;
static uint16_t g_culled_count;
static uint8_t g_last_slot;

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
    sat_mat4_t view;
    sat_mat4_t proj;

    sat_example_must(sat_mat4_look_at(&view, &eye, &target, &up));
    sat_example_must(sat_mat4_perspective(
        &proj,
        FX(60),
        sat_fx16_div(FX(SCREEN_W), FX(SCREEN_H)),
        FX(1),
        FX(320)));
    sat_example_must(sat_mat4_multiply(&g_view_proj, &proj, &view));
}

static void update_input(const sat_pad_state_t* pad) {
    if ((pad->pressed & SAT_PAD_A) != 0u) {
        g_mode = (demo_mode_t)(((uint8_t)g_mode + 1u) % (uint8_t)DEMO_MODE_COUNT);
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

static uint8_t slot_for_level(uint8_t level) {
    if (g_mode == DEMO_FADE_4) {
        return (uint8_t)(level * 2u); /* 0,2,4,6 */
    }
    return level; /* 0..7 */
}

static void draw_object(uint16_t index) {
    const sat_fx16_t x = FX(kObjectX[index]);
    const sat_fx16_t z = FX(kObjectZ[index]);
    const sat_vec3_t center = {x, FX(5), z};
    sat_projected_vertex_t projected_center;
    sat_quad3_t world;
    sat_quad2_t projected;
    sat_fade3d_result_t fade_result = {};
    sat_fade3d_t fade;
    sat_result_t st;
    uint8_t use_color_calc = 0u;
    uint8_t slot = 0u;

    st = sat_project_vertices(&g_view_proj, &center, 1u, &projected_center);
    if (st != SAT_OK || projected_center.w <= 0) {
        ++g_culled_count;
        return;
    }

    if (g_mode == DEMO_HARD_CUT) {
        if (projected_center.w >= FADE_END) {
            ++g_culled_count;
            return;
        }
    } else if (g_mode == DEMO_FADE_4 || g_mode == DEMO_FADE_8) {
        fade.start = FADE_START;
        fade.end = FADE_END;
        fade.levels = (g_mode == DEMO_FADE_4) ? 4u : 8u;
        fade.flags = SAT_FADE3D_CULL_AFTER_END;
        fade.reserved = 0u;
        st = sat_fade3d_eval(&fade, projected_center.w, &fade_result);
        if (st != SAT_OK || fade_result.culled != 0u) {
            ++g_culled_count;
            return;
        }
        if (projected_center.w > FADE_START) {
            slot = slot_for_level(fade_result.level);
            use_color_calc = 1u;
            g_last_slot = slot;
        }
    }

    sat_quad3_billboard(&world, x, z, SAT_FX16_ONE, 0, FX(5), FX(10));
    st = sat_project_quad(&g_view_proj, &world, &projected);
    if (st != SAT_OK) {
        ++g_culled_count;
        return;
    }

    if (use_color_calc != 0u) {
        sat_distorted_sprite_cmd_t cmd = {};
        int i;
        for (i = 0; i < 4; ++i) {
            cmd.x[i] = projected.x[i];
            cmd.y[i] = projected.y[i];
        }
        cmd.texture = &g_object_texture;
        cmd.palette_override = 0u;
        cmd.flags = 0u;
        st = sat_draw_sprite_distorted_color_calc(&cmd, slot);
    } else {
        st = sat_draw_quad2_sprite(&projected, &g_object_texture, 0u, 0u);
    }

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
    sat_example_must(sat_vdp1_set_erase_transparent());
    init_color_calc();

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

        /* Farthest first: VDP1 has no depth buffer. */
        for (i = (int)OBJECT_COUNT - 1; i >= 0; --i) {
            draw_object((uint16_t)i);
        }
        draw_hud();

        sat_example_must(sat_end_frame());
    }

    return 0;
}
