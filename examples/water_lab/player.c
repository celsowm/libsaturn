#include "lab.h"
#include "example_util.h"

static sat_fx16_t clamp(sat_fx16_t value, sat_fx16_t lo, sat_fx16_t hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

sat_fx16_t lab_floor_height(sat_fx16_t x, sat_fx16_t z) {
    if (x <= LAB_UNIT(-8) || x >= LAB_UNIT(8) || z >= LAB_UNIT(8) ||
        (z <= LAB_UNIT(-8) && (x <= LAB_UNIT(-2) || x >= LAB_UNIT(2))) ||
        (x >= LAB_UNIT(-2) && x <= LAB_UNIT(2) &&
         z >= LAB_UNIT(-2) && z <= LAB_UNIT(1))) return 0;
    if (x >= LAB_UNIT(-1) && x <= LAB_UNIT(1)) {
        if (z > LAB_UNIT(1) && z <= LAB_UNIT(2)) return LAB_UNIT(-1);
        if (z > LAB_UNIT(2) && z <= LAB_UNIT(3)) return LAB_UNIT(-2);
    }
    return LAB_UNIT(-3);
}

void lab_player_reset(lab_player_t* player) {
    *player = (lab_player_t){.position = {LAB_UNIT(4), LAB_UNIT(-3), LAB_UNIT(6)}};
}

void lab_player_step(lab_player_t* player, const sat_pad_state_t* pad,
                     const sat_water_volume_t* volume) {
    const sat_fx16_t speed = SAT_FX16_ONE / 8;
    const int x = !!(pad->held & SAT_PAD_RIGHT) - !!(pad->held & SAT_PAD_LEFT);
    const int z = !!(pad->held & SAT_PAD_DOWN) - !!(pad->held & SAT_PAD_UP);
    const sat_fx16_t floor = lab_floor_height(player->position.x, player->position.z);
    sat_water_sample_t sample = {0};
    sat_example_must(sat_water_volume_sample(volume, &player->position, LAB_UNIT(1), &sample));
    sat_body3_t body = {
        .vel = {x * speed, player->velocity.y, z * speed}
    };
    const sat_body3_params_t physics = {
        .gravity = {0, -SAT_FX16_ONE / 36, 0},
        .max_fall = SAT_FX16_ONE / 2, .drag = SAT_FX16_ONE
    };
    if (sample.fraction > 0) {
        if (pad->held & SAT_PAD_A) body.vel.y += SAT_FX16_ONE / 10;
        if (pad->held & SAT_PAD_B) body.vel.y -= SAT_FX16_ONE / 10;
        sat_example_must(sat_water_volume_apply(volume, &sample, &body.vel));
    } else if ((pad->pressed & SAT_PAD_A) && player->position.y <= floor) {
        body.vel.y = SAT_FX16_ONE / 3;
    }
    sat_body3_step(&body, &physics);
    player->velocity = body.vel;
    const sat_fx16_t next_x = clamp(player->position.x + body.vel.x, LAB_UNIT(-11), LAB_UNIT(11));
    const sat_fx16_t next_z = clamp(player->position.z + body.vel.z, LAB_UNIT(-9), LAB_UNIT(9));
    if (lab_floor_height(next_x, player->position.z) <= player->position.y + SAT_FX16_ONE) {
        player->position.x = next_x;
    }
    if (lab_floor_height(player->position.x, next_z) <= player->position.y + SAT_FX16_ONE) {
        player->position.z = next_z;
    }
    player->position.y += body.vel.y;
    const sat_fx16_t next_floor = lab_floor_height(player->position.x, player->position.z);
    if (player->position.y <= next_floor) {
        player->position.y = next_floor;
        player->velocity.y = 0;
    }
    sat_example_must(sat_water_volume_sample(volume, &player->position, LAB_UNIT(1), &sample));
    player->immersed = sample.fraction;
}
