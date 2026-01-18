#include "saturn/dsp.h"
#include "saturn/shared.h"

void dsp_init(void) {
}

void dsp_reset(void) {
}

void dsp_start(void) {
}

void dsp_stop(void) {
}

void dsp_wait(void) {
}

void dsp_load_program(const u32* code, u32 size) {
    (void)code;
    (void)size;
}

void dsp_execute_program(void) {
}

void dsp_set_register(u8 reg, u32 value) {
    (void)reg;
    (void)value;
}

u32 dsp_get_register(u8 reg) {
    (void)reg;
    return 0;
}

void dsp_matrix_mul(const Mat4* a, const Mat4* b, Mat4* result) {
    extern void mat4_mul(const Mat4*, const Mat4*, Mat4*);
    mat4_mul(a, b, result);
}

void dsp_vector_transform(const Vec3* vec, const Mat4* mat, Vec3* result) {
    extern void vec3_transform(const Vec3*, const Mat4*, Vec3*);
    vec3_transform(vec, mat, result);
}
