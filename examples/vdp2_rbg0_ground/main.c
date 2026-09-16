/* vdp2_rbg0_ground.c - Infinite floor plane using VDP2 RBG0.
 *
 * Sky in top half: RBG0's sky scanlines are transparent, so a procedural
 * 360-degree panorama on NBG0 (gradient, clouds, two mountain ridges) shows
 * through. It is 512 px wide -- exactly the NBG0 plane width -- so it wraps
 * seamlessly and scrolls with the camera for an endless horizon. Floor in
 * bottom half rendered through a per-scanline coefficient table with KMD=0
 * (k applied to both kx and ky), giving classic Mode-7 depth.
 *
 * D-Pad UP/DOWN walks forward/back, LEFT/RIGHT strafes. START exits.
 * Uses the same floor.tga texture as vdp2_nbg0_image, tiled into the RBG0
 * bitmap so the plane repeats while walking.
 *
 * VRAM layout:
 *   A0 (0x00000..0x0FFFF words): RBG0 bitmap, 512x256 8bpp
 *   A1 (0x10000..0x1002F words): rotation parameter A table (0x60 bytes = 48 words)
 *   A1 (0x12000..0x121BF words): coefficient table (224 lines x 2 words)
 *   A1 (0x3FFFF): BACK screen color
 *   B1: NBG0 sky character data + map (placed by sat_vdp2_nbg0_upload_indexed8)
 */
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"
#include "saturn/font.h"
#include "saturn/color.h"
#include "vdp2_rbg0_ground/bg.h"
#include "rbg0_math.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 224

#define BITMAP_WIDTH  512
#define BITMAP_HEIGHT 256

/* Mode-7 layout */
#define HORIZON       96u                               /* y row of vanishing point */
#define CX            (SCREEN_WIDTH / 2)                /* 160 */
#define CY            HORIZON
#define FOCAL         96u
#define MIN_DEPTH     8u

/* Screen-space distance projected onto the ground plane, i.e. the effective
 * focal length of the Y axis. MUST be non-zero: the hardware computes
 * Ysp = E * (Yst - Py), so Yst == Py collapses every scanline onto a single
 * texture row (only X gets scaled by k, producing radial streaks).
 * Equal to FOCAL keeps the plane isotropic: one world unit sideways covers the
 * same screen area as one unit forward. Lower values stretch the ground in
 * depth (a "longer" look), higher values flatten it.
 */
#define GROUND_FORWARD 96u

#define BM_BASE_WORD   0x00000u
#define RP_BASE_WORD   0x10000u
#define COEF_BASE_WORD 0x12000u
#define BACK_COLOR_WORD 0x3FFFFu

#define SKY_COLOR     0x7F45u

#define FX16_SHIFT 16

/* Palette index 0 is the RBG0 ground bitmap; the pad-debug text uses its own
 * bank so uploading the font never disturbs bg_asset's palette.
 */
#define PAD_DEBUG_PALETTE 1u

/* Sky panorama. SKY_W must equal the 512 px NBG0 plane so the image repeats
 * with no seam; its bottom row lands on the horizon scanline. */
#define SKY_W           512u
#define SKY_H           128u
#define SKY_PALETTE     2u
#define SKY_PARALLAX_SHIFT 2u   /* horizon moves 1/4 as fast as the ground */

#define SKY_GRAD_BASE   1u      /* 32 shades, zenith -> horizon */
#define SKY_CLOUD_BASE  33u     /* 16 shades */
#define SKY_FAR_BASE    49u     /* 16 shades, hazy far ridge */
#define SKY_NEAR_BASE   65u     /* 16 shades, darker near ridge */
#define SKY_SNOW        81u

/* HUD font: stock 8x8 scaled 2x (16x16) for readability in small emulator
 * windows. Bright yellow contrasts with both the blue sky and the tan ground.
 */
#define PAD_DEBUG_FONT_PX 16u

static sat_result_t init_pad_debug_font_2x(sat_ascii_font_t* font) {
    return sat_ascii_font_init_scaled_indexed8(
        font,
        SAT_COLOR_YELLOW,
        0x0000u,
        PAD_DEBUG_PALETTE,
        2u
    );
}

static uint8_t g_sky_pixels[SKY_W * SKY_H];
static uint16_t g_sky_map_scratch[SAT_VDP2_NBG0_MAP_CELLS];
static uint16_t g_sky_palette[256];

static sat_fx16_t cam_x = 0;
static sat_fx16_t cam_y = 0;
static sat_ascii_font_t g_pad_debug_font;
static uint32_t g_frame_count = 0;

