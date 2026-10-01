#include <cstdio>
#include <cstdlib>

#include "examples/ikemen_saturn/ikemen_entity.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

int main() {
    ik_entity_pool_t pool{};
    ik_entity_pool_init(&pool);

    ik_entity_handle_t p1{}, p2{}, helper{}, projectile{};
    OK(ik_entity_spawn(
        &pool, IK_ENTITY_PLAYER, 1, 0u,
        ik_entity_invalid_handle(), &p1));
    OK(ik_entity_spawn(
        &pool, IK_ENTITY_PLAYER, 2, 1u,
        ik_entity_invalid_handle(), &p2));
    OK(ik_entity_spawn(
        &pool, IK_ENTITY_HELPER, 100, 0u, p1, &helper));
    OK(ik_entity_spawn(
        &pool, IK_ENTITY_PROJECTILE, 200, 0u, helper, &projectile));

    OK(ik_entity_count_type(&pool, IK_ENTITY_PLAYER) == 2u);
    OK(ik_entity_count_type(&pool, IK_ENTITY_HELPER) == 1u);
    OK(ik_entity_handle_equal(
        ik_entity_redirect(&pool, helper, IK_EXPR_REDIRECT_PARENT), p1));
    OK(ik_entity_handle_equal(
        ik_entity_redirect(&pool, projectile, IK_EXPR_REDIRECT_ROOT), p1));
    OK(ik_entity_handle_equal(
        ik_entity_redirect(&pool, p1, IK_EXPR_REDIRECT_P2), p2));

    OK(ik_entity_set_target(&pool, p1, helper));
    OK(ik_entity_handle_equal(
        ik_entity_redirect(&pool, p1, IK_EXPR_REDIRECT_TARGET), helper));

    ik_entity_clear_targets(&pool, p1);
    OK(ik_entity_add_target(&pool, p1, helper, 10));
    OK(ik_entity_add_target(&pool, p1, p2, 20));
    OK(ik_entity_add_target(&pool, p1, projectile, 20));
    OK(ik_entity_handle_equal(
        ik_entity_target_at(&pool, p1, 20, 0u), p2));
    OK(ik_entity_handle_equal(
        ik_entity_target_at(&pool, p1, 20, 1u), projectile));
    OK(ik_entity_remove_target(&pool, p1, p2));
    OK(ik_entity_handle_equal(
        ik_entity_target_at(&pool, p1, 20, 0u), projectile));
    OK(ik_entity_handle_equal(
        ik_entity_redirect(&pool, p1, IK_EXPR_REDIRECT_TARGET), helper));
    OK(ik_entity_target_count(&pool, p1, -1) == 2u);
    OK(ik_entity_target_count(&pool, p1, 20) == 1u);
    OK(ik_entity_handle_equal(
        ik_entity_redirect_target(&pool, p1, 20, 0u), projectile));

    ik_entity_expr_binding_t binding{&pool, p1};

    const ik_expr_instr_t target_state_code[] = {
        {IK_EXPR_LOAD_FIELD, IK_EXPR_FIELD_STATE_NO,
         IK_EXPR_REDIRECT_TARGET, 0u, 0, 20},
    };
    ik_entity_t* ep = ik_entity_get(&pool, projectile);
    OK(ep != nullptr);
    ep->state_no = 3456;
    ik_expr_context_t expr_ctx{
        &binding, ik_entity_expr_read_field, nullptr
    };
    int32_t target_state = 0;
    OK(ik_expr_eval(
        target_state_code, 1u, &expr_ctx, &target_state));
    OK(target_state == 3456);

    OK(ik_entity_add_target(&pool, p1, p2, 20));
    OK(ik_entity_target_count(&pool, p1, 20) == 2u);
    ik_entity_drop_targets(&pool, p1, 20, 1u);
    OK(ik_entity_target_count(&pool, p1, -1) == 1u);
    OK(ik_entity_target_count(&pool, p1, 20) == 1u);
    OK(ik_entity_handle_equal(
        ik_entity_redirect_target(&pool, p1, 20, 0u), projectile));

    ik_entity_t* e1 = ik_entity_get(&pool, p1);
    ik_entity_t* e2 = ik_entity_get(&pool, p2);
    ik_entity_t* eh = ik_entity_get(&pool, helper);
    OK(e1 && e2 && eh);
    e1->x_q8 = 100 * IK_ENTITY_Q8_ONE;
    e1->facing = 1;
    e1->push_front = 16;
    e1->vars[1] = 42;
    e2->x_q8 = 150 * IK_ENTITY_Q8_ONE;
    e2->push_front = 16;
    e2->state_no = 5000;
    eh->vars[3] = 77;

    int32_t value = 0;
    OK(ik_entity_expr_read_field(
        &binding, IK_EXPR_REDIRECT_P2, IK_EXPR_FIELD_BODY_DIST_X,
        0, -1, 0u, &value));
    OK(value == 18);
    OK(ik_entity_expr_read_field(
        &binding, IK_EXPR_REDIRECT_SELF, IK_EXPR_FIELD_VAR,
        1, -1, 0u, &value));
    OK(value == 42);

    binding.self = helper;
    OK(ik_entity_expr_read_field(
        &binding, IK_EXPR_REDIRECT_SELF, IK_EXPR_FIELD_VAR,
        3, -1, 0u, &value));
    OK(value == 77);

    const ik_entity_handle_t stale = helper;
    OK(ik_entity_destroy(&pool, helper));
    OK(ik_entity_get(&pool, stale) == nullptr);
    ik_entity_handle_t replacement{};
    OK(ik_entity_spawn(
        &pool, IK_ENTITY_EXPLOD, 300, 0u, p1, &replacement));
    OK(replacement.slot == stale.slot);
    OK(replacement.generation != stale.generation);
    OK(ik_entity_get(&pool, stale) == nullptr);

    std::puts("[test] ikemen_entity OK");
    return 0;
}
