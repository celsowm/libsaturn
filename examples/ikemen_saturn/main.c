/* ikemen_saturn - Ikemen GO screenpack content on Saturn 2D hardware.
 *
 * Real Kung Fu Man sprites/palettes and the stage0 "Training Room"
 * converted offline by tools/ikemen_sff (SFFv2 LZ5/PNG decode, AIR
 * frame times, palette merge). The runtime here owns only platform
 * wiring: one INDEX8 texture per unique sprite (shared between
 * players), the stage as a VDP2 NBG0 plane, and P2 differentiated by a
 * draw-time palette override (sat_render2d_set_palette) so both
 * fighters share one pixel copy in VDP1 VRAM. Fight logic lives in
 * examples/ikemen_saturn/ikemen_fight.c, animation sampling in
 * examples/ikemen_saturn/ikemen_anim.c (both host-tested).
 */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/example_util.h"
#include "saturn/font.h"
#include "saturn/hud.h"
#include "saturn/input.h"
#include "saturn/render2d.h"
#include "saturn/surface.h"
#include "saturn/texture.h"
#include "saturn/time.h"
#include "saturn/vdp2.h"
#include "saturn/video.h"

#include "ikemen_anim.h"
#include "ikemen_audio.h"
#include "ikemen_fight.h"
#include "ikemen_saturn/kfm_frames.h"
#include "ikemen_saturn/stage0_plane.h"

/* VDP2 NBG0 stage plane: the composed stage0 image is exactly 320x224
 * (240-space source cropped 16 rows from the top), so the hardware
 * horizontal tiling wraps with an invisible seam while the layer
 * scrolls with the fight midpoint at stage delta 1:1. */
#define STAGE_PALETTE_ID 4u
#define FLOOR_SCREEN_Y 178

/* CRAM bank layout at runtime:
 *   bank 1: ASCII font atlas (external upload in the font init)
 *   bank 4: stage plane palette (external upload)
 *   logical: KFM palette (1,1) shared by every frame texture, plus the
 *   registered (1,4) override for P2; tint variants borrow free banks. */

static sat_texture_t g_frame_textures[KFM_FRAME_COUNT];
static uint32_t g_frame_texture_count;
static sat_palette_t g_p2_palette;
static uint16_t g_map_scratch[SAT_VDP2_NBG0_MAP_CELLS];
static sat_ascii_font_t g_font;
static sat_hud_t g_hud;

static const ik_frame_table_t g_kfm_table = {
    kfm_frames, KFM_FRAME_COUNT, kfm_clsn_boxes, KFM_CLSN_BOX_COUNT
};

static void stage_init(void) {
    {
        const sat_vdp2_nbg0_config_t nbg0 = {
            SAT_VDP2_CHAR_SIZE_1X1, SAT_VDP2_COLOR_MODE_256, 0x3Bu, 0u, 0u
        };
        sat_example_must(sat_vdp2_nbg0_init(&nbg0));
    }
    sat_example_must(sat_vdp2_palette_upload(
        stage0_palette, 256u, (uint16_t)(STAGE_PALETTE_ID * 256u)));
    sat_example_must(sat_vdp2_nbg0_upload_indexed8(
        stage0_pixels, stage0_width, stage0_height,
        STAGE_PALETTE_ID, g_map_scratch));
    /* Background below, VDP1 sprites above. */
    sat_example_must(sat_vdp2_nbg0_set_priority(2u));
    sat_example_must(sat_vdp2_sprite_set_priority(7u));
}

/* Stage0's main background runs at delta 1:1, so the plane tracks the
 * fighters' midpoint directly (clamped to the stage's +/-125 camera
 * bounds); the hardware wraps the 320px image horizontally. */
static void stage_scroll_for_fight(const ik_fight_t* fight) {
    int mid = (int)fight->fighters[0].x + (int)fight->fighters[1].x;
    mid = (mid / 2) - 160;
    if (mid < -125) mid = -125;
    if (mid > 125) mid = 125;
    {
        const sat_vdp2_scroll_t sc = {(uint16_t)mid, 0u, 0u, 0u};
        sat_example_must(sat_vdp2_nbg0_set_scroll(&sc));
    }
}

