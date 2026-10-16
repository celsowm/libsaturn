/* vdp2_dual_rotation: RBG1, the second rotation screen, beside RBG0 (E01).
 *
 * Two independently moving planes on the two rotation scroll screens, each
 * with its own distinct pattern so a wrong bank mapping shows immediately:
 *
 *   RBG0 (parameter A) - a slate-blue framed grid with transparent holes,
 *                        spinning clockwise, drifting through Mx/My.
 *   RBG1 (parameter B) - an orange/dark-red fine checker, spinning the
 *                        other way at its own rate, drifting separately.
 *
 * RBG1 is always drawn by rotation parameter B (manual 6.3) and needs RBG0
 * with parameter mode A ("Mode 0 must be set when displaying RBG1", the
 * RPMD page), and no normal scroll screen may display while it does
 * (manual 6.2). Its image data is fixed by the manual: the pattern names in
 * VRAM-B1, the character window in VRAM-B0 - so RBG0's own banks stay in
 * VRAM-A while RBG1 displays.
 *
 * Three pad modes (A skips ahead; they also advance every 120 frames):
 *
 *   0 A-ONLY    parameter A on RBG0, RBG1 hidden: the spinning grid.
 *   1 B-ON-RBG0 parameter B on RBG0 (RPMD 1), RBG1 hidden: the same
 *               checker image RBG1 would show, read through RBG0's own
 *               name banks - the live sat_vdp2_rotation_set_parameter_mode
 *               path.
 *   2 DUAL      parameter A on RBG0 and parameter B on RBG1 at once: the
 *               grid over the checker, RBG1 visible through RBG0's
 *               transparent holes.
 *
 * Every frame writes the next frame's A+B parameter region into the
 * inactive half of a double buffer while the VDP2 reads the active half;
 * sat_vdp2_layers_commit() switches RPTA in VBlank.
 *
 * The illegal combinations are demonstrated and recorded in
 * g_dual_rotation: RBG1 with a non-A parameter mode, RBG1 while RBG0's
 * banks sit in VRAM-B, parameter B's map outside VRAM-B1, a normal screen
 * configured while RBG1 displays, a parameter-mode switch away from A
 * while RBG1 displays, and RBG1 enabled while parameter mode is B.
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

#define DROT_MAGIC SAT_FOURCC('D', 'R', 'O', 'T')
#define MODES 3u
#define MODE_FRAMES 120u
#define CAPTURE_FRAME 300u

/* ---- VRAM layout (bytes; every range is reserved through the plan) ---- */
#define GRID_NAMES      0x00000u    /* RBG0's map: 64x64 1-word names (A0) */
#define CHECK_NAMES_A0  0x08000u    /* the checker copy RBG0's B-mode reads (A0) */
#define CHECK_NAMES_B1  0x60000u    /* RBG1's own map (VRAM-B1, fixed) */
#define REGION0         0x10000u    /* A+B parameter region ping (0xE0) */
#define REGION1         0x10200u    /* pong */
#define GRID_CHARS      0x28000u    /* RBG0's character window (VRAM-A1) */
#define CHECK_CHARS     0x40000u    /* RBG1's character window (VRAM-B0, fixed) */
#define NAMES_BYTES     0x2000u
#define CHARS_BYTES     0x100u

#define SCREEN_W 320u
#define CX 160
#define CY 112
#define SPIN_A_TURNS 240u    /* frames per RBG0 revolution */
#define SPIN_B_TURNS 180u    /* frames per RBG1 revolution, opposite way */

