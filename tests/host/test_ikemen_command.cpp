#include <cstdio>
#include <cstdlib>

#include "examples/ikemen_saturn/ikemen_command.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

enum {
    CMD_X = 0,
    CMD_A,
    CMD_HOLDDOWN,
    CMD_QCF_X,
    CMD_FF
};

static const ik_cmd_key_spec_t k_keys[] = {
    {IK_CMD_KEY_X, 0u, 0u, 0u},
    {IK_CMD_KEY_A, 0u, 0u, 0u},
    {IK_CMD_KEY_D, IK_CMD_KEY_SLASH | IK_CMD_KEY_DOLLAR, 0u, 0u},

    {IK_CMD_KEY_D, IK_CMD_KEY_TILDE, 0u, 0u},
    {IK_CMD_KEY_DF, 0u, 0u, 0u},
    {IK_CMD_KEY_F, 0u, 0u, 0u},
    {IK_CMD_KEY_X, 0u, 0u, 0u},

    {IK_CMD_KEY_F, 0u, 0u, 0u},
    {IK_CMD_KEY_F, IK_CMD_KEY_TILDE, 0u, 0u},
    {IK_CMD_KEY_F, 0u, 0u, 0u},
};

static const ik_cmd_step_t k_steps[] = {
    {0u, 1u, 0u},
    {1u, 1u, 0u},
    {2u, 1u, 0u},

    {3u, 1u, 0u},
    {4u, 1u, 0u},
    {5u, 1u, 0u},
    {6u, 1u, 0u},

    {7u, 1u, 0u},
    {8u, 1u, IK_CMD_STEP_GREATER},
    {9u, 1u, IK_CMD_STEP_GREATER},
};

static const uint8_t k_loop[] = {
    0u,
    0u,
    0u,
    2u, 3u, 0u, 1u,
    2u, 1u, 0u
};

static const ik_cmd_pattern_t k_patterns[] = {
    {0u, 0u, CMD_X,        1u, 1u, 1u, 1u, IK_CMD_PATTERN_BUFFER_HITPAUSE, 0u},
    {1u, 1u, CMD_A,        1u, 1u, 1u, 1u, IK_CMD_PATTERN_BUFFER_HITPAUSE, 0u},
    {2u, 2u, CMD_HOLDDOWN, 1u, 1u, 1u, 1u, IK_CMD_PATTERN_BUFFER_HITPAUSE, 0u},
    {3u, 3u, CMD_QCF_X,    4u, 15u, 15u, 1u, IK_CMD_PATTERN_BUFFER_HITPAUSE, 0u},
    {7u, 7u, CMD_FF,       3u, 10u, 10u, 1u, IK_CMD_PATTERN_BUFFER_HITPAUSE, 0u},
};

static const ik_cmd_name_t k_names[] = {
    {"x", 0u, 1u, 0u},
    {"a", 1u, 1u, 0u},
    {"holddown", 2u, 1u, 0u},
    {"QCF_x", 3u, 1u, 0u},
    {"FF", 4u, 1u, 0u},
};

static const ik_command_asset_t k_asset = {
    k_keys, (uint16_t)(sizeof(k_keys) / sizeof(k_keys[0])),
    k_steps, (uint16_t)(sizeof(k_steps) / sizeof(k_steps[0])),
    k_loop, (uint16_t)(sizeof(k_loop) / sizeof(k_loop[0])),
    k_patterns, (uint16_t)(sizeof(k_patterns) / sizeof(k_patterns[0])),
    k_names, (uint16_t)(sizeof(k_names) / sizeof(k_names[0]))
};

static void sample(ik_command_state_t* s, uint16_t held, int facing = 1, int hp = 0) {
    sat_pad_state_t pad{};
    pad.held = held;
    ik_command_update(s, &k_asset, &pad, facing, hp);
}

int main() {
    ik_command_state_t state{};
    ik_command_state_init(&state);

    OK(ik_command_find(&k_asset, "QCF_x") == CMD_QCF_X);
    OK(ik_command_find(&k_asset, "missing") == IK_CMD_INVALID_ID);

    sample(&state, SAT_PAD_X);
    OK(ik_command_active(&state, &k_asset, CMD_X));
    sample(&state, 0u);
    OK(!ik_command_active(&state, &k_asset, CMD_X));

    ik_command_state_init(&state);
    sample(&state, SAT_PAD_DOWN);
    OK(ik_command_active(&state, &k_asset, CMD_HOLDDOWN));
    sample(&state, SAT_PAD_DOWN);
    OK(ik_command_active(&state, &k_asset, CMD_HOLDDOWN));

    /* Canonical Ikemen/MUGEN quarter-circle: ~D, DF, F, x. */
    ik_command_state_init(&state);
    sample(&state, SAT_PAD_DOWN);
    OK(!ik_command_active(&state, &k_asset, CMD_QCF_X));
    sample(&state, SAT_PAD_DOWN | SAT_PAD_RIGHT);
    OK(!ik_command_active(&state, &k_asset, CMD_QCF_X));
    sample(&state, SAT_PAD_RIGHT | SAT_PAD_X);
    OK(ik_command_active(&state, &k_asset, CMD_QCF_X));

    /* Facing left flips B/F exactly like Ikemen's character input buffer. */
    ik_command_state_init(&state);
    sample(&state, SAT_PAD_DOWN, -1);
    sample(&state, SAT_PAD_DOWN | SAT_PAD_LEFT, -1);
    sample(&state, SAT_PAD_LEFT | SAT_PAD_X, -1);
    OK(ik_command_active(&state, &k_asset, CMD_QCF_X));

    /* AutoGreater expansion of F,F -> F, >~F, >F. */
    ik_command_state_init(&state);
    sample(&state, SAT_PAD_RIGHT);
    sample(&state, 0u);
    sample(&state, SAT_PAD_RIGHT);
    OK(ik_command_active(&state, &k_asset, CMD_FF));

    /* Completion buffer is frozen during hit pause. */
    ik_command_state_init(&state);
    sample(&state, SAT_PAD_X);
    OK(ik_command_active(&state, &k_asset, CMD_X));
    sample(&state, 0u, 1, 1);
    OK(ik_command_active(&state, &k_asset, CMD_X));
    sample(&state, 0u, 1, 0);
    OK(!ik_command_active(&state, &k_asset, CMD_X));

    std::puts("[test] ikemen_command OK");
    return 0;
}
