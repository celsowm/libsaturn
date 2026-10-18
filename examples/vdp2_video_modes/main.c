/* VDP2 video modes (E09).
 *
 * A deterministic resolution pattern shown through every TV screen mode this
 * library supports: 320/352/640/704 wide, 224/240/256 lines, non-interlaced
 * or single-density interlaced. The pattern is an NBG0 cell map with 8x8
 * cells: a one-dot grid (the grid line is one cell edge, so a 704-wide
 * picture shows 88 cells), solid colour bars, a 1-dot checker and a diagonal,
 * plus a VDP1 one-dot border and corner markers. The HUD states the active
 * mode from sat_video_width/height/interlace and the TVMD value the library
 * wrote.
 *
 * A mode change is a full sat_shutdown() + sat_init(): VDP2 manual 2.4
 * requires the BIOS horizontal-resolution routine and a reset when leaving
 * dedicated-monitor modes, and the library deliberately has no live
 * mode-switch path. At 640/704 the VDP1 framebuffer is 8 bits/pixel, so the
 * border and text are palette codes in the font's bank 7 (one shared bank,
 * CRAOFB); at 320/352 they are RGB555.
 *
 * 256-line modes exist only on a PAL console (VRESO 10b). Double-density
 * interlace is reported as unsupported, never written: it needs a different
 * VDP1 picture in each field, which no public framebuffer path exposes.
 *
 * Controls: A next mode, C previous mode, START exits.
 * The mode advances automatically every 60 fields so a harness capture run
 * reaches several of them without a pad script.
 *
 * g_video_modes holds what harness/tests/test_vdp2_video_modes.py checks. */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/saturn.h"
#include "saturn/video.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"
#include "saturn/vdp2_compose.h"
#include "saturn/vdp2_layers.h"

#define VIDEO_MODES_MAGIC SAT_FOURCC('V', 'M', 'D', '1')
#define AUTO_FIELDS 60u
/* Each of the four planes is 2x2 pages (128x128 cells, 0x8000 bytes), so
 * the map covers 256x256 cells = 2048x2048 dots, the manual's table 4.12
 * display area for a 2x2-page plane. */
#define PLANE_WORDS (128u * 128u)
#define PLANE_BYTES 0x8000u
#define NAMES0 0x00000u
#define NAMES1 0x08000u
#define NAMES2 0x10000u
#define NAMES3 0x18000u
#define CHARS 0x40000u
#define FONT_BANK 7u
#define PATTERN_PALETTE 0u

typedef struct video_mode_entry {
    uint16_t width;
    uint16_t height;
    uint8_t interlace;
} video_mode_entry_t;

typedef struct video_modes_results {
    uint32_t magic;
    uint32_t init_ok;
    uint32_t requested;
    uint32_t supported_mask;
    uint32_t refused_double;
    uint32_t refused_width;
    uint32_t refused_height;
    uint32_t refused_pal;
    uint32_t mode_changes;
    uint32_t mode;
    uint32_t frames;
    uint32_t width;
    uint32_t height;
    uint32_t interlace;
    uint32_t pal;
    uint32_t ntsc_timing;
    uint32_t hires;
    uint32_t cram_mode;
    uint32_t tvmd;
    uint32_t tvstat;
    uint32_t tvmd_ok;
    uint32_t last_error;
} video_modes_results_t;

volatile video_modes_results_t g_video_modes;

static const video_mode_entry_t kModes[] = {
    {320u, 224u, SAT_VIDEO_INTERLACE_NONE},
    {352u, 240u, SAT_VIDEO_INTERLACE_NONE},
    {640u, 240u, SAT_VIDEO_INTERLACE_NONE},
    {320u, 240u, SAT_VIDEO_INTERLACE_SINGLE},
    {640u, 240u, SAT_VIDEO_INTERLACE_SINGLE},
    {704u, 256u, SAT_VIDEO_INTERLACE_NONE},
    {320u, 256u, SAT_VIDEO_INTERLACE_SINGLE},
    {640u, 256u, SAT_VIDEO_INTERLACE_SINGLE},
};
#define MODE_COUNT (sizeof(kModes) / sizeof(kModes[0]))

static sat_ascii_font_t g_font;
static uint16_t g_plane[PLANE_WORDS];
static uint16_t g_tiles[8u * 32u];

