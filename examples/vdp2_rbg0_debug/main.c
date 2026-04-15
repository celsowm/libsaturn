/* vdp2_rbg0_debug.c - Button-driven RBG0 smoke tests
 *
 * Stages:
 *   0. Backdrop only
 *   1. NBG0 solid fill
 *   2. RBG0 solid fill
 *   3. RBG0 checkerboard
 *   4. RBG0 asset upload
 *   5. RBG0 asset upload with auto-scroll
 *   6. RBG0 tilted ground-plane probe
 *
 * Controls:
 *   A      Next stage
 *   B      Previous stage
 *   C      Toggle legacy A0 cycle pattern
 *   X/Y    Toggle HUD text
 *   Start  Exit
 */
#include <stdint.h>
#include "saturn/saturn.h"
#include "saturn/example_util.h"
#include "vdp2_rbg0_debug/seamless_sea.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 224

#define BITMAP_WIDTH  512
#define BITMAP_HEIGHT 256

#define BITMAP_BASE_WORD  0x00000u
#define ROT_PARAM_BASE    0x10000u
#define NBG0_MAP_PLANE_INDEX 0x0010u
#define NBG0_TILE_WORDS 16u
#define AUTO_STAGE_FRAMES 180u
#define VDP2_REG_BASE 0x25F80000u
#define CYCA0L_LEGACY 0x0E4Eu
#define CYCA0U_LEGACY 0x4E4Eu

#define FX16_SHIFT 16

typedef enum debug_stage {
    STAGE_BACKDROP = 0,
    STAGE_NBG0 = 1,
    STAGE_SOLID = 2,
    STAGE_CHECKER = 3,
    STAGE_ASSET = 4,
    STAGE_SCROLL = 5,
    STAGE_PERSPECTIVE = 6,
    STAGE_COUNT
} debug_stage_t;

static sat_fx16_t scroll_x = 0;
static sat_fx16_t scroll_y = 0;
static debug_stage_t g_stage = STAGE_BACKDROP;
static uint16_t g_backdrop_color = SAT_COLOR_BLUE;
static sat_ascii_font_t g_font;
static uint32_t g_frame_counter = 0;
static uint8_t g_force_legacy_cycles = 0u;
static uint8_t g_show_hud = 1u;

static uint16_t read_vdp2_reg(uint32_t offset) {
    volatile uint16_t* const reg = (volatile uint16_t*)(VDP2_REG_BASE + offset);
    return *reg;
}

static void write_vdp2_reg(uint32_t offset, uint16_t value) {
    volatile uint16_t* const reg = (volatile uint16_t*)(VDP2_REG_BASE + offset);
    *reg = value;
}

static void write_hex16(char* out, uint16_t value) {
    static const char hex[] = "0123456789ABCDEF";
    out[0] = hex[(value >> 12u) & 0x0Fu];
    out[1] = hex[(value >> 8u) & 0x0Fu];
    out[2] = hex[(value >> 4u) & 0x0Fu];
    out[3] = hex[value & 0x0Fu];
}

static void vdp2_line0(char* out) {
    static const char prefix[] = "TVMD 0000 BGON 0000 RAMC 0000";
    for (uint32_t i = 0; i < sizeof(prefix); ++i) {
        out[i] = prefix[i];
    }

    write_hex16(&out[5], read_vdp2_reg(0x000u));   /* TVMD */
    write_hex16(&out[15], read_vdp2_reg(0x020u));  /* BGON */
    write_hex16(&out[25], read_vdp2_reg(0x00Eu));  /* RAMCTL */
}

static void vdp2_line1(char* out) {
    static const char prefix[] = "A0L  0000 A0U  0000 A1L  0000";
    for (uint32_t i = 0; i < sizeof(prefix); ++i) {
        out[i] = prefix[i];
    }

    write_hex16(&out[5], read_vdp2_reg(0x010u));
    write_hex16(&out[15], read_vdp2_reg(0x012u));
    write_hex16(&out[25], read_vdp2_reg(0x014u));
}

