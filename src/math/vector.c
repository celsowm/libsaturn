#include "saturn/vector.h"
#include "saturn/fixed.h"
#include "saturn/matrix.h"
#include <string.h>
#include <math.h>

static fix16_t fix16_sqrt(fix16_t x) {
    return (fix16_t)(sqrt((double)x / FIX16_ONE) * FIX16_ONE);
}

void vec3_zero(Vec3* v) {
    v->x = 0;
    v->y = 0;
    v->z = 0;
}

void vec3_set(Vec3* v, fix16_t x, fix16_t y, fix16_t z) {
    v->x = x;
    v->y = y;
    v->z = z;
}

void vec3_copy(const Vec3* src, Vec3* dst) {
    memcpy(dst, src, sizeof(Vec3));
}

void vec3_add(const Vec3* a, const Vec3* b, Vec3* result) {
    result->x = a->x + b->x;
    result->y = a->y + b->y;
    result->z = a->z + b->z;
}

void vec3_sub(const Vec3* a, const Vec3* b, Vec3* result) {
    result->x = a->x - b->x;
    result->y = a->y - b->y;
    result->z = a->z - b->z;
}

void vec3_mul(const Vec3* v, fix16_t s, Vec3* result) {
    result->x = fix16_mul(v->x, s);
    result->y = fix16_mul(v->y, s);
    result->z = fix16_mul(v->z, s);
}

fix16_t vec3_dot(const Vec3* a, const Vec3* b) {
    return fix16_mul(a->x, b->x) + fix16_mul(a->y, b->y) + fix16_mul(a->z, b->z);
}

void vec3_cross(const Vec3* a, const Vec3* b, Vec3* result) {
    Vec3 temp;
    temp.x = fix16_mul(a->y, b->z) - fix16_mul(a->z, b->y);
    temp.y = fix16_mul(a->z, b->x) - fix16_mul(a->x, b->z);
    temp.z = fix16_mul(a->x, b->y) - fix16_mul(a->y, b->x);
    vec3_copy(&temp, result);
}

fix16_t vec3_length(const Vec3* v) {
    return fix16_sqrt(fix16_mul(v->x, v->x) + fix16_mul(v->y, v->y) + fix16_mul(v->z, v->z));
}

void vec3_normalize(Vec3* v) {
    fix16_t len = vec3_length(v);
    if (len != 0) {
        fix16_t inv = fix16_div(FIX16_ONE, len);
        v->x = fix16_mul(v->x, inv);
        v->y = fix16_mul(v->y, inv);
        v->z = fix16_mul(v->z, inv);
    }
}

void vec3_transform(const Vec3* v, const Mat4* m, Vec3* result) {
    Vec3 temp;
    temp.x = fix16_mul(v->x, m->m[0][0]) + fix16_mul(v->y, m->m[1][0]) + fix16_mul(v->z, m->m[2][0]) + m->m[3][0];
    temp.y = fix16_mul(v->x, m->m[0][1]) + fix16_mul(v->y, m->m[1][1]) + fix16_mul(v->z, m->m[2][1]) + m->m[3][1];
    temp.z = fix16_mul(v->x, m->m[0][2]) + fix16_mul(v->y, m->m[1][2]) + fix16_mul(v->z, m->m[2][2]) + m->m[3][2];
    vec3_copy(&temp, result);
}

void vec2_zero(Vec2* v) {
    v->x = 0;
    v->y = 0;
}

void vec2_set(Vec2* v, fix16_t x, fix16_t y) {
    v->x = x;
    v->y = y;
}

void vec2_copy(const Vec2* src, Vec2* dst) {
    memcpy(dst, src, sizeof(Vec2));
}

void vec2_add(const Vec2* a, const Vec2* b, Vec2* result) {
    result->x = a->x + b->x;
    result->y = a->y + b->y;
}

void vec2_sub(const Vec2* a, const Vec2* b, Vec2* result) {
    result->x = a->x - b->x;
    result->y = a->y - b->y;
}

fix16_t vec2_dot(const Vec2* a, const Vec2* b) {
    return fix16_mul(a->x, b->x) + fix16_mul(a->y, b->y);
}

fix16_t vec2_length(const Vec2* v) {
    return fix16_sqrt(fix16_mul(v->x, v->x) + fix16_mul(v->y, v->y));
}

void vec2_normalize(Vec2* v) {
    fix16_t len = vec2_length(v);
    if (len != 0) {
        fix16_t inv = fix16_div(FIX16_ONE, len);
        v->x = fix16_mul(v->x, inv);
        v->y = fix16_mul(v->y, inv);
    }
}
