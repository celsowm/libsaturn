/* vdp2_rotation_coefficients: RBG0's parameter sets A and B and the
 * coefficient table contract, live (E02).
 *
 * An endless procedural ocean on the rotation scroll screen, with a sky of
 * colour bands on NBG0 above the horizon. Four scenes cycle, each one a
 * documented behaviour of the modern rotation manager:
 *
 *   0 SWELL   parameter set A, coefficient mode kx/ky: k(y) = focal/depth
 *             plus a sine that travels toward the horizon; rows at the
 *             horizon hide through the coefficient flag bit.
 *   1 WAVE    parameter set A, coefficient mode Xp: each line's sampling
 *             origin is displaced by a travelling sine - negative values,
 *             which the sign-magnitude entry format now carries correctly.
 *   2 YAW B   parameter set B (RPMD 1): the same floor yawed 30 degrees by
 *             its rotation matrix, its own coefficient table driving k.
 *   3 MIX     RPMD 2: the coefficient flag bit picks the image per line -
 *             the yawed B above an animated boundary, the flat A below.
 *
 * Every frame writes the NEXT frame's parameter region and coefficient
 * tables into the inactive half of a double buffer while the VDP2 reads
 * the active half; sat_vdp2_layers_commit() switches RPTA and both KTAOF
 * offsets in VBlank, so no table is ever rewritten while it is being read.
 *
 * The VRAM layout is owned by the transactional resource plan: the sky and
 * the rotation parameter regions and coefficient tables are reserved as
 * their kinds, banks A0/A1 are whole-claimed for rotation, and the rotation
 * map and tile window are reserved as RotationNames/RotationChars - the
 * kinds that only live inside claimed banks.
 *
 * The illegal combinations are demonstrated and recorded in
 * g_rotation_coefficients: the window parameter mode (RPMD 3, not modeled)
 * is refused by both configure and set_parameter_mode, and a parameter
 * region whose byte address has bit 7 set (RPTA bit 6 is ignored, fixed to
 * 0 for A) is refused by write_params before anything is written.
 *
 * Controls: A advances to the next scene (they also advance every 60
 * frames), B toggles the debug overlay, START exits.
 */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/render2d.h"
#include "saturn/saturn.h"
#include "saturn/vdp2_alloc.h"
#include "saturn/vdp2_layers.h"
#include "saturn/vdp2_rotation_coefficients.h"
#include "example_util.h"

#define RCF_MAGIC SAT_FOURCC('R', 'C', 'F', '1')
#define STATES 4u
#define STATE_FRAMES 60u
#define CAPTURE_FRAME 200u
#define CAPTURE_ROWS 5u

/* ---- VRAM layout (bytes; every range is reserved through the plan) ---- */
#define ROT_NAMES_BYTES  0x2000u    /* one 64x64 plane of 1-word names */
#define ROT_CHARS_BYTES  0x100u     /* the tile window in A1 */
#define REGION0          0x40000u   /* parameter region ping (A+B: 0xE0) */
#define REGION1          0x40200u   /* parameter region pong */
#define COEF_A0          0x40400u   /* coefficient tables: A ping */
#define COEF_A1          0x40800u   /* A pong */
#define COEF_B0          0x40C00u   /* B ping */
#define COEF_B1          0x41000u   /* B pong */
#define COEF_BYTES       (224u * 4u)
#define SKY_NAMES        0x44000u
#define SKY_CHARS        0x48000u
#define SKY_CHARS_BYTES  0x100u

#define SCREEN_H 224u
#define YAW_COS 56756               /* cos(30 degrees), 16.16 */
#define YAW_SIN 32768               /* sin(30 degrees), 16.16 */