/* Renders pad.held as "PAD:xxxx UDLRSABC" using the library helper. */
static void format_pad_debug(uint16_t held, char* out) {
    sat_pad_format_held(held, out, 19);
}

/* Renders a free-running frame counter as "FRM:xxxxxxxx" (hex). */
static void format_frame_debug(uint32_t count, char* out) {
    sat_pad_format_frame(count, out, 14);
}

static uint16_t lerp5(uint16_t a, uint16_t b, uint16_t t, uint16_t n) {
    return (uint16_t)((a * (n - t) + b * t) / n);
}

static uint16_t sky_rgb(uint16_t r0, uint16_t g0, uint16_t b0,
                        uint16_t r1, uint16_t g1, uint16_t b1,
                        uint16_t t, uint16_t n) {
    return SAT_BGR555(lerp5(r0, r1, t, n), lerp5(g0, g1, t, n), lerp5(b0, b1, t, n));
}

static void build_sky_palette(void) {
    uint16_t i;
    for (i = 0u; i < 32u; ++i) {
        /* deep blue zenith -> pale warm haze at the horizon */
        g_sky_palette[SKY_GRAD_BASE + i] = sky_rgb(3u, 8u, 20u, 27u, 27u, 29u, i, 31u);
    }
    for (i = 0u; i < 16u; ++i) {
        g_sky_palette[SKY_CLOUD_BASE + i] = sky_rgb(20u, 22u, 26u, 31u, 31u, 31u, i, 15u);
        g_sky_palette[SKY_FAR_BASE + i] = sky_rgb(13u, 15u, 21u, 20u, 21u, 25u, i, 15u);
        g_sky_palette[SKY_NEAR_BASE + i] = sky_rgb(5u, 9u, 11u, 11u, 14u, 16u, i, 15u);
    }
    g_sky_palette[SKY_SNOW] = SAT_BGR555(29u, 30u, 31u);
}

/* Integer hash -> 0..255. */
static uint32_t hash8(uint32_t n) {
    n = (n ^ 61u) ^ (n >> 16u);
    n *= 9u;
    n ^= n >> 4u;
    n *= 0x27D4EB2Du;
    n ^= n >> 15u;
    return n & 0xFFu;
}

/* Smoothstep on 0..255. */
static uint32_t smooth8(uint32_t t) {
    return (t * t * (765u - 2u * t)) / 65025u;
}

/* 1-D value noise, 0..255, periodic over SKY_W. `cell` must divide SKY_W. */
static uint32_t ridge_noise(uint32_t x, uint32_t cell, uint32_t seed) {
    const uint32_t cells = SKY_W / cell;
    const uint32_t i0 = x / cell;
    const uint32_t i1 = (i0 + 1u) % cells;
    const uint32_t t = smooth8(((x % cell) * 255u) / cell);
    const uint32_t a = hash8(i0 * 131u + seed);
    const uint32_t b = hash8(i1 * 131u + seed);
    return (a * (255u - t) + b * t) / 255u;
}

/* 2-D value noise, 0..255, periodic over SKY_W horizontally. */
static uint32_t cloud_noise(uint32_t x, uint32_t y, uint32_t cell, uint32_t seed) {
    const uint32_t cells = SKY_W / cell;
    const uint32_t ix0 = x / cell;
    const uint32_t ix1 = (ix0 + 1u) % cells;
    const uint32_t iy0 = y / cell;
    const uint32_t tx = smooth8(((x % cell) * 255u) / cell);
    const uint32_t ty = smooth8(((y % cell) * 255u) / cell);
    const uint32_t a = hash8(ix0 + iy0 * 977u + seed);
    const uint32_t b = hash8(ix1 + iy0 * 977u + seed);
    const uint32_t c = hash8(ix0 + (iy0 + 1u) * 977u + seed);
    const uint32_t d = hash8(ix1 + (iy0 + 1u) * 977u + seed);
    const uint32_t top = (a * (255u - tx) + b * tx) / 255u;
    const uint32_t bot = (c * (255u - tx) + d * tx) / 255u;
    return (top * (255u - ty) + bot * ty) / 255u;
}

