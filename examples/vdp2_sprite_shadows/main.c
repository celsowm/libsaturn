/* VDP2 sprite shadows (manual 14.1, p14_10).
 *
 * NBG0 is a scrolling vertical-stripe ground at priority 3. The sprite layer
 * is switched to type 2 (palette, 16-bit dots, two priority selectors) and a
 * 32x32 silhouette is drawn with a LUT4 texture whose entry 1 is a shadow
 * word:
 *
 *   NORMAL   0x47FE  normal shadow data (low 11 bits 0x7FE), selector 1
 *   MSB      0x8000  transparent MSB shadow, all 15 low bits zero (S0PRIN)
 *
 * SDCTL selects NBG0; TPSDSL is enabled in MSB mode. The shadow sprite is
 * excluded from the layer stack and halves the ground underneath. The
 * TARGETS OFF mode keeps the same normal shadow sprite but writes SDCTL=0,
 * proving the per-screen target restriction: the silhouette is drawn yet
 * darkens nothing.
 *
 * A palette sprite at (248,56) is drawn normally and, in MSB mode, a second
 * pass with SAT_SPRITE_FLAG_MSB_ON sets bit 15 of the already-written dots.
 * That is the manual's sprite shadow (self-shadow): the lower 15 bits are
 * preserved, so VDP2 fetches the sprite's own colour and halves it.
 *
 * The VDP1 mode is the control: the VDP2 shadow configuration is off and a
 * VDP1 RGB panel is drawn with a VDP1 shadow sprite over it. VDP1 halves the
 * 5-bit destination before VDP2 expands it; VDP2 halves the 8-bit result.
 * The two are therefore distinguishable in the captured pixels (248 -> 120
 * for VDP1, 248 -> 124 for VDP2).
 *
 * Timeline (auto; A cycles by hand, START exits):
 *   frame  42  NORMAL      VDP2 normal shadow over NBG0
 *   frame 102  MSB         transparent MSB shadow + sprite self-shadow
 *   frame 162  VDP1        VDP1 RGB panel + VDP1 shadow (control)
 *   frame 222  TARGETS OFF normal silhouette, SDCTL=0 -> ground unchanged
 *   frame 282  OFF         shadow disabled, ground unchanged
 *
 * g_sprite_shadows holds the refusal and mode telemetry. The ground scroll,
 * shadow x and frame counter let the harness recompute the expected ground
 * colour instead of using a golden image. */

#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/saturn.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"
#include "saturn/vdp2_layers.h"
#include "saturn/vdp2_sprite_shadow.h"

#define SHADOW_MAGIC SAT_FOURCC('S', 'H', 'D', '1')

typedef struct shadow_results {
    uint32_t magic;
    uint32_t layers_ok;
    uint32_t shadow_ok;
    uint32_t refused_palette;
    uint32_t refused_mode;
    uint32_t refused_target;
    uint32_t refused_priority;
    uint32_t refused_busy;
    uint32_t mode_ok;
    uint32_t mode;
    uint32_t scroll;
    uint32_t shadow_x;
    uint32_t frames;
} shadow_results_t;

volatile shadow_results_t g_sprite_shadows;

static sat_ascii_font_t font;

/* NBG0 pattern names at 0, character data at 0x10000 (bank A0). */
#define GROUND_NAMES 0x00000u
#define GROUND_CHARS 0x10000u
#define GROUND_CHAR_TILES 8u
#define GROUND_TILE_WORDS 16u

static uint16_t g_ground_map[64u * 64u];
static uint16_t g_ground_chars[GROUND_CHAR_TILES * GROUND_TILE_WORDS];

static sat_vdp1_texture_t g_normal_shadow;
static sat_vdp1_texture_t g_msb_shadow;
static sat_vdp1_texture_t g_self_sprite;

#define MODE_NORMAL     0u
#define MODE_MSB        1u
#define MODE_VDP1       2u
#define MODE_TARGET_OFF 3u
#define MODE_OFF        4u
#define MODE_COUNT      5u

static const char*const kModeName[MODE_COUNT] = {
    "NORMAL", "MSB", "VDP1", "TARGETS OFF", "OFF"
};
static const uint32_t kAutoAt[MODE_COUNT - 1u] = {60u, 120u, 180u, 240u};

/* 5-bit ground palette. The tests expand these exactly like the VDP2 does. */
static const uint16_t kGround5[8] = {
    SAT_RGB555_RAW(31, 0, 0),   /* red */
    SAT_RGB555_RAW(0, 31, 0),   /* green */
    SAT_RGB555_RAW(0, 0, 31),   /* blue */
    SAT_RGB555_RAW(31, 31, 0),  /* yellow */
    SAT_RGB555_RAW(31, 0, 31),  /* magenta */
    SAT_RGB555_RAW(0, 31, 31),  /* cyan */
    SAT_RGB555_RAW(31, 16, 0),  /* orange */
    SAT_RGB555_RAW(24, 24, 24)  /* grey */
};