static void vdp2_line2(char* out) {
    static const char prefix[] = "A1U  0000 B0L  0000 B0U  0000";
    for (uint32_t i = 0; i < sizeof(prefix); ++i) {
        out[i] = prefix[i];
    }

    write_hex16(&out[5], read_vdp2_reg(0x016u));
    write_hex16(&out[15], read_vdp2_reg(0x018u));
    write_hex16(&out[25], read_vdp2_reg(0x01Au));
}

static void vdp2_line3(char* out) {
    static const char prefix[] = "B1L  0000 B1U  0000 RPMD 0000";
    for (uint32_t i = 0; i < sizeof(prefix); ++i) {
        out[i] = prefix[i];
    }

    write_hex16(&out[5], read_vdp2_reg(0x01Cu));
    write_hex16(&out[15], read_vdp2_reg(0x01Eu));
    write_hex16(&out[25], read_vdp2_reg(0x0B0u));
}

static void vdp2_line4(char* out) {
    static const char prefix[] = "CHC  0000 MPOF 0000 RPAU 0000";
    for (uint32_t i = 0; i < sizeof(prefix); ++i) {
        out[i] = prefix[i];
    }

    write_hex16(&out[5], read_vdp2_reg(0x02Au));
    write_hex16(&out[15], read_vdp2_reg(0x03Eu));
    write_hex16(&out[25], read_vdp2_reg(0x0BCu));
}

static void vdp2_line5(char* out) {
    static const char prefix[] = "RPAL 0000 RPRC 0000 PRIR 0000";
    for (uint32_t i = 0; i < sizeof(prefix); ++i) {
        out[i] = prefix[i];
    }

    write_hex16(&out[5], read_vdp2_reg(0x0BEu));
    write_hex16(&out[15], sat_vdp2_rbg0_last_rprctl_written());
    write_hex16(&out[25], read_vdp2_reg(0x0FCu));
}

static void vdp2_line6(char* out) {
    static const char prefix[] = "WCHC 0000 WMPO 0000 WRMC 0000";
    for (uint32_t i = 0; i < sizeof(prefix); ++i) {
        out[i] = prefix[i];
    }

    write_hex16(&out[5], sat_vdp2_rbg0_last_chctlb_written());
    write_hex16(&out[15], sat_vdp2_rbg0_last_mpofr_written());
    write_hex16(&out[25], sat_vdp2_rbg0_last_ramctl_written());
}

static void vdp2_line7(char* out) {
    static const char prefix[] = "WRPA 0000 WRPL 0000 WBGN 0000";
    for (uint32_t i = 0; i < sizeof(prefix); ++i) {
        out[i] = prefix[i];
    }

    write_hex16(&out[5], sat_vdp2_rbg0_last_rptau_written());
    write_hex16(&out[15], sat_vdp2_rbg0_last_rptal_written());
    write_hex16(&out[25], sat_vdp2_rbg0_last_bgon_written());
}

static void probe_line(char* out) {
    static const char prefix[] = "CYCDBG HAL C=LEGACY X=HUD";
    for (uint32_t i = 0; i < sizeof(prefix); ++i) {
        out[i] = prefix[i];
    }

    out[7] = (g_force_legacy_cycles != 0u) ? 'L' : 'H';
    out[8] = (g_force_legacy_cycles != 0u) ? 'E' : 'A';
    out[9] = (g_force_legacy_cycles != 0u) ? 'G' : 'L';
    out[25] = (g_show_hud != 0u) ? '1' : '0';
}

static const char* stage_name(debug_stage_t stage) {
    switch (stage) {
    case STAGE_BACKDROP: return "STG0";
    case STAGE_NBG0: return "STG1";
    case STAGE_SOLID: return "STG2";
    case STAGE_CHECKER: return "STG3";
    case STAGE_ASSET: return "STG4";
    case STAGE_SCROLL: return "STG5";
    case STAGE_PERSPECTIVE: return "STG6";
    default: return "STG?";
    }
}

static void frame_line(char* out, uint32_t frame_counter) {
    static const char prefix[] = "FRAME 000000 A NEXT B PREV";
    for (uint32_t i = 0; i < sizeof(prefix); ++i) {
        out[i] = prefix[i];
    }

    for (int i = 10; i >= 5; --i) {
        out[i] = (char)('0' + (frame_counter % 10u));
        frame_counter /= 10u;
    }
}