typedef struct dual_rotation_results {
    uint32_t magic;
    uint32_t frames;
    uint32_t mode;                 /* 0..2 */
    uint32_t mode_ran;              /* a bit per mode that ran */
    uint32_t parameter_mode;        /* RPMD the manager holds now */
    uint32_t rbg1_enabled;
    uint32_t refused_configure_mode2;     /* RBG1 + parameter COEFFICIENT */
    uint32_t refused_banks_in_vram_b;     /* RBG0 names claimed into B0 */
    uint32_t refused_map_outside_b1;      /* parameter B's map in A1 */
    uint32_t refused_nbg_while_dual;      /* NBG0 configured while RBG1 shows */
    uint32_t refused_mode_switch_while_rbg1;  /* parameter COEFFICIENT live in DUAL */
    uint32_t refused_rbg1_enable_in_mode_b;   /* RBG1 on while mode B */
    uint32_t transition_result[MODES];   /* the call that entered each mode */
    uint32_t live_hide_result;            /* rbg1_set_enabled(0) leaving DUAL */
    uint32_t params_writes;               /* write_params calls (one per frame) */
    uint32_t region;                      /* the region written for the next frame */
    uint32_t capture_frame;
    uint32_t capture_mode;
    int32_t  capture_cos_a, capture_sin_a;   /* parameter A's matrix, 16.16 */
    int32_t  capture_cos_b, capture_sin_b;   /* parameter B's matrix, 16.16 */
    int32_t  capture_mx_a, capture_my_a;     /* parameter A's pivot drift */
    int32_t  capture_mx_b, capture_my_b;     /* parameter B's pivot drift */
    uint32_t capture_region;
    uint32_t rbg0_priority;
    uint32_t rbg1_priority;
} dual_rotation_results_t;

volatile dual_rotation_results_t g_dual_rotation;

static sat_ascii_font_t font;
static uint16_t g_names[64u * 64u];
static uint16_t g_cells[8u * 16u];
static uint32_t g_debug_overlay = 1u;

/* The fixed-point sine of the public scene-math header: quarter-wave
 * Taylor series, 16.16 turns in, 16.16 out (error under 0.05%). */
static int32_t sine16(uint32_t turns) {
    return sat_vdp2_rotation_coefficients_sine(turns);
}

static int32_t cosine16(uint32_t turns) {
    return sine16(turns + 0x4000u);   /* cos(t) = sin(t + 1/4 turn) */
}

/* ---- tiles, palettes and maps --------------------------------------------- */

static void upload_palettes(void) {
    uint16_t pal[16];
    for (uint32_t i = 0u; i < 16u; ++i) pal[i] = 0u;
    pal[1] = SAT_RGB555(6, 10, 22);     /* slate grid fill */
    pal[2] = SAT_RGB555(28, 28, 30);    /* grid frame lines */
    (void)sat_vdp2_palette_upload(pal, 16u, 1u * 16u);
    for (uint32_t i = 0u; i < 16u; ++i) pal[i] = 0u;
    pal[1] = SAT_RGB555(28, 14, 2);     /* warm checker */
    pal[2] = SAT_RGB555(12, 4, 2);      /* dark checker */
    (void)sat_vdp2_palette_upload(pal, 16u, 2u * 16u);
}

/* RBG0's character window holds BOTH planes' tiles: whichever parameter
 * set draws RBG0, the characters come through RBG0's own window (CHCTLB/
 * PNCR are RBG0's screen registers). RBG1 fetches its own window in B0.
 * tile 1: slate grid fill, tile 2: grid frame with a transparent middle
 * (RBG1 shows through it in DUAL), tile 3: warm checker, tile 4: dark
 * checker. */
static void upload_tiles(void) {
    for (uint32_t t = 0u; t < 5u * 16u; ++t) g_cells[t] = 0u;
    for (uint32_t i = 0u; i < 16u; ++i) g_cells[16u + i] = 0x1111u;
    for (uint32_t r = 0u; r < 16u; ++r) {
        uint16_t row = 0u;
        if (r == 0u || r == 15u) row = 0x2222u;
        else row = (uint16_t)((r == 1u || r == 14u) ? 0x2222u : 0x0000u);
        g_cells[32u + r * 2u] = row;
        g_cells[32u + r * 2u + 1u] = row;
    }
    for (uint32_t i = 0u; i < 16u; ++i) g_cells[48u + i] = 0x1111u;
    for (uint32_t i = 0u; i < 16u; ++i) g_cells[64u + i] = 0x2222u;
    sat_example_must(sat_vdp2_vram_write_words(GRID_CHARS / 2u, g_cells, 5u * 16u));
    /* RBG1's own window in B0: just the two checker tiles */
    for (uint32_t i = 0u; i < 16u; ++i) g_cells[i] = 0x1111u;
    for (uint32_t i = 0u; i < 16u; ++i) g_cells[16u + i] = 0x2222u;
    sat_example_must(sat_vdp2_vram_write_words(CHECK_CHARS / 2u, g_cells, 2u * 16u));
}