static sat_vdp2_sprite_shadow_config_t shadow_config(uint8_t mode, uint8_t targets,
                                                     uint8_t transparent) {
    sat_vdp2_sprite_shadow_config_t config = {0};
    config.mode = mode;
    config.target_mask = targets;
    config.transparent_shadow = transparent;
    config.base_priority = 4u;
    config.shadow_priority = 7u;
    return config;
}

static void build_ground_tiles(void) {
    for (uint32_t tile = 0u; tile < GROUND_CHAR_TILES; ++tile) {
        const uint16_t code = (uint16_t)(tile + 1u); /* code 0 stays transparent */
        const uint16_t word = (uint16_t)((code << 12u) | (code << 8u) |
                                         (code << 4u) | code);
        for (uint32_t i = 0u; i < GROUND_TILE_WORDS; ++i) {
            g_ground_chars[tile * GROUND_TILE_WORDS + i] = word;
        }
    }
}

static sat_result_t setup_ground(void) {
    /* Every row is identical: vertical 8-pixel stripes. The test computes the
     * colour at any (x,y) from the scroll, so the shadowed pixel can be
     * compared with an unshadowed pixel at the same x on the same stripes. */
    for (uint32_t y = 0u; y < 64u; ++y) {
        for (uint32_t x = 0u; x < 64u; ++x) {
            g_ground_map[y * 64u + x] = (uint16_t)(x & 7u);
        }
    }

    SAT_TRY(sat_vdp2_vram_write_words(GROUND_NAMES / 2u, g_ground_map, 64u * 64u));
    SAT_TRY(sat_vdp2_vram_write_words(GROUND_CHARS / 2u, g_ground_chars,
                                      GROUND_CHAR_TILES * GROUND_TILE_WORDS));

    sat_vdp2_layer_config_t c;
    sat_vdp2_layer_config_default(SAT_VDP2_NBG0, &c);
    c.color_mode = SAT_VDP2_COLOR_MODE_16;
    c.char_size = SAT_VDP2_CHAR_SIZE_1X1;
    c.pattern_name_words = 1u;
    c.plane_pages_x = 1u;
    c.plane_pages_y = 1u;
    c.priority = 3u;
    c.transparent = 1u;
    c.char_bank_mask = SAT_VDP2_BANK_A0;
    c.char_base_address = GROUND_CHARS;
    for (uint32_t p = 0u; p < 4u; ++p) c.plane_address[p] = GROUND_NAMES;
    SAT_TRY(sat_vdp2_layer_configure(&c));
    return sat_vdp2_layer_set_scroll(SAT_VDP2_NBG0, 0, 0);
}

/* Filled 32x32 circle, LUT4: 1 inside, 0 (transparent) outside. */
static void build_shadow_shape(uint8_t pixels[32u * 32u / 2u]) {
    for (uint32_t i = 0u; i < 32u * 32u / 2u; ++i) pixels[i] = 0u;
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            const int dx = x - 16;
            const int dy = y - 16;
            if (dx * dx + dy * dy <= 15 * 15) {
                const uint32_t index = (uint32_t)(y * 32 + x);
                pixels[index >> 1u] |= (uint8_t)((x & 1) == 0 ? 0x10u : 0x01u);
            }
        }
    }
}

/* 16x16 filled sprite, indexed8: 1 inside, 0 (transparent) outside. */
static void build_self_sprite(uint8_t pixels[16u * 16u]) {
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            const int dx = x - 8;
            const int dy = y - 8;
            pixels[y * 16 + x] = (dx * dx + dy * dy <= 7 * 7) ? 1u : 0u;
        }
    }
}

static sat_result_t setup_sprite_assets(void) {
    uint8_t shape[32u * 32u / 2u];
    uint8_t self_pixels[16u * 16u];
    uint16_t lut[16] = {0};
    uint16_t normal_lut = 0u;
    uint16_t msb_lut = 0u;

    build_shadow_shape(shape);
    build_self_sprite(self_pixels);

    /* Entry 0 stays 0: LUT4 code 0 is the transparent texel. */
    lut[1] = sat_vdp2_shadow_normal_word(1u, 0u);       /* 0x47FE */
    SAT_TRY(sat_vdp1_upload_lut(lut, &normal_lut));
    lut[1] = sat_vdp2_shadow_transparent_msb_word();    /* 0x8000, S0PRIN */
    SAT_TRY(sat_vdp1_upload_lut(lut, &msb_lut));

    SAT_TRY(sat_tex_upload_lut4_pixels(&g_normal_shadow, shape, 32u, 32u, normal_lut));
    SAT_TRY(sat_tex_upload_lut4_pixels(&g_msb_shadow, shape, 32u, 32u, msb_lut));

    uint16_t palette[256] = {0};
    palette[1] = SAT_RGB555_RAW(31, 31, 31); /* white, sprite bank 4 */
    return sat_tex_upload_indexed8(&g_self_sprite, self_pixels, 16u, 16u, palette, 4u);
}