typedef struct rotation_coefficients_results {
    uint32_t magic;
    uint32_t frames;
    uint32_t state;                 /* 0..3 */
    uint32_t state_ran;             /* a bit per scene that ran */
    uint32_t parameter_mode;        /* RPMD the manager holds now */
    uint32_t coefficient_mode_a;    /* A's contract mode: 0 kx/ky, 3 Xp */
    uint32_t refused_set_window;    /* set_parameter_mode(3) */
    uint32_t refused_configure_window;  /* configure with mode 3 */
    uint32_t refused_region_bit7;   /* write_params into 0x40080 */
    uint32_t transition_result[STATES];  /* the call that entered each scene */
    uint32_t params_writes;         /* write_params calls (one per frame) */
    uint32_t coefficient_writes;    /* write_coefficients calls (two per frame) */
    uint32_t region;                /* the region written for the next frame */
    uint32_t capture_frame;
    uint32_t capture_state;
    uint32_t capture_boundary;       /* the MIX scene's boundary row */
    int32_t  capture_xp_base;       /* A's rotated viewpoint base, 16.16 */
    int32_t  capture_coef_a[CAPTURE_ROWS];   /* raw A entries at capture */
    uint32_t capture_flags_a[CAPTURE_ROWS];
    int32_t  capture_coef_b[CAPTURE_ROWS];   /* raw B entries at capture */
    uint32_t capture_region;        /* which region the samples went to */
} rotation_coefficients_results_t;

volatile rotation_coefficients_results_t g_rotation_coefficients;

static sat_ascii_font_t font;
static uint16_t g_names[64u * 64u];
static uint16_t g_cells[8u * 16u];
static uint32_t g_coef_a[SCREEN_H];
static uint32_t g_coef_b[SCREEN_H];
static uint8_t g_flags_a[SCREEN_H];
static uint32_t g_debug_overlay = 1u;

static const sat_vdp2_rotation_coefficients_config_t ocean = {
    .horizon = 96u,
    .cx = 160u,
    .focal = 96u,
    .min_depth = 8u,
    .ground_forward = 96u,
    .swell_amplitude = 2 * 65536,
    .swell_wavelength = 64u,
    .swell_period = 128u,
    .wave_amplitude = 12 * 65536,
    .wave_wavelength = 32u,
    .wave_period = 64u,
};

/* ---- tiles and palettes ---------------------------------------------- */

static void upload_palettes(void) {
    uint16_t pal[16];
    for (uint32_t i = 0u; i < 16u; ++i) pal[i] = 0u;
    pal[1] = SAT_RGB555(2, 6, 17);      /* deep water */
    pal[2] = SAT_RGB555(4, 13, 25);     /* stripe water */
    pal[3] = SAT_RGB555(9, 24, 28);     /* crest */
    pal[4] = SAT_RGB555(28, 28, 28);    /* grid */
    (void)sat_vdp2_palette_upload(pal, 16u, 1u * 16u);
    for (uint32_t i = 0u; i < 16u; ++i) pal[i] = 0u;
    pal[1] = SAT_RGB555(4, 7, 19);      /* sky bands, dark to light */
    pal[2] = SAT_RGB555(6, 10, 26);
    pal[3] = SAT_RGB555(9, 14, 30);
    (void)sat_vdp2_palette_upload(pal, 16u, 2u * 16u);
}

static void build_ocean_cells(void) {
    /* tile 0 transparent, 1 solid water, 2 stripes, 3 crest, 4 grid node */
    for (uint32_t t = 0u; t < 5u; ++t) {
        for (uint32_t i = 0u; i < 16u; ++i) g_cells[t * 16u + i] = 0u;
    }
    for (uint32_t i = 0u; i < 16u; ++i) g_cells[16u + i] = 0x1111u;
    for (uint32_t r = 0u; r < 16u; ++r) {
        const uint16_t row = (r % 4u == 3u) ? 0x2222u : 0x1111u;
        for (uint32_t c = 0u; c < 2u; ++c) g_cells[32u + r * 2u + c] = row;
    }
    for (uint32_t r = 0u; r < 16u; ++r) {
        const uint16_t row = (r < 4u) ? 0x3333u : ((r % 4u == 1u) ? 0x3333u : 0x1111u);
        for (uint32_t c = 0u; c < 2u; ++c) g_cells[48u + r * 2u + c] = row;
    }
    for (uint32_t i = 0u; i < 16u; ++i) g_cells[64u + i] = 0x1414u;
}