static uint32_t min_u32(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

static void build_sky(void) {
    const uint32_t horizon_row = SKY_H - 1u;
    uint32_t x;
    for (x = 0u; x < SKY_W; ++x) {
        /* ridge heights in pixels above the horizon */
        const uint32_t far_h = 14u + (ridge_noise(x, 64u, 7u) * 30u) / 255u +
                               (ridge_noise(x, 16u, 19u) * 8u) / 255u;
        const uint32_t near_h = 3u + (ridge_noise(x, 32u, 41u) * 14u) / 255u +
                                (ridge_noise(x, 8u, 53u) * 4u) / 255u;
        uint32_t y;
        for (y = 0u; y < SKY_H; ++y) {
            const uint32_t above = horizon_row - y;
            uint8_t index;

            if (above < near_h) {
                index = (uint8_t)(SKY_NEAR_BASE + min_u32(15u, (near_h - above) / 2u));
            } else if (above < far_h) {
                const uint32_t depth = far_h - above;
                if (far_h > 38u && depth < 3u) {
                    index = (uint8_t)SKY_SNOW;
                } else {
                    index = (uint8_t)(SKY_FAR_BASE + 15u - min_u32(15u, depth / 3u));
                }
            } else {
                const uint32_t grad = (y * 31u) / horizon_row;
                const uint32_t n = (cloud_noise(x, y, 32u, 101u) * 3u +
                                    cloud_noise(x, y, 8u, 211u)) / 4u;
                /* clouds only in a band well above the ridges */
                if (y > 36u && y < 96u && n > 150u) {
                    index = (uint8_t)(SKY_CLOUD_BASE + min_u32(15u, (n - 150u) / 5u));
                } else {
                    index = (uint8_t)(SKY_GRAD_BASE + grad);
                }
            }
            g_sky_pixels[y * SKY_W + x] = index;
        }
    }
}

/* NBG0 carries the sky; it sits behind RBG0, whose sky scanlines are
 * transparent, and the VDP1 HUD stays on top. */
static void init_sky_layer(void) {
    const sat_vdp2_nbg0_config_t sky = {
        SAT_VDP2_CHAR_SIZE_1X1,
        SAT_VDP2_COLOR_MODE_256,
        0x3Bu,
        0u,
        0u,
    };
    build_sky_palette();
    build_sky();
    sat_example_must(sat_vdp2_palette_upload(g_sky_palette, 256u, (uint16_t)(SKY_PALETTE * 256u)));
    sat_example_must(sat_vdp2_nbg0_init(&sky));
    sat_example_must(sat_vdp2_nbg0_upload_indexed8(
        g_sky_pixels, SKY_W, SKY_H, SKY_PALETTE, g_sky_map_scratch));
}

static void update_sky_scroll(sat_fx16_t camera_x) {
    const sat_vdp2_scroll_t scroll = {
        (uint16_t)(((uint32_t)(camera_x >> (FX16_SHIFT + SKY_PARALLAX_SHIFT))) & (SKY_W - 1u)),
        0u,
        (uint16_t)(SKY_H - HORIZON),
        0u,
    };
    sat_example_must(sat_vdp2_nbg0_set_scroll(&scroll));
}

/* Single source of truth for the Mode-7 layout, shared verbatim with
 * tests/host/test_rbg0_ground.cpp via rbg0_math.h.
 */
static const rbg0_ground_config_t rbg0_cfg = {
    BITMAP_WIDTH,
    BITMAP_HEIGHT,
    CX,
    HORIZON, /* == CY */
    FOCAL,
    MIN_DEPTH,
    GROUND_FORWARD,
    COEF_BASE_WORD,
};

static void upload_tiled_bitmap(const sat_indexed8_asset_t* asset) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    uint32_t off = BM_BASE_WORD;

    for (uint32_t y = 0; y < BITMAP_HEIGHT; y++) {
        const uint32_t src_y = y % asset->height;
        for (uint32_t x = 0; x < BITMAP_WIDTH; x += 2u) {
            const uint32_t src_x0 = x % asset->width;
            const uint32_t src_x1 = (x + 1u) % asset->width;
            const uint8_t p0 = asset->pixels[(src_y * asset->width) + src_x0];
            const uint8_t p1 = asset->pixels[(src_y * asset->width) + src_x1];
            vram[off++] = (uint16_t)(((uint16_t)p0 << 8u) | p1);
        }
    }
}

/* Coefficient table (2-word, mode 0) — see rbg0_ground_encode_coefficient()
 * in rbg0_math.h for the encoding and the k(y) = FOCAL / depth derivation.
 */
static void write_coefficient_table(void) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    for (uint32_t y = 0; y < (uint32_t)SCREEN_HEIGHT; y++) {
        uint16_t w0, w1;
        rbg0_ground_encode_coefficient(&rbg0_cfg, y, &w0, &w1);
        uint32_t base = COEF_BASE_WORD + (y * 2u);
        vram[base + 0u] = w0;
        vram[base + 1u] = w1;
    }
}

/* Rotation parameter A table at word 0x10000. Built by
 * rbg0_ground_build_params() (rbg0_math.h) — see that function's comment for
 * the Xst/Yst/Px/Py derivation and why Yst must differ from Py.
 * Camera position lives in Mx/My (parallel-translation) so the per-line
 * coefficient does NOT scale the camera, only the per-pixel deltas.
 */
