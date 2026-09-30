#include "ikemen_expr.h"

static int push(int32_t* stack, uint8_t* sp, int32_t value) {
    if (!stack || !sp || *sp >= IK_EXPR_STACK_CAPACITY) return 0;
    stack[(*sp)++] = value;
    return 1;
}

static int unary(int32_t* stack, uint8_t sp, uint8_t op) {
    if (!stack || sp < 1u) return 0;
    switch ((ik_expr_op_t)op) {
        case IK_EXPR_NOT:
            stack[sp - 1u] = stack[sp - 1u] == 0;
            return 1;
        case IK_EXPR_NEG:
            stack[sp - 1u] = -stack[sp - 1u];
            return 1;
        default:
            return 0;
    }
}

static int binary(int32_t* stack, uint8_t* sp, uint8_t op) {
    if (!stack || !sp || *sp < 2u) return 0;
    const int32_t rhs = stack[*sp - 1u];
    const int32_t lhs = stack[*sp - 2u];
    int32_t result = 0;

    switch ((ik_expr_op_t)op) {
        case IK_EXPR_EQ: result = lhs == rhs; break;
        case IK_EXPR_NE: result = lhs != rhs; break;
        case IK_EXPR_LT: result = lhs < rhs; break;
        case IK_EXPR_LE: result = lhs <= rhs; break;
        case IK_EXPR_GT: result = lhs > rhs; break;
        case IK_EXPR_GE: result = lhs >= rhs; break;
        case IK_EXPR_AND: result = lhs != 0 && rhs != 0; break;
        case IK_EXPR_OR: result = lhs != 0 || rhs != 0; break;
        case IK_EXPR_ADD: result = lhs + rhs; break;
        case IK_EXPR_SUB: result = lhs - rhs; break;
        case IK_EXPR_MUL: result = lhs * rhs; break;
        case IK_EXPR_DIV:
            if (rhs == 0) return 0;
            result = lhs / rhs;
            break;
        case IK_EXPR_MOD:
            if (rhs == 0) return 0;
            result = lhs % rhs;
            break;
        default:
            return 0;
    }

    stack[*sp - 2u] = result;
    --(*sp);
    return 1;
}

int ik_expr_eval(
    const ik_expr_instr_t* code,
    uint16_t count,
    const ik_expr_context_t* context,
    int32_t* out_value
) {
    int32_t stack[IK_EXPR_STACK_CAPACITY];
    uint8_t sp = 0u;

    if (!code || !context || !out_value) return 0;

    for (uint16_t i = 0u; i < count; ++i) {
        const ik_expr_instr_t* ins = &code[i];
        int32_t value = 0;

        switch ((ik_expr_op_t)ins->op) {
            case IK_EXPR_PUSH_CONST:
                if (!push(stack, &sp, ins->value0)) return 0;
                break;

            case IK_EXPR_LOAD_FIELD:
                if (!context->read_field ||
                    !context->read_field(
                        context->user, ins->redirect, ins->field,
                        ins->value0, &value) ||
                    !push(stack, &sp, value)) {
                    return 0;
                }
                break;

            case IK_EXPR_LOAD_COMMAND:
                if (!context->read_command ||
                    !context->read_command(
                        context->user, (uint16_t)ins->value0, &value) ||
                    !push(stack, &sp, value)) {
                    return 0;
                }
                break;

            case IK_EXPR_NOT:
            case IK_EXPR_NEG:
                if (!unary(stack, sp, ins->op)) return 0;
                break;

            case IK_EXPR_EQ:
            case IK_EXPR_NE:
            case IK_EXPR_LT:
            case IK_EXPR_LE:
            case IK_EXPR_GT:
            case IK_EXPR_GE:
            case IK_EXPR_AND:
            case IK_EXPR_OR:
            case IK_EXPR_ADD:
            case IK_EXPR_SUB:
            case IK_EXPR_MUL:
            case IK_EXPR_DIV:
            case IK_EXPR_MOD:
                if (!binary(stack, &sp, ins->op)) return 0;
                break;

            default:
                return 0;
        }
    }

    if (sp != 1u) return 0;
    *out_value = stack[0];
    return 1;
}
