#include "saturn/follow_camera3d.h"

#include <stdint.h>

namespace {
sat_fx16_t approach(sat_fx16_t current, sat_fx16_t desired, uint8_t divisor) {
    const int64_t delta=static_cast<int64_t>(desired)-current;
    return static_cast<sat_fx16_t>(current+delta/divisor);
}
}

extern "C" sat_result_t sat_follow_camera3d_init(
    sat_follow_camera3d_t* controller,
    const sat_vec3_t* anchor,
    uint8_t follow_x,
    uint8_t follow_y,
    uint8_t follow_z) {
    if (!controller || !anchor || !follow_x || !follow_y || !follow_z)
        return SAT_ERR_INVALID_ARG;
    *controller={};
    controller->anchor=*anchor;
    controller->follow_x=follow_x;
    controller->follow_y=follow_y;
    controller->follow_z=follow_z;
    return SAT_OK;
}

extern "C" sat_result_t sat_follow_camera3d_set_offsets(
    sat_follow_camera3d_t* controller,
    const sat_vec3_t* eye_offset,
    const sat_vec3_t* target_offset) {
    if (!controller || !eye_offset || !target_offset ||
        !controller->follow_x || !controller->follow_y || !controller->follow_z)
        return SAT_ERR_INVALID_ARG;
    controller->eye_offset=*eye_offset;
    controller->target_offset=*target_offset;
    return SAT_OK;
}

extern "C" sat_result_t sat_follow_camera3d_step(
    sat_follow_camera3d_t* controller,
    const sat_vec3_t* desired,
    uint8_t snap,
    sat_vec3_t* out_eye,
    sat_vec3_t* out_target) {
    if (!controller || !desired || !out_eye || !out_target || snap>1u ||
        !controller->follow_x || !controller->follow_y || !controller->follow_z)
        return SAT_ERR_INVALID_ARG;
    if (snap) {
        controller->anchor=*desired;
    } else {
        controller->anchor.x=approach(controller->anchor.x,desired->x,
                                       controller->follow_x);
        controller->anchor.y=approach(controller->anchor.y,desired->y,
                                       controller->follow_y);
        controller->anchor.z=approach(controller->anchor.z,desired->z,
                                       controller->follow_z);
    }
    out_eye->x=controller->anchor.x+controller->eye_offset.x;
    out_eye->y=controller->anchor.y+controller->eye_offset.y;
    out_eye->z=controller->anchor.z+controller->eye_offset.z;
    out_target->x=controller->anchor.x+controller->target_offset.x;
    out_target->y=controller->anchor.y+controller->target_offset.y;
    out_target->z=controller->anchor.z+controller->target_offset.z;
    return SAT_OK;
}