static void build_ocean_names(void) {
    for (uint32_t y = 0u; y < 64u; ++y) {
        for (uint32_t x = 0u; x < 64u; ++x) {
            uint16_t cell = 2u;                      /* striped water */
            if ((x % 8u) == 0u || (y % 8u) == 0u) cell = 4u;   /* grid */
            if (((x + y) % 16u) == 0u) cell = 3u;    /* diagonal crests */
            g_names[y * 64u + x] = (uint16_t)((1u << 12u) | cell);
        }
    }
}

static void build_sky_cells(void) {
    for (uint32_t t = 0u; t < 5u; ++t) {
        for (uint32_t i = 0u; i < 16u; ++i) g_cells[t * 16u + i] = 0u;
    }
    for (uint32_t t = 1u; t <= 3u; ++t) {
        for (uint32_t i = 0u; i < 16u; ++i) g_cells[t * 16u + i] = (uint16_t)(0x1111u * t);
    }
}

static void build_sky_names(void) {
    for (uint32_t y = 0u; y < 64u; ++y) {
        const uint32_t band = y < 4u ? 1u : (y < 8u ? 2u : 3u);
        for (uint32_t x = 0u; x < 64u; ++x) {
            g_names[y * 64u + x] = (uint16_t)((2u << 12u) | band);
        }
    }
}

/* ---- scenes ------------------------------------------------------------ */

/* The MIX scene's boundary row: an oscillation around two thirds depth. */
static uint32_t mix_boundary(uint32_t frame) {
    const int32_t sway = sat_vdp2_rotation_coefficients_sine((frame << 16) / 96u);
    return ocean.horizon + 32u + (uint32_t)((sway * 16) >> 16);
}

/* Fills both coefficient tables for `frame` of scene `state`; the entries
 * carry the flag bit the scene needs (HIDE above the horizon, or USE_B
 * above the MIX boundary). */
static void build_coefficients(uint32_t state, uint32_t frame) {
    const uint32_t boundary = mix_boundary(frame);
    for (uint32_t y = 0u; y < SCREEN_H; ++y) {
        uint32_t entry_a = 0u;
        uint32_t flags_a = 0u;
        int32_t value_b = 0;
        uint32_t flags_b = 0u;
        switch (state) {
            case 0u:   /* SWELL: k(y) plus the travelling ripple */
                if (y <= ocean.horizon) {
                    flags_a = SAT_VDP2_COEFFICIENT_HIDE;
                } else {
                    value_b = 0;
                    entry_a = (uint32_t)sat_vdp2_rotation_coefficients_swell(&ocean, y, frame);
                }
                break;
            case 1u: {  /* WAVE: Xp base plus the travelling sine */
                if (y <= ocean.horizon) {
                    flags_a = SAT_VDP2_COEFFICIENT_HIDE;
                } else {
                    const int32_t base = sat_vdp2_rotation_coefficients_xp_base(
                        SAT_VDP2_ROTATION_COEFFICIENTS_ONE, ocean.cx, 0);
                    entry_a = (uint32_t)(base
                        + sat_vdp2_rotation_coefficients_wave(&ocean, y, frame));
                }
                break;
            }
            case 2u:   /* YAW B: only B's table matters */
                value_b = sat_vdp2_rotation_coefficients_floor_k(&ocean, y);
                break;
            default: {  /* MIX: the flag bit slices the two images */
                entry_a = (uint32_t)sat_vdp2_rotation_coefficients_floor_k(&ocean, y);
                if (y < boundary) flags_a = SAT_VDP2_COEFFICIENT_USE_B;
                value_b = sat_vdp2_rotation_coefficients_floor_k(&ocean, y);
                break;
            }
        }
        if (state == 2u) {
            g_coef_b[y] = sat_vdp2_rotation_coefficient_2word(value_b, flags_b);
            g_coef_a[y] = sat_vdp2_rotation_coefficient_2word(0, 0u);
        } else {
            g_coef_a[y] = sat_vdp2_rotation_coefficient_2word((int32_t)entry_a, flags_a);
            g_coef_b[y] = sat_vdp2_rotation_coefficient_2word(value_b, flags_b);
        }
        g_flags_a[y] = (uint8_t)flags_a;
    }
}

