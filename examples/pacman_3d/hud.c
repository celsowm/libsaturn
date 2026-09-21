#include "hud.h"

#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/hud.h"
#include "saturn/render3d.h"
#include "saturn/vdp1.h"
#include "saturn/video.h"
#include "saturn/example_util.h"

#include "../common/pacman_stages.h"
#include "p3d_config.h"
#include "render_status.h"

#define HUD_PALETTE 0u

/* How long a stage's name stays up after it starts. */
#define STAGE_BANNER_FRAMES 150u

static sat_ascii_font_t g_font;
static sat_hud_t g_hud;
static uint32_t g_stage_started;
static uint16_t g_banner_stage = 0xFFFFu;

static void draw_text(const char* text, int x, int y) {
    sat_example_must(sat_hud_text(&g_hud, text, x, y));
}

static void draw_text_centered(const char* text, int y) {
    sat_example_must(sat_hud_text_centered(&g_hud, text, P3D_SCREEN_W / 2, y));
}

/* "STAGE 2  TWIN LANES" */
static void draw_stage_title(uint16_t stage, int y) {
    const pac_stage_t* info = pac_stage_get(stage);
    char text[40];
    char* p = text;
    const char* s;

    sat_example_must(sat_fmt_label_u32("STAGE ", (uint32_t)stage + 1u, text, 16u, NULL));
    while (*p != '\0') {
        ++p;
    }
    if (info != NULL) {
        *p++ = ' ';
        *p++ = ' ';
        for (s = info->name; *s != '\0' && p < &text[sizeof(text) - 1u]; ++s) {
            *p++ = *s;
        }
    }
    *p = '\0';
    draw_text_centered(text, y);
}

void p3d_hud_init(void) {
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, 0x0000u, HUD_PALETTE));
    sat_example_must(sat_hud_init(&g_hud, &g_font, HUD_PALETTE, SAT_ASCII_FONT_GLYPH_WIDTH));
}

void p3d_hud_draw(const pac_game_t* game) {
    const char* status;
    int i;

    /* By index as well as by event: the frame a stage loads on is skipped
     * while its walls are baked, and its events with it. */
    if (game->stage != g_banner_stage || (game->events & PAC_EVENT_STAGE_START) != 0u) {
        g_banner_stage = game->stage;
        g_stage_started = game->frame;
    }

    sat_example_must(sat_hud_value(&g_hud, "SCORE ", game->score, 4, 2));

    draw_text("LIVES", 232, 2);
    for (i = 0; i < game->lives; ++i) {
        sat_example_must(sat_draw_rect_screen(
            (int16_t)(280 + (i * 10)), 3, 6, 6, P3D_COLOR_PAC));
    }

    /* Message and PRESS START prompt: shared with pacman_2d so the two
     * examples never say this differently. */
    status = pac_game_status_text(game);
    if (status != NULL) {
        draw_text_centered(status, 104);
        if (pac_game_status_needs_start(game)) {
            draw_text_centered("PRESS START", 116);
        }
    } else if (game->frame - g_stage_started < STAGE_BANNER_FRAMES) {
        draw_stage_title(game->stage, 104);
    }
    if (game->frame < 240u) {
        draw_text_centered("DPAD MOVE  L R TURN  START RESET", 216);
    }

    if (p3d_render_overflowed()) {
        draw_text("RENDER LIMIT", 4, 14);
    }
}

void p3d_hud_bake_progress(uint16_t stage, uint16_t done, uint16_t total) {
    SAT_PANIC_IF_ERROR(sat_wait_vblank());
    SAT_PANIC_IF_ERROR(sat_vdp1_set_erase_transparent());
    SAT_PANIC_IF_ERROR(sat_begin_frame());
    draw_stage_title(stage, 88);
    draw_text_centered("BUILDING CAMERA VIEWS", 100);
    SAT_PANIC_IF_ERROR(sat_hud_bar(
        &g_hud, 60, 116, 200, 6, done, total, P3D_COLOR_BOARD, P3D_COLOR_PAC));
    SAT_PANIC_IF_ERROR(sat_end_frame());
}
