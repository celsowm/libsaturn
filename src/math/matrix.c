#include "saturn/matrix.h"
#include "saturn/fixed.h"
#include <string.h>

void mat4_identity(Mat4* m) {
    memset(m, 0, sizeof(Mat4));
    m->m[0][0] = FIX16_ONE;
    m->m[1][1] = FIX16_ONE;
    m->m[2][2] = FIX16_ONE;
    m->m[3][3] = FIX16_ONE;
}

void mat4_zero(Mat4* m) {
    memset(m, 0, sizeof(Mat4));
}

void mat4_copy(const Mat4* src, Mat4* dst) {
    memcpy(dst, src, sizeof(Mat4));
}

void mat4_mul(const Mat4* a, const Mat4* b, Mat4* result) {
    Mat4 temp;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            fix16_t sum = 0;
            for (int k = 0; k < 4; k++) {
                sum += fix16_mul(a->m[i][k], b->m[k][j]);
            }
            temp.m[i][j] = sum;
        }
    }
    mat4_copy(&temp, result);
}

void mat4_translate(const Mat4* m, fix16_t x, fix16_t y, fix16_t z, Mat4* result) {
    Mat4 temp;
    mat4_identity(&temp);
    temp.m[3][0] = x;
    temp.m[3][1] = y;
    temp.m[3][2] = z;
    mat4_mul(m, &temp, result);
}

void mat4_rotate_x(const Mat4* m, fix16_t angle, Mat4* result) {
    Mat4 temp;
    mat4_identity(&temp);
    fix16_t s = fix16_sin(angle);
    fix16_t c = fix16_cos(angle);
    temp.m[1][1] = c;
    temp.m[1][2] = -s;
    temp.m[2][1] = s;
    temp.m[2][2] = c;
    mat4_mul(m, &temp, result);
}

void mat4_rotate_y(const Mat4* m, fix16_t angle, Mat4* result) {
    Mat4 temp;
    mat4_identity(&temp);
    fix16_t s = fix16_sin(angle);
    fix16_t c = fix16_cos(angle);
    temp.m[0][0] = c;
    temp.m[0][2] = s;
    temp.m[2][0] = -s;
    temp.m[2][2] = c;
    mat4_mul(m, &temp, result);
}

void mat4_rotate_z(const Mat4* m, fix16_t angle, Mat4* result) {
    Mat4 temp;
    mat4_identity(&temp);
    fix16_t s = fix16_sin(angle);
    fix16_t c = fix16_cos(angle);
    temp.m[0][0] = c;
    temp.m[0][1] = -s;
    temp.m[1][0] = s;
    temp.m[1][1] = c;
    mat4_mul(m, &temp, result);
}

void mat4_scale(const Mat4* m, fix16_t x, fix16_t y, fix16_t z, Mat4* result) {
    Mat4 temp;
    mat4_identity(&temp);
    temp.m[0][0] = x;
    temp.m[1][1] = y;
    temp.m[2][2] = z;
    mat4_mul(m, &temp, result);
}

void mat4_perspective(fix16_t fov, fix16_t aspect, fix16_t near, fix16_t far, Mat4* result) {
    fix16_t f = fix16_div(FIX16_ONE, fix16_tan(fov >> 1));
    mat4_zero(result);
    result->m[0][0] = fix16_div(f, aspect);
    result->m[1][1] = f;
    result->m[2][2] = fix16_div(far + near, near - far);
    result->m[2][3] = FIX16_ONE;
    result->m[3][2] = fix16_mul(fix16_mul(far << 1, near), fix16_div(FIX16_ONE, near - far));
}

void mat4_lookat(const Vec3* eye, const Vec3* center, const Vec3* up, Mat4* result) {
    Vec3 f, s, u;
    Vec3 temp;
    
    vec3_sub(center, eye, &temp);
    vec3_normalize(&temp);
    vec3_copy(&temp, &f);
    
    vec3_cross(&f, up, &s);
    vec3_normalize(&s);
    
    vec3_cross(&s, &f, &u);
    
    mat4_identity(result);
    result->m[0][0] = s.x;
    result->m[0][1] = s.y;
    result->m[0][2] = s.z;
    result->m[1][0] = u.x;
    result->m[1][1] = u.y;
    result->m[1][2] = u.z;
    result->m[2][0] = -f.x;
    result->m[2][1] = -f.y;
    result->m[2][2] = -f.z;
    result->m[3][0] = -fix16_mul(vec3_dot(&s, eye), FIX16_ONE);
    result->m[3][1] = -fix16_mul(vec3_dot(&u, eye), FIX16_ONE);
    result->m[3][2] = fix16_mul(vec3_dot(&f, eye), FIX16_ONE);
}

static fix16_t fix16_tan(fix16_t angle) {
    return fix16_div(fix16_sin(angle), fix16_cos(angle));
}