static void capture_samples(uint32_t state, uint32_t frame, uint32_t region) {
    g_rotation_coefficients.capture_frame = frame;
    g_rotation_coefficients.capture_state = state;
    g_rotation_coefficients.capture_boundary = mix_boundary(frame);
    /* The yawed set B's rotated viewpoint coordinate: (1 - cos) * cx.
     * Set A's is the camera offset, which rests at 0 here. */
    g_rotation_coefficients.capture_xp_base =
        sat_vdp2_rotation_coefficients_xp_base(YAW_COS, ocean.cx, 0);
    static const uint32_t rows[CAPTURE_ROWS] = {16u, 96u, 112u, 160u, 200u};
    for (uint32_t i = 0u; i < CAPTURE_ROWS; ++i) {
        const uint32_t y = rows[i];
        g_rotation_coefficients.capture_coef_a[i] = (int32_t)g_coef_a[y];
        g_rotation_coefficients.capture_flags_a[i] = g_flags_a[y];
        g_rotation_coefficients.capture_coef_b[i] = (int32_t)g_coef_b[y];
    }
    g_rotation_coefficients.capture_region = region;
}

/* Writes next frame's parameter region and both coefficient tables into
 * the inactive half of the double buffer. */
static void update_scene(uint32_t state, uint32_t frame) {
    const uint32_t write_region = (frame & 1u) ? REGION0 : REGION1;
    const uint32_t coef_a = (frame & 1u) ? COEF_A0 : COEF_A1;
    const uint32_t coef_b = (frame & 1u) ? COEF_B0 : COEF_B1;

    build_coefficients(state, frame);
    sat_vdp2_rotation_params_t pa;
    sat_vdp2_rotation_coefficients_build_params_a(&ocean, coef_a, &pa);
    sat_vdp2_rotation_params_t pb;
    sat_vdp2_rotation_coefficients_build_params_b(&ocean, YAW_COS, YAW_SIN, coef_b, &pb);
    /* The camera rests at the origin; Mx/My stay 0 (the waves animate). */
    if (sat_vdp2_rotation_write_params(write_region, &pa, &pb) == SAT_OK) {
        ++g_rotation_coefficients.params_writes;
        g_rotation_coefficients.region = write_region;
    }
    if (sat_vdp2_rotation_write_coefficients(coef_a, g_coef_a, SCREEN_H) == SAT_OK) {
        ++g_rotation_coefficients.coefficient_writes;
    }
    (void)sat_vdp2_rotation_write_coefficients(coef_b, g_coef_b, SCREEN_H);
    ++g_rotation_coefficients.coefficient_writes;
    if (frame == CAPTURE_FRAME) capture_samples(state, frame, write_region);
}

/* The rotation configuration of one scene: only the parameter mode and
 * A's coefficient mode change between them. */