static void pad_line(char* out, const sat_pad_state_t* pad) {
    static const char prefix[] = "U0 D0 L0 R0 A0 B0 C0 X0 Y0 S0";
    for (uint32_t i = 0; i < sizeof(prefix); ++i) {
        out[i] = prefix[i];
    }

    out[1] = ((pad->held & SAT_PAD_UP) != 0u) ? '1' : '0';
    out[4] = ((pad->held & SAT_PAD_DOWN) != 0u) ? '1' : '0';
    out[7] = ((pad->held & SAT_PAD_LEFT) != 0u) ? '1' : '0';
    out[10] = ((pad->held & SAT_PAD_RIGHT) != 0u) ? '1' : '0';
    out[13] = ((pad->held & SAT_PAD_A) != 0u) ? '1' : '0';
    out[16] = ((pad->held & SAT_PAD_B) != 0u) ? '1' : '0';
    out[19] = ((pad->held & SAT_PAD_C) != 0u) ? '1' : '0';
    out[22] = ((pad->held & SAT_PAD_X) != 0u) ? '1' : '0';
    out[25] = ((pad->held & SAT_PAD_Y) != 0u) ? '1' : '0';
    out[28] = ((pad->held & SAT_PAD_START) != 0u) ? '1' : '0';
}

static void pressed_line(char* out, const sat_pad_state_t* pad) {
    static const char prefix[] = "pU0 pD0 pL0 pR0 pA0 pB0 pC0 pX0 pY0 pS0";
    for (uint32_t i = 0; i < sizeof(prefix); ++i) {
        out[i] = prefix[i];
    }

    out[2] = ((pad->pressed & SAT_PAD_UP) != 0u) ? '1' : '0';
    out[6] = ((pad->pressed & SAT_PAD_DOWN) != 0u) ? '1' : '0';
    out[10] = ((pad->pressed & SAT_PAD_LEFT) != 0u) ? '1' : '0';
    out[14] = ((pad->pressed & SAT_PAD_RIGHT) != 0u) ? '1' : '0';
    out[18] = ((pad->pressed & SAT_PAD_A) != 0u) ? '1' : '0';
    out[22] = ((pad->pressed & SAT_PAD_B) != 0u) ? '1' : '0';
    out[26] = ((pad->pressed & SAT_PAD_C) != 0u) ? '1' : '0';
    out[30] = ((pad->pressed & SAT_PAD_X) != 0u) ? '1' : '0';
    out[34] = ((pad->pressed & SAT_PAD_Y) != 0u) ? '1' : '0';
    out[38] = ((pad->pressed & SAT_PAD_START) != 0u) ? '1' : '0';
}

static debug_stage_t next_stage(debug_stage_t stage) {
    return (debug_stage_t)(((uint32_t)stage + 1u) % STAGE_COUNT);
}

static debug_stage_t prev_stage(debug_stage_t stage) {
    return (stage == STAGE_BACKDROP)
        ? (debug_stage_t)(STAGE_COUNT - 1u)
        : (debug_stage_t)((uint32_t)stage - 1u);
}

static sat_result_t upload_bitmap_from_pixels(const uint8_t* pixels, uint32_t src_width, uint32_t src_height) {
    volatile uint16_t* const vram = (volatile uint16_t*)(0x20000000u | 0x05E00000u);
    uint32_t word_offset = BITMAP_BASE_WORD;

    for (uint32_t y = 0; y < BITMAP_HEIGHT; ++y) {
        const uint32_t src_y = y % src_height;
        for (uint32_t x = 0; x < BITMAP_WIDTH; x += 2u) {
            const uint32_t src_x0 = x % src_width;
            const uint32_t src_x1 = (x + 1u) % src_width;
            const uint16_t word = (uint16_t)((pixels[src_y * src_width + src_x0] << 8u) |
                                             pixels[src_y * src_width + src_x1]);
            vram[word_offset++] = word;
        }
    }

    return SAT_OK;
}

