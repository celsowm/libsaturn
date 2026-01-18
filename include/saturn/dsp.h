#ifndef SATURN_DSP_H
#define SATURN_DSP_H

#include "saturn/types.h"
#include "saturn/shared.h"

typedef enum {
    DSP_OP_NOP = 0,
    DSP_OP_LOAD,
    DSP_OP_STORE,
    DSP_OP_MOV,
    DSP_OP_ADD,
    DSP_OP_SUB,
    DSP_OP_MUL,
    DSP_OP_MAC,
    DSP_OP_AND,
    DSP_OP_OR,
    DSP_OP_XOR,
    DSP_OP_NOT
} DspOpCode;

typedef struct {
    u32 prog_addr;
    u32 ram_addr;
} DspRegs;

void dsp_init(void);
void dsp_reset(void);
void dsp_start(void);
void dsp_stop(void);
void dsp_wait(void);

void dsp_load_program(const u32* code, u32 size);
void dsp_execute_program(void);

void dsp_set_register(u8 reg, u32 value);
u32 dsp_get_register(u8 reg);

void dsp_matrix_mul(const Mat4* a, const Mat4* b, Mat4* result);
void dsp_vector_transform(const Vec3* vec, const Mat4* mat, Vec3* result);

#endif
