#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "examples/ikemen_saturn/ikemen_anim.h"
#include "examples/ikemen_saturn/ikemen_cns.h"
#include "examples/ikemen_saturn/ikemen_command.h"
#include "examples/ikemen_saturn/ikemen_entity.h"
#include "examples/ikemen_saturn/ikemen_fight.h"
#include "examples/ikemen_saturn/ikemen_frame.h"
#include "ikemen_saturn/kfm_cns.h"
#include "ikemen_saturn/kfm_commands.h"
#include "ikemen_saturn/kfm_frames.h"
#include "ikemen_saturn/kfm_state_rules.h"

static const ik_frame_table_t k_frames = {
    kfm_frames, KFM_FRAME_COUNT, kfm_clsn_boxes, KFM_CLSN_BOX_COUNT
};

static double q8(int32_t v) {
    return static_cast<double>(v) / 256.0;
}

static int state_type(uint8_t v) {
    return v > 0 ? static_cast<int>(v) - 1 : 0;
}

static int move_type(uint8_t v) {
    return v > 0 ? static_cast<int>(v) - 1 : 0;
}

static int anim_elem(const ik_frame_table_t* table, int action, uint32_t time) {
    int elem = 0;
    ik_anim_trace_state(table, action, time, &elem, nullptr);
    return elem;
}

static unsigned anim_curtime(
    const ik_frame_table_t* table, int action, uint32_t time
) {
    uint32_t cur = time;
    ik_anim_trace_state(table, action, time, nullptr, &cur);
    return static_cast<unsigned>(cur);
}

static int entity_id(const ik_entity_pool_t* pool, ik_entity_handle_t h) {
    const ik_entity_t* e = ik_entity_get_const(pool, h);
    return e ? static_cast<int>(e->id) : -1;
}

static void print_handle_targets(
    FILE* out, const ik_entity_pool_t* pool, const ik_entity_t* e
) {
    std::fputc('[', out);
    bool first = true;
    if (e) {
        for (uint8_t i = 0; i < e->target_count; ++i) {
            const int id = entity_id(pool, e->targets[i]);
            if (id < 0) continue;
            if (!first) std::fputc(',', out);
            std::fprintf(out, "%d", id);
            first = false;
        }
    }
    std::fputc(']', out);
}

static void print_target_ids(FILE* out, const ik_entity_t* e) {
    std::fputc('[', out);
    bool first = true;
    if (e) {
        for (uint8_t i = 0; i < e->target_count; ++i) {
            if (!first) std::fputc(',', out);
            std::fprintf(out, "%d", static_cast<int>(e->target_ids[i]));
            first = false;
        }
    }
    std::fputc(']', out);
}

/* Trace contract: `juggle` is the air-juggle budget this fighter still has
 * against whoever hit it (0 before the first hit), like upstream's
 * ghv.targetedBy; `gethit_chain_id` is 0 when nothing has hit it. */
static int juggle_left(const ik_fighter_t& f) {
    return f.juggle_owner >= 0 ? static_cast<int>(f.juggle_points) : 0;
}

static int chain_id(int32_t last_hit_id) {
    return last_hit_id < 0 ? 0 : static_cast<int>(last_hit_id);
}

