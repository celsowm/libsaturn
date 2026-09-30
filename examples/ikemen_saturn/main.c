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
#include "saturn/vdp2_color_calc.h"
#include "saturn/vdp2_color_offset.h"
#include "saturn/video.h"

#include "ikemen_anim.h"
#include "ikemen_asset_store.h"
#include "ikemen_audio.h"
#include "ikemen_command.h"
#include "ikemen_entity.h"
#include "ikemen_fight.h"
#include "ikemen_saturn/kfm_commands.h"
#include "ikemen_saturn/kfm_state_rules.h"
#include "ikemen_saturn/kfm_cns.h"
#include "ikemen_saturn/fightfx_frames.h"
#include "ikemen_saturn/kfm_frames.h"
#include "ikemen_saturn/kfm_zss_frames.h"
#include "ikemen_saturn/stage0_plane.h"

#define STAGE_PALETTE_ID 4u
#define FLOOR_SCREEN_Y 178
#define IK_FRAME_TEXTURE_CACHE_SIZE 32u
#define IK_EFFECT_INSTANCE_COUNT 4u
#define IK_AFTERIMAGE_HISTORY 32u
#define IK_EFFECT_TEXTURE_CACHE_LIMIT 3u
#define IK_ASSET_LOAD_CHUNK_BYTES (64u * 1024u)
#define IK_PREFETCH_LOOKAHEAD_TICKS 32u
#define IK_CHARACTER_MAX_SPRITE_SOURCE_BYTES \
    ((KFM_MAX_SPRITE_SOURCE_BYTES > KFM_ZSS_MAX_SPRITE_SOURCE_BYTES) ? \
         KFM_MAX_SPRITE_SOURCE_BYTES : KFM_ZSS_MAX_SPRITE_SOURCE_BYTES)
#define IK_MAX_SPRITE_SOURCE_BYTES \
    ((IK_CHARACTER_MAX_SPRITE_SOURCE_BYTES > FIGHTFX_MAX_SPRITE_SOURCE_BYTES) ? \
         IK_CHARACTER_MAX_SPRITE_SOURCE_BYTES : FIGHTFX_MAX_SPRITE_SOURCE_BYTES)
#define IK_ASSET_IO_SCRATCH_BYTES \
    ((IK_MAX_SPRITE_SOURCE_BYTES > IK_ASSET_LOAD_CHUNK_BYTES) ? \
         IK_MAX_SPRITE_SOURCE_BYTES : IK_ASSET_LOAD_CHUNK_BYTES)

typedef struct ik_frame_texture_cache_entry {
    uint16_t sprite_index;
    uint32_t last_use;
    uint32_t pin_epoch;
    sat_texture_t texture;
    uint8_t asset_slot;
    uint8_t used;
} ik_frame_texture_cache_entry_t;

typedef struct ik_afterimage_snapshot {
    const ik_frame_t* frame;
    int16_t x;
    int16_t y;
    int8_t facing;
    uint8_t valid;
} ik_afterimage_snapshot_t;

typedef struct ik_sprite_prefetch {
    uint16_t sprite_index;
    uint32_t bytes;
    uint8_t asset_slot;
    uint8_t valid;
} ik_sprite_prefetch_t;

typedef struct ik_character_runtime {
    const ik_frame_table_t* frames;
    const ik_sprite_source_t* sprites;
    const uint16_t* palette;
    uint16_t sprite_count;
    uint8_t asset_slot;
} ik_character_runtime_t;

typedef struct ik_effect_instance {
    int16_t action;
    int16_t x;
    int16_t y;
    uint16_t age;
    uint8_t active;
} ik_effect_instance_t;

static ik_frame_texture_cache_entry_t
    g_frame_cache[IK_FRAME_TEXTURE_CACHE_SIZE];
static uint32_t g_frame_cache_clock;
static uint32_t g_frame_cache_epoch;
static ik_asset_store_t g_asset_store;
static ik_sprite_prefetch_t g_prefetch[2];
static ik_effect_instance_t g_effect_instances[IK_EFFECT_INSTANCE_COUNT];
static uint8_t g_effect_cursor;
static ik_afterimage_snapshot_t
    g_afterimages[2][IK_AFTERIMAGE_HISTORY];
static uint8_t g_afterimage_head[2];
static uint8_t g_afterimage_count[2];
static uint16_t g_afterimage_capture_tick[2];
static uint8_t g_asset_io_scratch[IK_ASSET_IO_SCRATCH_BYTES]
    __attribute__((section(".wram_l"), aligned(32)));
static uint8_t g_prefetch_data[2][IK_MAX_SPRITE_SOURCE_BYTES]
    __attribute__((section(".wram_l"), aligned(32)));
#define IK_CHARACTER_MAX_SPRITE_BYTES \
    ((KFM_MAX_SPRITE_BYTES > KFM_ZSS_MAX_SPRITE_BYTES) ? \
         KFM_MAX_SPRITE_BYTES : KFM_ZSS_MAX_SPRITE_BYTES)
#define IK_MAX_SPRITE_BYTES \
    ((IK_CHARACTER_MAX_SPRITE_BYTES > FIGHTFX_MAX_SPRITE_BYTES) ? \
         IK_CHARACTER_MAX_SPRITE_BYTES : FIGHTFX_MAX_SPRITE_BYTES)
static uint8_t g_frame_decode_scratch[IK_MAX_SPRITE_BYTES]
    __attribute__((section(".wram_l"), aligned(32)));

static uint16_t g_map_scratch[SAT_VDP2_NBG0_MAP_CELLS];
static sat_ascii_font_t g_font;
static sat_hud_t g_hud;
static ik_command_state_t g_command_states[2];
static ik_entity_pool_t g_entity_pool;
static ik_entity_handle_t g_player_entities[2];

