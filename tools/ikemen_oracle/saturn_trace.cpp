#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "examples/ikemen_saturn/ikemen_anim.h"
#include "examples/ikemen_saturn/ikemen_cns.h"
#include "examples/ikemen_saturn/ikemen_entity.h"
#include "examples/ikemen_saturn/ikemen_fight.h"
#include "ikemen_saturn/kfm_cns.h"
#include "ikemen_saturn/kfm_frames.h"

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
        p, mirror ? static_cast<int>(mirror->id) : p + 1, p + 1,
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
        parent_id(pool, e), static_cast<unsigned>(e->owner_player + 1u),
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

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s TRACE FRAMES [SEED]\n", argv[0]);
        return 2;
    }
    const int frames = std::atoi(argv[2]);
    const int seed = argc >= 4 ? std::atoi(argv[3]) : 1;
    if (frames <= 0) return 2;

    FILE* out = std::fopen(argv[1], "wb");
    if (!out) {
        std::perror(argv[1]);
        return 2;
    }

    ik_fight_t fight{};
    ik_entity_pool_t pool{};
    ik_entity_handle_t p1{};
    ik_entity_handle_t p2{};
    ik_fight_controls_t idle{};

    ik_fight_init(&fight, &kfm_cns);
    ik_entity_pool_init(&pool);
    if (!ik_entity_spawn(
            &pool, IK_ENTITY_PLAYER, 1, 0u,
            ik_entity_invalid_handle(), &p1) ||
        !ik_entity_spawn(
            &pool, IK_ENTITY_PLAYER, 2, 1u,
            ik_entity_invalid_handle(), &p2)) {
        std::fclose(out);
        return 3;
    }
    ik_fight_bind_entities(&fight, &pool, p1, p2);

    while (fight.round_state != 2u) {
        ik_fight_update(&fight, &idle, &idle, &k_frames, &k_frames);
        if (fight.frame > 3600u) {
            std::fprintf(stderr, "active round state not reached\n");
            std::fclose(out);
            return 4;
        }
    }

    for (int i = 0; i < frames; ++i) {
        ik_fight_update(&fight, &idle, &idle, &k_frames, &k_frames);
        emit_frame(out, &fight, &pool, i, seed);
    }

    std::fclose(out);
    return 0;
}