static void print_root(
    FILE* out, const ik_fight_t* fight, const ik_entity_pool_t* pool, int p
) {
    const ik_fighter_t& f = fight->fighters[p];
    const ik_entity_t* mirror =
        ik_entity_get_const(pool, fight->player_entities[p]);
    std::fprintf(out,
        "{\"player_no\":%d,\"helper_index\":0,\"id\":%d,"
        "\"helper_id\":0,\"parent_id\":-1,\"team_side\":%d,"
        "\"state_no\":%d,\"state_time\":%u,"
        "\"state_type\":%d,\"move_type\":%d,\"ctrl\":%s,"
        "\"anim\":%d,\"anim_elem\":%d,\"anim_time\":%u,"
        "\"pos\":[%.6f,%.6f,0],\"vel\":[%.6f,%.6f,0],"
        "\"facing\":%d,\"life\":%d,\"power\":%d,\"juggle\":%d,"
        "\"hit_pause\":%u,\"move_contact_type\":%u,"
        "\"move_contact_time\":%u,\"gethit_chain_id\":%d,"
        "\"targets\":",
        p, mirror ? static_cast<int>(mirror->id) : p + 1, p,
        static_cast<int>(f.state), static_cast<unsigned>(f.state_time),
        state_type(ik_fight_state_type(fight, &f)),
        move_type(f.cur_move_type),
        f.ctrl ? "true" : "false",
        static_cast<int>(f.anim), anim_elem(&k_frames, f.anim, f.anim_time),
        anim_curtime(&k_frames, f.anim, f.anim_time),
        q8(f.x_q8 - 160 * 256), q8(f.y_q8 - IK_FLOOR_Y * 256),
        q8(f.vx_q8 * f.facing), q8(f.vy_q8),
        static_cast<int>(f.facing), static_cast<int>(f.hp),
        static_cast<int>(f.power), juggle_left(f),
        static_cast<unsigned>(f.hit_pause),
        static_cast<unsigned>(f.move_contact_type),
        static_cast<unsigned>(f.move_contact_time),
        chain_id(f.last_hit_id));
    if (f.target_index >= 0 && f.target_index < 2) {
        std::fprintf(out, "[%d]", f.target_index + 1);
    } else {
        std::fputs("[]", out);
    }
    std::fputs(",\"hitdef_targets\":[", out);
    if (f.hitdef_target >= 0 && f.hitdef_target < 2) {
        std::fprintf(out, "%d", f.hitdef_target + 1);
    }
    std::fputs("]}", out);
}

static int parent_id(const ik_entity_pool_t* pool, const ik_entity_t* e) {
    return e ? entity_id(pool, e->parent) : -1;
}

static void print_helper(
    FILE* out, const ik_entity_pool_t* pool, const ik_entity_t* e,
    int helper_index
) {
    std::fprintf(out,
        "{\"player_no\":%u,\"helper_index\":%d,\"id\":%d,"
        "\"helper_id\":%d,\"parent_id\":%d,\"team_side\":%u,"
        "\"state_no\":%d,\"state_time\":%u,"
        "\"state_type\":%d,\"move_type\":%d,\"ctrl\":%s,"
        "\"anim\":%d,\"anim_elem\":%d,\"anim_time\":%u,"
        "\"pos\":[%.6f,%.6f,0],\"vel\":[%.6f,%.6f,0],"
        "\"facing\":%d,\"life\":%d,\"power\":%d,\"juggle\":0,"
        "\"hit_pause\":%u,\"move_contact_type\":%u,"
        "\"move_contact_time\":0,\"gethit_chain_id\":0,"
        "\"targets\":",
        static_cast<unsigned>(e->owner_player), helper_index,
        static_cast<int>(e->id), static_cast<int>(e->id),
        parent_id(pool, e), static_cast<unsigned>(e->owner_player),
        static_cast<int>(e->state_no), static_cast<unsigned>(e->state_time),
        state_type(e->state_type), move_type(e->move_type),
        e->ctrl ? "true" : "false",
        static_cast<int>(e->anim_no),
        anim_elem(&k_frames, e->anim_no, e->anim_time),
        anim_curtime(&k_frames, e->anim_no, e->anim_time),
        q8(e->x_q8 - 160 * 256), q8(e->y_q8 - IK_FLOOR_Y * 256),
        q8(e->vx_q8 * e->facing), q8(e->vy_q8),
        static_cast<int>(e->facing), static_cast<int>(e->life),
        static_cast<int>(e->power), static_cast<unsigned>(e->hit_pause),
        static_cast<unsigned>(e->move_contact));
    print_handle_targets(out, pool, e);
    std::fputs(",\"hitdef_targets\":", out);
    print_target_ids(out, e);
    std::fputc('}', out);
}

