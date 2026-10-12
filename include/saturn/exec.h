#ifndef SATURN_EXEC_H
#define SATURN_EXEC_H

#include <stdint.h>

#include "saturn/cdfs.h"
#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Chain-loading: replace the running program with another program file on the
 * disc, the way the BIOS starts 0.BIN. One disc can then hold several programs
 * (a menu and the games it starts), each owning the whole machine.
 *
 * The file is a program image like 0.BIN (an app.bin: linked at
 * SAT_EXEC_LOAD_ADDRESS, entry at its first byte). sat_exec reads it into Work
 * RAM Low, stops what the running program left going (interrupts back to the
 * BIOS vectors, the Slave SH-2, SCU DMA and DSP, every SCSP slot and the sound
 * CPU, VDP1 drawing, the VDP2 registers and display), then a small copy routine
 * at the top of Work RAM Low moves the image to SAT_EXEC_LOAD_ADDRESS, purges
 * the CPU cache and jumps to it. The new program starts from crt0 as on a cold
 * boot (.bss and .wram_l zeroed); only the RAM cartridge, VRAM/CRAM contents,
 * Sound RAM and backup RAM keep what the old one left there.
 *
 * Work RAM Low is the staging area, so the caller's .wram_l is overwritten:
 * the volume, its device and the CD block behind it must not live there
 * (SAT_ERR_INVALID_ARG). The CD block's progress service and the arrival
 * observer are dropped before the read. */
#define SAT_EXEC_LOAD_ADDRESS 0x06004000u
/* Largest image: the same limit libsaturn_split_cart_image enforces. */
#define SAT_EXEC_MAX_BYTES 0x000F0000u
/* Bytes a program can hand to the next one (sat_exec_arg). */
#define SAT_EXEC_ARG_MAX 16u

/* Runs `disc_path`, passing `arg_bytes` bytes of `arg` (NULL with 0 for none).
 * Returns only when nothing has been stopped yet: SAT_ERR_INVALID_ARG (bad
 * arguments, state in Work RAM Low, a directory, an empty file),
 * SAT_ERR_NOT_FOUND, SAT_ERR_CAPACITY (the file is over SAT_EXEC_MAX_BYTES or
 * the argument over SAT_EXEC_ARG_MAX), or a lookup's I/O error. A read that
 * fails after that point cannot return to a program whose Work RAM Low is
 * gone: the machine is reset through the SMPC, which boots the disc again. */
sat_result_t sat_exec(sat_cdfs_volume_t* volume, const char* disc_path,
                      const void* arg, uint32_t arg_bytes);

/* The argument the previous program passed to sat_exec. Copies up to
 * `capacity` bytes to `out` (may be NULL with 0 to only ask), stores the full
 * size in *out_bytes (may be NULL) and forgets it, so a later reset does not
 * see it again. SAT_ERR_NOT_FOUND when the program was not started by
 * sat_exec (the BIOS booted it, or it already read the argument);
 * SAT_ERR_CAPACITY when `capacity` is too small (nothing is copied or
 * forgotten). A sat_exec with no argument still counts: SAT_OK, 0 bytes. */
sat_result_t sat_exec_arg(void* out, uint32_t capacity, uint32_t* out_bytes);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_EXEC_H */
