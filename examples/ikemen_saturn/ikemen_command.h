#ifndef IKEMEN_COMMAND_H
#define IKEMEN_COMMAND_H

#include <stdint.h>

#include "saturn/input.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IK_CMD_INVALID_ID 0xFFFFu
#define IK_CMD_MAX_PATTERNS 64u
#define IK_CMD_MAX_STEPS_PER_PATTERN 16u

typedef enum ik_cmd_key {
    IK_CMD_KEY_U = 0,
    IK_CMD_KEY_D,
    IK_CMD_KEY_B,
    IK_CMD_KEY_F,
    IK_CMD_KEY_L,
    IK_CMD_KEY_R,
    IK_CMD_KEY_UB,
    IK_CMD_KEY_UF,
    IK_CMD_KEY_DB,
    IK_CMD_KEY_DF,
    IK_CMD_KEY_UL,
    IK_CMD_KEY_UR,
    IK_CMD_KEY_DL,
    IK_CMD_KEY_DR,
    IK_CMD_KEY_N,
    IK_CMD_KEY_A,
    IK_CMD_KEY_BTN_B,
    IK_CMD_KEY_C,
    IK_CMD_KEY_X,
    IK_CMD_KEY_Y,
    IK_CMD_KEY_Z,
    IK_CMD_KEY_S,
    IK_CMD_KEY_BTN_D,
    IK_CMD_KEY_W,
    IK_CMD_KEY_M
} ik_cmd_key_t;

enum {
    IK_CMD_KEY_SLASH = 1u << 0,
    IK_CMD_KEY_TILDE = 1u << 1,
    IK_CMD_KEY_DOLLAR = 1u << 2
};

enum {
    IK_CMD_STEP_GREATER = 1u << 0,
    IK_CMD_STEP_OR = 1u << 1
};

enum {
    IK_CMD_PATTERN_BUFFER_HITPAUSE = 1u << 0
};

typedef enum ik_cmd_rule_op {
    IK_CMD_RULE_COMMAND_ACTIVE = 1,
    IK_CMD_RULE_COMMAND_INACTIVE,
    IK_CMD_RULE_STATE_TYPE_EQ,
    IK_CMD_RULE_STATE_TYPE_NE,
    IK_CMD_RULE_STATE_NO_EQ,
    IK_CMD_RULE_STATE_NO_NE,
    IK_CMD_RULE_STATE_NO_RANGE,
    IK_CMD_RULE_STATE_TIME_EQ,
    IK_CMD_RULE_STATE_TIME_GT,
    IK_CMD_RULE_STATE_TIME_GE,
    IK_CMD_RULE_STATE_TIME_LT,
    IK_CMD_RULE_STATE_TIME_LE,
    IK_CMD_RULE_CTRL,
    IK_CMD_RULE_MOVE_CONTACT,
    IK_CMD_RULE_NOT,
    IK_CMD_RULE_AND,
    IK_CMD_RULE_OR
} ik_cmd_rule_op_t;

typedef struct ik_cmd_key_spec {
    uint8_t key;
    uint8_t flags;
    uint8_t charge_time;
    uint8_t reserved;
} ik_cmd_key_spec_t;

typedef struct ik_cmd_step {
    uint16_t key_ofs;
    uint8_t key_count;
    uint8_t flags;
} ik_cmd_step_t;

typedef struct ik_cmd_pattern {
    uint16_t step_ofs;
    uint16_t loop_ofs;
    uint16_t name_id;
    uint8_t step_count;
    uint8_t max_time;
    uint8_t max_step_time;
    uint8_t buffer_time;
    uint8_t flags;
    uint8_t reserved;
} ik_cmd_pattern_t;

typedef struct ik_cmd_name {
    const char* name;
    uint16_t pattern_ofs;
    uint8_t pattern_count;
    uint8_t reserved;
} ik_cmd_name_t;

typedef struct ik_command_asset {
    const ik_cmd_key_spec_t* keys;
    uint16_t key_count;
    const ik_cmd_step_t* steps;
    uint16_t step_count;
    const uint8_t* loop_order;
    uint16_t loop_count;
    const ik_cmd_pattern_t* patterns;
    uint16_t pattern_count;
    const ik_cmd_name_t* names;
    uint16_t name_count;
} ik_command_asset_t;

typedef struct ik_state_rule_instr {
    uint8_t op;
    uint8_t reserved;
    int16_t value0;
    int16_t value1;
} ik_state_rule_instr_t;

typedef struct ik_state_rule {
    uint16_t instruction_ofs;
    uint8_t instruction_count;
    int16_t target_state;
    uint8_t reserved;
} ik_state_rule_t;

typedef struct ik_state_rule_asset {
    const ik_state_rule_instr_t* instructions;
    uint16_t instruction_count;
    const ik_state_rule_t* rules;
    uint16_t rule_count;
} ik_state_rule_asset_t;

typedef struct ik_state_rule_context {
    int16_t state_no;
    uint16_t state_time;
    uint8_t state_type;
    uint8_t ctrl;
    uint8_t move_contact;
    uint8_t reserved;
} ik_state_rule_context_t;

typedef struct ik_command_pattern_state {
    uint16_t completed_mask;
    uint8_t step_timer[IK_CMD_MAX_STEPS_PER_PATTERN];
    uint8_t cur_time;
    uint8_t buffer_time;
} ik_command_pattern_state_t;

typedef struct ik_command_input_state {
    int16_t current[17];
    int16_t previous[17];
} ik_command_input_state_t;

typedef struct ik_command_state {
    ik_command_input_state_t input;
    ik_command_pattern_state_t patterns[IK_CMD_MAX_PATTERNS];
} ik_command_state_t;

void ik_command_state_init(ik_command_state_t* state);
uint16_t ik_command_find(const ik_command_asset_t* asset, const char* name);
int ik_command_active(const ik_command_state_t* state,
                      const ik_command_asset_t* asset,
                      uint16_t name_id);
int ik_command_eval_state_change(const ik_command_state_t* state,
                                 const ik_command_asset_t* commands,
                                 const ik_state_rule_asset_t* rules,
                                 const ik_state_rule_context_t* context,
                                 int16_t* out_state);
void ik_command_update(ik_command_state_t* state,
                       const ik_command_asset_t* asset,
                       const sat_pad_state_t* pad,
                       int facing,
                       int hit_pause);

#ifdef __cplusplus
}
#endif

#endif