static sat_result_t configure_rotation(uint32_t state) {
    sat_vdp2_rotation_config_t cfg;
    sat_vdp2_rotation_config_default(&cfg);
    cfg.color_mode = SAT_VDP2_COLOR_MODE_16;
    cfg.pattern_name_words = 1u;
    cfg.priority = 2u;
    cfg.transparent = 1u;
    cfg.over = SAT_VDP2_ROTATION_OVER_REPEAT;
    cfg.name_bank_mask = SAT_VDP2_BANK_A0;
    cfg.char_bank_mask = SAT_VDP2_BANK_A1;
    cfg.plane_address[0] = 0x00000u;
    for (uint32_t p = 1u; p < 16u; ++p) cfg.plane_address[p] = 0x00000u;
    cfg.char_base_address = 0x28000u;
    cfg.table_address = REGION0;
    cfg.coefficients_a.enabled = 1u;
    cfg.coefficients_b.enabled = 1u;
    cfg.coefficients_b.mode = SAT_VDP2_COEFFICIENT_KXKY;
    switch (state) {
        case 0u:
            cfg.parameter_mode = SAT_VDP2_ROTATION_PARAMETER_A;
            cfg.coefficients_a.mode = SAT_VDP2_COEFFICIENT_KXKY;
            break;
        case 1u:
            cfg.parameter_mode = SAT_VDP2_ROTATION_PARAMETER_A;
            cfg.coefficients_a.mode = SAT_VDP2_COEFFICIENT_XP;
            break;
        case 2u:
            cfg.parameter_mode = SAT_VDP2_ROTATION_PARAMETER_B;
            cfg.coefficients_a.mode = SAT_VDP2_COEFFICIENT_KXKY;
            break;
        default:
            cfg.parameter_mode = SAT_VDP2_ROTATION_PARAMETER_COEFFICIENT;
            cfg.coefficients_a.mode = SAT_VDP2_COEFFICIENT_KXKY;
            break;
    }
    const sat_result_t result = sat_vdp2_rotation_configure(&cfg);
    g_rotation_coefficients.parameter_mode = cfg.parameter_mode;
    g_rotation_coefficients.coefficient_mode_a = cfg.coefficients_a.mode;
    return result;
}

/* Entering a scene uses the precise API for what changes: configure when
 * A's coefficient mode changes, the live parameter-mode switch otherwise. */
static void enter_state(uint32_t state) {
    sat_result_t result;
    const uint32_t previous = g_rotation_coefficients.state;
    const uint32_t mode_a_now = g_rotation_coefficients.coefficient_mode_a;
    const uint32_t mode_a_next = state == 1u ? SAT_VDP2_COEFFICIENT_XP
                                             : SAT_VDP2_COEFFICIENT_KXKY;
    if (mode_a_next != mode_a_now || previous == 0u) {
        result = configure_rotation(state);
    } else {
        static const uint8_t rpmd[STATES] = {
            SAT_VDP2_ROTATION_PARAMETER_A, SAT_VDP2_ROTATION_PARAMETER_A,
            SAT_VDP2_ROTATION_PARAMETER_B, SAT_VDP2_ROTATION_PARAMETER_COEFFICIENT};
        result = sat_vdp2_rotation_set_parameter_mode(rpmd[state]);
        g_rotation_coefficients.parameter_mode = rpmd[state];
        g_rotation_coefficients.coefficient_mode_a = mode_a_next;
    }
    g_rotation_coefficients.transition_result[state] = result;
    g_rotation_coefficients.state = state;
    g_rotation_coefficients.state_ran |= 1u << state;
}

/* ---- VRAM through the resource plan ----------------------------------- */

static void plan_vram(void) {
    sat_example_must(sat_vdp2_plan_begin());
    /* The sky and the rotation tables live in the unclaimed banks. */
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_PATTERN_NAMES, SKY_NAMES, 0x2000u));
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_CHARACTERS, SKY_CHARS, SKY_CHARS_BYTES));
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_PARAMS, REGION0, 0xE0u));
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_PARAMS, REGION1, 0xE0u));
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_COEFFICIENTS, COEF_A0, COEF_BYTES));
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_COEFFICIENTS, COEF_A1, COEF_BYTES));
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_COEFFICIENTS, COEF_B0, COEF_BYTES));
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_COEFFICIENTS, COEF_B1, COEF_BYTES));
    sat_example_must(sat_vdp2_plan_commit());
    /* Banks A0/A1 go to rotation wholesale; no NBG kind lives there. */
    sat_example_must(sat_vdp2_plan_claim_banks(SAT_VDP2_BANK_A0 | SAT_VDP2_BANK_A1));
    /* RBG0's own image data only exists inside claimed banks. */
    sat_example_must(sat_vdp2_plan_begin());
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_NAMES, 0x00000u, ROT_NAMES_BYTES));
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_CHARS, 0x28000u, ROT_CHARS_BYTES));
    sat_example_must(sat_vdp2_plan_commit());
}

