/* Controller family: Projectile, Explod and Helper spawning. */
#include "ikemen_fight_internal.h"

int ikf_ctrl_apply_spawn(const ik_ctrl_exec_t* x) {
    ik_fight_t* fight = x->fight;
    ik_fighter_t* f = x->fighter;
    const ik_cns_controller_t* ctrl = x->ctrl;
    const uint8_t i = x->index;
    const ik_cns_asset_t* state_cns = x->state_cns;
    const ik_frame_table_t* frames = x->frames;
    const uint16_t command_mask = x->context->command_mask;
    switch ((ik_cns_controller_type_t)ctrl->type) {
        case IK_CNS_CTRL_MODIFY_PROJECTILE: {
            if (ctrl->value1 != 0 && i < 64u) {
                const uint64_t bit = (uint64_t)1u << i;
                if ((f->one_shot_controller_mask & bit) != 0u) {
                    break;
                }
                f->one_shot_controller_mask |= bit;
            }
            if (!fight->entities || !state_cns->projectile_mods ||
                ctrl->value0 < 0 ||
                ctrl->value0 >= state_cns->projectile_mod_count) {
                break;
            }
            const uint8_t owner =
                ikf_fighter_player_index(fight, f);
            if (owner >= 2u) break;
            ikf_sync_fighter_entity(fight, f);
            ik_entity_runtime_t runtime;
            ik_entity_runtime_init(
                &runtime, fight->entities, fight->cns, frames, frames);
            ikf_configure_fight_entity_runtime(fight, &runtime);
            (void)ik_entity_runtime_modify_projectiles(
                &runtime, owner,
                &state_cns->projectile_mods[ctrl->value0]);
            break;
        }

        case IK_CNS_CTRL_PROJECTILE: {
            if (!fight->entities || !state_cns->projectiles ||
                ctrl->value0 < 0 ||
                ctrl->value0 >= state_cns->projectile_count) {
                break;
            }
            if (ctrl->value1 != 0 && i < 64u) {
                const uint64_t bit = (uint64_t)1u << i;
                if ((f->one_shot_controller_mask & bit) != 0u) {
                    break;
                }
                f->one_shot_controller_mask |= bit;
            }
            ikf_sync_fighter_entity(fight, f);
            ik_entity_runtime_t runtime;
            ik_entity_runtime_init(
                &runtime, fight->entities, fight->cns, frames, frames);
            ikf_configure_fight_entity_runtime(fight, &runtime);
            ik_entity_handle_t spawned = ik_entity_invalid_handle();
            (void)ik_entity_runtime_spawn_projectile_spec(
                &runtime, ikf_fighter_entity_handle(fight, f),
                &state_cns->projectiles[ctrl->value0],
                &spawned);
            break;
        }

        case IK_CNS_CTRL_EXPLOD: {
            if (!fight->entities || !state_cns->explods ||
                ctrl->value0 < 0 ||
                ctrl->value0 >= state_cns->explod_count) {
                break;
            }
            if (ctrl->value1 != 0 && i < 64u) {
                const uint64_t bit = (uint64_t)1u << i;
                if ((f->one_shot_controller_mask & bit) != 0u) {
                    break;
                }
                f->one_shot_controller_mask |= bit;
            }
            ikf_sync_fighter_entity(fight, f);
            ik_entity_runtime_t runtime;
            ik_entity_runtime_init(
                &runtime, fight->entities, fight->cns, frames, frames);
            ikf_configure_fight_entity_runtime(fight, &runtime);
            ik_entity_handle_t spawned = ik_entity_invalid_handle();
            (void)ik_entity_runtime_spawn_explod(
                &runtime, ikf_fighter_entity_handle(fight, f),
                &state_cns->explods[ctrl->value0], &spawned);
            break;
        }

        case IK_CNS_CTRL_HELPER: {
            if (!fight->entities || !state_cns->helpers ||
                ctrl->value0 < 0 ||
                ctrl->value0 >= state_cns->helper_count) {
                break;
            }
            ikf_sync_fighter_entity(fight, f);
            ik_entity_runtime_t runtime;
            ik_entity_runtime_init(
                &runtime, fight->entities, fight->cns, frames, frames);
            ikf_configure_fight_entity_runtime(fight, &runtime);
            const uint8_t player = ikf_fighter_player_index(fight, f);
            if (player < 2u) {
                ik_entity_runtime_set_command_mask(
                    &runtime, player, command_mask);
            }
            ik_entity_handle_t spawned = ik_entity_invalid_handle();
            (void)ik_entity_runtime_spawn_helper(
                &runtime, ikf_fighter_entity_handle(fight, f),
                &state_cns->helpers[ctrl->value0], &spawned);
            break;
        }

        case IK_CNS_CTRL_DESTROY_SELF:
            /* Root players are persistent entities. DestroySelf is
             * meaningful for helpers and is executed by entity runtime. */
            break;

        default:
            return IKF_CTRL_UNHANDLED;
    }
    return IKF_CTRL_NEXT;
}