static const ik_frame_table_t g_kfm_table = {
    kfm_frames, KFM_FRAME_COUNT, kfm_clsn_boxes, KFM_CLSN_BOX_COUNT
};
static const ik_frame_table_t g_kfm_zss_table = {
    kfm_zss_frames, KFM_ZSS_FRAME_COUNT,
    kfm_zss_clsn_boxes, KFM_ZSS_CLSN_BOX_COUNT
};
static const ik_frame_table_t g_fightfx_table = {
    fightfx_frames, FIGHTFX_FRAME_COUNT,
    fightfx_clsn_boxes, FIGHTFX_CLSN_BOX_COUNT
};
static const ik_character_runtime_t g_characters[2] = {
    {&g_kfm_table, kfm_sprites, kfm_palette_main,
     KFM_SPRITE_COUNT, IK_ASSET_SLOT_P1},
    {&g_kfm_zss_table, kfm_zss_sprites, kfm_zss_palette_main,
     KFM_ZSS_SPRITE_COUNT, IK_ASSET_SLOT_P2}
};

static void asset_store_stop(sat_result_t st) {
    const char* detail =
        st == SAT_ERR_NOT_CONNECTED ? "NO EXPANSION CARTRIDGE FOUND" :
        st == SAT_ERR_UNSUPPORTED ? "1 MB CARTRIDGE IS NOT SUPPORTED" :
        "SPRITE ASSET LOAD FAILED";
    for (;;) {
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_vdp2_back_color_set(SAT_RGB555(12u, 1u, 1u)));
        sat_example_must(sat_vdp2_layers_commit());
        sat_example_must(sat_begin_frame());
        sat_example_must(sat_hud_text_centered(
            &g_hud, "4 MB RAM CARTRIDGE REQUIRED", 160, 76));
        sat_example_must(sat_hud_text_centered(&g_hud, detail, 160, 100));
        sat_example_must(sat_hud_text_centered(
            &g_hud, "INSERT 4 MB CART AND RESET", 160, 124));
        if (st != SAT_ERR_NOT_CONNECTED && st != SAT_ERR_UNSUPPORTED) {
            sat_example_must(sat_hud_value(
                &g_hud, "ERROR ", (uint32_t)(-st), 112, 148));
        }
        sat_example_must(sat_app_frame_end());
    }
}

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

static int env_shake_y(const ik_fight_t* fight) {
    static const int8_t sine16[16] = {
        0, 6, 11, 15, 16, 15, 11, 6,
        0, -6, -11, -15, -16, -15, -11, -6
    };
    if (!fight || fight->env_shake_time == 0u ||
        fight->env_shake_ampl == 0) {
        return 0;
    }
    const uint8_t index = (uint8_t)((fight->env_shake_phase >> 6) & 15u);
    return ((int)fight->env_shake_ampl * (int)sine16[index]) / 16;
}

static void stage_scroll_for_fight(const ik_fight_t* fight) {
    int mid = (int)fight->fighters[0].x + (int)fight->fighters[1].x;
    mid = (mid / 2) - 160;
    if (mid < -125) mid = -125;
    if (mid > 125) mid = 125;
    {
        const sat_vdp2_scroll_t sc = {
            (uint16_t)mid, (uint16_t)env_shake_y(fight), 0u, 0u
        };
        sat_example_must(sat_vdp2_nbg0_set_scroll(&sc));
    }
}

static void fighters_init(void) {
    for (uint32_t i = 0u; i < IK_FRAME_TEXTURE_CACHE_SIZE; ++i) {
        g_frame_cache[i] = (ik_frame_texture_cache_entry_t){0};
    }
    for (uint32_t i = 0u; i < 2u; ++i) {
        g_prefetch[i] = (ik_sprite_prefetch_t){0};
    }
    for (uint32_t i = 0u; i < IK_EFFECT_INSTANCE_COUNT; ++i) {
        g_effect_instances[i] = (ik_effect_instance_t){0};
    }
    g_effect_cursor = 0u;
    for (uint32_t p = 0u; p < 2u; ++p) {
        g_afterimage_head[p] = 0u;
        g_afterimage_count[p] = 0u;
        g_afterimage_capture_tick[p] = 0u;
        for (uint32_t i = 0u; i < IK_AFTERIMAGE_HISTORY; ++i) {
            g_afterimages[p][i] = (ik_afterimage_snapshot_t){0};
        }
    }
    g_frame_cache_clock = 0u;
    g_frame_cache_epoch = 0u;
}

static const ik_frame_t* current_frame(uint32_t player,
                                       const ik_fighter_t* f) {
    if (!f || player >= 2u) return 0;
    const ik_frame_table_t* table = g_characters[player].frames;
    const ik_frame_t* frame =
        ik_frame_at_time(table, f->anim, f->anim_time);
    if (!frame) frame = ik_frame_at_time(table, 0, 0u);
    return frame;
}

static int texture_cache_contains(uint8_t asset_slot, uint16_t sprite_index) {
    for (uint32_t i = 0u; i < IK_FRAME_TEXTURE_CACHE_SIZE; ++i) {
        const ik_frame_texture_cache_entry_t* e = &g_frame_cache[i];
        if (e->used && e->asset_slot == asset_slot &&
            e->sprite_index == sprite_index) {
            return 1;
        }
    }
    return 0;
}

/* KFM now needs more than LibSaturn's 64 logical texture slots when all
 * actions are generated. Packed SFF sprite payloads live in the mandatory
 * 4 MiB RAM cartridge, not in the executable. A cache miss copies only that
 * sprite into aligned WRAM-L staging, decodes it, and uploads the result to
 * VDP1. Both fighters' current frames are pinned before any VDP1 command is
 * emitted, so an LRU eviction can never invalidate a current-frame texture. */
