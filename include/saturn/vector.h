#ifndef SATURN_VECTOR_H
#define SATURN_VECTOR_H

#include "saturn/types.h"
#include "saturn/shared.h"

void vec3_zero(Vec3* v);
void vec3_set(Vec3* v, fix16_t x, fix16_t y, fix16_t z);
void vec3_copy(const Vec3* src, Vec3* dst);
void vec3_add(const Vec3* a, const Vec3* b, Vec3* result);
void vec3_sub(const Vec3* a, const Vec3* b, Vec3* result);
void vec3_mul(const Vec3* v, fix16_t s, Vec3* result);
fix16_t vec3_dot(const Vec3* a, const Vec3* b);
void vec3_cross(const Vec3* a, const Vec3* b, Vec3* result);
fix16_t vec3_length(const Vec3* v);
void vec3_normalize(Vec3* v);
void vec3_transform(const Vec3* v, const Mat4* m, Vec3* result);

void vec2_zero(Vec2* v);
void vec2_set(Vec2* v, fix16_t x, fix16_t y);
void vec2_copy(const Vec2* src, Vec2* dst);
void vec2_add(const Vec2* a, const Vec2* b, Vec2* result);
void vec2_sub(const Vec2* a, const Vec2* b, Vec2* result);
fix16_t vec2_dot(const Vec2* a, const Vec2* b);
fix16_t vec2_length(const Vec2* v);
void vec2_normalize(Vec2* v);

#endif
