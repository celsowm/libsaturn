#ifndef SATURN_VIDEO_MEMORY_H
#define SATURN_VIDEO_MEMORY_H
/* Where the VDP1 command list and the Gouraud tables are staged in Work RAM (optional; most programs never include this).
 *
 * Each frame the library builds the command list (32 bytes per command) and the Gouraud tables (8 bytes each) in Work
 * RAM and copies them to VDP1 VRAM in sat_end_frame. The library supplies the table and the storage it names
 * (src/core/runtime/video_memory.cpp): 2048 commands and 2048 Gouraud tables, the most the VRAM areas hold, which is
 * 64 KiB + 16 KiB of Work RAM. A program that knows it never draws that much can define the table itself in one
 * source file with its own storage; the linker then leaves the library's table, and its storage, out of the program:
 *
 *     #include "saturn/video_memory.h"
 *     static uint32_t commands[1500u * SAT_VDP1_COMMAND_WORDS];
 *     static uint16_t gouraud[1200u * SAT_VDP1_GOURAUD_WORDS];
 *     SAT_VIDEO_MEMORY(commands, 1500u, gouraud, 1200u);
 *
 * Drawing past the capacity is the same as drawing past the default one: the draw call returns SAT_ERR_CAPACITY.
 * A capacity above what the VRAM areas hold (2048 commands, 2048 Gouraud tables) is clamped to it. */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAT_VDP1_COMMAND_WORDS 8u      /* 32 bytes, as 32-bit words */
#define SAT_VDP1_GOURAUD_WORDS 4u      /* 8 bytes, as 16-bit words */

typedef struct sat_video_memory {
    uint32_t* commands;                /* command_capacity * SAT_VDP1_COMMAND_WORDS words, 4-byte aligned */
    uint16_t command_capacity;
    uint16_t gouraud_capacity;
    uint16_t* gouraud;                 /* gouraud_capacity * SAT_VDP1_GOURAUD_WORDS words, 2-byte aligned */
} sat_video_memory_t;

extern const sat_video_memory_t sat_video_memory;

#ifdef __cplusplus
}
#endif

#define SAT_VIDEO_MEMORY(command_storage, command_count, gouraud_storage, gouraud_count) \
    const sat_video_memory_t sat_video_memory = {(command_storage), (command_count), (gouraud_count), (gouraud_storage)}

#endif