static sat_result_t frame_texture_resolve(uint32_t player,
                                          const ik_frame_t* frame,
                                          uint32_t epoch,
                                          sat_texture_t* out_texture) {
    if (player >= 2u || !frame || !out_texture) return SAT_ERR_INVALID_ARG;
    const ik_character_runtime_t* character = &g_characters[player];

    ++g_frame_cache_clock;
    if (g_frame_cache_clock == 0u) g_frame_cache_clock = 1u;

    for (uint32_t i = 0u; i < IK_FRAME_TEXTURE_CACHE_SIZE; ++i) {
        ik_frame_texture_cache_entry_t* e = &g_frame_cache[i];
        if (e->used &&
            e->asset_slot == character->asset_slot &&
            e->sprite_index == frame->sprite_index) {
            e->last_use = g_frame_cache_clock;
            e->pin_epoch = epoch;
            *out_texture = e->texture;
            return SAT_OK;
        }
    }

    uint32_t victim = IK_FRAME_TEXTURE_CACHE_SIZE;
    uint32_t oldest = 0xFFFFFFFFu;
    uint32_t effect_entries = 0u;
    for (uint32_t i = 0u; i < IK_FRAME_TEXTURE_CACHE_SIZE; ++i) {
        const ik_frame_texture_cache_entry_t* e = &g_frame_cache[i];
        if (e->used && e->asset_slot == IK_ASSET_SLOT_FIGHTFX) {
            ++effect_entries;
        }
    }

    if (effect_entries >= IK_EFFECT_TEXTURE_CACHE_LIMIT) {
        for (uint32_t i = 0u; i < IK_FRAME_TEXTURE_CACHE_SIZE; ++i) {
            ik_frame_texture_cache_entry_t* e = &g_frame_cache[i];
            if (e->used && e->asset_slot == IK_ASSET_SLOT_FIGHTFX &&
                e->pin_epoch != epoch && e->last_use < oldest) {
                oldest = e->last_use;
                victim = i;
            }
        }
    }

    if (victim >= IK_FRAME_TEXTURE_CACHE_SIZE) {
        oldest = 0xFFFFFFFFu;
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
    }
    if (victim >= IK_FRAME_TEXTURE_CACHE_SIZE) return SAT_ERR_BUSY;

    ik_frame_texture_cache_entry_t* e = &g_frame_cache[victim];
    if (e->used) {
        sat_result_t st = sat_texture_destroy(e->texture);
        if (st != SAT_OK) return st;
        *e = (ik_frame_texture_cache_entry_t){0};
    }

    if (frame->sprite_index >= character->sprite_count) {
        return SAT_ERR_INVALID_ARG;
    }
    const ik_sprite_source_t* source =
        &character->sprites[frame->sprite_index];
    if (source->data_size > IK_MAX_SPRITE_SOURCE_BYTES) {
        return SAT_ERR_CAPACITY;
    }

    const uint8_t* packed = g_asset_io_scratch;
    if (g_prefetch[player].valid &&
        g_prefetch[player].asset_slot == character->asset_slot &&
        g_prefetch[player].sprite_index == frame->sprite_index &&
        g_prefetch[player].bytes == source->data_size) {
        packed = g_prefetch_data[player];
    } else {
        sat_result_t st = ik_asset_store_read_sprite(
            &g_asset_store, character->asset_slot, source,
            g_asset_io_scratch, (uint32_t)sizeof(g_asset_io_scratch));
        if (st != SAT_OK) return st;
    }

    ik_sprite_source_t local_source = *source;
    local_source.data_ofs = 0u;
    if (!ik_sprite_decode(
            &local_source,
            packed, source->data_size,
            g_frame_decode_scratch, sizeof(g_frame_decode_scratch))) {
        return SAT_ERR_IO;
    }

    sat_surface_t surface;
    sat_result_t st = sat_surface_init(
        &surface,
        g_frame_decode_scratch,
        frame->w, frame->h, frame->w, SAT_PIXEL_INDEX8,
        character->palette, 256u);
    if (st != SAT_OK) return st;

    sat_texture_t tex = {0u, 0u};
    st = sat_texture_create_from_surface(
        &tex, &surface, SAT_TEXTURE_UPLOAD_ONLY);
    if (st != SAT_OK) return st;

    e->used = 1u;
    e->asset_slot = character->asset_slot;
    e->sprite_index = frame->sprite_index;
    e->last_use = g_frame_cache_clock;
    e->pin_epoch = epoch;
    e->texture = tex;
    *out_texture = tex;
    return SAT_OK;
}

static sat_result_t prefetch_next_frame(
    uint32_t player,
    const ik_fighter_t* fighter,
    const ik_frame_t* current
) {
    if (player >= 2u || !fighter || !current) return SAT_ERR_INVALID_ARG;
    const ik_character_runtime_t* character = &g_characters[player];

    const ik_frame_t* next = 0;
    for (uint32_t dt = 1u; dt <= IK_PREFETCH_LOOKAHEAD_TICKS; ++dt) {
        const ik_frame_t* candidate = ik_frame_at_time(
            character->frames, fighter->anim, fighter->anim_time + dt);
        if (!candidate) break;
        if (candidate->sprite_index != current->sprite_index) {
            next = candidate;
            break;
        }
    }

    if (!next ||
        texture_cache_contains(character->asset_slot, next->sprite_index)) {
        g_prefetch[player].valid = 0u;
        return SAT_OK;
    }
    if (next->sprite_index >= character->sprite_count) {
        return SAT_ERR_INVALID_ARG;
    }

    const ik_sprite_source_t* source =
        &character->sprites[next->sprite_index];
    if (source->data_size > IK_MAX_SPRITE_SOURCE_BYTES) {
        return SAT_ERR_CAPACITY;
    }

    SAT_TRY(ik_asset_store_read_sprite(
        &g_asset_store, character->asset_slot, source,
        g_prefetch_data[player], IK_MAX_SPRITE_SOURCE_BYTES));
    g_prefetch[player].sprite_index = next->sprite_index;
    g_prefetch[player].bytes = source->data_size;
    g_prefetch[player].asset_slot = character->asset_slot;
    g_prefetch[player].valid = 1u;
    return SAT_OK;
}

