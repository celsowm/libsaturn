/* vdp2_nbg0_rbg0_combo.c - NBG0 sky + RBG0 Mode-7 ground acceptance test.
 *
 * The upper part of the frame shows a seamless sky panorama on NBG0
 * (examples/vdp2_rbg0_ground/assets/sky.png, 512 px wide = one full plane, so
 * it wraps forever while it scrolls), the lower part a green RBG0 perspective
 * floor, and a white VDP1 marker must remain visible on top.
 *
 * D-Pad UP/DOWN walks forward/back, LEFT/RIGHT strafes. The sky follows the
 * strafe at 1/4 speed (parallax) and its clouds drift slowly. START exits.  If the upper part is black while the floor is visible, the failure
 * is in NBG0/RBG0 composition rather than in an image converter or asset.
 */
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"
#include "examples/vdp2_rbg0_ground/rbg0_math.h"
#include "vdp2_nbg0_rbg0_combo/sky.h"

#define SCREEN_WIDTH  320u
#define SCREEN_HEIGHT 224u
#define HORIZON       96u
#define FOCAL         96u
#define MIN_DEPTH     8u
#define GROUND_FORWARD 96u

#define SKY_IMG_W 512u
#define SKY_IMG_H 128u
#define SKY_PARALLAX_SHIFT 2u   /* horizon moves 1/4 as fast as the ground */
#define SKY_DRIFT_SHIFT    3u   /* clouds drift 1 px every 8 frames */

#define FX16_SHIFT 16
#define WALK_SPEED ((int32_t)2 << FX16_SHIFT)   /* texels per frame */

#define BM_BASE_WORD    0x00000u
#define RP_BASE_WORD    0x10000u
#define COEF_BASE_WORD  0x12000u

static uint16_t g_map_scratch[SAT_VDP2_NBG0_MAP_CELLS];
static uint16_t g_ground_palette[256];

static const rbg0_ground_config_t g_ground_cfg = {
    512u,
    256u,
    SCREEN_WIDTH / 2u,
    HORIZON,
    FOCAL,
    MIN_DEPTH,
    GROUND_FORWARD,
    COEF_BASE_WORD,
};

static void build_palettes(void) {
    uint16_t i;
    for (i = 0u; i < 256u; ++i) {
        uint16_t v = (uint16_t)(i & 31u);
        g_ground_palette[i] = SAT_BGR555((uint16_t)(v / 3u), v, (uint16_t)(v / 4u));
    }
}

static void build_ground_bitmap(void) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    uint32_t off = BM_BASE_WORD;
    uint32_t y;

    for (y = 0u; y < 256u; ++y) {
        uint32_t x;
        for (x = 0u; x < 512u; x += 2u) {
            uint8_t a = (uint8_t)(32u + (((x >> 4u) ^ (y >> 4u)) & 31u));
            uint8_t b = (uint8_t)(32u + ((((x + 1u) >> 4u) ^ (y >> 4u)) & 31u));
            vram[off++] = (uint16_t)(((uint16_t)a << 8u) | b);
        }
    }
}

static void write_coefficients(void) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    uint32_t y;

    for (y = 0u; y < SCREEN_HEIGHT; ++y) {
        uint16_t w0;
        uint16_t w1;
        rbg0_ground_encode_coefficient(&g_ground_cfg, y, &w0, &w1);
        vram[COEF_BASE_WORD + (y * 2u)] = w0;
        vram[COEF_BASE_WORD + (y * 2u) + 1u] = w1;
    }
}

static void write_rotation_params(void) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    uint16_t params[48];
    uint32_t i;

    rbg0_ground_build_params(&g_ground_cfg, 0, 0, params);
    for (i = 0u; i < 48u; ++i) {
        vram[RP_BASE_WORD + i] = params[i];
    }
}

/* Only Mx/My change while walking, so rewrite just those words. */
static void write_camera_translation(int32_t cam_xi, int32_t cam_yi) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    int32_t mx = rbg0_ground_wrap_translation(cam_xi, g_ground_cfg.bitmap_width, g_ground_cfg.cx);
    int32_t my = rbg0_ground_wrap_translation(cam_yi, g_ground_cfg.bitmap_height, g_ground_cfg.horizon);

    vram[RP_BASE_WORD + 34u] = (uint16_t)((uint32_t)mx & 0x1FFFu);
    vram[RP_BASE_WORD + 35u] = 0x0000u;
    vram[RP_BASE_WORD + 36u] = (uint16_t)((uint32_t)my & 0x1FFFu);
    vram[RP_BASE_WORD + 37u] = 0x0000u;
}