static sat_result_t upload_solid_bitmap(uint8_t palette_index) {
    volatile uint16_t* const vram = (volatile uint16_t*)(0x20000000u | 0x05E00000u);
    const uint16_t word = (uint16_t)(((uint16_t)palette_index << 8u) | palette_index);

    for (uint32_t i = 0; i < ((BITMAP_WIDTH * BITMAP_HEIGHT) / 2u); ++i) {
        vram[BITMAP_BASE_WORD + i] = word;
    }

    return SAT_OK;
}

static sat_result_t upload_checker_bitmap(void) {
    volatile uint16_t* const vram = (volatile uint16_t*)(0x20000000u | 0x05E00000u);
    uint32_t word_offset = BITMAP_BASE_WORD;

    for (uint32_t y = 0; y < BITMAP_HEIGHT; ++y) {
        for (uint32_t x = 0; x < BITMAP_WIDTH; x += 2u) {
            const uint8_t p0 = (((x >> 5u) ^ (y >> 5u)) & 1u) ? 2u : 1u;
            const uint8_t p1 = ((((x + 1u) >> 5u) ^ (y >> 5u)) & 1u) ? 2u : 1u;
            vram[word_offset++] = (uint16_t)(((uint16_t)p0 << 8u) | p1);
        }
    }

    return SAT_OK;
}

static sat_result_t setup_nbg0_solid(void) {
    static const uint16_t tile_words[NBG0_TILE_WORDS] = {
        0x1111u, 0x1111u, 0x1111u, 0x1111u,
        0x1111u, 0x1111u, 0x1111u, 0x1111u,
        0x1111u, 0x1111u, 0x1111u, 0x1111u,
        0x1111u, 0x1111u, 0x1111u, 0x1111u
    };
    static const uint16_t palette[16] = {
        SAT_COLOR_BLACK, SAT_COLOR_GREEN
    };
    const sat_vdp2_nbg0_config_t nbg0_cfg = {
        SAT_VDP2_CHAR_SIZE_1X1,
        SAT_VDP2_COLOR_MODE_16,
        NBG0_MAP_PLANE_INDEX,
        0u,
        0u
    };
    const sat_vdp2_scroll_t scroll = {0u, 0u, 0u, 0u};

    SAT_TRY(sat_vdp2_palette_upload(palette, 16, 0));
    SAT_TRY(sat_vdp2_vram_write_words(BITMAP_BASE_WORD, tile_words, NBG0_TILE_WORDS));
    SAT_TRY(sat_vdp2_nbg0_init(&nbg0_cfg));
    SAT_TRY(sat_vdp2_nbg0_set_scroll(&scroll));
    SAT_TRY(sat_vdp2_nbg0_map_fill(0u));
    SAT_TRY(sat_vdp2_nbg0_set_enabled(1));

    return SAT_OK;
}

static sat_result_t setup_rotation_params(void) {
    volatile uint16_t* const vram = (volatile uint16_t*)(0x20000000u | 0x05E00000u);

    for (uint32_t i = 0; i < 48u; ++i) {
        vram[ROT_PARAM_BASE + i] = 0x0000u;
    }

    SAT_TRY(sat_vdp2_rbg0_set_rotation_matrix(ROT_PARAM_BASE, 0, 0, 0));
    SAT_TRY(sat_vdp2_rbg0_set_scaling(ROT_PARAM_BASE, SAT_FX16_ONE, SAT_FX16_ONE));
    /* ΔXst=0, ΔYst=1: advance Y by 1 per scanline */
    SAT_TRY(sat_vdp2_rbg0_set_vertical_increments(ROT_PARAM_BASE, 0, 0, 1, 0));
    /* ΔX=1, ΔY=0: advance X by 1 per dot */
    SAT_TRY(sat_vdp2_rbg0_set_coordinate_increments(ROT_PARAM_BASE, 1, 0, 0, 0));
    SAT_TRY(sat_vdp2_rbg0_set_scroll(ROT_PARAM_BASE, 0, 0, 0, 0));

    return SAT_OK;
}

