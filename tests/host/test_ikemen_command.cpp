#include <cstdio>
#include <cstdlib>

#include "examples/ikemen_saturn/ikemen_command.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

#define EXPR(op) {op,0u,IK_EXPR_REDIRECT_SELF,0u,0,0}
#define CONST(v) {IK_EXPR_PUSH_CONST,0u,IK_EXPR_REDIRECT_SELF,0u,v,0}
#define LOAD_SELF(field) {IK_EXPR_LOAD_FIELD,field,IK_EXPR_REDIRECT_SELF,0u,0,0}
#define LOAD_P2(field) {IK_EXPR_LOAD_FIELD,field,IK_EXPR_REDIRECT_P2,0u,0,0}
#define LOAD_CMD(id) {IK_EXPR_LOAD_COMMAND,0u,IK_EXPR_REDIRECT_SELF,0u,id,0}

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
    {0u, 0u, CMD_X,        1u, 1u, 1u, 1u, IK_CMD_PATTERN_BUFFER_HITPAUSE | IK_CMD_PATTERN_BUFFER_PAUSE_END, 0u},
    {1u, 1u, CMD_A,        1u, 1u, 1u, 1u, IK_CMD_PATTERN_BUFFER_HITPAUSE | IK_CMD_PATTERN_BUFFER_PAUSE_END, 0u},
    {2u, 2u, CMD_HOLDDOWN, 1u, 1u, 1u, 1u, IK_CMD_PATTERN_BUFFER_HITPAUSE | IK_CMD_PATTERN_BUFFER_PAUSE_END, 0u},
    {3u, 3u, CMD_QCF_X,    4u, 15u, 15u, 1u, IK_CMD_PATTERN_BUFFER_HITPAUSE | IK_CMD_PATTERN_BUFFER_PAUSE_END, 0u},
    {7u, 7u, CMD_FF,       3u, 10u, 10u, 1u, IK_CMD_PATTERN_BUFFER_HITPAUSE | IK_CMD_PATTERN_BUFFER_PAUSE_END, 0u},
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