/* The grid map RBG0's A-mode reads: framed cells (tiles 1/2, palette 1). */
static void upload_grid_names(void) {
    for (uint32_t y = 0u; y < 64u; ++y) {
        for (uint32_t x = 0u; x < 64u; ++x) {
            uint16_t cell = 1u;
            if ((x % 4u) == 0u || (y % 4u) == 0u) cell = 2u;
            g_names[y * 64u + x] = (uint16_t)((1u << 12u) | cell);
        }
    }
    sat_example_must(sat_vdp2_vram_write_words(GRID_NAMES / 2u, g_names, 64u * 64u));
}

/* The checker map, twice: the copy RBG0's B-mode reads through its own
 * name banks names tiles 3/4 in RBG0's window; RBG1's own map in B1
 * names tiles 1/2 in its B0 window. Same picture, two fetch paths - a
 * wrong bank mapping shows the wrong pattern immediately. */
static void upload_checker_names(uint32_t at_byte, uint16_t tile_warm, uint16_t tile_dark) {
    for (uint32_t y = 0u; y < 64u; ++y) {
        for (uint32_t x = 0u; x < 64u; ++x) {
            const uint16_t cell = ((x ^ y) & 1u) != 0u ? tile_dark : tile_warm;
            g_names[y * 64u + x] = (uint16_t)((2u << 12u) | cell);
        }
    }
    sat_example_must(sat_vdp2_vram_write_words(at_byte / 2u, g_names, 64u * 64u));
}

/* ---- the two parameter sets --------------------------------------------- */

/* Parameter A: RBG0's grid, spinning clockwise, pivoting near the screen
 * centre with a slow drift. Parameter B: RBG1's checker, the other way,
 * at its own rate and drift. Both are 2D affine: the map point is the
 * rotation matrix applied to the screen offset plus the pivot. */
static void build_params(uint32_t frame, sat_vdp2_rotation_params_t* out_a,
                         sat_vdp2_rotation_params_t* out_b) {
    const uint32_t turn_a = (frame << 16) / SPIN_A_TURNS;
    const uint32_t turn_b = 0u - ((frame << 16) / SPIN_B_TURNS);   /* counter-spin */
    const int32_t cos_a = cosine16(turn_a);
    const int32_t sin_a = sine16(turn_a);
    const int32_t cos_b = cosine16(turn_b);
    const int32_t sin_b = sine16(turn_b);
    /* the pivots wander a quarter screen each, out of phase */
    const int32_t mx_a = sine16((frame << 16) / 384u) * 80 >> 16;
    const int32_t my_a = cosine16((frame << 16) / 384u) * 60 >> 16;
    const int32_t mx_b = cosine16((frame << 16) / 320u) * 80 >> 16;
    const int32_t my_b = sine16((frame << 16) / 320u) * 60 >> 16;

    sat_vdp2_rotation_params_t* sets[2] = {out_a, out_b};
    const int32_t cosv[2] = {cos_a, cos_b};
    const int32_t sinv[2] = {sin_a, sin_b};
    const int32_t mxv[2] = {mx_a, mx_b};
    const int32_t myv[2] = {my_a, my_b};
    for (uint32_t s = 0u; s < 2u; ++s) {
        sat_vdp2_rotation_params_t* p = sets[s];
        p->xst = 0;
        p->yst = 0;
        p->zst = 0;
        p->dxst = 0;
        p->dyst = SAT_VDP2_ROTATION_COEFFICIENTS_ONE;   /* the map walks down per line */
        p->dx = SAT_VDP2_ROTATION_COEFFICIENTS_ONE;
        p->dy = 0;
        p->matrix[0] = cosv[s];     /* A */
        p->matrix[1] = -sinv[s];   /* B */
        p->matrix[2] = 0;
        p->matrix[3] = sinv[s];    /* D */
        p->matrix[4] = cosv[s];    /* E */
        p->matrix[5] = 0;
        p->px = 0;
        p->py = 0;
        p->pz = 0;
        p->cx = CX;
        p->cy = CY;
        p->cz = 0;
        p->mx = mxv[s] * SAT_VDP2_ROTATION_COEFFICIENTS_ONE;
        p->my = myv[s] * SAT_VDP2_ROTATION_COEFFICIENTS_ONE;
        p->kx = SAT_VDP2_ROTATION_COEFFICIENTS_ONE;
        p->ky = SAT_VDP2_ROTATION_COEFFICIENTS_ONE;
        p->coefficient_address = 0u;      /* no coefficient tables in this scene */
        p->coefficient_step = 0;
    }
}

