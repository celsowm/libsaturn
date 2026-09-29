/* ikemen_saturn - Ikemen GO Kung Fu Man content on Saturn 2D hardware.
 *
 * Source assets are compiled offline. Runtime code only consumes compact
 * SFF/AIR, SND, CMD and CNS tables suitable for SH-2.
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
#include "ikemen_command.h"
#include "ikemen_fight.h"
#include "ikemen_saturn/kfm_commands.h"
#include "ikemen_saturn/kfm_state_rules.h"
#include "ikemen_saturn/kfm_cns.h"
#include "ikemen_saturn/kfm_frames.h"
#include "ikemen_saturn/stage0_plane.h"

#define STAGE_PALETTE_ID 4u
#define FLOOR_SCREEN_Y 178
#define IK_FRAME_TEXTURE_CACHE_SIZE 32u

typedef struct ik_frame_texture_cache_entry {
    uint32_t pixel_ofs;
    uint32_t last_use;
    uint32_t pin_epoch;
    sat_texture_t texture;
    uint8_t used;
} ik_frame_texture_cache_entry_t;

static ik_frame_texture_cache_entry_t
    g_frame_cache[IK_FRAME_TEXTURE_CACHE_SIZE];
static uint32_t g_frame_cache_clock;
static uint32_t g_frame_cache_epoch;

static sat_palette_t g_p2_palette;
static uint16_t g_map_scratch[SAT_VDP2_NBG0_MAP_CELLS];
static sat_ascii_font_t g_font;
static sat_hud_t g_hud;
static ik_command_state_t g_command_states[2];

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
    sat_example_must(sat_vdp2_nbg0_set_priority(2u));
    sat_example_must(sat_vdp2_sprite_set_priority(7u));
}

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

static void fighters_init(void) {
    for (uint32_t i = 0u; i < IK_FRAME_TEXTURE_CACHE_SIZE; ++i) {
        g_frame_cache[i] = (ik_frame_texture_cache_entry_t){0};
    }
    g_frame_cache_clock = 0u;
    g_frame_cache_epoch = 0u;
    sat_example_must(sat_palette_register(kfm_palette_alt1, &g_p2_palette));
}

static const ik_frame_t* current_frame(const ik_fighter_t* f) {
    if (!f) return 0;
    const ik_frame_t* frame =
        ik_frame_at_time(&g_kfm_table, f->anim, f->anim_time);
    if (!frame) frame = ik_frame_at_time(&g_kfm_table, 0, 0u);
    return frame;
}

/* KFM now needs more than LibSaturn's 64 logical texture slots when all
 * standing+crouching actions are generated. Keep the source pixels in ROM and
 * upload only a bounded working set. Both fighters' current frames are pinned
 * before any VDP1 command is emitted, so an LRU eviction can never invalidate
 * a texture referenced by the current frame's command list. */
static sat_result_t frame_texture_resolve(const ik_frame_t* frame,
                                          uint32_t epoch,
                                          sat_texture_t* out_texture) {
    if (!frame || !out_texture) return SAT_ERR_INVALID_ARG;

    ++g_frame_cache_clock;
    if (g_frame_cache_clock == 0u) g_frame_cache_clock = 1u;

    for (uint32_t i = 0u; i < IK_FRAME_TEXTURE_CACHE_SIZE; ++i) {
        ik_frame_texture_cache_entry_t* e = &g_frame_cache[i];
        if (e->used && e->pixel_ofs == frame->pixel_ofs) {
            e->last_use = g_frame_cache_clock;
            e->pin_epoch = epoch;
            *out_texture = e->texture;
            return SAT_OK;
        }
    }

    uint32_t victim = IK_FRAME_TEXTURE_CACHE_SIZE;
    uint32_t oldest = 0xFFFFFFFFu;
    for (uint32_t i = 0u; i < IK_FRAME_TEXTURE_CACHE_SIZE; ++i) {
        ik_frame_texture_cache_entry_t* e = &g_frame_cache[i];
        if (!e->used) {
            victim = i;
            break;
        }
        if (e->pin_epoch != epoch && e->last_use < oldest) {
            oldest = e->last_use;
            victim = i;
        }
    }
    if (victim >= IK_FRAME_TEXTURE_CACHE_SIZE) return SAT_ERR_BUSY;

    ik_frame_texture_cache_entry_t* e = &g_frame_cache[victim];
    if (e->used) {
        sat_result_t st = sat_texture_destroy(e->texture);
        if (st != SAT_OK) return st;
        *e = (ik_frame_texture_cache_entry_t){0};
    }

    sat_surface_t surface;
    sat_result_t st = sat_surface_init(
        &surface,
        (uint8_t*)kfm_pixels + frame->pixel_ofs,
        frame->w, frame->h, frame->w, SAT_PIXEL_INDEX8,
        kfm_palette_main, 256u);
    if (st != SAT_OK) return st;

    sat_texture_t tex = {0u, 0u};
    st = sat_texture_create_from_surface(
        &tex, &surface, SAT_TEXTURE_UPLOAD_ONLY);
    if (st != SAT_OK) return st;

    e->used = 1u;
    e->pixel_ofs = frame->pixel_ofs;
    e->last_use = g_frame_cache_clock;
    e->pin_epoch = epoch;
    e->texture = tex;
    *out_texture = tex;
    return SAT_OK;
}