static void init_layers(void) {
    const sat_vdp2_nbg0_config_t sky = {
        SAT_VDP2_CHAR_SIZE_1X1,
        SAT_VDP2_COLOR_MODE_256,
        0x3Bu,
        0u,
        0u,
    };
    const sat_vdp2_rbg0_mode7_config_t ground = {
        SAT_VDP2_RBG0_BITMAP_512x256,
        SAT_VDP2_COLOR_MODE_256,
        BM_BASE_WORD,
        RP_BASE_WORD,
        SAT_COLOR_BLACK,
        5u,
        7u,
    };

    build_palettes();
    if (sky_asset.width != SKY_IMG_W || sky_asset.height != SKY_IMG_H) {
        sat_example_must(SAT_ERR_INVALID_ARG);
    }
    build_ground_bitmap();

    sat_example_must(sat_vdp2_palette_upload(g_ground_palette, 256u, 0u));
    sat_example_must(sat_vdp2_palette_upload(
        sky_asset.palette, (uint16_t)sky_asset.palette_count, 256u));

    sat_example_must(sat_vdp2_nbg0_init(&sky));
    sat_example_must(sat_vdp2_nbg0_upload_indexed8(
        sky_asset.pixels, SKY_IMG_W, SKY_IMG_H, 1u, g_map_scratch));

    write_coefficients();
    write_rotation_params();
    sat_example_must(sat_vdp2_rbg0_mode7_init(&ground));

    sat_example_must(sat_vdp2_nbg0_set_priority(2u));
    sat_example_must(sat_vdp2_rbg0_set_priority(5u));
    sat_example_must(sat_vdp2_sprite_set_priority(7u));
}

int main(void) {
    const sat_video_config_t video = {320u, 224u, 1u, 0u};
    sat_vdp2_scroll_t sky_scroll = {0u, 0u, (uint16_t)(SKY_IMG_H - HORIZON), 0u};
    uint32_t frame = 0u;
    int32_t cam_x = 0;
    int32_t cam_y = 0;
    int32_t last_xi = 0;
    int32_t last_yi = 0;

    sat_example_must(sat_init(&video));
    init_layers();
    sat_example_must(sat_vdp1_set_erase_transparent());

    for (;;) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_pad_poll(&pad));
        if ((pad.pressed & SAT_PAD_START) != 0u) break;

        if ((pad.held & SAT_PAD_LEFT) != 0u)  cam_x -= WALK_SPEED;
        if ((pad.held & SAT_PAD_RIGHT) != 0u) cam_x += WALK_SPEED;
        /* Larger texture Y is farther away, so walking forward grows cam_y. */
        if ((pad.held & SAT_PAD_UP) != 0u)    cam_y += WALK_SPEED;
        if ((pad.held & SAT_PAD_DOWN) != 0u)  cam_y -= WALK_SPEED;

        {
            const int32_t xi = cam_x >> FX16_SHIFT;
            const int32_t yi = cam_y >> FX16_SHIFT;
            if (xi != last_xi || yi != last_yi) {
                write_camera_translation(xi, yi);
                last_xi = xi;
                last_yi = yi;
            }
        }

        sky_scroll.x_integer = (uint16_t)(((uint32_t)(cam_x >> (FX16_SHIFT + SKY_PARALLAX_SHIFT)) +
                                           (frame >> SKY_DRIFT_SHIFT)) & (SKY_IMG_W - 1u));
        sat_example_must(sat_vdp2_nbg0_set_scroll(&sky_scroll));
        sat_example_must(sat_vdp2_layers_commit());

        sat_example_must(sat_begin_frame());
        sat_example_must(sat_draw_rect_screen(152, 12, 16u, 6u, SAT_RGB555(31u, 31u, 31u)));
        sat_example_must(sat_draw_rect_screen(157, 7, 6u, 16u, SAT_RGB555(31u, 31u, 31u)));
        sat_example_must(sat_end_frame());

        ++frame;
    }

    return 0;
}