static void update_scene(uint32_t frame) {
    const uint32_t write_region = (frame & 1u) ? REGION0 : REGION1;
    sat_vdp2_rotation_params_t pa, pb;
    build_params(frame, &pa, &pb);
    if (sat_vdp2_rotation_write_params(write_region, &pa, &pb) == SAT_OK) {
        ++g_dual_rotation.params_writes;
        g_dual_rotation.region = write_region;
    }
    if (frame == CAPTURE_FRAME) {
        const uint32_t turn_a = (frame << 16) / SPIN_A_TURNS;
        const uint32_t turn_b = 0u - ((frame << 16) / SPIN_B_TURNS);
        g_dual_rotation.capture_frame = frame;
        g_dual_rotation.capture_mode = g_dual_rotation.mode;
        g_dual_rotation.capture_cos_a = cosine16(turn_a);
        g_dual_rotation.capture_sin_a = sine16(turn_a);
        g_dual_rotation.capture_cos_b = cosine16(turn_b);
        g_dual_rotation.capture_sin_b = sine16(turn_b);
        g_dual_rotation.capture_mx_a = sine16((frame << 16) / 384u) * 80 >> 16;
        g_dual_rotation.capture_my_a = cosine16((frame << 16) / 384u) * 60 >> 16;
        g_dual_rotation.capture_mx_b = cosine16((frame << 16) / 320u) * 80 >> 16;
        g_dual_rotation.capture_my_b = sine16((frame << 16) / 320u) * 60 >> 16;
        g_dual_rotation.capture_region = write_region;
    }
}

/* ---- the three modes ----------------------------------------------------- */

/* One whole configuration: RBG0's screen and banks, parameter A's map
 * (the grid), parameter B's map (the checker, at the bank that fetches
 * it: A0 for RBG0's B-mode, B1 for RBG1) and RBG1 itself. */
static sat_result_t configure(uint32_t mode) {
    sat_vdp2_rotation_config_t cfg;
    sat_vdp2_rotation_config_default(&cfg);
    cfg.color_mode = SAT_VDP2_COLOR_MODE_256;
    cfg.pattern_name_words = 1u;
    cfg.priority = 4u;               /* RBG0 over RBG1 */
    cfg.transparent = 1u;
    cfg.over = SAT_VDP2_ROTATION_OVER_REPEAT;
    cfg.name_bank_mask = SAT_VDP2_BANK_A0;
    cfg.char_bank_mask = SAT_VDP2_BANK_A1;
    cfg.parameter_mode = SAT_VDP2_ROTATION_PARAMETER_A;
    cfg.char_base_address = GRID_CHARS;
    for (uint32_t p = 0u; p < 16u; ++p) cfg.plane_address[p] = GRID_NAMES;
    cfg.map_b.plane_pages_x = 1u;
    cfg.map_b.plane_pages_y = 1u;
    cfg.map_b.over = SAT_VDP2_ROTATION_OVER_REPEAT;
    /* B1 only in DUAL (RBG1's fixed fetch); otherwise the A0 copy, so the
     * live B-mode switch at the next transition reads through RBG0's own
     * name banks. */
    const uint32_t checker = (mode == 2u) ? CHECK_NAMES_B1 : CHECK_NAMES_A0;
    for (uint32_t p = 0u; p < 16u; ++p) cfg.map_b.plane_address[p] = checker;
    cfg.rbg1.color_mode = SAT_VDP2_COLOR_MODE_256;
    cfg.rbg1.pattern_name_words = 1u;
    cfg.rbg1.priority = 3u;
    cfg.rbg1.transparent = 1u;
    cfg.rbg1.palette = 2u;
    cfg.rbg1.char_base_address = CHECK_CHARS;
    cfg.rbg1.enabled = (mode == 2u) ? 1u : 0u;
    cfg.table_address = REGION0;
    const sat_result_t result = sat_vdp2_rotation_configure(&cfg);
    g_dual_rotation.parameter_mode = SAT_VDP2_ROTATION_PARAMETER_A;
    g_dual_rotation.rbg1_enabled = cfg.rbg1.enabled;
    g_dual_rotation.rbg0_priority = cfg.priority;
    g_dual_rotation.rbg1_priority = cfg.rbg1.priority;
    return result;
}