static void build_tiles(void) {
    /* 16-colour, 1x1 characters: 8 rows x 8 dots, four bits per dot, 32 bytes
     * (16 words) each. */
    for (uint32_t tile = 0u; tile < 8u; ++tile) {
        for (uint32_t y = 0u; y < 8u; ++y) {
            for (uint32_t x = 0u; x < 8u; ++x) {
                uint32_t c = 1u;
                if (tile == 1u) c = (x == 0u || y == 0u) ? 2u : 1u;
                else if (tile == 2u) c = 3u;
                else if (tile == 3u) c = 4u;
                else if (tile == 4u) c = 5u;
                else if (tile == 5u) c = 6u;
                else if (tile == 6u) c = ((x ^ y) & 1u) ? 6u : 1u;
                else if (tile == 7u) c = (x == y) ? 7u : 1u;
                const uint32_t word = tile * 16u + y * 2u + x / 4u;
                const uint32_t shift = (3u - (x & 3u)) * 4u;
                if (x % 4u == 0u) g_tiles[word] = 0u;
                g_tiles[word] = (uint16_t)(g_tiles[word] | (c << shift));
            }
        }
    }
}

static uint16_t cell_pattern(uint32_t gx, uint32_t gy) {
    uint16_t tile = 0u;
    if (gy < 4u) {
        tile = (uint16_t)(2u + ((gx / 8u) & 3u)); /* colour bars */
    } else if (gx >= 8u && gx < 40u && gy >= 8u && gy < 24u) {
        tile = 6u; /* one-dot checker band */
    } else if (gx >= 48u && gx < 80u && gy >= 8u && gy < 40u) {
        tile = 7u; /* diagonal band */
    } else {
        tile = ((gx & 7u) == 0u || (gy & 7u) == 0u) ? 1u : 0u;
    }
    return (uint16_t)((PATTERN_PALETTE << 12u) | tile);
}

static void fill_plane(uint32_t plane) {
    const uint32_t ox = (plane & 1u) * 128u;
    const uint32_t oy = (plane >> 1u) * 128u;
    for (uint32_t ly = 0u; ly < 128u; ++ly) {
        for (uint32_t lx = 0u; lx < 128u; ++lx) {
            const uint32_t page = (ly / 64u) * 2u + lx / 64u;
            const uint32_t px = lx & 63u;
            const uint32_t py = ly & 63u;
            g_plane[page * 4096u + py * 64u + px] = cell_pattern(ox + lx, oy + ly);
        }
    }
}

static void upload_pattern(void) {
    uint16_t palette[16] = {0};
    palette[1] = SAT_RGB555(8, 10, 24);
    palette[2] = SAT_RGB555(0, 28, 28);
    palette[3] = SAT_RGB555(28, 4, 4);
    palette[4] = SAT_RGB555(4, 28, 4);
    palette[5] = SAT_RGB555(4, 8, 28);
    palette[6] = SAT_RGB555(31, 31, 31);
    palette[7] = SAT_RGB555(31, 24, 0);
    palette[8] = SAT_RGB555(31, 0, 31);
    (void)sat_vdp2_palette_upload(palette, 16u, PATTERN_PALETTE * 16u);
    (void)sat_vdp2_vram_write_words(CHARS / 2u, g_tiles, 8u * 16u);
    for (uint32_t p = 0u; p < 4u; ++p) {
        fill_plane(p);
        (void)sat_vdp2_vram_write_words((NAMES0 + p * PLANE_BYTES) / 2u, g_plane, PLANE_WORDS);
    }
}

static sat_result_t configure_pattern(void) {
    sat_vdp2_layer_config_t c;
    sat_vdp2_layer_config_default(SAT_VDP2_NBG0, &c);
    c.color_mode = SAT_VDP2_COLOR_MODE_16;
    c.char_size = SAT_VDP2_CHAR_SIZE_1X1;
    c.pattern_name_words = 1u;
    c.plane_pages_x = 2u;
    c.plane_pages_y = 2u;
    c.plane_address[0] = NAMES0;
    c.plane_address[1] = NAMES1;
    c.plane_address[2] = NAMES2;
    c.plane_address[3] = NAMES3;
    c.char_base_address = CHARS;
    c.char_bank_mask = SAT_VDP2_BANK_B0;
    c.priority = 3u;
    c.transparent = 1u;
    c.palette = 0u;
    return sat_vdp2_layer_configure(&c);
}