/* VDP1 local coordinates are relative to the screen centre (160,112). The
 * helper takes a screen-space centre, like sat_draw_sprite_screen, but also
 * lets the caller set the command flags. */
static void draw_sprite_flags(const sat_vdp1_texture_t* texture, int16_t center_x,
                              int16_t center_y, uint16_t flags) {
    const int32_t left = (int32_t)center_x - (int32_t)(texture->width / 2u);
    const int32_t top = (int32_t)center_y - (int32_t)(texture->height / 2u);
    const sat_sprite_cmd_t cmd = {
        .x = (sat_fx16_t)((left - 160) * SAT_FX16_ONE),
        .y = (sat_fx16_t)((top - 112) * SAT_FX16_ONE),
        .width = texture->width,
        .height = texture->height,
        .texture = texture,
        .palette_override = 0u,
        .flags = flags
    };
    (void)sat_draw_sprite(&cmd);
}

static sat_result_t apply_mode(uint8_t mode) {
    sat_vdp2_sprite_shadow_config_t c;
    switch (mode) {
        case MODE_NORMAL:
            c = shadow_config(SAT_VDP2_SHADOW_MODE_NORMAL,
                              SAT_VDP2_SHADOW_TARGET_NBG0, 0u);
            return sat_vdp2_sprite_shadow_configure(&c);
        case MODE_MSB:
            c = shadow_config(SAT_VDP2_SHADOW_MODE_MSB,
                              SAT_VDP2_SHADOW_TARGET_NBG0, 1u);
            return sat_vdp2_sprite_shadow_configure(&c);
        case MODE_TARGET_OFF:
            /* Same normal shadow word, but no target screens. */
            c = shadow_config(SAT_VDP2_SHADOW_MODE_NORMAL, 0u, 0u);
            return sat_vdp2_sprite_shadow_configure(&c);
        default:
            return sat_vdp2_sprite_shadow_disable();
    }
}

static void draw_mode(uint8_t mode) {
    const int32_t shadow_x = (int32_t)g_sprite_shadows.shadow_x;
    const int16_t shadow_cx = (int16_t)(shadow_x + 16);

    switch (mode) {
        case MODE_NORMAL:
            draw_sprite_flags(&g_normal_shadow, shadow_cx, 96, 0u);
            draw_sprite_flags(&g_self_sprite, 248, 56, 0u);
            break;
        case MODE_MSB:
            draw_sprite_flags(&g_msb_shadow, shadow_cx, 96, 0u);
            /* The normal sprite first, then MSB-on over the same shape:
             * bit 15 changes in place and the sprite self-shadows. */
            draw_sprite_flags(&g_self_sprite, 248, 56, 0u);
            draw_sprite_flags(&g_self_sprite, 248, 56, SAT_SPRITE_FLAG_MSB_ON);
            break;
        case MODE_TARGET_OFF:
            draw_sprite_flags(&g_normal_shadow, shadow_cx, 96, 0u);
            draw_sprite_flags(&g_self_sprite, 248, 56, 0u);
            break;
        case MODE_VDP1:
            /* VDP1 control: an RGB panel at priority S0=4, then the VDP1
             * shadow silhouette. VDP1 only halves RGB framebuffer pixels. */
            (void)sat_draw_rect_screen(200, 80, 80, 80, SAT_RGB555(0, 31, 31));
            draw_sprite_flags(&g_normal_shadow, 240, 120, SAT_SPRITE_FLAG_SHADOW);
            draw_sprite_flags(&g_self_sprite, 248, 56, 0u);
            break;
        default:
            draw_sprite_flags(&g_self_sprite, 248, 56, 0u);
            break;
    }
}