static sat_result_t setup_tilted_ground_plane(void) {
    /* Pitch the plane toward the horizon and place the camera above it.
     * The matrix helper now emits real rotation coefficients, so this stage
     * becomes the first practical camera test for the future infinite-plane
     * path.
     */
    SAT_TRY(sat_vdp2_rbg0_set_rotation_matrix(ROT_PARAM_BASE, -58, 0, 0));
    SAT_TRY(sat_vdp2_rbg0_set_viewpoint(
        ROT_PARAM_BASE,
        0,
        72 * SAT_FX16_ONE,
        -192 * SAT_FX16_ONE
    ));
    SAT_TRY(sat_vdp2_rbg0_set_center(ROT_PARAM_BASE, 0, 0, 0));
    SAT_TRY(sat_vdp2_rbg0_set_scaling(ROT_PARAM_BASE, SAT_FX16_ONE, SAT_FX16_ONE));
    SAT_TRY(sat_vdp2_rbg0_set_rotation_read_control(0x0007u));
    SAT_TRY(sat_vdp2_rbg0_set_coefficient_control(0x0000u));
    SAT_TRY(sat_vdp2_rbg0_set_scroll(ROT_PARAM_BASE, 0, 0, 0, 0));

    return SAT_OK;
}

static sat_result_t configure_rbg0(void) {
    const sat_vdp2_rbg0_config_t rbg0_cfg = {
        SAT_VDP2_RBG0_BITMAP_512x256,
        SAT_VDP2_COLOR_MODE_256,
        BITMAP_BASE_WORD,
        ROT_PARAM_BASE
    };

    SAT_TRY(sat_vdp2_rbg0_init(&rbg0_cfg));
    SAT_TRY(sat_vdp2_rbg0_set_param_mode(SAT_VDP2_RBG0_PARAM_A));
    SAT_TRY(setup_rotation_params());

    return SAT_OK;
}

static sat_result_t configure_rbg0_stage(debug_stage_t stage) {
    switch (stage) {
    case STAGE_SCROLL:
        return SAT_OK;
    case STAGE_PERSPECTIVE:
        return setup_tilted_ground_plane();
    default:
        return SAT_OK;
    }
}

static uint8_t is_rbg0_stage(debug_stage_t stage) {
    switch (stage) {
    case STAGE_SOLID:
    case STAGE_CHECKER:
    case STAGE_ASSET:
    case STAGE_SCROLL:
    case STAGE_PERSPECTIVE:
        return 1u;
    default:
        return 0u;
    }
}

static void apply_debug_cycle_probe(void) {
    if ((is_rbg0_stage(g_stage) == 0u) || (g_force_legacy_cycles == 0u)) {
        return;
    }

    /* Reapply the original A0 cycle setup so the debug build can compare the
     * legacy fetch path against the HAL-managed RBG0 cycle policy.
     */
    write_vdp2_reg(0x010u, CYCA0L_LEGACY);
    write_vdp2_reg(0x012u, CYCA0U_LEGACY);
}

static sat_result_t refresh_rbg0_state(void) {
    switch (g_stage) {
    case STAGE_SOLID:
    case STAGE_CHECKER:
    case STAGE_ASSET:
    case STAGE_SCROLL:
    case STAGE_PERSPECTIVE:
        SAT_TRY(sat_vdp2_rbg0_set_enabled(1));
        break;
    default:
        break;
    }

    apply_debug_cycle_probe();
    return SAT_OK;
}