static uint32_t expected_mode_bits(void) {
    uint32_t bits = 0u;
    bits |= (uint32_t)(kModes[g_video_modes.mode].interlace == SAT_VIDEO_INTERLACE_SINGLE ? 2u : 0u) << 6u;
    bits |= (uint32_t)(kModes[g_video_modes.mode].height == 240u ? 1u
                      : (kModes[g_video_modes.mode].height == 256u ? 2u : 0u)) << 4u;
    bits |= kModes[g_video_modes.mode].width == 352u ? 1u
          : (kModes[g_video_modes.mode].width == 640u ? 2u
          : (kModes[g_video_modes.mode].width == 704u ? 3u : 0u));
    return bits;
}

static sat_result_t enter_mode(uint32_t index, uint8_t initial) {
    const video_mode_entry_t* m = &kModes[index];
    if (!initial) {
        (void)sat_shutdown();
    }
    sat_video_config_t cfg = {m->width, m->height, SAT_VIDEO_AUTO, 0u};
    cfg.interlace = m->interlace;
    sat_result_t st = sat_init(&cfg);
    if (st != SAT_OK) return st;
    st = sat_ascii_font_init_8x8_indexed8(&g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, FONT_BANK);
    if (st != SAT_OK) return st;
    upload_pattern();
    st = configure_pattern();
    if (st != SAT_OK) return st;
    /* 640/704: every VDP1 dot is a code in one shared CRAM bank. The font and
     * the border both use the font's bank 7 (index 1 = white). */
    if (sat_video_is_hires()) {
        st = sat_vdp2_sprite_palette_bank_set(FONT_BANK);
        if (st != SAT_OK) return st;
    }
    (void)sat_vdp2_back_color_set(SAT_RGB555(2, 2, 6));
    (void)sat_vdp2_sprite_set_priority(7u);
    (void)sat_vdp2_layers_commit();
    g_video_modes.mode = index;
    g_video_modes.width = m->width;
    g_video_modes.height = m->height;
    g_video_modes.interlace = m->interlace;
    g_video_modes.hires = sat_video_is_hires();
    g_video_modes.pal = sat_video_is_pal();
    g_video_modes.ntsc_timing = sat_video_is_ntsc_timing();
    g_video_modes.cram_mode = sat_vdp2_cram_mode();
    return SAT_OK;
}

static uint32_t supported_bit(uint32_t index) {
    sat_video_mode_t m = {kModes[index].width, kModes[index].height,
                          kModes[index].interlace, 0u};
    return sat_video_mode_supported(&m) == SAT_OK ? (1u << index) : 0u;
}

static uint32_t next_supported(uint32_t from, int direction) {
    for (uint32_t step = 1u; step <= MODE_COUNT; ++step) {
        const uint32_t index = direction > 0
            ? (from + step) % MODE_COUNT
            : (from + MODE_COUNT - (step % MODE_COUNT)) % MODE_COUNT;
        if ((g_video_modes.supported_mask & (1u << index)) != 0u) return index;
    }
    return from;
}

static void draw_marker(uint16_t colour, int x, int y, int w, int h) {
    (void)sat_draw_rect_screen((int16_t)x, (int16_t)y, (uint16_t)w, (uint16_t)h, colour);
}

static void draw_pattern(void) {
    const uint16_t white = sat_video_is_hires() ? 1u : SAT_RGB555(31, 31, 31);
    const int w = (int)sat_video_width();
    const int h = (int)sat_video_height();
    /* A one-dot border, corner ticks every 32 dots and a centre cross. The
     * checker band in the NBG0 map proves the VDP2 side; these prove VDP1. */
    draw_marker(white, 0, 0, w, 1);
    draw_marker(white, 0, h - 1, w, 1);
    draw_marker(white, 0, 0, 1, h);
    draw_marker(white, w - 1, 0, 1, h);
    for (int x = 32; x < w; x += 32) draw_marker(white, x, 0, 1, 3);
    for (int y = 32; y < h; y += 32) draw_marker(white, 0, y, 3, 1);
    draw_marker(white, w / 2 - 1, h / 2, 2, 1);
    draw_marker(white, w / 2, h / 2 - 1, 1, 2);
}

