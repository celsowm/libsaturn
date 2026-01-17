#ifndef SATURN_MATRIX_H
#define SATURN_MATRIX_H

#include "saturn/types.h"

void mat4_identity(Mat4* m);
void mat4_zero(Mat4* m);
void mat4_copy(const Mat4* src, Mat4* dst);
void mat4_mul(const Mat4* a, const Mat4* b, Mat4* result);
void mat4_translate(const Mat4* m, fix16_t x, fix16_t y, fix16_t z, Mat4* result);
void mat4_rotate_x(const Mat4* m, fix16_t angle, Mat4* result);
void mat4_rotate_y(const Mat4* m, fix16_t angle, Mat4* result);
void mat4_rotate_z(const Mat4* m, fix16_t angle, Mat4* result);
void mat4_scale(const Mat4* m, fix16_t x, fix16_t y, fix16_t z, Mat4* result);
void mat4_perspective(fix16_t fov, fix16_t aspect, fix16_t near, fix16_t far, Mat4* result);
void mat4_lookat(const Vec3* eye, const Vec3* center, const Vec3* up, Mat4* result);

#endif
