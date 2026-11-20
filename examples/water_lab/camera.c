#include "lab.h"
#include "example_util.h"

void lab_camera_update(sat_camera3d_t* camera, const lab_player_t* player,
                       uint32_t tick, int32_t orbit, uint8_t automatic) {
    sat_vec3_t target = {0, LAB_UNIT(-1), LAB_UNIT(-2)};
    sat_fx16_t yaw = LAB_UNIT(orbit);
    sat_fx16_t radius = LAB_UNIT(23);
    sat_fx16_t height = LAB_UNIT(11);
    if (automatic == 1) {
        yaw += 29 * sat_sin_deg(LAB_UNIT(tick % 360u));
    } else if (automatic == 2) {
        target = (sat_vec3_t){LAB_UNIT(3), LAB_UNIT(-1), LAB_UNIT(-5)};
        radius = LAB_UNIT(9);
        height = -SAT_FX16_ONE / 2;
    } else {
        target = player->position;
        target.y += LAB_UNIT(1);
        radius = LAB_UNIT(8);
        height = LAB_UNIT(4);
    }
    const sat_vec3_t eye = {
        target.x + sat_fx16_mul(radius, sat_sin_deg(yaw)),
        target.y + height,
        target.z + sat_fx16_mul(radius, sat_cos_deg(yaw))
    };
    const sat_vec3_t up = {0, SAT_FX16_ONE, 0};
    sat_example_must(sat_camera3d_init(
        camera, &eye, &target, &up, LAB_UNIT(51),
        sat_fx16_div(LAB_UNIT(LAB_WIDTH), LAB_UNIT(LAB_HEIGHT)),
        LAB_NEAR, LAB_UNIT(90)));
}
