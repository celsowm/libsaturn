#ifndef IKEMEN_EXPR_H
#define IKEMEN_EXPR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IK_EXPR_STACK_CAPACITY 32u

typedef enum ik_expr_redirect {
    IK_EXPR_REDIRECT_SELF = 0,
    IK_EXPR_REDIRECT_P2,
    IK_EXPR_REDIRECT_PARENT,
    IK_EXPR_REDIRECT_ROOT,
    IK_EXPR_REDIRECT_TARGET
} ik_expr_redirect_t;

typedef enum ik_expr_field {
    IK_EXPR_FIELD_STATE_NO = 1,
    IK_EXPR_FIELD_STATE_TIME,
    IK_EXPR_FIELD_STATE_TYPE,
    IK_EXPR_FIELD_MOVE_TYPE,
    IK_EXPR_FIELD_CTRL,
    IK_EXPR_FIELD_MOVE_CONTACT,
    IK_EXPR_FIELD_POWER,
    IK_EXPR_FIELD_LIFE,
    IK_EXPR_FIELD_BODY_DIST_X,
    IK_EXPR_FIELD_POS_X_Q8,
    IK_EXPR_FIELD_POS_Y_Q8,
    IK_EXPR_FIELD_VEL_X_Q8,
    IK_EXPR_FIELD_VEL_Y_Q8,
    IK_EXPR_FIELD_FACING,
    IK_EXPR_FIELD_ENTITY_ID,
    IK_EXPR_FIELD_ENTITY_TYPE,
    IK_EXPR_FIELD_VAR,
    IK_EXPR_FIELD_FVAR_Q16,
    IK_EXPR_FIELD_SYSVAR
} ik_expr_field_t;

typedef enum ik_expr_op {
    IK_EXPR_PUSH_CONST = 1,
    IK_EXPR_LOAD_FIELD,
    IK_EXPR_LOAD_COMMAND,
    IK_EXPR_EQ,
    IK_EXPR_NE,
    IK_EXPR_LT,
    IK_EXPR_LE,
    IK_EXPR_GT,
    IK_EXPR_GE,
    IK_EXPR_NOT,
    IK_EXPR_AND,
    IK_EXPR_OR,
    IK_EXPR_ADD,
    IK_EXPR_SUB,
    IK_EXPR_MUL,
    IK_EXPR_DIV,
    IK_EXPR_MOD,
    IK_EXPR_NEG
} ik_expr_op_t;

typedef struct ik_expr_instr {
    uint8_t op;
    uint8_t field;
    uint8_t redirect;
    uint8_t reserved;
    int32_t value0;
    int32_t value1;
} ik_expr_instr_t;

typedef int (*ik_expr_read_field_fn)(
    void* user,
    uint8_t redirect,
    uint8_t field,
    int16_t index,
    int32_t* out_value);

typedef int (*ik_expr_read_command_fn)(
    void* user,
    uint16_t command_id,
    int32_t* out_value);

typedef struct ik_expr_context {
    void* user;
    ik_expr_read_field_fn read_field;
    ik_expr_read_command_fn read_command;
} ik_expr_context_t;

int ik_expr_eval(
    const ik_expr_instr_t* code,
    uint16_t count,
    const ik_expr_context_t* context,
    int32_t* out_value);

#ifdef __cplusplus
}
#endif

#endif