int main(void) {
    uint16_t ground_palette[16] = {0};
    uint8_t priorities[8] = {4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u};

    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) {
        return 1;
    }

    for (uint32_t i = 0u; i < 8u; ++i) ground_palette[i + 1u] = kGround5[i];
    if (sat_vdp2_palette_upload(ground_palette, 16u, 0u) != SAT_OK) return 1;

    build_ground_tiles();
    g_sprite_shadows.magic = SHADOW_MAGIC;
    g_sprite_shadows.layers_ok = setup_ground() == SAT_OK;
    if (!g_sprite_shadows.layers_ok) return 1;

    g_sprite_shadows.layers_ok &= setup_sprite_assets() == SAT_OK;
    if (!g_sprite_shadows.layers_ok) return 1;

    if (sat_vdp2_sprite_set_priority(4u) != SAT_OK) return 1;

    /* Enable the normal-shadow configuration, then prove the exclusive
     * ownership of SPCTL/PRISA from both sides. */
    sat_vdp2_sprite_shadow_config_t c =
        shadow_config(SAT_VDP2_SHADOW_MODE_NORMAL, SAT_VDP2_SHADOW_TARGET_NBG0, 0u);
    g_sprite_shadows.shadow_ok = sat_vdp2_sprite_shadow_configure(&c) == SAT_OK;
    g_sprite_shadows.refused_palette =
        sat_vdp2_sprite_set_palette_priorities(priorities) == SAT_ERR_BUSY;

    c = shadow_config(3u, SAT_VDP2_SHADOW_TARGET_NBG0, 0u);
    g_sprite_shadows.refused_mode = sat_vdp2_sprite_shadow_configure(&c) == SAT_ERR_INVALID_ARG;
    c = shadow_config(SAT_VDP2_SHADOW_MODE_NORMAL, 0x40u, 0u);
    g_sprite_shadows.refused_target = sat_vdp2_sprite_shadow_configure(&c) == SAT_ERR_INVALID_ARG;
    c = shadow_config(SAT_VDP2_SHADOW_MODE_NORMAL, SAT_VDP2_SHADOW_TARGET_NBG0, 0u);
    c.base_priority = 8u;
    g_sprite_shadows.refused_priority =
        sat_vdp2_sprite_shadow_configure(&c) == SAT_ERR_INVALID_ARG;

    (void)sat_vdp2_sprite_shadow_disable();
    if (sat_vdp2_sprite_set_palette_priorities(priorities) == SAT_OK) {
        c = shadow_config(SAT_VDP2_SHADOW_MODE_MSB, SAT_VDP2_SHADOW_TARGET_NBG0, 1u);
        g_sprite_shadows.refused_busy =
            sat_vdp2_sprite_shadow_configure(&c) == SAT_ERR_BUSY;
        (void)sat_vdp2_sprite_set_palette_priorities(0);
    }
    (void)sat_vdp2_sprite_set_priority(4u);

    uint8_t mode = MODE_NORMAL;
    if (apply_mode(mode) == SAT_OK) g_sprite_shadows.mode_ok = 1u << mode;
    g_sprite_shadows.mode = mode;

    uint32_t pending_scroll = 0u;
    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        (void)sat_vdp2_layers_commit();
        g_sprite_shadows.scroll = pending_scroll;

        (void)sat_vdp1_set_erase_transparent();
        if (sat_begin_frame() != SAT_OK) break;
        (void)sat_pad_poll(&pad);
        ++g_sprite_shadows.frames;

        uint8_t next = mode;
        if ((pad.pressed & SAT_PAD_A) != 0u) {
            next = (uint8_t)((mode + 1u) % MODE_COUNT);
        } else if (mode + 1u < MODE_COUNT &&
                   g_sprite_shadows.frames == kAutoAt[mode]) {
            next = (uint8_t)(mode + 1u);
        }
        if (next != mode) {
            mode = next;
            if (apply_mode(mode) == SAT_OK) {
                g_sprite_shadows.mode_ok |= 1u << mode;
            }
            g_sprite_shadows.mode = mode;
        }

        g_sprite_shadows.shadow_x = 64u + (g_sprite_shadows.frames & 63u);
        draw_mode(mode);

        (void)sat_ascii_font_draw_text_screen_indexed8(
            &font, "VDP2 SPRITE SHADOWS", 8, 8, 8, 0u, 0u);
        (void)sat_ascii_font_draw_text_screen_indexed8(
            &font, kModeName[mode], 8, 24, 8, 0u, 0u);
        (void)sat_ascii_font_draw_label_u32(
            &font, "FRAME ", g_sprite_shadows.frames, 8, 40, 8, 0u, 0u);

        /* Stage next frame's scroll. The VBlank commit above applied the
         * value staged last frame, which is what g_sprite_shadows.scroll
         * reports and what the screenshot shows. */
        pending_scroll = (g_sprite_shadows.frames * 2u) & 0x1FFu;
        (void)sat_vdp2_layer_set_scroll(
            SAT_VDP2_NBG0, (int32_t)(pending_scroll << 16u), 0);

        (void)sat_end_frame();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