static sat_result_t effect_texture_resolve(const ik_frame_t* frame,
                                           uint32_t epoch,
                                           sat_texture_t* out_texture) {
    if (!frame || !out_texture ||
        frame->sprite_index >= FIGHTFX_SPRITE_COUNT) {
        return SAT_ERR_INVALID_ARG;
    }

    ++g_frame_cache_clock;
    if (g_frame_cache_clock == 0u) g_frame_cache_clock = 1u;

    for (uint32_t i = 0u; i < IK_FRAME_TEXTURE_CACHE_SIZE; ++i) {
        ik_frame_texture_cache_entry_t* e = &g_frame_cache[i];
        if (e->used && e->asset_slot == IK_ASSET_SLOT_FIGHTFX &&
            e->sprite_index == frame->sprite_index) {
            e->last_use = g_frame_cache_clock;
            e->pin_epoch = epoch;
            *out_texture = e->texture;
            return SAT_OK;
        }
    }

    uint32_t victim = IK_FRAME_TEXTURE_CACHE_SIZE;
    uint32_t oldest = 0xFFFFFFFFu;
    uint32_t effect_entries = 0u;
    for (uint32_t i = 0u; i < IK_FRAME_TEXTURE_CACHE_SIZE; ++i) {
        const ik_frame_texture_cache_entry_t* e = &g_frame_cache[i];
        if (e->used && e->asset_slot == IK_ASSET_SLOT_FIGHTFX) {
            ++effect_entries;
        }
    }

    if (effect_entries >= IK_EFFECT_TEXTURE_CACHE_LIMIT) {
        for (uint32_t i = 0u; i < IK_FRAME_TEXTURE_CACHE_SIZE; ++i) {
            ik_frame_texture_cache_entry_t* e = &g_frame_cache[i];
            if (e->used && e->asset_slot == IK_ASSET_SLOT_FIGHTFX &&
                e->pin_epoch != epoch && e->last_use < oldest) {
                oldest = e->last_use;
                victim = i;
            }
        }
    }

    if (victim >= IK_FRAME_TEXTURE_CACHE_SIZE) {
        oldest = 0xFFFFFFFFu;
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
    }
    if (victim >= IK_FRAME_TEXTURE_CACHE_SIZE) return SAT_ERR_BUSY;

    ik_frame_texture_cache_entry_t* e = &g_frame_cache[victim];
    if (e->used) {
        SAT_TRY(sat_texture_destroy(e->texture));
        *e = (ik_frame_texture_cache_entry_t){0};
    }

    const ik_sprite_source_t* source =
        &fightfx_sprites[frame->sprite_index];
    if (source->data_size > sizeof(g_asset_io_scratch) ||
        source->palette_index >= FIGHTFX_PALETTE_COUNT) {
        return SAT_ERR_CAPACITY;
    }
    SAT_TRY(ik_asset_store_read_sprite(
        &g_asset_store, IK_ASSET_SLOT_FIGHTFX, source,
        g_asset_io_scratch, (uint32_t)sizeof(g_asset_io_scratch)));

    ik_sprite_source_t local_source = *source;
    local_source.data_ofs = 0u;
    if (!ik_sprite_decode(
            &local_source, g_asset_io_scratch, source->data_size,
            g_frame_decode_scratch, sizeof(g_frame_decode_scratch))) {
        return SAT_ERR_IO;
    }

    sat_surface_t surface;
    SAT_TRY(sat_surface_init(
        &surface, g_frame_decode_scratch,
        frame->w, frame->h, frame->w, SAT_PIXEL_INDEX8,
        fightfx_palettes[source->palette_index], 256u));

    sat_texture_t texture = {0u, 0u};
    SAT_TRY(sat_texture_create_from_surface(
        &texture, &surface, SAT_TEXTURE_UPLOAD_ONLY));

    e->used = 1u;
    e->asset_slot = IK_ASSET_SLOT_FIGHTFX;
    e->sprite_index = frame->sprite_index;
    e->last_use = g_frame_cache_clock;
    e->pin_epoch = epoch;
    e->texture = texture;
    *out_texture = texture;
    return SAT_OK;
}

static void spawn_effect_events(const ik_fight_t* fight) {
    if (!fight) return;
    for (uint32_t i = 0u; i < fight->effect_count; ++i) {
        const ik_effect_event_t* event = &fight->effect_events[i];
        if (!ik_frames_bounds(&g_fightfx_table, event->action, 0, 0)) {
            continue;
        }

        uint32_t slot = IK_EFFECT_INSTANCE_COUNT;
        for (uint32_t j = 0u; j < IK_EFFECT_INSTANCE_COUNT; ++j) {
            if (!g_effect_instances[j].active) {
                slot = j;
                break;
            }
        }
        if (slot >= IK_EFFECT_INSTANCE_COUNT) {
            slot = g_effect_cursor++ % IK_EFFECT_INSTANCE_COUNT;
        }

        g_effect_instances[slot].action = event->action;
        g_effect_instances[slot].x = event->x;
        g_effect_instances[slot].y = event->y;
        g_effect_instances[slot].age = 0u;
        g_effect_instances[slot].active = 1u;
    }
}