static void print_projectile(FILE* out, const ik_entity_t* e) {
    std::fprintf(out,
        "{\"player_no\":%u,\"owner_id\":%u,\"id\":%d,\"status\":1,"
        "\"anim\":%d,\"anim_elem\":%d,\"time\":%u,"
        "\"pos\":[%.6f,%.6f,0],\"vel\":[%.6f,%.6f,0],"
        "\"facing\":%d,\"hits\":%u,\"miss_time\":%u,"
        "\"hit_pause\":%u,\"contact\":%s,\"remove\":%s,"
        "\"remove_time\":%d}",
        static_cast<unsigned>(e->owner_player),
        static_cast<unsigned>(e->owner_player + 1u),
        static_cast<int>(e->id), static_cast<int>(e->anim_no),
        anim_elem(&k_frames, e->anim_no, e->anim_time),
        static_cast<unsigned>(e->state_time),
        q8(e->x_q8 - 160 * 256), q8(e->y_q8 - IK_FLOOR_Y * 256),
        q8(e->vx_q8 * e->facing), q8(e->vy_q8), static_cast<int>(e->facing),
        static_cast<unsigned>(e->projectile_hits_left),
        static_cast<unsigned>(e->projectile_hit_cooldown),
        static_cast<unsigned>(e->hit_pause),
        e->proj_query_contact ? "true" : "false",
        e->remove_time == 0 ? "true" : "false",
        static_cast<int>(e->remove_time));
}

static void emit_frame(
    FILE* out, const ik_fight_t* fight, const ik_entity_pool_t* pool,
    int frame, int seed
) {
    std::fprintf(out,
        "{\"schema\":1,\"frame\":%d,\"tick\":%u,"
        "\"round_state\":%u,\"rand_seed\":%d,\"chars\":[",
        frame, static_cast<unsigned>(fight->frame),
        static_cast<unsigned>(fight->round_state), seed);

    for (int owner = 0; owner < 2; ++owner) {
        if (owner != 0) std::fputc(',', out);
        print_root(out, fight, pool, owner);
        int helper_index = 1;
        for (uint8_t slot = 0; slot < IK_ENTITY_CAPACITY; ++slot) {
            const ik_entity_t* e = &pool->entities[slot];
            if (e->type != IK_ENTITY_HELPER ||
                e->owner_player != static_cast<uint8_t>(owner)) {
                continue;
            }
            std::fputc(',', out);
            print_helper(out, pool, e, helper_index++);
        }
    }
    std::fputs("],\"projectiles\":[", out);
    bool first = true;
    for (int owner = 0; owner < 2; ++owner) {
        for (uint8_t slot = 0; slot < IK_ENTITY_CAPACITY; ++slot) {
            const ik_entity_t* e = &pool->entities[slot];
            if (e->type != IK_ENTITY_PROJECTILE ||
                e->owner_player != static_cast<uint8_t>(owner)) {
                continue;
            }
            if (!first) std::fputc(',', out);
            print_projectile(out, e);
            first = false;
        }
    }
    std::fputs("]}\n", out);
}


struct oracle_input_row {
    uint16_t p1;
    uint16_t p2;
};

static std::vector<oracle_input_row> load_inputs(const char* path, int frames) {
    std::vector<oracle_input_row> rows;
    rows.reserve(static_cast<size_t>(frames));
    FILE* f = std::fopen(path, "rb");
    if (!f) return rows;
    unsigned p1 = 0, p2 = 0;
    while (std::fscanf(f, "%u %u", &p1, &p2) == 2) {
        rows.push_back({
            static_cast<uint16_t>(p1),
            static_cast<uint16_t>(p2)
        });
    }
    std::fclose(f);
    return rows;
}

