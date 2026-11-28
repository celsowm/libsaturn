#ifndef HETEROGENEOUS_LIGHTING_H
#define HETEROGENEOUS_LIGHTING_H

#include "saturn/core.h"

enum {
    TILE = 16, MAX_VERTICES = 2048, SIZE_COUNT = 8, REPEATS = 16, TIMEOUT = 12000
};

enum {
    MASTER_C, MASTER_MAC, DUAL_C, DUAL_MAC, DSP_PORTS, DSP_STREAM,
    MASTER_DSP, TRIPLE_EIGHTH, TRIPLE_QUARTER, TRIPLE_THREE_EIGHTHS, MODE_COUNT,
    DUAL_SLAVE_WEIGHTED = MODE_COUNT, TRIPLE_SLAVE_WEIGHTED
};

typedef struct {
    uint32_t total, master, slave, dsp_elapsed, dispatch, join, ports, dsp_vertices, slave_vertices;
} lighting_ticks_t;

extern int32_t normals[MAX_VERTICES][3];
extern int32_t intensity[MAX_VERTICES];

sat_result_t lighting_init(void);
sat_result_t lighting_run(unsigned mode, unsigned count, lighting_ticks_t* ticks,
                          void (*independent_work)(void));
unsigned lighting_mismatches(unsigned count);

#endif