static void draw_fighter(const ik_frame_t* frame,
                         sat_texture_t texture,
                         const ik_fighter_t* f,
                         const ik_cns_asset_t* cns,
                         int player) {
    if (!frame || !f) return;

    const int flip_h = (f->facing < 0) !=
                       ((frame->flags & IK_FRAME_FLAG_FLIP_H) != 0u);
    const int flip_v = (frame->flags & IK_FRAME_FLAG_FLIP_V) != 0u;

    int16_t dx = 0;
    int16_t dy = 0;
    ik_frame_screen_anchor(
        frame, (int)f->x, (int)f->y, f->facing, &dx, &dy);

    sat_draw_params_t params = sat_draw_params_default();
    if (flip_h) params.flip = SAT_FLIP_X;
    if (flip_v) params.flip = (uint8_t)(params.flip | SAT_FLIP_Y);
    const ik_cns_state_t* state = ik_cns_find_state(cns, f->state);
    if (((state && state->move_type == IK_CNS_MOVE_HIT) ||
         f->state == IK_STATE_HIT) &&
        ((f->state_time & 2u) != 0u)) {
        params.tint.r = 255u;
        params.tint.g = 120u;
        params.tint.b = 120u;
    }

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

    if (player == 2) {
        sat_example_must(sat_render2d_set_palette(g_p2_palette));
    }
    sat_example_must(sat_draw_texture(
        texture, 0, &(sat_rect_t){dx, dy, frame->w, frame->h}, &params));
    if (player == 2) {
        sat_example_must(sat_render2d_set_palette(sat_palette_none()));
    }
}

static void draw_fighters(const ik_fight_t* fight) {
    const ik_frame_t* frames[2] = {
        current_frame(&fight->fighters[0]),
        current_frame(&fight->fighters[1])
    };
    if (!frames[0] || !frames[1]) return;

    ++g_frame_cache_epoch;
    if (g_frame_cache_epoch == 0u) ++g_frame_cache_epoch;

    sat_texture_t textures[2] = {{0u, 0u}, {0u, 0u}};
    sat_example_must(frame_texture_resolve(
        frames[0], g_frame_cache_epoch, &textures[0]));
    sat_example_must(frame_texture_resolve(
        frames[1], g_frame_cache_epoch, &textures[1]));

    int first = 0;
    int second = 1;
    if (fight->fighters[0].spr_priority > fight->fighters[1].spr_priority) {
        first = 1;
        second = 0;
    }

    draw_fighter(frames[first], textures[first],
                 &fight->fighters[first], fight->cns, first + 1);
    draw_fighter(frames[second], textures[second],
                 &fight->fighters[second], fight->cns, second + 1);
}

static void controls_from_commands(uint32_t player,
                                   const ik_fight_t* fight,
                                   const ik_fighter_t* fighter,
                                   ik_fight_controls_t* controls) {
    if (!controls || !fight || !fighter || player >= 2u) return;
    *controls = (ik_fight_controls_t){0};
    const ik_command_state_t* state = &g_command_states[player];

    controls->forward = (uint8_t)ik_command_active(
        state, &kfm_commands, KFM_CMD_HOLDFWD);
    controls->back = (uint8_t)ik_command_active(
        state, &kfm_commands, KFM_CMD_HOLDBACK);
    controls->up = (uint8_t)ik_command_active(
        state, &kfm_commands, KFM_CMD_HOLDUP);
    controls->down = (uint8_t)ik_command_active(
        state, &kfm_commands, KFM_CMD_HOLDDOWN);

    controls->a = (uint8_t)ik_command_active(
        state, &kfm_commands, KFM_CMD_A);
    controls->b = (uint8_t)ik_command_active(
        state, &kfm_commands, KFM_CMD_B);
    controls->c = (uint8_t)ik_command_active(
        state, &kfm_commands, KFM_CMD_C);
    controls->x = (uint8_t)ik_command_active(
        state, &kfm_commands, KFM_CMD_X);
    controls->y = (uint8_t)ik_command_active(
        state, &kfm_commands, KFM_CMD_Y);
    controls->z = (uint8_t)ik_command_active(
        state, &kfm_commands, KFM_CMD_Z);
    controls->start = (uint8_t)ik_command_active(
        state, &kfm_commands, KFM_CMD_START);
    controls->recovery = (uint8_t)ik_command_active(
        state, &kfm_commands, KFM_CMD_RECOVERY);

    {
        const uint16_t projected_time = (uint16_t)(
            fighter->state_time + (fighter->hit_pause == 0u ? 1u : 0u));
        const ik_fighter_t* p2 = &fight->fighters[player ^ 1u];
        const ik_cns_state_t* p2_state =
            ik_cns_find_state(fight->cns, p2->state);
        int body_dist_x =
            ((int)p2->x - (int)fighter->x) * (int)fighter->facing -
            fighter->push_front - p2->push_front;
        if (body_dist_x < -32768) body_dist_x = -32768;
        if (body_dist_x > 32767) body_dist_x = 32767;
        const ik_state_rule_context_t context = {
            fighter->state,
            projected_time,
            (int16_t)body_dist_x,
            ik_fight_state_type(fight, fighter),
            (uint8_t)(fighter->ctrl != 0),
            fighter->move_contact,
            ik_fight_state_type(fight, p2),
            (uint8_t)(p2_state ? p2_state->move_type : IK_CNS_MOVE_IDLE),
            0u
        };
        int16_t requested = 0;
        if (ik_command_eval_state_change(
                state, &kfm_commands, &kfm_state_rules,
                &context, &requested)) {
            controls->requested_state = requested;
            controls->has_state_request = 1u;
        }
    }
}