static sat_result_t apply_stage(debug_stage_t stage) {
    static const uint16_t solid_palette[256] = {
        0x0000u, 0x7C00u, 0x03E0u, 0x7FFFu
    };

    scroll_x = 0;
    scroll_y = 0;
    SAT_TRY(sat_vdp2_nbg0_set_enabled(0));
    SAT_TRY(sat_vdp2_rbg0_set_enabled(0));

    switch (stage) {
    case STAGE_BACKDROP:
        g_backdrop_color = SAT_COLOR_BLUE;
        SAT_TRY(sat_vdp2_back_color_set(g_backdrop_color));
        break;
    case STAGE_NBG0:
        SAT_TRY(setup_nbg0_solid());
        g_backdrop_color = SAT_COLOR_BLACK;
        SAT_TRY(sat_vdp2_back_color_set(g_backdrop_color));
        break;
    case STAGE_SOLID:
        SAT_TRY(sat_vdp2_palette_upload(solid_palette, 4, 0));
        SAT_TRY(upload_solid_bitmap(1u));
        SAT_TRY(configure_rbg0());
        SAT_TRY(sat_vdp2_rbg0_set_enabled(1));
        g_backdrop_color = SAT_COLOR_BLACK;
        SAT_TRY(sat_vdp2_back_color_set(g_backdrop_color));
        break;
    case STAGE_CHECKER:
        SAT_TRY(sat_vdp2_palette_upload(solid_palette, 4, 0));
        SAT_TRY(upload_checker_bitmap());
        SAT_TRY(configure_rbg0());
        SAT_TRY(sat_vdp2_rbg0_set_enabled(1));
        g_backdrop_color = SAT_COLOR_BLACK;
        SAT_TRY(sat_vdp2_back_color_set(g_backdrop_color));
        break;
    case STAGE_ASSET:
    case STAGE_SCROLL:
        SAT_TRY(sat_vdp2_palette_upload(seamless_sea_asset.palette, 256, 0));
        SAT_TRY(upload_bitmap_from_pixels(
            seamless_sea_asset.pixels,
            seamless_sea_asset.width,
            seamless_sea_asset.height));
        SAT_TRY(configure_rbg0());
        SAT_TRY(configure_rbg0_stage(stage));
        SAT_TRY(sat_vdp2_rbg0_set_enabled(1));
        g_backdrop_color = SAT_COLOR_BLACK;
        SAT_TRY(sat_vdp2_back_color_set(g_backdrop_color));
        break;
    case STAGE_PERSPECTIVE:
        SAT_TRY(sat_vdp2_palette_upload(solid_palette, 4, 0));
        SAT_TRY(upload_checker_bitmap());
        SAT_TRY(configure_rbg0());
        SAT_TRY(configure_rbg0_stage(stage));
        SAT_TRY(sat_vdp2_rbg0_set_enabled(1));
        g_backdrop_color = SAT_COLOR_BLACK;
        SAT_TRY(sat_vdp2_back_color_set(g_backdrop_color));
        break;
    default:
        return SAT_ERR_INVALID_ARG;
    }

    g_stage = stage;
    return SAT_OK;
}

static void update_scroll(void) {
    if (g_stage != STAGE_SCROLL && g_stage != STAGE_PERSPECTIVE) {
        return;
    }

    if (g_stage == STAGE_SCROLL) {
        scroll_x += (sat_fx16_t)(SAT_FX16_ONE * 2);
        scroll_y += (sat_fx16_t)(SAT_FX16_ONE / 2);
    } else {
        scroll_x += (sat_fx16_t)(SAT_FX16_ONE / 4);
        scroll_y += (sat_fx16_t)(SAT_FX16_ONE / 8);
    }

    sat_example_must(sat_vdp2_rbg0_set_scroll(
        ROT_PARAM_BASE,
        (int32_t)(scroll_x >> FX16_SHIFT),
        (int32_t)((scroll_x >> 6) & 0x03FF),
        (int32_t)(scroll_y >> FX16_SHIFT),
        (int32_t)((scroll_y >> 6) & 0x03FF)));
}

