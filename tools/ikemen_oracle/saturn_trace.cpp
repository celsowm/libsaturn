#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "examples/ikemen_saturn/ikemen_anim.h"
#include "examples/ikemen_saturn/ikemen_cns.h"
#include "examples/ikemen_saturn/ikemen_command.h"
#include "examples/ikemen_saturn/ikemen_entity.h"
#include "examples/ikemen_saturn/ikemen_fight.h"
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
    uint32_t first = 0, count = 0;
    if (!ik_frames_bounds(table, action, &first, &count) || count == 0) {
        return 0;
    }
    uint32_t remaining = time;
    for (uint32_t i = 0; i < count; ++i) {
        const uint16_t ticks = ik_frame_ticks(&table->frames[first + i]);
        if (ticks == 0u || remaining < ticks) return static_cast<int>(i + 1u);
        remaining -= ticks;
    }
    return static_cast<int>(count);
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
        "\"move_contact_time\":0,\"gethit_chain_id\":%d,"
        "\"targets\":",
        p, mirror ? static_cast<int>(mirror->id) : p + 1, p,
        static_cast<int>(f.state), static_cast<unsigned>(f.state_time),
        state_type(ik_fight_state_type(fight, &f)),
        move_type(mirror ? mirror->move_type : 0),
        f.ctrl ? "true" : "false",
        static_cast<int>(f.anim), anim_elem(&k_frames, f.anim, f.anim_time),
        static_cast<unsigned>(f.anim_time),
        q8(f.x_q8 - 160 * 256), q8(f.y_q8 - IK_FLOOR_Y * 256),
        q8(f.vx_q8), q8(f.vy_q8),
        static_cast<int>(f.facing), static_cast<int>(f.hp),
        static_cast<int>(f.power), static_cast<int>(f.juggle_points),
        static_cast<unsigned>(f.hit_pause),
        static_cast<unsigned>(f.move_contact),
        static_cast<int>(f.last_hit_id));
    if (f.target_index >= 0 && f.target_index < 2) {
        std::fprintf(out, "[%d]", f.target_index + 1);
    } else {
        std::fputs("[]", out);
    }
    std::fputs(",\"hitdef_targets\":[", out);
    if (f.target_index >= 0 && f.target_index < 2) {
        std::fprintf(out, "%d", static_cast<int>(f.target_id));
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
        static_cast<unsigned>(e->anim_time),
        q8(e->x_q8 - 160 * 256), q8(e->y_q8 - IK_FLOOR_Y * 256),
        q8(e->vx_q8), q8(e->vy_q8),
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
        q8(e->vx_q8), q8(e->vy_q8), static_cast<int>(e->facing),
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

struct oracle_rule_user {
    ik_entity_expr_binding_t entity;
    const ik_command_state_t* commands;
};

static int oracle_read_field(
    void* user,
    uint8_t redirect,
    uint8_t field,
    int32_t index,
    int32_t redirect_id,
    uint8_t redirect_index,
    int32_t* out_value
) {
    if (!user) return 0;
    oracle_rule_user* u = static_cast<oracle_rule_user*>(user);
    return ik_entity_expr_read_field(
        &u->entity, redirect, field, index,
        redirect_id, redirect_index, out_value);
}

static int oracle_read_command(
    void* user, uint16_t command_id, int32_t* out_value
) {
    if (!user || !out_value) return 0;
    const oracle_rule_user* u =
        static_cast<const oracle_rule_user*>(user);
    *out_value = ik_command_active(
        u->commands, &kfm_commands, command_id);
    return 1;
}

static int pause_end_buffer(const ik_fight_t* fight, uint32_t player) {
    if (!fight || player >= 2u || fight->pause_time == 0u ||
        fight->pause_end_cmd_buffer_time == 0u ||
        fight->pause_time > fight->pause_end_cmd_buffer_time) {
        return 0;
    }
    const int can_act =
        fight->pause_owner == static_cast<int8_t>(player) &&
        fight->pause_move_time > 0u;
    return !can_act;
}

static void controls_from_commands(
    uint32_t player,
    const ik_fight_t* fight,
    const ik_entity_pool_t* pool,
    const ik_entity_handle_t roots[2],
    const ik_command_state_t states[2],
    ik_fight_controls_t* controls
) {
    if (!fight || !controls || player >= 2u) return;
    *controls = {};
    const ik_command_state_t* state = &states[player];
    controls->forward = static_cast<uint8_t>(
        ik_command_active(state, &kfm_commands, KFM_CMD_HOLDFWD));
    controls->back = static_cast<uint8_t>(
        ik_command_active(state, &kfm_commands, KFM_CMD_HOLDBACK));
    controls->up = static_cast<uint8_t>(
        ik_command_active(state, &kfm_commands, KFM_CMD_HOLDUP));
    controls->down = static_cast<uint8_t>(
        ik_command_active(state, &kfm_commands, KFM_CMD_HOLDDOWN));
    controls->a = static_cast<uint8_t>(
        ik_command_active(state, &kfm_commands, KFM_CMD_A));
    controls->b = static_cast<uint8_t>(
        ik_command_active(state, &kfm_commands, KFM_CMD_B));
    controls->c = static_cast<uint8_t>(
        ik_command_active(state, &kfm_commands, KFM_CMD_C));
    controls->x = static_cast<uint8_t>(
        ik_command_active(state, &kfm_commands, KFM_CMD_X));
    controls->y = static_cast<uint8_t>(
        ik_command_active(state, &kfm_commands, KFM_CMD_Y));
    controls->z = static_cast<uint8_t>(
        ik_command_active(state, &kfm_commands, KFM_CMD_Z));
    controls->start = static_cast<uint8_t>(
        ik_command_active(state, &kfm_commands, KFM_CMD_START));
    controls->recovery = static_cast<uint8_t>(
        ik_command_active(state, &kfm_commands, KFM_CMD_RECOVERY));

    oracle_rule_user user{
        {pool, roots[player]},
        state
    };
    const ik_expr_context_t expression = {
        &user, oracle_read_field, oracle_read_command
    };
    int16_t requested = 0;
    if (ik_command_eval_state_change_expr(
            &kfm_state_rules, &expression, &requested)) {
        controls->requested_state = requested;
        controls->has_state_request = 1u;
    }
}

int main(int argc, char** argv) {
    if (argc < 6) {
        std::fprintf(
            stderr, "usage: %s TRACE FRAMES SEED INPUTS\n", argv[0]);
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
    ik_entity_pool_t pool{};
    ik_entity_handle_t roots[2]{};
    ik_command_state_t command_states[2]{};
    ik_fight_controls_t idle{};

    ik_fight_init(&fight, &kfm_cns);
    ik_entity_pool_init(&pool);
    if (!ik_entity_spawn(
            &pool, IK_ENTITY_PLAYER, 1, 0u,
            ik_entity_invalid_handle(), &roots[0]) ||
        !ik_entity_spawn(
            &pool, IK_ENTITY_PLAYER, 2, 1u,
            ik_entity_invalid_handle(), &roots[1])) {
        std::fclose(out);
        return 3;
    }
    ik_fight_bind_entities(&fight, &pool, roots[0], roots[1]);
    ik_command_state_init(&command_states[0]);
    ik_command_state_init(&command_states[1]);

    while (fight.round_state != 2u) {
        ik_fight_update(&fight, &idle, &idle, &k_frames, &k_frames);
        if (fight.frame > 3600u) {
            std::fprintf(stderr, "active round state not reached\n");
            std::fclose(out);
            return 4;
        }
    }

    uint16_t previous[2] = {0u, 0u};
    for (int i = 0; i < frames; ++i) {
        const uint16_t logical[2] = {inputs[i].p1, inputs[i].p2};
        ik_fight_controls_t controls[2]{};

        for (uint32_t p = 0u; p < 2u; ++p) {
            const sat_pad_state_t pad = pad_from_mask(
                logical[p], previous[p], fight.fighters[p].facing);
            ik_command_update(
                &command_states[p], &kfm_commands, &pad,
                fight.fighters[p].facing,
                fight.fighters[p].hit_pause != 0u,
                pause_end_buffer(&fight, p));
            controls_from_commands(
                p, &fight, &pool, roots,
                command_states, &controls[p]);
            previous[p] = logical[p];
        }

        ik_fight_update(
            &fight, &controls[0], &controls[1],
            &k_frames, &k_frames);
        emit_frame(out, &fight, &pool, i, seed);
    }

    std::fclose(out);
    return 0;
}
