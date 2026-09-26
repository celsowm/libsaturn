#ifndef SATURN_SCU_DSP_H
#define SATURN_SCU_DSP_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The SCU DSP: a 32-bit signal processor inside the SCU with a 256-word
 * program RAM and four 64-word data RAMs, running beside the two SH-2s
 * (about 14 MHz, one instruction per step, a 32x32 multiplier feeding a 48-bit
 * accumulator). Programs are written in Sega assembler syntax and assembled
 * by tools/scu_dsp.py, which also simulates them on the host.
 *
 * While a program runs the DSP RAMs cannot be read or written from the CPU:
 * sat_dsp_write_data and friends refuse with SAT_ERR_BUSY. Master SH-2 only. */

#define SAT_DSP_PROGRAM_WORDS 256u
#define SAT_DSP_BANKS 4u
#define SAT_DSP_BANK_WORDS 64u

typedef struct sat_dsp_status {
    uint8_t running;      /* the program is executing */
    uint8_t ended;        /* ENDI ran since the last status read */
    uint8_t dma;          /* a DSP DMA transfer is still moving data */
    uint8_t overflow;     /* ALU flags of the last operation */
    uint8_t carry;
    uint8_t zero;
    uint8_t sign;
    uint8_t pc;           /* program counter */
} sat_dsp_status_t;

/* Writes `count` instruction words into program RAM at `at`. */
sat_result_t sat_dsp_load_program(const uint32_t* words, uint32_t count, uint32_t at);

sat_result_t sat_dsp_write_data(uint32_t bank, uint32_t offset, const uint32_t* words, uint32_t count);
sat_result_t sat_dsp_read_data(uint32_t bank, uint32_t offset, uint32_t* words, uint32_t count);

/* Starts the program at `entry` and returns at once. */
sat_result_t sat_dsp_start(uint32_t entry);

/* Stops a running program where it is. */
sat_result_t sat_dsp_stop(void);

/* Reading the status clears the ended and overflow flags. */
sat_result_t sat_dsp_status(sat_dsp_status_t* out_status);
uint8_t sat_dsp_running(void);

/* Waits for the program to end (END or ENDI) and its DMA to drain, at most
 * `timeout_ticks` FRT ticks (65535 at most). SAT_ERR_TIMEOUT leaves it
 * running. The ENDI form also raises the SCU DSP-end interrupt, which
 * sat_irq can handle instead of polling. */
sat_result_t sat_dsp_wait(uint32_t timeout_ticks);

/* ------------------------------------------------------------------ */
/* Built-in batch transform: out = M * v for integer vectors            */
/* ------------------------------------------------------------------ */

/* The DSP multiplies each integer (x, y, z) by a 3x3 matrix of 16.16 fixed
 * point and returns 16.16 results: every component is m0*x + m1*y + m2*z as a
 * 48-bit product-sum, kept in 32 bits, exactly what the SH-2 gets from 64-bit
 * multiplies. The library program takes over the DSP program RAM and data
 * RAMs; loading another program makes the next set_matrix reload it. */
#define SAT_DSP_TRANSFORM_MAX_VERTICES 21u

/* Loads the transform program if it is not there and stores the matrix (nine
 * 16.16 words, row by row). The DSP must be idle. */
sat_result_t sat_dsp_transform_set_matrix(const int32_t matrix[9]);

/* Copies `count` (1..21) vectors of three words each into the DSP and starts
 * it. Returns at once; the SH-2 is free until sat_dsp_transform_end. */
sat_result_t sat_dsp_transform_begin(const int32_t* xyz, uint32_t count);

/* Waits for the run started by sat_dsp_transform_begin and copies its
 * `count` output vectors (three words each). */
sat_result_t sat_dsp_transform_end(int32_t* out_xyz, uint32_t count, uint32_t timeout_ticks);

/* Transforms any number of vectors, in runs of 21, waiting for each. */
sat_result_t sat_dsp_transform_vertices(const int32_t* xyz, int32_t* out_xyz, uint32_t count,
                                        uint32_t timeout_ticks);

/* The same transform with the data moved by the DSP: it DMAs the vectors out
 * of Work RAM-H, transforms them and DMAs the results back, so the SH-2 only
 * writes a few parameters and is free until the end. Both buffers must lie in
 * Work RAM-H (any cache alias) and be longword aligned, and must stay valid
 * until sat_dsp_transform_end_dma. The results are written behind the cache:
 * the end call drops the stale lines of `out_xyz` before returning. */
sat_result_t sat_dsp_transform_begin_dma(const int32_t* xyz, int32_t* out_xyz, uint32_t count);
sat_result_t sat_dsp_transform_end_dma(uint32_t timeout_ticks);
sat_result_t sat_dsp_transform_vertices_dma(const int32_t* xyz, int32_t* out_xyz, uint32_t count,
                                            uint32_t timeout_ticks);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SCU_DSP_H */