static void write_rotation_params(int32_t cam_xi, int32_t cam_yi) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    uint16_t table[48];
    int i;

    rbg0_ground_build_params(&rbg0_cfg, cam_xi, cam_yi, table);
    for (i = 0; i < 48; i++) {
        vram[RP_BASE_WORD + (uint32_t)i] = table[i];
    }
}

static void write_camera_translation(int32_t cam_xi, int32_t cam_yi) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    int32_t mx = rbg0_ground_wrap_translation(cam_xi, rbg0_cfg.bitmap_width, rbg0_cfg.cx);
    int32_t my = rbg0_ground_wrap_translation(cam_yi, rbg0_cfg.bitmap_height, rbg0_cfg.horizon);

    vram[RP_BASE_WORD + 34u] = (uint16_t)((uint32_t)mx & 0x1FFFu);
    vram[RP_BASE_WORD + 35u] = 0x0000;
    vram[RP_BASE_WORD + 36u] = (uint16_t)((uint32_t)my & 0x1FFFu);
    vram[RP_BASE_WORD + 37u] = 0x0000;
}

/* Configure RBG0 in bitmap mode with per-line coefficient (Mode-7 floor). */
static void init_rbg0_mode7(void) {
    sat_vdp2_rbg0_mode7_config_t cfg = {
        SAT_VDP2_RBG0_BITMAP_512x256,
        SAT_VDP2_COLOR_MODE_256,
        BM_BASE_WORD,
        RP_BASE_WORD,
        SKY_COLOR,
        6u,   /* RBG0 priority below VDP1 HUD */
        7u    /* Sprite priority above ground */
    };
    sat_example_must(sat_vdp2_rbg0_mode7_init(&cfg));
}

int main(void) {
    sat_video_config_t cfg = {SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0};
    sat_example_must(sat_init(&cfg));
    sat_example_must(init_pad_debug_font_2x(&g_pad_debug_font));

    /* Upload palette + bitmap + coefficient table before enabling display. */
    sat_example_must(sat_vdp2_palette_upload(
        bg_asset.palette,
        (uint16_t)bg_asset.palette_count,
        0
    ));
    init_sky_layer();
    upload_tiled_bitmap(&bg_asset);
    write_coefficient_table();
    write_rotation_params(0, 0);

    /* Bring up RBG0 with our Mode-7 setup. */
    init_rbg0_mode7();
    sat_example_must(sat_vdp2_nbg0_set_priority(2u));
    update_sky_scroll(cam_x);

    int32_t last_x = -1, last_y = -1;
    sat_pad_state_t pad = {0};

    while (1) {
        sat_example_must(sat_wait_vblank());
        g_frame_count++;
        update_sky_scroll(cam_x);
        sat_example_must(sat_vdp2_layers_commit());
        sat_example_must(sat_pad_poll(&pad));

        char pad_debug_text[19];
        char frame_debug_text[14];
        format_pad_debug(pad.held, pad_debug_text);
        format_frame_debug(g_frame_count, frame_debug_text);
        sat_example_must(sat_begin_frame());
        sat_example_must(sat_ascii_font_draw_text_indexed8(
            &g_pad_debug_font, pad_debug_text, -152, -104, 16, 0, 0
        ));
        sat_example_must(sat_ascii_font_draw_text_indexed8(
            &g_pad_debug_font, frame_debug_text, -152, -80, 16, 0, 0
        ));
        sat_example_must(sat_end_frame());

        /* Walking speed in texels/frame; only Mx/My are rewritten during VBlank. */
        const sat_fx16_t spd = 2 << FX16_SHIFT;

        if ((pad.held & SAT_PAD_LEFT))  cam_x -= spd;
        if ((pad.held & SAT_PAD_RIGHT)) cam_x += spd;
        /* tex_y = cam_y + k(y) * GROUND_FORWARD, so larger tex_y is farther away:
         * walking forward moves distant ground toward the viewer => cam_y grows.
         */
        if ((pad.held & SAT_PAD_UP))    cam_y += spd;
        if ((pad.held & SAT_PAD_DOWN))  cam_y -= spd;

        int32_t xi = (int32_t)(cam_x >> FX16_SHIFT);
        int32_t yi = (int32_t)(cam_y >> FX16_SHIFT);
        if (xi != last_x || yi != last_y) {
            write_camera_translation(xi, yi);
            last_x = xi;
            last_y = yi;
        }

        if ((pad.pressed & SAT_PAD_START) != 0) break;
    }
    return 0;
}