static void init_sky(void) {
    build_sky_cells();
    sat_example_must(sat_vdp2_vram_write_words(SKY_CHARS / 2u, g_cells, 5u * 16u));
    build_sky_names();
    sat_example_must(sat_vdp2_vram_write_words(SKY_NAMES / 2u, g_names, 64u * 64u));
    sat_vdp2_layer_config_t cfg;
    sat_vdp2_layer_config_default(SAT_VDP2_NBG0, &cfg);
    for (uint32_t p = 0u; p < 4u; ++p) cfg.plane_address[p] = SKY_NAMES;
    cfg.char_base_address = SKY_CHARS;
    cfg.char_bank_mask = SAT_VDP2_BANK_B0;
    cfg.priority = 1u;
    sat_example_must(sat_vdp2_layer_configure(&cfg));
}

static void init_ocean(void) {
    build_ocean_cells();
    sat_example_must(sat_vdp2_vram_write_words(0x28000u / 2u, g_cells, 5u * 16u));
    build_ocean_names();
    sat_example_must(sat_vdp2_vram_write_words(0x00000u / 2u, g_names, 64u * 64u));
}

/* ---- main --------------------------------------------------------------- */

static void draw_debug_overlay(void) {
    static const char* const names[STATES] = {"SWELL", "WAVE", "YAW B", "MIX"};
    (void)sat_ascii_font_draw_text_screen_indexed8(&font, names[g_rotation_coefficients.state],
                                                   8u, 24u, 8u, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&font, "MODE ",
                                        g_rotation_coefficients.parameter_mode,
                                        72u, 24u, 8u, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&font, "KMD ",
                                        g_rotation_coefficients.coefficient_mode_a,
                                        120u, 24u, 8u, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&font, "PW ",
                                        g_rotation_coefficients.params_writes,
                                        168u, 24u, 8u, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&font, "CW ",
                                        g_rotation_coefficients.coefficient_writes,
                                        216u, 24u, 8u, 0u, 0u);
}

int main(void) {
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) {
        return 1;
    }
    g_rotation_coefficients.magic = RCF_MAGIC;

    upload_palettes();
    plan_vram();
    init_ocean();
    init_sky();

    /* The illegal combinations, recorded for the harness. */
    g_rotation_coefficients.refused_set_window =
        sat_vdp2_rotation_set_parameter_mode(3u);
    {
        sat_vdp2_rotation_config_t window_cfg;
        sat_vdp2_rotation_config_default(&window_cfg);
        window_cfg.parameter_mode = 3u;
        window_cfg.name_bank_mask = SAT_VDP2_BANK_A0;
        window_cfg.char_bank_mask = SAT_VDP2_BANK_A1;
        window_cfg.table_address = REGION0;
        g_rotation_coefficients.refused_configure_window =
            sat_vdp2_rotation_configure(&window_cfg);
    }
    {
        /* RPTA bit 6 is ignored and fixed: a region at |0x80 is not what it
         * seems (p06_32), so write_params refuses it outright. */
        sat_vdp2_rotation_params_t p;
        sat_vdp2_rotation_coefficients_build_params_a(&ocean, COEF_A0, &p);
        g_rotation_coefficients.refused_region_bit7 =
            sat_vdp2_rotation_write_params(REGION0 | 0x80u, &p, &p);
    }

    enter_state(0u);
    uint32_t next = 1u;

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        if (sat_pad_poll(&pad) != SAT_OK) break;
        ++g_rotation_coefficients.frames;
        if (next < STATES
            && (((pad.pressed & SAT_PAD_A) != 0u)
                || g_rotation_coefficients.frames >= (next * STATE_FRAMES))) {
            enter_state(next);
            ++next;
        }
        update_scene(g_rotation_coefficients.state, g_rotation_coefficients.frames);
        if (sat_vdp2_layers_commit() != SAT_OK) break;
        if (sat_begin_frame() != SAT_OK) break;
        if ((pad.pressed & SAT_PAD_B) != 0u) g_debug_overlay ^= 1u;
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "ROTATION COEFFICIENTS",
                                                       8u, 4u, 8u, 0u, 0u);
        if (g_debug_overlay) draw_debug_overlay();
        if (sat_end_frame() != SAT_OK) break;
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