static void draw_bars(const ik_fight_t* fight) {
    const uint16_t bar_bg = SAT_BGR555(6u, 6u, 8u);
    const uint16_t p1_fg = SAT_BGR555(28u, 6u, 6u);
    const uint16_t p2_fg = SAT_BGR555(6u, 12u, 28u);
    const uint32_t max_hp = (uint32_t)ik_fight_max_hp(fight);

    sat_example_must(sat_hud_bar(
        &g_hud, 12, 10, 120, 8,
        (uint32_t)fight->fighters[0].hp, max_hp, bar_bg, p1_fg));
    sat_example_must(sat_hud_bar(
        &g_hud, 188, 10, 120, 8,
        (uint32_t)fight->fighters[1].hp, max_hp, bar_bg, p2_fg));
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
        if (status) {
            sat_example_must(
                sat_hud_text_centered(&g_hud, status, 160, 100));
        } else {
            sat_example_must(sat_hud_text_centered(
                &g_hud, "X/Y PUNCH A/B KICK  DOWN+BTN CROUCH", 160, 208));
        }
    }

    sat_example_must(sat_hud_value(
        &g_hud, "HITS P1 ", fight->hits_p1, 12, 196));
    sat_example_must(sat_hud_value(
        &g_hud, "HITS P2 ", fight->hits_p2, 200, 196));
}

int main(void) {
    ik_audio_t audio = {0};
    ik_fight_t fight;

    sat_example_must(sat_app_init_default());
    stage_init();
    sat_example_must(sat_vdp1_set_erase_transparent());
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font,
        SAT_BGR555(31u, 31u, 31u),
        SAT_BGR555(0u, 0u, 0u),
        1u));
    sat_example_must(sat_hud_init(&g_hud, &g_font, 1u, 8));

    fighters_init();
    sat_example_must(ik_audio_init(&audio));

    ik_fight_init(&fight, &kfm_cns);
    ik_command_state_init(&g_command_states[0]);
    ik_command_state_init(&g_command_states[1]);

    for (;;) {
        sat_pad_state_t pad1 = {0};
        sat_pad_state_t pad2 = {0};
        int have_p2 = 0;

        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_vdp2_back_color_set(SAT_COLOR_BLACK));
        sat_example_must(sat_vdp2_layers_commit());
        sat_example_must(sat_vdp1_set_erase_transparent());
        sat_example_must(sat_begin_frame());
        sat_example_must(sat_pad_poll(&pad1));
        if (sat_pad_poll_port(1u, &pad2) == SAT_OK && pad2.connected) {
            have_p2 = 1;
        }

        ik_fight_controls_t p1_controls = {0};
        ik_fight_controls_t p2_controls = {0};

        ik_command_update(
            &g_command_states[0], &kfm_commands, &pad1,
            fight.fighters[0].facing,
            fight.fighters[0].hit_pause != 0u);
        controls_from_commands(
            0u, &fight, &fight.fighters[0], &p1_controls);

        if (have_p2) {
            ik_command_update(
                &g_command_states[1], &kfm_commands, &pad2,
                fight.fighters[1].facing,
                fight.fighters[1].hit_pause != 0u);
            controls_from_commands(
                1u, &fight, &fight.fighters[1], &p2_controls);
        }

        ik_fight_update(
            &fight, &p1_controls, have_p2 ? &p2_controls : 0,
            &g_kfm_table);
        ik_audio_process_fight(&audio, &fight);
        sat_example_must(ik_audio_update());

        stage_scroll_for_fight(&fight);
        draw_fighters(&fight);
        draw_bars(&fight);

        sat_example_must(sat_app_frame_end());
        (void)sat_time_ms();
    }

    return 0;
}