static void draw_hud(void) {
    (void)sat_ascii_font_draw_text_screen_indexed8(&g_font, "VDP2 VIDEO MODES", 8, 8, 8, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&g_font, "MODE ", g_video_modes.mode, 8, 24, 8, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&g_font, "W ", g_video_modes.width, 8, 40, 8, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&g_font, "H ", g_video_modes.height, 96, 40, 8, 0u, 0u);
    (void)sat_ascii_font_draw_text_screen_indexed8(
        &g_font, g_video_modes.interlace != 0u ? "I1" : "I0",
        sat_video_is_hires() ? 176 : 160, 40, 8, 0u, 0u);
    (void)sat_ascii_font_draw_text_screen_indexed8(
        &g_font, g_video_modes.pal != 0u ? "PAL" : "NTSC",
        8, 56, 8, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&g_font, "CRAM ", g_video_modes.cram_mode, 8, 72, 8, 0u, 0u);
    (void)sat_ascii_font_draw_text_screen_indexed8(&g_font, "A NEXT  C PREV  START",
        8, (int)sat_video_height() - 16, 8, 0u, 0u);
}

static void update_registers(void) {
    g_video_modes.tvmd = sat_video_tvmd();
    g_video_modes.tvstat = (uint32_t)sat_video_tvstat();
    g_video_modes.tvmd_ok =
        (g_video_modes.tvmd & 0x00FFu) == expected_mode_bits() ? 1u : 0u;
    g_video_modes.width = sat_video_width();
    g_video_modes.height = sat_video_height();
    g_video_modes.interlace = sat_video_interlace_mode();
    g_video_modes.hires = sat_video_is_hires();
    g_video_modes.pal = sat_video_is_pal();
}

int main(void) {
    g_video_modes.magic = VIDEO_MODES_MAGIC;
    g_video_modes.requested = MODE_COUNT;
    for (uint32_t i = 0u; i < MODE_COUNT; ++i) {
        g_video_modes.supported_mask |= supported_bit(i);
    }
    {
        sat_video_mode_t m = {320u, 240u, SAT_VIDEO_INTERLACE_DOUBLE, 0u};
        sat_video_mode_t badw = {480u, 240u, SAT_VIDEO_INTERLACE_NONE, 0u};
        sat_video_mode_t badh = {320u, 480u, SAT_VIDEO_INTERLACE_NONE, 0u};
        sat_video_mode_t pal256 = {320u, 256u, SAT_VIDEO_INTERLACE_NONE, 0u};
        g_video_modes.refused_double =
            sat_video_mode_supported(&m) == SAT_ERR_UNSUPPORTED;
        g_video_modes.refused_width =
            sat_video_mode_supported(&badw) == SAT_ERR_INVALID_ARG;
        g_video_modes.refused_height =
            sat_video_mode_supported(&badh) == SAT_ERR_INVALID_ARG;
        const sat_result_t pal_result = sat_video_mode_supported(&pal256);
        g_video_modes.refused_pal = sat_video_is_pal()
            ? (pal_result == SAT_OK) : (pal_result == SAT_ERR_UNSUPPORTED);
    }
    g_video_modes.init_ok =
        (g_video_modes.supported_mask != 0u && g_video_modes.refused_double != 0u &&
         g_video_modes.refused_width != 0u && g_video_modes.refused_height != 0u &&
         g_video_modes.refused_pal != 0u);

    build_tiles();
    uint32_t mode = next_supported(MODE_COUNT - 1u, 1);
    if (enter_mode(mode, 1u) != SAT_OK) {
        g_video_modes.last_error = (uint32_t)SAT_ERR_IO;
        g_video_modes.init_ok = 0u;
        for (;;) { }
    }
    uint32_t mode_fields = 0u;
    int32_t request = -1;

    for (;;) {
        if (request >= 0) {
            const uint32_t wanted = (uint32_t)request;
            request = -1;
            if (enter_mode(wanted, 0u) != SAT_OK) {
                /* Keep the last good mode rather than a half-configured one. */
                g_video_modes.last_error = 1u;
            } else {
                ++g_video_modes.mode_changes;
                mode_fields = 0u;
            }
        }
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        (void)sat_vdp2_layers_commit();
        (void)sat_vdp1_set_erase_transparent();
        if (sat_begin_frame() != SAT_OK) break;
        (void)sat_pad_poll(&pad);
        ++g_video_modes.frames;
        ++mode_fields;
        if ((pad.pressed & SAT_PAD_A) != 0u) request = (int32_t)next_supported(g_video_modes.mode, 1);
        if ((pad.pressed & SAT_PAD_C) != 0u) request = (int32_t)next_supported(g_video_modes.mode, -1);
        if (mode_fields >= AUTO_FIELDS) request = (int32_t)next_supported(g_video_modes.mode, 1);
        update_registers();
        draw_pattern();
        draw_hud();
        (void)sat_end_frame();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