/* Uploads every unique sprite once (frames sharing pixel_ofs share the
 * texture handle) and registers the P2 palette for the draw override. */
static void fighters_init(void) {
    g_frame_texture_count = 0u;
    for (uint32_t i = 0u; i < g_kfm_table.count; ++i) {
        const ik_frame_t* frame = &kfm_frames[i];
        sat_texture_t tex = {0u, 0u};
        for (uint32_t j = 0u; j < i; ++j) {
            if (kfm_frames[j].pixel_ofs == frame->pixel_ofs) {
                tex = g_frame_textures[j];
                break;
            }
        }
        if (tex.generation == 0u) {
            sat_surface_t surface;
            sat_example_must(sat_surface_init(&surface,
                (uint8_t*)kfm_pixels + frame->pixel_ofs,
                frame->w, frame->h, frame->w, SAT_PIXEL_INDEX8,
                kfm_palette_main, 256u));
            sat_example_must(sat_texture_create_from_surface(
                &tex, &surface, SAT_TEXTURE_UPLOAD_ONLY));
        }
        g_frame_textures[i] = tex;
        ++g_frame_texture_count;
    }
    sat_example_must(sat_palette_register(kfm_palette_alt1, &g_p2_palette));
}

static void draw_fighter(const ik_fighter_t* f, int player) {
    const ik_frame_t* frame = ik_frame_at_time(
        &g_kfm_table, ik_action_for_state(f->state), f->state_time);
    if (frame == 0) {
        frame = ik_frame_at_time(&g_kfm_table, 0, 0u);
        if (frame == 0) return;
    }
    const uint32_t frame_index = (uint32_t)(frame - kfm_frames);

    /* Facing + any AIR-baked flip mirror both the sprite pixels and the
     * anchor axis (ik_frame_screen_anchor uses the same rule). */
    const int flip_h = (f->facing < 0) !=
                       ((frame->flags & IK_FRAME_FLAG_FLIP_H) != 0u);
    const int flip_v = (frame->flags & IK_FRAME_FLAG_FLIP_V) != 0u;

    int16_t dx = 0;
    int16_t dy = 0;
    ik_frame_screen_anchor(frame, (int)f->x, (int)f->y,
                           f->facing, &dx, &dy);

    sat_draw_params_t params = sat_draw_params_default();
    if (flip_h) params.flip = SAT_FLIP_X;
    if (flip_v) params.flip = (uint8_t)(params.flip | SAT_FLIP_Y);
    /* Hit flash: white tint through a palette variant bank. */
    if (f->state == IK_STATE_HIT && ((f->state_time & 2u) != 0u)) {
        params.tint.r = 255u; params.tint.g = 120u; params.tint.b = 120u;
    }

    /* Half-transparent VDP1 RGB cannot blend with a VDP2-only pixel:
     * it replaces the stage color and becomes a solid bar. Mesh is a true
     * hardware coverage effect, so use a small trapezoid for the floor shadow. */
    {
        const int hw = ik_body_half_w(f) + 4;
        const sat_polygon_cmd_t shadow = {
            {(int16_t)(f->x - hw), (int16_t)(f->x + hw),
             (int16_t)(f->x + hw - 3), (int16_t)(f->x - hw + 3)},
            {FLOOR_SCREEN_Y - 2, FLOOR_SCREEN_Y - 2,
             FLOOR_SCREEN_Y + 2, FLOOR_SCREEN_Y + 2},
            SAT_RGB555(2u, 2u, 4u),
            SAT_SPRITE_FLAG_MESH
        };
        sat_example_must(sat_vdp1_draw_polygon(&shadow));
    }

    /* P2 draws through the registered (1,4) palette: same pixels, one
     * VDP1 VRAM copy, distinct colours. */
    if (player == 2) {
        sat_example_must(sat_render2d_set_palette(g_p2_palette));
    }
    sat_example_must(sat_draw_texture(g_frame_textures[frame_index], 0,
        &(sat_rect_t){dx, dy, frame->w, frame->h}, &params));
    if (player == 2) {
        sat_example_must(sat_render2d_set_palette(sat_palette_none()));
    }

    /* Attack boxes stay simulation-only. A half-transparent VDP1 rectangle
     * over the VDP2 stage turns into an opaque brown block on real hardware. */
}