static void enter_mode(uint32_t mode) {
    sat_result_t result;
    switch (mode) {
        case 0u:
            if (g_dual_rotation.rbg1_enabled != 0u) {
                /* the live path first: RBG1 hides without a reconfigure */
                g_dual_rotation.live_hide_result = sat_vdp2_rotation_rbg1_set_enabled(0u);
                g_dual_rotation.rbg1_enabled = 0u;
            }
            /* re-point parameter B's map at the A0 copy the next B-mode
             * live switch will read */
            result = configure(0u);
            break;
        case 1u:
            /* the live parameter-mode switch: RBG0 starts drawing the B
             * image, whose map the mode-0 configure left in RBG0's banks */
            result = sat_vdp2_rotation_set_parameter_mode(SAT_VDP2_ROTATION_PARAMETER_B);
            g_dual_rotation.parameter_mode = SAT_VDP2_ROTATION_PARAMETER_B;
            break;
        default:
            /* DUAL needs the map back in B1 (RBG1's fixed fetch) and RBG1
             * on: one atomic reconfigure */
            result = configure(2u);
            /* and while RBG1 displays, the live exclusions: */
            {
                sat_vdp2_layer_config_t sky;
                sat_vdp2_layer_config_default(SAT_VDP2_NBG0, &sky);
                sky.plane_address[0] = 0x0A000u;
                sky.char_bank_mask = SAT_VDP2_BANK_A0;
                g_dual_rotation.refused_nbg_while_dual = sat_vdp2_layer_configure(&sky);
                g_dual_rotation.refused_mode_switch_while_rbg1 =
                    sat_vdp2_rotation_set_parameter_mode(SAT_VDP2_ROTATION_PARAMETER_COEFFICIENT);
            }
            break;
    }
    g_dual_rotation.transition_result[mode] = result;
    g_dual_rotation.mode = mode;
    g_dual_rotation.mode_ran |= 1u << mode;
}

/* ---- VRAM through the resource plan -------------------------------------- */

static void plan_vram(void) {
    /* The A+B parameter regions may sit in any bank (p06_30: no RDBS role):
     * A0's tail, off the image bytes. */
    sat_example_must(sat_vdp2_plan_begin());
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_PARAMS, REGION0, 0xE0u));
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_PARAMS, REGION1, 0xE0u));
    sat_example_must(sat_vdp2_plan_commit());
    /* All four banks are rotation-consumed: RBG0's image in VRAM-A (the
     * RDBS banks), RBG1's fixed in VRAM-B - B0/B1 are claimed in the
     * ledger only; RDBSB0/B1 stay 00B (p06_20, the RAMCTL page). */
    sat_example_must(sat_vdp2_plan_claim_banks(SAT_VDP2_BANK_A0 | SAT_VDP2_BANK_A1 |
                                               SAT_VDP2_BANK_B0 | SAT_VDP2_BANK_B1));
    sat_example_must(sat_vdp2_plan_begin());
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_NAMES, GRID_NAMES, NAMES_BYTES));
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_NAMES, CHECK_NAMES_A0, NAMES_BYTES));
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_NAMES, CHECK_NAMES_B1, NAMES_BYTES));
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_CHARS, GRID_CHARS, CHARS_BYTES));
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_ROTATION_CHARS, CHECK_CHARS, CHARS_BYTES));
    sat_example_must(sat_vdp2_plan_commit());
}

/* ---- main ------------------------------------------------------------------ */