static void sample(
    ik_command_state_t* s,
    uint16_t held,
    int facing = 1,
    int hp = 0,
    int pause_end = 0
) {
    sat_pad_state_t pad{};
    pad.held = held;
    ik_command_update(
        s, &k_asset, &pad, facing, hp, pause_end);
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

    /* endcmdbuftime freezes an existing command buffer and grants the
     * completion frame one extra tick, matching Ikemen's pausebuf path. */
    ik_command_state_init(&state);
    sample(&state, SAT_PAD_X, 1, 0, 1);
    OK(ik_command_active(&state, &k_asset, CMD_X));
    sample(&state, 0u, 1, 0, 1);
    OK(ik_command_active(&state, &k_asset, CMD_X));
    sample(&state, 0u, 1, 0, 0);
    OK(ik_command_active(&state, &k_asset, CMD_X));
    sample(&state, 0u, 1, 0, 0);
    OK(!ik_command_active(&state, &k_asset, CMD_X));

    static const ik_state_rule_instr_t rule_code[] = {
        LOAD_CMD(CMD_X),
        LOAD_CMD(CMD_HOLDDOWN),
        EXPR(IK_EXPR_NOT),
        EXPR(IK_EXPR_AND),

        LOAD_SELF(IK_EXPR_FIELD_STATE_TYPE),
        CONST(1),
        EXPR(IK_EXPR_EQ),
        LOAD_SELF(IK_EXPR_FIELD_CTRL),
        EXPR(IK_EXPR_AND),

        LOAD_SELF(IK_EXPR_FIELD_STATE_NO),
        CONST(200),
        EXPR(IK_EXPR_EQ),
        LOAD_SELF(IK_EXPR_FIELD_STATE_TIME),
        CONST(6),
        EXPR(IK_EXPR_GT),
        EXPR(IK_EXPR_AND),

        EXPR(IK_EXPR_OR),
        EXPR(IK_EXPR_AND),
    };
    static const ik_state_rule_t rules[] = {
        {0u,(uint8_t)(sizeof(rule_code)/sizeof(rule_code[0])),200,0u}
    };
    static const ik_state_rule_asset_t rule_asset = {
        rule_code,(uint16_t)(sizeof(rule_code)/sizeof(rule_code[0])),
        rules,1u
    };

    ik_command_state_init(&state);
    sample(&state,SAT_PAD_X);
    ik_state_rule_context_t ctx{};
    ctx.state_no=0;
    ctx.state_time=1;
    ctx.state_type=1;
    ctx.ctrl=1;
    int16_t requested=0;
    OK(ik_command_eval_state_change(
        &state,&k_asset,&rule_asset,&ctx,&requested));
    OK(requested==200);

    ik_command_state_init(&state);
    sample(&state,SAT_PAD_DOWN|SAT_PAD_X);
    requested=0;
    OK(!ik_command_eval_state_change(
        &state,&k_asset,&rule_asset,&ctx,&requested));

    ik_command_state_init(&state);
    sample(&state,SAT_PAD_X);
    ctx={};
    ctx.state_no=200;
    ctx.state_time=7;
    ctx.state_type=1;
    OK(ik_command_eval_state_change(
        &state,&k_asset,&rule_asset,&ctx,&requested));

    static const ik_state_rule_instr_t throw_code[] = {
        LOAD_CMD(CMD_X),
        LOAD_SELF(IK_EXPR_FIELD_STATE_TYPE),
        CONST(1),
        EXPR(IK_EXPR_EQ),
        EXPR(IK_EXPR_AND),

        LOAD_SELF(IK_EXPR_FIELD_CTRL),
        EXPR(IK_EXPR_AND),

        LOAD_SELF(IK_EXPR_FIELD_STATE_NO),
        CONST(100),
        EXPR(IK_EXPR_NE),
        EXPR(IK_EXPR_AND),

        LOAD_P2(IK_EXPR_FIELD_BODY_DIST_X),
        CONST(3),
        EXPR(IK_EXPR_LT),
        EXPR(IK_EXPR_AND),

        LOAD_P2(IK_EXPR_FIELD_STATE_TYPE),
        CONST(1),
        EXPR(IK_EXPR_EQ),
        LOAD_P2(IK_EXPR_FIELD_STATE_TYPE),
        CONST(2),
        EXPR(IK_EXPR_EQ),
        EXPR(IK_EXPR_OR),
        EXPR(IK_EXPR_AND),

        LOAD_P2(IK_EXPR_FIELD_MOVE_TYPE),
        CONST(3),
        EXPR(IK_EXPR_NE),
        EXPR(IK_EXPR_AND),
    };
    static const ik_state_rule_t throw_rules[] = {
        {0u,(uint8_t)(sizeof(throw_code)/sizeof(throw_code[0])),800,0u}
    };
    static const ik_state_rule_asset_t throw_asset = {
        throw_code,(uint16_t)(sizeof(throw_code)/sizeof(throw_code[0])),
        throw_rules,1u
    };

    ik_command_state_init(&state);
    sample(&state,SAT_PAD_X);
    ctx={};
    ctx.state_no=0;
    ctx.state_time=1;
    ctx.state_type=1;
    ctx.ctrl=1;
    ctx.p2_body_dist_x=2;
    ctx.p2_state_type=1;
    ctx.p2_move_type=1;
    requested=0;
    OK(ik_command_eval_state_change(
        &state,&k_asset,&throw_asset,&ctx,&requested));
    OK(requested==800);

    ctx.p2_body_dist_x=3;
    OK(!ik_command_eval_state_change(
        &state,&k_asset,&throw_asset,&ctx,&requested));
    ctx.p2_body_dist_x=2;
    ctx.p2_move_type=3;
    OK(!ik_command_eval_state_change(
        &state,&k_asset,&throw_asset,&ctx,&requested));

    static const ik_state_rule_instr_t power_code[] = {
        LOAD_CMD(CMD_X),
        LOAD_SELF(IK_EXPR_FIELD_POWER),
        CONST(330),
        EXPR(IK_EXPR_GE),
        EXPR(IK_EXPR_AND),
    };
    static const ik_state_rule_t power_rules[] = {
        {0u,(uint8_t)(sizeof(power_code)/sizeof(power_code[0])),1020,0u}
    };
    static const ik_state_rule_asset_t power_asset = {
        power_code,(uint16_t)(sizeof(power_code)/sizeof(power_code[0])),
        power_rules,1u
    };

    ik_command_state_init(&state);
    sample(&state,SAT_PAD_X);
    ctx={};
    ctx.power=329;
    requested=0;
    OK(!ik_command_eval_state_change(
        &state,&k_asset,&power_asset,&ctx,&requested));
    ctx.power=330;
    OK(ik_command_eval_state_change(
        &state,&k_asset,&power_asset,&ctx,&requested));
    OK(requested==1020);

    /* command_gate: a rule whose expression is always true is still skipped
     * while its gated command is inactive, and always evaluated at gate 0. */
    static const ik_state_rule_instr_t always_code[] = { CONST(1) };
    static const ik_state_rule_t gated_rules[] = {
        {0u,1u,111,(uint8_t)(CMD_X + 1u)},   /* needs command X */
        {0u,1u,222,0u},                      /* ungated */
    };
    static const ik_state_rule_asset_t gated_asset = {
        always_code,1u,gated_rules,2u
    };
    ctx={};
    ik_command_state_init(&state);
    requested=0;
    /* X idle: the gated rule is skipped, so the ungated rule below wins. */
    OK(ik_command_eval_state_change(
        &state,&k_asset,&gated_asset,&ctx,&requested));
    OK(requested==222);
    /* X pressed: the gated rule is evaluated first and wins. */
    sample(&state,SAT_PAD_X);
    OK(ik_command_active(&state,&k_asset,CMD_X));
    OK(ik_command_eval_state_change(
        &state,&k_asset,&gated_asset,&ctx,&requested));
    OK(requested==111);
    /* A gated rule with nothing after it reports "no change" when idle. */
    static const ik_state_rule_asset_t gated_only = {
        always_code,1u,gated_rules,1u
    };
    ik_command_state_init(&state);
    OK(!ik_command_eval_state_change(
        &state,&k_asset,&gated_only,&ctx,&requested));

    std::puts("[test] ikemen_command OK");
    return 0;
}