static void draw_bars(const ik_fight_t* fight) {
    const uint16_t bar_bg = SAT_BGR555(6u, 6u, 8u);
    const uint16_t p1_fg = SAT_BGR555(28u, 6u, 6u);
    const uint16_t p2_fg = SAT_BGR555(6u, 12u, 28u);
    sat_example_must(sat_hud_bar(&g_hud, 12, 10, 120, 8,
        (uint32_t)fight->fighters[0].hp, IK_MAX_HP, bar_bg, p1_fg));
    sat_example_must(sat_hud_bar(&g_hud, 188, 10, 120, 8,
        (uint32_t)fight->fighters[1].hp, IK_MAX_HP, bar_bg, p2_fg));
    sat_example_must(sat_hud_text(&g_hud, "P1", 12, 22));
    sat_example_must(sat_hud_text(&g_hud, "P2 DUMMY", 236, 22));
    {
        char timer[16];
        uint32_t seconds = fight->timer_frames / 60u;
        timer[0] = (char)('0' + (seconds / 10u) % 10u);
        timer[1] = (char)('0' + seconds % 10u);
        timer[2] = 0;
        sat_example_must(sat_hud_text(&g_hud, timer, 152, 8));
    }
    {
        const char* status = ik_fight_status_text(fight);
        if (status) sat_example_must(sat_hud_text_centered(&g_hud, status, 160, 100));
        else sat_example_must(sat_hud_text_centered(&g_hud,
            "A/X PUNCH B/Y KICK UP JUMP START RESET", 160, 208));
    }
    sat_example_must(sat_hud_value(&g_hud, "HITS P1 ", fight->hits_p1, 12, 196));
    sat_example_must(sat_hud_value(&g_hud, "HITS P2 ", fight->hits_p2, 200, 196));
}

int main(void) {
    ik_audio_t audio = {0};
    ik_fight_t fight;

    sat_example_must(sat_app_init_default());
    /* VDP2 stage first: NBG0 plane behind the VDP1 sprite layer, with a
     * transparent VDP1 erase so the background shows through. */
    stage_init();
    sat_example_must(sat_vdp1_set_erase_transparent());
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&g_font,
        SAT_BGR555(31u, 31u, 31u), SAT_BGR555(0u, 0u, 0u), 1u));
    sat_example_must(sat_hud_init(&g_hud, &g_font, 1u, 8));

    fighters_init();
    sat_example_must(ik_audio_init(&audio));

    ik_fight_init(&fight);

    for (;;) {
        sat_pad_state_t pad1 = {0};
        sat_pad_state_t pad2 = {0};
        int have_p2 = 0;
        /* Manual frame (not sat_app_frame_begin): that helper makes the
         * VDP1 erase opaque, which would cover the VDP2 stage. */
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_vdp2_back_color_set(SAT_COLOR_BLACK));
        /* Re-apply NBG0 state inside VBlank: VDP2 register writes outside
         * VBlank are dropped by strict hardware/emulators, and stage_init
         * runs at boot (same pattern as infinite_explorer). */
        sat_example_must(sat_vdp2_layers_commit());
        sat_example_must(sat_vdp1_set_erase_transparent());
        sat_example_must(sat_begin_frame());
        sat_example_must(sat_pad_poll(&pad1));
        if (sat_pad_poll_port(1u, &pad2) == SAT_OK && pad2.connected) have_p2 = 1;

        ik_fight_update(&fight, &pad1, have_p2 ? &pad2 : 0, &g_kfm_table);
        ik_audio_process_fight(&audio, &fight);
        sat_example_must(ik_audio_update());

        /* VDP1 draws fighters + FX + HUD; the stage is VDP2. */
        stage_scroll_for_fight(&fight);
        draw_fighter(&fight.fighters[0], 1);
        draw_fighter(&fight.fighters[1], 2);
        draw_bars(&fight);

        sat_example_must(sat_app_frame_end());
        (void)sat_time_ms();
    }

    return 0;
}