static sat_pad_state_t pad_from_mask(
    uint16_t logical, uint16_t previous, int facing
) {
    auto held_from = [facing](uint16_t mask) {
        uint16_t held = 0u;
        const bool forward = (mask & (1u << 0)) != 0u;
        const bool back = (mask & (1u << 1)) != 0u;
        if (forward) held |= facing >= 0 ? SAT_PAD_RIGHT : SAT_PAD_LEFT;
        if (back) held |= facing >= 0 ? SAT_PAD_LEFT : SAT_PAD_RIGHT;
        if (mask & (1u << 2)) held |= SAT_PAD_UP;
        if (mask & (1u << 3)) held |= SAT_PAD_DOWN;
        if (mask & (1u << 4)) held |= SAT_PAD_A;
        if (mask & (1u << 5)) held |= SAT_PAD_B;
        if (mask & (1u << 6)) held |= SAT_PAD_C;
        if (mask & (1u << 7)) held |= SAT_PAD_X;
        if (mask & (1u << 8)) held |= SAT_PAD_Y;
        if (mask & (1u << 9)) held |= SAT_PAD_Z;
        if (mask & (1u << 10)) held |= SAT_PAD_START;
        return held;
    };
    sat_pad_state_t pad{};
    pad.held = held_from(logical);
    const uint16_t prev_held = held_from(previous);
    pad.pressed = static_cast<uint16_t>(pad.held & ~prev_held);
    pad.released = static_cast<uint16_t>(prev_held & ~pad.held);
    pad.connected = 1u;
    return pad;
}

/* "p1_life=1,p2_power=1000": same overrides the upstream hook applies. */
static void apply_setup(ik_fight_t* fight, const char* setup) {
    const char* p = setup;
    while (*p) {
        const int side = p[1] - '1';
        const char* key = p + 3;
        const char* eq = std::strchr(key, '=');
        if (p[0] != 'p' || side < 0 || side > 1 || !eq) {
            std::fprintf(stderr, "bad setup near '%s'\n", p);
            std::exit(2);
        }
        char* end = nullptr;
        const long value = std::strtol(eq + 1, &end, 0);
        const size_t len = static_cast<size_t>(eq - key);
        if (len == 4 && std::strncmp(key, "life", 4) == 0) {
            fight->fighters[side].hp = static_cast<int16_t>(value);
        } else if (len == 5 && std::strncmp(key, "power", 5) == 0) {
            fight->fighters[side].power = static_cast<int16_t>(value);
        } else {
            std::fprintf(stderr, "bad setup key near '%s'\n", key);
            std::exit(2);
        }
        p = *end == ',' ? end + 1 : end;
    }
}

int main(int argc, char** argv) {
    if (argc < 5) {
        std::fprintf(
            stderr, "usage: %s TRACE FRAMES SEED INPUTS [SETUP]\n", argv[0]);
        return 2;
    }
    const int frames = std::atoi(argv[2]);
    const int seed = std::atoi(argv[3]);
    if (frames <= 0) return 2;

    const auto inputs = load_inputs(argv[4], frames);
    if (static_cast<int>(inputs.size()) != frames) {
        std::fprintf(
            stderr, "input timeline has %zu rows, expected %d\n",
            inputs.size(), frames);
        return 2;
    }

    FILE* out = std::fopen(argv[1], "wb");
    if (!out) {
        std::perror(argv[1]);
        return 2;
    }

    ik_fight_t fight{};
    static ik_frame_ctx_t ctx;
    ik_fight_controls_t idle{};

    ik_fight_init(&fight, &kfm_cns);
    if (!ik_frame_ctx_init(&ctx, &fight)) {
        std::fclose(out);
        return 3;
    }

    while (fight.round_state != 2u) {
        ik_fight_update(&fight, &idle, &idle, &k_frames, &k_frames);
        if (fight.frame > 3600u) {
            std::fprintf(stderr, "active round state not reached\n");
            std::fclose(out);
            return 4;
        }
    }

    apply_setup(&fight, argc > 5 ? argv[5] : "");

    uint16_t previous[2] = {0u, 0u};
    for (int i = 0; i < frames; ++i) {
        const uint16_t logical[2] = {inputs[i].p1, inputs[i].p2};
        const sat_pad_state_t pad1 = pad_from_mask(
            logical[0], previous[0], fight.fighters[0].facing);
        const sat_pad_state_t pad2 = pad_from_mask(
            logical[1], previous[1], fight.fighters[1].facing);
        previous[0] = logical[0];
        previous[1] = logical[1];

        /* Same per-frame step the console build runs (ikemen_frame.c). */
        ik_frame_step(&ctx, &fight, &pad1, &pad2, &k_frames, &k_frames);
        emit_frame(out, &fight, &ctx.pool, i, seed);
    }

    std::fclose(out);
    return 0;
}