static void draw_debug_overlay(void) {
    static const char* const names[MODES] = {"A-ONLY", "B-ON-RBG0", "DUAL"};
    (void)sat_ascii_font_draw_text_screen_indexed8(&font, names[g_dual_rotation.mode],
                                                   8u, 24u, 8u, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&font, "R1 ",
                                        g_dual_rotation.rbg1_enabled,
                                        96u, 24u, 8u, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&font, "PW ",
                                        g_dual_rotation.params_writes,
                                        160u, 24u, 8u, 0u, 0u);
}

int main(void) {
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) {
        return 1;
    }
    g_dual_rotation.magic = DROT_MAGIC;

    upload_palettes();
    plan_vram();
    upload_tiles();
    upload_grid_names();
    upload_checker_names(CHECK_NAMES_A0, 3u, 4u);   /* through RBG0's window */
    upload_checker_names(CHECK_NAMES_B1, 1u, 2u);   /* through RBG1's window */

    /* The illegal combinations, recorded for the harness. */
    {
        sat_vdp2_rotation_config_t bad;
        sat_vdp2_rotation_config_default(&bad);
        bad.name_bank_mask = SAT_VDP2_BANK_A0;
        bad.char_bank_mask = SAT_VDP2_BANK_A1;
        bad.table_address = REGION0;
        bad.rbg1.enabled = 1u;
        bad.rbg1.char_base_address = CHECK_CHARS;
        bad.rbg1.color_mode = SAT_VDP2_COLOR_MODE_256;
        bad.rbg1.pattern_name_words = 1u;
        bad.rbg1.priority = 3u;
        bad.map_b.plane_pages_x = 1u;
        bad.map_b.plane_pages_y = 1u;
        for (uint32_t p = 0u; p < 16u; ++p) bad.map_b.plane_address[p] = CHECK_NAMES_B1;
        /* RBG1 with a switching parameter mode: "Mode 0 must be set when
         * displaying RBG1" (the RPMD page). */
        bad.parameter_mode = SAT_VDP2_ROTATION_PARAMETER_COEFFICIENT;
        g_dual_rotation.refused_configure_mode2 = sat_vdp2_rotation_configure(&bad);
        /* RBG0's image banks inside VRAM-B while RBG1 displays: the fixed
         * B1/B0 fetches and the separate-VRAM rule (p06_20). */
        bad.parameter_mode = SAT_VDP2_ROTATION_PARAMETER_A;
        bad.name_bank_mask = SAT_VDP2_BANK_B0;
        g_dual_rotation.refused_banks_in_vram_b = sat_vdp2_rotation_configure(&bad);
        /* parameter B's map outside VRAM-B1 while RBG1 displays. */
        bad.name_bank_mask = SAT_VDP2_BANK_A0;
        bad.map_b.plane_address[0] = 0x20000u;
        g_dual_rotation.refused_map_outside_b1 = sat_vdp2_rotation_configure(&bad);
    }

    enter_mode(0u);
    uint32_t entered = 0u;   /* mode transitions done */

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        if (sat_pad_poll(&pad) != SAT_OK) break;
        ++g_dual_rotation.frames;
        if (((pad.pressed & SAT_PAD_A) != 0u)
            || g_dual_rotation.frames >= ((entered + 1u) * MODE_FRAMES) + 1u) {
            const uint32_t target = (g_dual_rotation.mode + 1u) % MODES;
            if (g_dual_rotation.mode == 1u) {
                /* the live refusal first: RBG1 cannot come on in mode B */
                g_dual_rotation.refused_rbg1_enable_in_mode_b =
                    sat_vdp2_rotation_rbg1_set_enabled(1u);
            }
            enter_mode(target);
            ++entered;
        }
        update_scene(g_dual_rotation.frames);
        if (sat_vdp2_layers_commit() != SAT_OK) break;
        if (sat_begin_frame() != SAT_OK) break;
        if ((pad.pressed & SAT_PAD_B) != 0u) g_debug_overlay ^= 1u;
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "DUAL ROTATION",
                                                       8u, 4u, 8u, 0u, 0u);
        if (g_debug_overlay) draw_debug_overlay();
        if (sat_end_frame() != SAT_OK) break;
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