static void draw_effects(void) {
    for (uint32_t i = 0u; i < IK_EFFECT_INSTANCE_COUNT; ++i) {
        ik_effect_instance_t* effect = &g_effect_instances[i];
        if (!effect->active) continue;

        const uint32_t duration =
            ik_action_duration_ticks(&g_fightfx_table, effect->action);
        if (duration != 0u && effect->age >= duration) {
            effect->active = 0u;
            continue;
        }

        const ik_frame_t* frame =
            ik_frame_at_time(&g_fightfx_table, effect->action, effect->age);
        if (!frame) {
            effect->active = 0u;
            continue;
        }

        sat_texture_t texture = {0u, 0u};
        sat_example_must(effect_texture_resolve(
            frame, g_frame_cache_epoch, &texture));

        int16_t dx = 0;
        int16_t dy = 0;
        ik_frame_screen_anchor(
            frame, effect->x, effect->y, 1, &dx, &dy);

        sat_draw_params_t params = sat_draw_params_default();
        if ((frame->flags & IK_FRAME_FLAG_BLEND_ADD) != 0u) {
            params.blend_mode = SAT_BLEND_ADD;
        } else if ((frame->flags & IK_FRAME_FLAG_BLEND_SUBTRACT) != 0u) {
            params.blend_mode = SAT_BLEND_SUBTRACT;
        }

        sat_example_must(sat_draw_texture(
            texture, 0,
            &(sat_rect_t){dx, dy, frame->w, frame->h}, &params));
        effect->age++;
    }
}

static void afterimage_capture(
    uint8_t player,
    const ik_fighter_t* fighter,
    const ik_frame_t* frame
) {
    if (player >= 2u || !fighter || !frame) return;
    if (fighter->afterimage_time == 0u) {
        g_afterimage_count[player] = 0u;
        g_afterimage_capture_tick[player] = 0u;
        return;
    }

    const uint8_t timegap =
        fighter->afterimage_timegap ? fighter->afterimage_timegap : 1u;
    if ((g_afterimage_capture_tick[player]++ % timegap) != 0u) return;

    const uint8_t slot = g_afterimage_head[player];
    g_afterimages[player][slot].frame = frame;
    g_afterimages[player][slot].x = fighter->x;
    g_afterimages[player][slot].y = fighter->y;
    g_afterimages[player][slot].facing = fighter->facing;
    g_afterimages[player][slot].valid = 1u;

    g_afterimage_head[player] =
        (uint8_t)((slot + 1u) % IK_AFTERIMAGE_HISTORY);
    if (g_afterimage_count[player] < IK_AFTERIMAGE_HISTORY) {
        ++g_afterimage_count[player];
    }
}

static uint8_t clamp_u8_int(int value);

static void draw_afterimages(
    uint8_t player,
    const ik_fighter_t* fighter,
    int darken
) {
    if (player >= 2u || !fighter || fighter->afterimage_time == 0u) return;
    const uint8_t count = g_afterimage_count[player];
    const uint8_t framegap =
        fighter->afterimage_framegap ? fighter->afterimage_framegap : 1u;
    const uint8_t max_trail =
        fighter->afterimage_length ? fighter->afterimage_length : 1u;

    uint8_t drawn = 0u;
    for (uint8_t step = framegap;
         step <= count && drawn < max_trail;
         step = (uint8_t)(step + framegap)) {
        const uint8_t index = (uint8_t)(
            (g_afterimage_head[player] + IK_AFTERIMAGE_HISTORY - step) %
            IK_AFTERIMAGE_HISTORY);
        const ik_afterimage_snapshot_t* snap =
            &g_afterimages[player][index];
        if (!snap->valid || !snap->frame) continue;

        sat_texture_t texture = {0u, 0u};
        sat_example_must(frame_texture_resolve(
            player, snap->frame, g_frame_cache_epoch, &texture));

        int16_t dx = 0;
        int16_t dy = 0;
        ik_frame_screen_anchor(
            snap->frame, snap->x, snap->y, snap->facing, &dx, &dy);

        sat_draw_params_t params = sat_draw_params_default();
        const int flip_h =
            (snap->facing < 0) !=
            ((snap->frame->flags & IK_FRAME_FLAG_FLIP_H) != 0u);
        const int flip_v =
            (snap->frame->flags & IK_FRAME_FLAG_FLIP_V) != 0u;
        if (flip_h) params.flip = SAT_FLIP_X;
        if (flip_v) params.flip =
            (uint8_t)(params.flip | SAT_FLIP_Y);
        params.blend_mode = SAT_BLEND_ADD;
        {
            const uint32_t bright = fighter->afterimage_bright_rgb;
            const uint32_t contrast = fighter->afterimage_contrast_rgb;
            const uint32_t add = fighter->afterimage_add_rgb;
            const uint32_t mul = fighter->afterimage_mul_rgb;
            const int br = (int8_t)(bright & 0xffu);
            const int bg = (int8_t)((bright >> 8) & 0xffu);
            const int bb = (int8_t)((bright >> 16) & 0xffu);
            const int ar = (int8_t)(add & 0xffu);
            const int ag = (int8_t)((add >> 8) & 0xffu);
            const int ab = (int8_t)((add >> 16) & 0xffu);
            const int cr = (int)(contrast & 0xffu);
            const int cg = (int)((contrast >> 8) & 0xffu);
            const int cb = (int)((contrast >> 16) & 0xffu);
            const int mr = (int)(mul & 0xffu);
            const int mg = (int)((mul >> 8) & 0xffu);
            const int mb = (int)((mul >> 16) & 0xffu);
            params.tint.r = clamp_u8_int((cr * mr) / 255 + br + ar);
            params.tint.g = clamp_u8_int((cg * mg) / 255 + bg + ag);
            params.tint.b = clamp_u8_int((cb * mb) / 255 + bb + ab);
        }
        if (darken) {
            params.tint.r = (uint8_t)((params.tint.r * 160u) / 255u);
            params.tint.g = (uint8_t)((params.tint.g * 160u) / 255u);
            params.tint.b = (uint8_t)((params.tint.b * 160u) / 255u);
        }

        sat_example_must(sat_draw_texture(
            texture, 0,
            &(sat_rect_t){dx, dy, snap->frame->w, snap->frame->h},
            &params));
        ++drawn;
    }
}

