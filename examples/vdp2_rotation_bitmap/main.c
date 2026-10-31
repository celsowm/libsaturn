/* vdp2_rotation_bitmap: RBG0 in bitmap format, live.
 *
 * A 512x256, 256-colour picture sits in VRAM-B0 and the rotation scroll
 * screen turns and zooms it about the middle of the screen, one new
 * parameter region per frame. There is no pattern name map and no character
 * data: the picture is the whole screen's source.
 *
 * VRAM: the picture fills bank B0 (0x40000, 128 KiB), which
 * sat_vdp2_rotation_bitmap_configure gives to RBG0, so the plan claims that
 * bank and reserves the picture in it as ROTATION_BITMAP; the A+B parameter
 * region sits in bank A1, outside it.
 *
 * The combinations the manager and the plan refuse are tried once and
 * recorded in g_rotation_bitmap for the harness: a picture address that is
 * not a multiple of 0x20000, the over-pattern mode (a picture has no pattern
 * names), a parameter region inside the picture's bank, RBG1 next to a
 * bitmap, and reserving the picture before its bank is claimed or off the
 * bank's edge.
 *
 * Controls: START exits.
 */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/math2d.h"
#include "saturn/saturn.h"
#include "saturn/vdp2_alloc.h"
#include "saturn/vdp2_layers.h"
#include "example_util.h"

#define RBM_MAGIC SAT_FOURCC('R', 'B', 'M', '1')
#define SCREEN_W 320
#define SCREEN_H 224
#define BITMAP_ADDRESS 0x40000u  /* VRAM-B0, a multiple of 0x20000 */
#define BITMAP_W 512u
#define BITMAP_H 256u
#define REGION 0x3C000u          /* the A+B parameter region, in VRAM-A1 */
#define REGION_BYTES 0xE0u

typedef struct rotation_bitmap_state {
    uint32_t magic;
    uint32_t frames;
    uint32_t configure_result;
    uint32_t params_result;
    uint32_t refused_misaligned;      /* bitmap address 0x50000 */
    uint32_t refused_over_pattern;    /* SAT_VDP2_ROTATION_OVER_PATTERN */
    uint32_t refused_table_in_bank;   /* parameter region inside B0 */
    uint32_t refused_rbg1;            /* RBG1 enabled next to the bitmap */
    uint32_t refused_plan_unclaimed;  /* plan: the picture before its bank's claim */
    uint32_t refused_plan_misaligned; /* plan: the picture at 0x50000 */
    uint32_t bitmap_bytes;            /* what the manager says the picture takes */
    uint32_t params_writes;
    int32_t last_k;                   /* kx = ky of the last frame, 16.16 */
    int32_t last_cos;                 /* A of the last frame, 16.16 */
    int32_t last_sin;                 /* D of the last frame, 16.16 */
} rotation_bitmap_state_t;

volatile rotation_bitmap_state_t g_rotation_bitmap;

static uint16_t g_row[BITMAP_W / 2u];

/* Palette indices: 1/2 and 3/4 are the two checker colours in two shades,
 * 5 the frame and 6 the cross through the middle. Index 0 stays unused. */
static uint8_t pixel(uint32_t x, uint32_t y) {
    if (x < 4u || y < 4u || x >= BITMAP_W - 4u || y >= BITMAP_H - 4u) return 5u;
    if ((x >= 252u && x < 260u) || (y >= 124u && y < 132u)) return 6u;
    return (uint8_t)(1u + ((((x >> 5) + (y >> 5)) & 1u) << 1) + (((x >> 3) + (y >> 3)) & 1u));
}

static void upload_palette(void) {
    uint16_t pal[256];
    for (uint32_t i = 0u; i < 256u; ++i) pal[i] = 0u;
    pal[1] = SAT_RGB555(3, 6, 18);
    pal[2] = SAT_RGB555(5, 10, 26);
    pal[3] = SAT_RGB555(18, 5, 5);
    pal[4] = SAT_RGB555(26, 9, 9);
    pal[5] = SAT_RGB555(31, 31, 31);
    pal[6] = SAT_RGB555(31, 28, 6);
    sat_example_must(sat_vdp2_palette_upload(pal, 256u, 0u));
}

static void upload_bitmap(void) {
    for (uint32_t y = 0u; y < BITMAP_H; ++y) {
        for (uint32_t x = 0u; x < BITMAP_W; x += 2u) {
            g_row[x / 2u] = (uint16_t)(((uint32_t)pixel(x, y) << 8) | pixel(x + 1u, y));
        }
        sat_example_must(sat_vdp2_vram_write_words((BITMAP_ADDRESS + y * BITMAP_W) / 2u, g_row,
                                                   BITMAP_W / 2u));
    }
}