int main(void) {
    sat_video_config_t cfg = {SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0};
    sat_example_must(sat_init(&cfg));
    /* Probe path: keep VDP1 from clearing over VDP2 so RBG0/NBG0 visibility
     * reflects only the VDP2 fetch/config path during this debug session.
     */
    sat_example_must(sat_vdp1_set_erase_enabled(0));
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 1));
    sat_example_must(apply_stage(STAGE_BACKDROP));

    while (1) {
        sat_pad_state_t pad = {0};
        char line1[] = "FRAME 000000 A NEXT B PREV";
        char line2[] = "U0 D0 L0 R0 A0 B0 C0 X0 Y0 S0";
        char line3[] = "pU0 pD0 pL0 pR0 pA0 pB0 pC0 pX0 pY0 pS0";
        char reg0[] = "TVMD 0000 BGON 0000 RAMC 0000";
        char reg1[] = "A0L  0000 A0U  0000 A1L  0000";
        char reg2[] = "A1U  0000 B0L  0000 B0U  0000";
        char reg3[] = "B1L  0000 B1U  0000 RPMD 0000";
        char reg4[] = "CHC  0000 MPOF 0000 RPAU 0000";
        char reg5[] = "RPAL 0000 RPRC 0000 PRIR 0000";
        char reg6[] = "WCHC 0000 WMPO 0000 WRMC 0000";
        char reg7[] = "WRPA 0000 WRPL 0000 WBGN 0000";
        char dbg0[] = "CYCDBG HAL C=LEGACY X=HUD0";
        sat_example_must(sat_app_frame_begin(g_backdrop_color, SAT_COLOR_BLACK, &pad));

        if ((pad.pressed & SAT_PAD_START) != 0u) {
            sat_example_must(sat_end_frame());
            break;
        }

        const uint16_t next_pressed = (uint16_t)(pad.pressed & (SAT_PAD_A | SAT_PAD_RIGHT | SAT_PAD_DOWN));
        const uint16_t prev_pressed = (uint16_t)(pad.pressed & (SAT_PAD_B | SAT_PAD_LEFT | SAT_PAD_UP));

        if ((next_pressed != 0u) && (prev_pressed == 0u)) {
            sat_example_must(apply_stage(next_stage(g_stage)));
        } else if ((prev_pressed != 0u) && (next_pressed == 0u)) {
            sat_example_must(apply_stage(prev_stage(g_stage)));
        } else if ((g_frame_counter != 0u) && ((g_frame_counter % AUTO_STAGE_FRAMES) == 0u)) {
            sat_example_must(apply_stage(next_stage(g_stage)));
        }
        if ((pad.pressed & SAT_PAD_C) != 0u) {
            g_force_legacy_cycles = (uint8_t)(g_force_legacy_cycles == 0u);
            if ((g_force_legacy_cycles == 0u) && (is_rbg0_stage(g_stage) != 0u)) {
                sat_example_must(configure_rbg0());
                sat_example_must(configure_rbg0_stage(g_stage));
            }
        }
        if ((pad.pressed & (SAT_PAD_X | SAT_PAD_Y)) != 0u) {
            g_show_hud = (uint8_t)(g_show_hud == 0u);
        }

        sat_example_must(refresh_rbg0_state());
        update_scroll();
        frame_line(line1, g_frame_counter);
        pad_line(line2, &pad);
        pressed_line(line3, &pad);
        vdp2_line0(reg0);
        vdp2_line1(reg1);
        vdp2_line2(reg2);
        vdp2_line3(reg3);
        vdp2_line4(reg4);
        vdp2_line5(reg5);
        vdp2_line6(reg6);
        vdp2_line7(reg7);
        probe_line(dbg0);
        sat_example_must(sat_ascii_font_draw_text_centered_indexed8(&g_font, stage_name(g_stage), 0, -108, 8, 0, SAT_SPRITE_FLAG_OPAQUE));
        if (g_show_hud != 0u) {
            sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, line1, -152, -96, 8, 0, SAT_SPRITE_FLAG_OPAQUE));
            sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, line2, -152, -84, 8, 0, SAT_SPRITE_FLAG_OPAQUE));
            sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, line3, -152, -72, 8, 0, SAT_SPRITE_FLAG_OPAQUE));
            sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, dbg0, -152, -60, 8, 0, SAT_SPRITE_FLAG_OPAQUE));
            sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, reg0, -152, -48, 8, 0, SAT_SPRITE_FLAG_OPAQUE));
            sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, reg1, -152, -36, 8, 0, SAT_SPRITE_FLAG_OPAQUE));
            sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, reg2, -152, -24, 8, 0, SAT_SPRITE_FLAG_OPAQUE));
            sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, reg3, -152, -12, 8, 0, SAT_SPRITE_FLAG_OPAQUE));
            sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, reg4, -152, 0, 8, 0, SAT_SPRITE_FLAG_OPAQUE));
            sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, reg5, -152, 12, 8, 0, SAT_SPRITE_FLAG_OPAQUE));
            sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, reg6, -152, 24, 8, 0, SAT_SPRITE_FLAG_OPAQUE));
            sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, reg7, -152, 36, 8, 0, SAT_SPRITE_FLAG_OPAQUE));
        }
        sat_example_must(sat_end_frame());
        ++g_frame_counter;
    }

    return 0;
}