static uint8_t clamp_u8_int(int value) {
    if (value < 0) return 0u;
    if (value > 255) return 255u;
    return (uint8_t)value;
}

static void apply_fighter_palfx(
    const ik_fighter_t* fighter,
    sat_draw_params_t* params
) {
    static const int8_t sine16[16] = {
        0, 6, 11, 15, 16, 15, 11, 6,
        0, -6, -11, -15, -16, -15, -11, -6
    };
    if (!fighter || !params || fighter->palfx_time == 0u) return;

    const uint16_t cycle =
        fighter->palfx_cycle ? fighter->palfx_cycle : 1u;
    const uint16_t phase =
        (uint16_t)((fighter->palfx_phase * 16u) / cycle);
    const int wave = sine16[phase & 15u];

    const int bias_r =
        fighter->palfx_add_r + fighter->palfx_sin_r * wave / 16;
    const int bias_g =
        fighter->palfx_add_g + fighter->palfx_sin_g * wave / 16;
    const int bias_b =
        fighter->palfx_add_b + fighter->palfx_sin_b * wave / 16;
    int max_bias = bias_r;
    if (bias_g > max_bias) max_bias = bias_g;
    if (bias_b > max_bias) max_bias = bias_b;

    params->tint.r = clamp_u8_int(255 + bias_r - max_bias);
    params->tint.g = clamp_u8_int(255 + bias_g - max_bias);
    params->tint.b = clamp_u8_int(255 + bias_b - max_bias);
}