static void config_bitmap(sat_vdp2_rotation_bitmap_config_t* cfg) {
    sat_vdp2_rotation_bitmap_config_default(cfg);
    cfg->color_mode = SAT_VDP2_COLOR_MODE_256;
    cfg->priority = 2u;
    cfg->bitmap_address = BITMAP_ADDRESS;
    cfg->table_address = REGION;
}

static void plan_vram(void) {
    sat_vdp2_rotation_bitmap_config_t cfg;
    config_bitmap(&cfg);
    const uint32_t bytes = sat_vdp2_rotation_bitmap_bytes(&cfg);
    g_rotation_bitmap.bitmap_bytes = bytes;

    sat_example_must(sat_vdp2_plan_begin());
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_PARAMS, REGION, REGION_BYTES));
    g_rotation_bitmap.refused_plan_unclaimed =
        (uint32_t)sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_BITMAP, BITMAP_ADDRESS, bytes);
    sat_example_must(sat_vdp2_plan_commit());

    /* The whole bank belongs to RBG0's picture; no NBG kind lives there. */
    sat_example_must(sat_vdp2_plan_claim_banks(SAT_VDP2_BANK_B0));
    sat_example_must(sat_vdp2_plan_begin());
    g_rotation_bitmap.refused_plan_misaligned =
        (uint32_t)sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_BITMAP, 0x50000u, 0x100u);
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_BITMAP, BITMAP_ADDRESS, bytes));
    sat_example_must(sat_vdp2_plan_commit());
}

/* One turn every 128 frames; the zoom swings between 1.0 and 1.5 of the
 * picture's scale (k above 1 shows more of the picture). */
static void update_params(uint32_t frame) {
    const sat_angle16_t angle = (sat_angle16_t)(frame * 512u);
    const int32_t c = sat_cos16(angle);
    const int32_t s = sat_sin16(angle);
    const int32_t k = 0x14000 + (sat_sin16((sat_angle16_t)(frame * 300u)) >> 2);
    sat_vdp2_rotation_params_t p = {0};
    /* X = k * (A (H - px) + B (V - py)) + px + mx, so the turn is about the
     * screen's middle and the shift mx, my puts the picture's middle there. */
    p.dyst = 0x10000;
    p.dx = 0x10000;
    p.matrix[0] = c;
    p.matrix[1] = -s;
    p.matrix[3] = s;
    p.matrix[4] = c;
    p.px = SCREEN_W / 2;
    p.py = SCREEN_H / 2;
    p.cx = SCREEN_W / 2;
    p.cy = SCREEN_H / 2;
    p.mx = (int32_t)((BITMAP_W / 2u - SCREEN_W / 2u) << 16);
    p.my = (int32_t)((BITMAP_H / 2u - SCREEN_H / 2u) << 16);
    p.kx = k;
    p.ky = k;
    g_rotation_bitmap.params_result = (uint32_t)sat_vdp2_rotation_write_params(REGION, &p, &p);
    if (g_rotation_bitmap.params_result == SAT_OK) ++g_rotation_bitmap.params_writes;
    g_rotation_bitmap.last_k = k;
    g_rotation_bitmap.last_cos = c;
    g_rotation_bitmap.last_sin = s;
}

/* The refused combinations leave the live configuration alone. */
static void try_refusals(void) {
    sat_vdp2_rotation_bitmap_config_t cfg;

    config_bitmap(&cfg);
    cfg.bitmap_address = 0x50000u;
    g_rotation_bitmap.refused_misaligned = (uint32_t)sat_vdp2_rotation_bitmap_configure(&cfg);

    config_bitmap(&cfg);
    cfg.over = SAT_VDP2_ROTATION_OVER_PATTERN;
    g_rotation_bitmap.refused_over_pattern = (uint32_t)sat_vdp2_rotation_bitmap_configure(&cfg);

    config_bitmap(&cfg);
    cfg.table_address = BITMAP_ADDRESS;
    g_rotation_bitmap.refused_table_in_bank = (uint32_t)sat_vdp2_rotation_bitmap_configure(&cfg);

    g_rotation_bitmap.refused_rbg1 = (uint32_t)sat_vdp2_rotation_rbg1_set_enabled(1u);
}

int main(void) {
    if (sat_app_init_default() != SAT_OK) return 1;
    g_rotation_bitmap.magic = RBM_MAGIC;

    upload_palette();
    plan_vram();
    upload_bitmap();

    sat_vdp2_rotation_bitmap_config_t cfg;
    config_bitmap(&cfg);
    g_rotation_bitmap.configure_result = (uint32_t)sat_vdp2_rotation_bitmap_configure(&cfg);
    update_params(0u);
    try_refusals();

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        if (sat_pad_poll(&pad) != SAT_OK) break;
        ++g_rotation_bitmap.frames;
        update_params(g_rotation_bitmap.frames);
        if (sat_vdp2_layers_commit() != SAT_OK) break;
        if (sat_begin_frame() != SAT_OK) break;
        if (sat_end_frame() != SAT_OK) break;
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
