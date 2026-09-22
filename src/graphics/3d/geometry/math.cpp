#include "saturn/math3d.h"

#include "src/graphics/3d/geometry/math_logic.hpp"

using namespace saturn::core::math3d;

extern "C" sat_fx16_t sat_fx16_from_int(int32_t v) {
    return fx_from_int(v);
}

extern "C" int32_t sat_fx16_to_int(sat_fx16_t v) {
    return fx_to_int(v);
}

extern "C" sat_fx16_t sat_fx16_mul(sat_fx16_t a, sat_fx16_t b) {
    return fx_mul(a, b);
}

extern "C" sat_fx16_t sat_fx16_div(sat_fx16_t a, sat_fx16_t b) {
    return fx_div(a, b);
}

extern "C" sat_fx16_t sat_fx16_sqrt(sat_fx16_t v) {
    return fx_sqrt(v);
}

extern "C" sat_fx16_t sat_sin_deg(sat_fx16_t degrees) {
    return sin_deg_fx(degrees);
}

extern "C" sat_fx16_t sat_cos_deg(sat_fx16_t degrees) {
    return cos_deg_fx(degrees);
}

extern "C" sat_fx16_t sat_tan_deg(sat_fx16_t degrees) {
    return tan_deg_fx(degrees);
}

extern "C" sat_result_t sat_mat4_identity(sat_mat4_t* out) {
    if (out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    mat4_identity(out->m);
    return SAT_OK;
}

extern "C" sat_result_t sat_mat4_multiply(sat_mat4_t* out, const sat_mat4_t* a, const sat_mat4_t* b) {
    if (out == nullptr || a == nullptr || b == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    mat4_multiply(out->m, a->m, b->m);
    return SAT_OK;
}

extern "C" sat_result_t sat_mat4_translate(sat_mat4_t* out, sat_fx16_t tx, sat_fx16_t ty, sat_fx16_t tz) {
    if (out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    mat4_translate(out->m, tx, ty, tz);
    return SAT_OK;
}

extern "C" sat_result_t sat_mat4_scale(sat_mat4_t* out, sat_fx16_t sx, sat_fx16_t sy, sat_fx16_t sz) {
    if (out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    mat4_scale(out->m, sx, sy, sz);
    return SAT_OK;
}

extern "C" sat_result_t sat_mat4_rotate_x(sat_mat4_t* out, sat_fx16_t degrees) {
    if (out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    mat4_rotate_x(out->m, degrees);
    return SAT_OK;
}

extern "C" sat_result_t sat_mat4_rotate_y(sat_mat4_t* out, sat_fx16_t degrees) {
    if (out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    mat4_rotate_y(out->m, degrees);
    return SAT_OK;
}

extern "C" sat_result_t sat_mat4_rotate_z(sat_mat4_t* out, sat_fx16_t degrees) {
    if (out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    mat4_rotate_z(out->m, degrees);
    return SAT_OK;
}

extern "C" sat_result_t sat_mat4_look_at(
    sat_mat4_t* out,
    const sat_vec3_t* eye,
    const sat_vec3_t* center,
    const sat_vec3_t* up
) {
    if (out == nullptr || eye == nullptr || center == nullptr || up == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    mat4_look_at(out->m, eye->x, eye->y, eye->z, center->x, center->y, center->z,
                 up->x, up->y, up->z);
    return SAT_OK;
}

extern "C" sat_result_t sat_mat4_perspective(
    sat_mat4_t* out,
    sat_fx16_t fov_y,
    sat_fx16_t aspect,
    sat_fx16_t near_z,
    sat_fx16_t far_z
) {
    if (out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    return mat4_perspective(out->m, fov_y, aspect, near_z, far_z);
}

extern "C" sat_result_t sat_mat4_transform_vec4(
    const sat_mat4_t* matrix,
    const sat_vec4_t* v,
    sat_vec4_t* out
) {
    if (matrix == nullptr || v == nullptr || out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    *out = mat4_transform_vec4(matrix->m, v->x, v->y, v->z, v->w);
    return SAT_OK;
}

extern "C" sat_result_t sat_project_to_screen(
    const sat_mat4_t* view_proj,
    sat_fx16_t x,
    sat_fx16_t y,
    sat_fx16_t z,
    int16_t screen_w,
    int16_t screen_h,
    int16_t* out_x,
    int16_t* out_y
) {
    if (view_proj == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    return project_to_screen(view_proj->m, x, y, z, screen_w, screen_h, out_x, out_y);
}

/* ------------------------------------------------------------------ */
/* Vector helpers                                                      */
/* ------------------------------------------------------------------ */

extern "C" void sat_vec3_set(sat_vec3_t* out, sat_fx16_t x, sat_fx16_t y, sat_fx16_t z) {
    if (out == nullptr) {
        return;
    }
    *out = saturn::core::math3d::vec3(x, y, z);
}

extern "C" void sat_vec3_add(sat_vec3_t* out, const sat_vec3_t* a, const sat_vec3_t* b) {
    if (out == nullptr || a == nullptr || b == nullptr) {
        return;
    }
    *out = saturn::core::math3d::vec3_add(*a, *b);
}

extern "C" void sat_vec3_sub(sat_vec3_t* out, const sat_vec3_t* a, const sat_vec3_t* b) {
    if (out == nullptr || a == nullptr || b == nullptr) {
        return;
    }
    *out = saturn::core::math3d::vec3_sub(*a, *b);
}

extern "C" void sat_vec3_scale(sat_vec3_t* out, const sat_vec3_t* a, sat_fx16_t s) {
    if (out == nullptr || a == nullptr) {
        return;
    }
    *out = saturn::core::math3d::vec3_scale(*a, s);
}

extern "C" sat_fx16_t sat_vec3_dot(const sat_vec3_t* a, const sat_vec3_t* b) {
    if (a == nullptr || b == nullptr) {
        return 0;
    }
    return saturn::core::math3d::vec3_dot(*a, *b);
}

extern "C" void sat_vec3_cross(sat_vec3_t* out, const sat_vec3_t* a, const sat_vec3_t* b) {
    if (out == nullptr || a == nullptr || b == nullptr) {
        return;
    }
    *out = saturn::core::math3d::vec3_cross(*a, *b);
}

extern "C" sat_fx16_t sat_vec3_length(const sat_vec3_t* v) {
    if (v == nullptr) {
        return 0;
    }
    return saturn::core::math3d::vec3_length(*v);
}

extern "C" void sat_vec3_normalize(sat_vec3_t* out, const sat_vec3_t* v) {
    if (out == nullptr || v == nullptr) {
        return;
    }
    *out = saturn::core::math3d::vec3_normalize(*v);
}