static void draw_fighter(const ik_frame_t* frame,
                         sat_texture_t texture,
                         const ik_fighter_t* f,
                         const ik_cns_asset_t* cns,
                         int darken) {
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
    apply_fighter_palfx(f, &params);
    if (darken) {
        params.tint.r = (uint8_t)((params.tint.r * 160u) / 255u);
        params.tint.g = (uint8_t)((params.tint.g * 160u) / 255u);
        params.tint.b = (uint8_t)((params.tint.b * 160u) / 255u);
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

    sat_example_must(sat_draw_texture(
        texture, 0, &(sat_rect_t){dx, dy, frame->w, frame->h}, &params));
}

static void draw_helper_entity(const ik_frame_t* frame,
                               sat_texture_t texture,
                               const ik_entity_t* entity,
                               int darken) {
    if (!frame || !entity) return;

    const int flip_h = (entity->facing < 0) !=
                       ((frame->flags & IK_FRAME_FLAG_FLIP_H) != 0u);
    const int flip_v =
        (frame->flags & IK_FRAME_FLAG_FLIP_V) != 0u;
    const int x = ik_cns_q8_to_int(entity->x_q8);
    const int y = ik_cns_q8_to_int(entity->y_q8);

    int16_t dx = 0;
    int16_t dy = 0;
    ik_frame_screen_anchor(
        frame, x, y, entity->facing, &dx, &dy);

    sat_draw_params_t params = sat_draw_params_default();
    if (flip_h) params.flip = SAT_FLIP_X;
    if (flip_v) params.flip =
        (uint8_t)(params.flip | SAT_FLIP_Y);
    if ((frame->flags & IK_FRAME_FLAG_BLEND_ADD) != 0u) {
        params.blend_mode = SAT_BLEND_ADD;
    } else if ((frame->flags & IK_FRAME_FLAG_BLEND_SUBTRACT) != 0u) {
        params.blend_mode = SAT_BLEND_SUBTRACT;
    }
    if (darken) {
        params.tint.r = 160u;
        params.tint.g = 160u;
        params.tint.b = 160u;
    }

    sat_example_must(sat_draw_texture(
        texture, 0,
        &(sat_rect_t){dx, dy, frame->w, frame->h},
        &params));
}

typedef struct ik_combat_render_item {
    const ik_frame_t* frame;
    sat_texture_t texture;
    const ik_fighter_t* fighter;
    const ik_entity_t* entity;
    int8_t priority;
    uint8_t player;
    uint8_t helper;
} ik_combat_render_item_t;

static void draw_combat_entities(const ik_fight_t* fight) {
    if (!fight) return;

    ++g_frame_cache_epoch;
    if (g_frame_cache_epoch == 0u) ++g_frame_cache_epoch;

    ik_combat_render_item_t items[IK_ENTITY_CAPACITY];
    uint8_t item_count = 0u;

    const ik_frame_t* fighter_frames[2] = {
        current_frame(0u, &fight->fighters[0]),
        current_frame(1u, &fight->fighters[1])
    };

    for (uint8_t player = 0u; player < 2u; ++player) {
        afterimage_capture(
            player, &fight->fighters[player], fighter_frames[player]);
    }
    for (uint8_t player = 0u; player < 2u; ++player) {
        draw_afterimages(
            player, &fight->fighters[player],
            fight->super_darken_time > 0u);
    }

    for (uint8_t player = 0u; player < 2u; ++player) {
        const ik_frame_t* frame = fighter_frames[player];
        if (!frame || item_count >= IK_ENTITY_CAPACITY) continue;

        sat_texture_t texture = {0u, 0u};
        sat_example_must(frame_texture_resolve(
            player, frame, g_frame_cache_epoch, &texture));
        sat_example_must(prefetch_next_frame(
            player, &fight->fighters[player], frame));

        ik_combat_render_item_t* item = &items[item_count++];
        item->frame = frame;
        item->texture = texture;
        item->fighter = &fight->fighters[player];
        item->entity = 0;
        item->priority = fight->fighters[player].spr_priority;
        item->player = player;
        item->helper = 0u;
    }

    for (uint8_t slot = 0u;
         slot < IK_ENTITY_CAPACITY &&
         item_count < IK_ENTITY_CAPACITY;
         ++slot) {
        const ik_entity_t* entity = &g_entity_pool.entities[slot];
        if ((entity->type != IK_ENTITY_HELPER &&
             entity->type != IK_ENTITY_PROJECTILE) ||
            entity->owner_player >= 2u) {
            continue;
        }

        const uint8_t player = entity->owner_player;
        const ik_frame_t* frame = ik_frame_at_time(
            g_characters[player].frames,
            entity->anim_no, entity->anim_time);
        if (!frame) continue;

        sat_texture_t texture = {0u, 0u};
        sat_example_must(frame_texture_resolve(
            player, frame, g_frame_cache_epoch, &texture));

        ik_combat_render_item_t* item = &items[item_count++];
        item->frame = frame;
        item->texture = texture;
        item->fighter = 0;
        item->entity = entity;
        item->priority = entity->spr_priority;
        item->player = player;
        item->helper = 1u;
    }

    /* MUGEN/Ikemen SprPriority is an ordering key. Stable insertion sort
     * keeps equal-priority player/helper creation order deterministic. */
    for (uint8_t i = 1u; i < item_count; ++i) {
        const ik_combat_render_item_t key = items[i];
        uint8_t j = i;
        while (j > 0u && items[j - 1u].priority > key.priority) {
            items[j] = items[j - 1u];
            --j;
        }
        items[j] = key;
    }

    for (uint8_t i = 0u; i < item_count; ++i) {
        const ik_combat_render_item_t* item = &items[i];
        if (item->helper) {
            draw_helper_entity(
                item->frame, item->texture, item->entity,
                fight->super_darken_time > 0u);
        } else {
            draw_fighter(
                item->frame, item->texture, item->fighter, fight->cns,
                fight->super_darken_time > 0u);
        }
    }
}

typedef struct ik_rule_expr_user {
    ik_entity_expr_binding_t entity;
    const ik_command_state_t* command_state;
} ik_rule_expr_user_t;

static int rule_expr_read_field(
    void* user,
    uint8_t redirect,
    uint8_t field,
    int16_t index,
    int32_t* out_value
) {
    if (!user) return 0;
    ik_rule_expr_user_t* rule = (ik_rule_expr_user_t*)user;
    return ik_entity_expr_read_field(
        &rule->entity, redirect, field, index, out_value);
}

static int rule_expr_read_command(
    void* user,
    uint16_t command_id,
    int32_t* out_value
) {
    if (!user || !out_value) return 0;
    const ik_rule_expr_user_t* rule =
        (const ik_rule_expr_user_t*)user;
    *out_value = ik_command_active(
        rule->command_state, &kfm_commands, command_id);
    return 1;
}

static void sync_player_entities(const ik_fight_t* fight) {
    if (!fight) return;
    for (uint32_t i = 0u; i < 2u; ++i) {
        const ik_fighter_t* fighter = &fight->fighters[i];
        ik_entity_t* entity =
            ik_entity_get(&g_entity_pool, g_player_entities[i]);
        if (!entity) continue;

        const ik_cns_state_t* state =
            ik_cns_find_state(fight->cns, fighter->state);
        entity->x_q8 = fighter->x_q8;
        entity->y_q8 = fighter->y_q8;
        entity->vx_q8 = fighter->vx_q8;
        entity->vy_q8 = fighter->vy_q8;
        entity->state_no = fighter->state;
        entity->prev_state_no = fighter->prev_state;
        entity->state_time = (uint16_t)(
            fighter->state_time + (fighter->hit_pause == 0u ? 1u : 0u));
        entity->anim_no = fighter->anim;
        entity->anim_time = fighter->anim_time;
        entity->life = fighter->hp;
        entity->power = fighter->power;
        entity->active_hit_attr_mask = 0u;
        if (fighter->active_hitdef_global >= 0 &&
            fighter->active_hitdef_global < (int16_t)fight->cns->hitdef_count) {
            entity->active_hit_attr_mask =
                fight->cns->hitdefs[(uint16_t)fighter->active_hitdef_global]
                    .attack_attr_mask;
        }
        entity->push_back = fighter->push_back;
        entity->push_front = fighter->push_front;
        entity->facing = fighter->facing;
        entity->ctrl = (uint8_t)(fighter->ctrl != 0);
        entity->state_type = ik_fight_state_type(fight, fighter);
        entity->move_type =
            (uint8_t)(state ? state->move_type : IK_CNS_MOVE_IDLE);
        entity->move_contact = fighter->move_contact;

        const int8_t target_index = fighter->target_index;
        const ik_entity_handle_t target =
            target_index >= 0 && target_index < 2
                ? g_player_entities[(uint8_t)target_index]
                : ik_entity_invalid_handle();
        (void)ik_entity_set_target(
            &g_entity_pool, g_player_entities[i], target);
    }
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
        ik_rule_expr_user_t user = {
            {&g_entity_pool, g_player_entities[player]},
            state
        };
        const ik_expr_context_t expression = {
            &user,
            rule_expr_read_field,
            rule_expr_read_command
        };
        int16_t requested = 0;
        if (ik_command_eval_state_change_expr(
                &kfm_state_rules, &expression, &requested)) {
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
    sat_example_must(sat_hud_text(&g_hud, "P2 ZSS", 252, 22));

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
    sat_example_must(sat_vdp1_set_erase_transparent());
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font,
        SAT_BGR555(31u, 31u, 31u),
        SAT_BGR555(0u, 0u, 0u),
        1u));
    sat_example_must(sat_hud_init(&g_hud, &g_font, 1u, 8));
    sat_example_must(sat_vdp2_sprite_color_calc_configure_alpha(6u));

    sat_example_loading_frame(
        &g_font, "IKEMEN SATURN", "LOADING 4 MB RAM CART", 5u, 320u, 224u);
    {
        sat_result_t st = ik_asset_store_init(&g_asset_store);
        if (st == SAT_OK) {
            st = ik_asset_store_load_blob(
                &g_asset_store, IK_ASSET_SLOT_P1,
                "KFM_SPR.BIN", KFM_SPRITE_DATA_BYTES,
                g_asset_io_scratch, (uint32_t)sizeof(g_asset_io_scratch));
        }
        if (st != SAT_OK) asset_store_stop(st);
    }
    sat_example_loading_frame(
        &g_font, "IKEMEN SATURN", "P1 RESIDENT IN CART", 45u, 320u, 224u);
    {
        const sat_result_t st = ik_asset_store_load_blob(
            &g_asset_store, IK_ASSET_SLOT_P2,
            "KFM_ZSS.BIN", KFM_ZSS_SPRITE_DATA_BYTES,
            g_asset_io_scratch, (uint32_t)sizeof(g_asset_io_scratch));
        if (st != SAT_OK) asset_store_stop(st);
    }
    sat_example_loading_frame(
        &g_font, "IKEMEN SATURN", "P1 + P2 RESIDENT IN CART", 75u, 320u, 224u);
    {
        const sat_result_t st = ik_asset_store_load_blob(
            &g_asset_store, IK_ASSET_SLOT_FIGHTFX,
            "FIGHTFX.BIN", FIGHTFX_SPRITE_DATA_BYTES,
            g_asset_io_scratch, (uint32_t)sizeof(g_asset_io_scratch));
        if (st != SAT_OK) asset_store_stop(st);
    }
    sat_example_loading_frame(
        &g_font, "IKEMEN SATURN", "FIGHTFX RESIDENT IN CART", 90u, 320u, 224u);

    stage_init();
    fighters_init();
    /* fightfx AIR actions 0-3/40 are additive. Claim the global VDP2
     * sprite color-calc mode up front so the very first spark frame uses
     * the correct Saturn compositor mode. */
    sat_example_must(sat_vdp2_sprite_color_calc_claim_mode(
        SAT_VDP2_COLOR_CALC_ADD));
    sat_example_must(ik_audio_init(&audio));

    ik_fight_init(&fight, &kfm_cns);
    ik_entity_pool_init(&g_entity_pool);
    sat_example_must(ik_entity_spawn(
        &g_entity_pool, IK_ENTITY_PLAYER, 1, 0u,
        ik_entity_invalid_handle(), &g_player_entities[0])
        ? SAT_OK : SAT_ERR_CAPACITY);
    sat_example_must(ik_entity_spawn(
        &g_entity_pool, IK_ENTITY_PLAYER, 2, 1u,
        ik_entity_invalid_handle(), &g_player_entities[1])
        ? SAT_OK : SAT_ERR_CAPACITY);
    ik_fight_bind_entities(
        &fight, &g_entity_pool,
        g_player_entities[0], g_player_entities[1]);
    sync_player_entities(&fight);
    ik_command_state_init(&g_command_states[0]);
    ik_command_state_init(&g_command_states[1]);

    for (;;) {
        sat_pad_state_t pad1 = {0};
        sat_pad_state_t pad2 = {0};
        int have_p2 = 0;

        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_vdp2_back_color_set(SAT_COLOR_BLACK));
        if (fight.super_darken_time > 0u) {
            const sat_vdp2_color_offset_t darken = {-96, -96, -96};
            sat_example_must(sat_vdp2_color_offset_set(
                SAT_VDP2_COLOR_OFFSET_A, &darken));
            sat_example_must(sat_vdp2_color_offset_enable(
                SAT_VDP2_LAYER_NBG0, SAT_VDP2_COLOR_OFFSET_A));
        } else {
            sat_example_must(sat_vdp2_color_offset_disable(
                SAT_VDP2_LAYER_NBG0));
        }
        sat_example_must(sat_vdp2_layers_commit());
        /* layers_commit rewrites PRISA; replay sprite color-calc state while
         * still in VBlank before submitting the new VDP1 list. */
        sat_example_must(sat_vdp2_sprite_color_calc_commit());
        sat_example_must(sat_vdp1_set_erase_transparent());
        sat_example_must(sat_begin_frame());
        sat_example_must(sat_pad_poll(&pad1));
        if (sat_pad_poll_port(1u, &pad2) == SAT_OK && pad2.connected) {
            have_p2 = 1;
        }

        ik_fight_controls_t p1_controls = {0};
        ik_fight_controls_t p2_controls = {0};

        sync_player_entities(&fight);
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
            g_characters[0].frames, g_characters[1].frames);
        spawn_effect_events(&fight);
        ik_audio_process_fight(&audio, &fight);
        sat_example_must(ik_audio_update());

        stage_scroll_for_fight(&fight);
        draw_combat_entities(&fight);
        draw_effects();
        draw_bars(&fight);

        sat_example_must(sat_app_frame_end());
        (void)sat_time_ms();
    }

    return 0;
}
