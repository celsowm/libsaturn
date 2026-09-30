#include <cstdio>
#include <cstdlib>

#include "examples/ikemen_saturn/ikemen_expr.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

struct Fixture {
    int32_t self_state = 200;
    int32_t p2_state = 5000;
    int32_t power = 330;
    int32_t command = 1;
};

static int read_field(void* user, uint8_t redirect, uint8_t field,
                      int16_t, int32_t* out) {
    Fixture* f = static_cast<Fixture*>(user);
    if (!f || !out) return 0;
    if (field == IK_EXPR_FIELD_STATE_NO) {
        *out = redirect == IK_EXPR_REDIRECT_P2
            ? f->p2_state : f->self_state;
        return 1;
    }
    if (field == IK_EXPR_FIELD_POWER) {
        *out = f->power;
        return 1;
    }
    return 0;
}

static int read_command(void* user, uint16_t id, int32_t* out) {
    Fixture* f = static_cast<Fixture*>(user);
    if (!f || !out || id != 7u) return 0;
    *out = f->command;
    return 1;
}

int main() {
    Fixture fixture{};
    const ik_expr_context_t ctx = {
        &fixture, read_field, read_command
    };

    const ik_expr_instr_t program[] = {
        {IK_EXPR_LOAD_COMMAND,0u,IK_EXPR_REDIRECT_SELF,0u,7,0},
        {IK_EXPR_LOAD_FIELD,IK_EXPR_FIELD_POWER,IK_EXPR_REDIRECT_SELF,0u,0,0},
        {IK_EXPR_PUSH_CONST,0u,0u,0u,330,0},
        {IK_EXPR_GE,0u,0u,0u,0,0},
        {IK_EXPR_AND,0u,0u,0u,0,0},
        {IK_EXPR_LOAD_FIELD,IK_EXPR_FIELD_STATE_NO,IK_EXPR_REDIRECT_P2,0u,0,0},
        {IK_EXPR_PUSH_CONST,0u,0u,0u,5000,0},
        {IK_EXPR_EQ,0u,0u,0u,0,0},
        {IK_EXPR_AND,0u,0u,0u,0,0},
    };

    int32_t value = 0;
    OK(ik_expr_eval(
        program, (uint16_t)(sizeof(program) / sizeof(program[0])),
        &ctx, &value));
    OK(value == 1);

    fixture.power = 329;
    OK(ik_expr_eval(
        program, (uint16_t)(sizeof(program) / sizeof(program[0])),
        &ctx, &value));
    OK(value == 0);

    const ik_expr_instr_t arithmetic[] = {
        {IK_EXPR_PUSH_CONST,0u,0u,0u,7,0},
        {IK_EXPR_PUSH_CONST,0u,0u,0u,5,0},
        {IK_EXPR_ADD,0u,0u,0u,0,0},
        {IK_EXPR_PUSH_CONST,0u,0u,0u,3,0},
        {IK_EXPR_MUL,0u,0u,0u,0,0},
        {IK_EXPR_PUSH_CONST,0u,0u,0u,36,0},
        {IK_EXPR_EQ,0u,0u,0u,0,0},
    };
    OK(ik_expr_eval(
        arithmetic,
        (uint16_t)(sizeof(arithmetic) / sizeof(arithmetic[0])),
        &ctx, &value));
    OK(value == 1);

    const ik_expr_instr_t bad_div[] = {
        {IK_EXPR_PUSH_CONST,0u,0u,0u,1,0},
        {IK_EXPR_PUSH_CONST,0u,0u,0u,0,0},
        {IK_EXPR_DIV,0u,0u,0u,0,0},
    };
    OK(!ik_expr_eval(
        bad_div, (uint16_t)(sizeof(bad_div) / sizeof(bad_div[0])),
        &ctx, &value));

    std::puts("[test] ikemen_expr OK");
    return 0;
}
